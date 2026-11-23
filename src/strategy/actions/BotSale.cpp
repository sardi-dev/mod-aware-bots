/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * The vendor trip of random bots (BotSale.h, BT-149). The decisions are in Utils/BotSalePolicy.h.
 */

#include "BotSale.h"

#include "BotAuctionPolicy.h"
#include "BotGuid.h"
#include "BotSalePolicy.h"
#include "BotToolPolicy.h"
#include "Creature.h"
#include "DBCStores.h"
#include "Item.h"
#include "ItemUsageValue.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"

#include <algorithm>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
    using namespace BotSalePolicy;

    // How far a bot walks to a vendor it can see, in yards (the repairer's range).
    constexpr float VENDOR_RANGE = 60.0f;

    // The answer of FindNearbyVendor is kept this long per bot: the trigger asks every tick while the bot walks.
    constexpr uint32 VENDOR_CACHE_MS = 5000;

    // The item checks of a trip decision run at most this often per bot.
    constexpr uint32 TRIP_EVAL_INTERVAL_MS = 5000;

    // A trip that is not done after this long is given up (a vendor it cannot reach).
    constexpr uint32 TRIP_TIMEOUT_MS = 3 * MINUTE * IN_MILLISECONDS;

    // After a trip that sold or bought something the next evaluation waits a short while; after one that did neither
    // (nothing to sell, the vendor had no food) it waits long, so the bot does not walk the same road again.
    constexpr uint32 PRODUCTIVE_COOLDOWN_MS = 60 * IN_MILLISECONDS;
    constexpr uint32 UNPRODUCTIVE_COOLDOWN_MS = 10 * MINUTE * IN_MILLISECONDS;

    // BT-213: a bot low on supplies that cannot pay even after selling is asked again only after this long, since
    // adding up what it could sell reads every bag item.
    constexpr uint32 CANNOT_PAY_RECHECK_MS = 60 * IN_MILLISECONDS;

    // Mining Pick and Skinning Knife: the tools the bot's gathering needs. Other tools carry a totem category.
    constexpr uint32 MINING_PICK_ITEM = 2901;
    constexpr uint32 SKINNING_KNIFE_ITEM = 7005;

    // The spell of the mage's own food and drink; a mage that knows it conjures instead of buying (BT-029).
    char const* const CONJURE_SPELL = "conjure refreshment";

    char const* const FOOD_KIND = "food";
    char const* const DRINK_KIND = "drink";

    struct TripState
    {
        uint32 tripStartMs = 0;
        uint32 lastEvalMs = 0;
        bool wanted = false;
        uint32 blockedAtMs = 0;
        uint32 blockedForMs = 0;
        uint32 lastLogMs = 0;
        bool logged = false;
        uint32 finderLogMs = 0;
        bool finderLogged = false;
    };

    struct CachedVendor
    {
        uint32 time;
        uint64 vendor;
    };

    // Bots update on the map threads, so the per-bot memory below is shared and locked.
    std::mutex sLock;
    std::unordered_map<uint64, TripState> sTrips;
    std::unordered_map<uint64, CachedVendor> sVendors;

    struct SupplyCounts
    {
        uint32 food = 0;
        uint32 drink = 0;
    };

    struct SupplyOffer
    {
        uint32 slot = 0;
        ItemTemplate const* proto = nullptr;
    };

    template <typename Visitor>
    void ForEachBagItem(Player* bot, Visitor visit)
    {
        for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
            if (Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
                visit(item);

        for (uint8 bagSlot = INVENTORY_SLOT_BAG_START; bagSlot < INVENTORY_SLOT_BAG_END; ++bagSlot)
            if (Bag* bag = static_cast<Bag*>(bot->GetItemByPos(INVENTORY_SLOT_BAG_0, bagSlot)))
                for (uint32 i = 0; i < bag->GetBagSize(); ++i)
                    if (Item* item = bag->GetItemByPos(i))
                        visit(item);
    }

    bool HasTradeSkill(Player* bot, uint32 skill) { return bot->HasSkill(skill) && bot->GetSkillValue(skill) > 0; }

    uint32 ProfessionsOf(Player* bot)
    {
        struct SkillBit
        {
            uint32 skill;
            uint32 bit;
        };
        static SkillBit const skills[] = {
            { SKILL_ALCHEMY, PROFESSION_ALCHEMY },           { SKILL_BLACKSMITHING, PROFESSION_BLACKSMITHING },
            { SKILL_COOKING, PROFESSION_COOKING },           { SKILL_ENGINEERING, PROFESSION_ENGINEERING },
            { SKILL_INSCRIPTION, PROFESSION_INSCRIPTION },   { SKILL_JEWELCRAFTING, PROFESSION_JEWELCRAFTING },
            { SKILL_LEATHERWORKING, PROFESSION_LEATHERWORKING }, { SKILL_TAILORING, PROFESSION_TAILORING }
        };

        uint32 mask = PROFESSION_NONE;
        for (SkillBit const& entry : skills)
            if (HasTradeSkill(bot, entry.skill))
                mask |= entry.bit;
        return mask;
    }

    MaterialKind MaterialKindOf(ItemTemplate const* proto)
    {
        if (proto->Class == ITEM_CLASS_GEM)
            return MaterialKind::Gem;

        if (proto->Class != ITEM_CLASS_TRADE_GOODS)
            return MaterialKind::Other;

        switch (proto->SubClass)
        {
            case ITEM_SUBCLASS_HERB:
                return MaterialKind::Herb;
            case ITEM_SUBCLASS_METAL_STONE:
                return MaterialKind::Metal;
            case ITEM_SUBCLASS_LEATHER:
                return MaterialKind::Leather;
            case ITEM_SUBCLASS_CLOTH:
                return MaterialKind::Cloth;
            case ITEM_SUBCLASS_MEAT:
                return MaterialKind::Meat;
            default:
                return MaterialKind::Other;
        }
    }

    // The skill ids the auction's keep levels use are the client data's; keep them tied to the core's names.
    static_assert(BotAuctionPolicy::Skill::FIRST_AID == SKILL_FIRST_AID && BotAuctionPolicy::Skill::BLACKSMITHING == SKILL_BLACKSMITHING &&
                  BotAuctionPolicy::Skill::LEATHERWORKING == SKILL_LEATHERWORKING && BotAuctionPolicy::Skill::ALCHEMY == SKILL_ALCHEMY &&
                  BotAuctionPolicy::Skill::HERBALISM == SKILL_HERBALISM && BotAuctionPolicy::Skill::COOKING == SKILL_COOKING &&
                  BotAuctionPolicy::Skill::MINING == SKILL_MINING && BotAuctionPolicy::Skill::TAILORING == SKILL_TAILORING &&
                  BotAuctionPolicy::Skill::ENGINEERING == SKILL_ENGINEERING && BotAuctionPolicy::Skill::ENCHANTING == SKILL_ENCHANTING &&
                  BotAuctionPolicy::Skill::FISHING == SKILL_FISHING && BotAuctionPolicy::Skill::SKINNING == SKILL_SKINNING &&
                  BotAuctionPolicy::Skill::JEWELCRAFTING == SKILL_JEWELCRAFTING && BotAuctionPolicy::Skill::INSCRIPTION == SKILL_INSCRIPTION &&
                  BotAuctionPolicy::Skill::ARCHAEOLOGY == SKILL_ARCHAEOLOGY,
                  "BotAuctionPolicy::Skill ids drifted from the core");

    using RecipeUses = std::unordered_map<uint32, std::vector<BotAuctionPolicy::RecipeUse>>;

    void AddUse(std::vector<BotAuctionPolicy::RecipeUse>& list, BotAuctionPolicy::RecipeUse const& use)
    {
        for (BotAuctionPolicy::RecipeUse const& have : list)
            if (have.skill == use.skill && have.spell == use.spell && have.alsoSpell == use.alsoSpell)
                return;
        list.push_back(use);
    }

    // The lowest skill value at which any trainer teaches a spell of a skill, by spell (the recipe a trainer spell teaches
    // counts too, as the core stores it in TrainerSpell::learnedSpell).
    std::unordered_map<uint64, uint32> TrainerRanks()
    {
        std::unordered_map<uint64, uint32> ranks;
        auto const note = [&ranks](uint32 spell, uint32 skill, uint32 rank)
        {
            auto const key = (uint64(skill) << 32) | spell;
            auto const itr = ranks.find(key);
            if (itr == ranks.end() || rank < itr->second)
                ranks[key] = rank;
        };

        for (auto const& entry : *sObjectMgr->GetCreatureTemplates())
        {
            TrainerSpellData const* trainer = sObjectMgr->GetNpcTrainerSpells(entry.first);
            if (!trainer)
                continue;

            for (auto const& [spellId, trainerSpell] : trainer->spellList)
            {
                note(spellId, trainerSpell.reqSkill, trainerSpell.reqSkillValue);
                for (uint32 learned : trainerSpell.learnedSpell)
                    if (learned && learned != spellId)
                        note(learned, trainerSpell.reqSkill, trainerSpell.reqSkillValue);
            }
        }
        return ranks;
    }

    // Every reagent of every recipe of the crafting skills, the secondary skills and Mining (smelting), from the skill
    // line abilities of the client data, plus the ore Jewelcrafting prospects and the herbs Inscription mills: item -> the
    // recipes that take it. Ore smelted into a bar that a crafting skill
    // works also counts for that skill, with the smelting needed too (a smelter feeds its own crafting).
    RecipeUses BuildRecipeUses()
    {
        using namespace BotAuctionPolicy;
        struct Smelting
        {
            std::vector<uint32> reagents;
            std::vector<uint32> products;
            uint32 spell;
            uint32 rank;
        };

        std::unordered_map<uint64, uint32> const trainerRanks = TrainerRanks();
        RecipeUses uses;
        std::vector<Smelting> smeltings;
        for (uint32 i = 0; i < sSkillLineAbilityStore.GetNumRows(); ++i)
        {
            SkillLineAbilityEntry const* ability = sSkillLineAbilityStore.LookupEntry(i);
            if (!ability || !(IsCraftingSkill(ability->skillId) || IsSecondarySkill(ability->skillId) || ability->skillId == Skill::MINING))
                continue;

            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(ability->spellId);
            if (!spellInfo)
                continue;

            // From the rank a trainer teaches it, and from the minimum rank of a recipe the profession gives on learning
            // (the core's own rule, Player::LearnSkillRewardedSpells); otherwise only a bot that knows the spell has it.
            uint32 rank = RECIPE_UNTAUGHT;
            if (auto const itr = trainerRanks.find((uint64(ability->skillId) << 32) | ability->spellId); itr != trainerRanks.end())
                rank = itr->second;
            if (ability->learnOnGetSkill == ABILITY_LEARNED_ON_GET_PROFESSION_SKILL && ability->req_skill_value < rank)
                rank = ability->req_skill_value;

            Smelting smelting;
            smelting.spell = ability->spellId;
            smelting.rank = rank;
            for (uint8 r = 0; r < MAX_SPELL_REAGENTS; ++r)
            {
                if (spellInfo->Reagent[r] <= 0 || !spellInfo->ReagentCount[r])
                    continue;

                AddUse(uses[uint32(spellInfo->Reagent[r])], { ability->skillId, ability->spellId, rank, 0, 0, RECIPE_UNTAUGHT });
                smelting.reagents.push_back(uint32(spellInfo->Reagent[r]));
            }

            if (ability->skillId != Skill::MINING)
                continue;

            for (uint8 e = 0; e < MAX_SPELL_EFFECTS; ++e)
                if (spellInfo->Effects[e].Effect == SPELL_EFFECT_CREATE_ITEM && spellInfo->Effects[e].ItemType)
                    smelting.products.push_back(spellInfo->Effects[e].ItemType);
            if (!smelting.reagents.empty() && !smelting.products.empty())
                smeltings.push_back(std::move(smelting));
        }

        // Prospecting (Jewelcrafting) and Milling (Inscription) take no reagent: the ore or herb is the target of the spell,
        // and the core asks for the skill value in the item's RequiredSkillRank (Spell::CheckCast). A bot knows both spells.
        for (auto const& entry : *sObjectMgr->GetItemTemplateStore())
        {
            if (entry.second.Flags & ITEM_PROTO_FLAG_PROSPECTABLE)
                AddUse(uses[entry.first], { Skill::JEWELCRAFTING, 0, entry.second.RequiredSkillRank, 0, 0, RECIPE_UNTAUGHT });
            if (entry.second.Flags & ITEM_PROTO_FLAG_MILLABLE)
                AddUse(uses[entry.first], { Skill::INSCRIPTION, 0, entry.second.RequiredSkillRank, 0, 0, RECIPE_UNTAUGHT });
        }

        RecipeUses chained;
        for (Smelting const& smelting : smeltings)
            for (uint32 product : smelting.products)
            {
                auto const itr = uses.find(product);
                if (itr == uses.end())
                    continue;

                for (RecipeUse const& use : itr->second)
                    if (IsCraftingSkill(use.skill))
                        for (uint32 reagent : smelting.reagents)
                            AddUse(chained[reagent], { use.skill, use.spell, use.rank, Skill::MINING, smelting.spell, smelting.rank });
            }

        for (auto const& entry : chained)
            for (RecipeUse const& use : entry.second)
                AddUse(uses[entry.first], use);

        return uses;
    }

    std::vector<BotAuctionPolicy::RecipeUse> const& RecipeUsesOf(uint32 itemId)
    {
        // Built on first use from the loaded client data, then read-only: bots update on several map threads.
        static RecipeUses const sUses = BuildRecipeUses();
        static std::vector<BotAuctionPolicy::RecipeUse> const none;
        auto const itr = sUses.find(itemId);
        return itr != sUses.end() ? itr->second : none;
    }

    // The tools of the gathering and crafting skills: the two the bot's gathering uses by id, anything with a totem
    // category (pick, hammer, spanner, rods), and fishing poles and other miscellaneous weapons.
    bool IsProfessionTool(ItemTemplate const* proto)
    {
        if (proto->ItemId == MINING_PICK_ITEM || proto->ItemId == SKINNING_KNIFE_ITEM || proto->TotemCategory)
            return true;

        return proto->Class == ITEM_CLASS_WEAPON &&
               (proto->SubClass == ITEM_SUBCLASS_WEAPON_MISCELLANEOUS || proto->SubClass == ITEM_SUBCLASS_WEAPON_FISHING_POLE);
    }

    // Loot the bot has no use for unless the usage value says otherwise (checked by the caller): gear, recipes, gems.
    bool IsUnwantedLootClass(ItemTemplate const* proto)
    {
        return proto->Class == ITEM_CLASS_ARMOR || proto->Class == ITEM_CLASS_WEAPON ||
               proto->Class == ITEM_CLASS_RECIPE || proto->Class == ITEM_CLASS_GEM;
    }

    bool IsManaUser(Player* bot) { return bot->GetMaxPower(POWER_MANA) > 0; }

    // "food", "drink" or "" for an item the bot can eat or drink at its level (the module's own classification).
    std::string SupplyKindOf(Player* bot, ItemTemplate const* proto)
    {
        if (proto->Class != ITEM_CLASS_CONSUMABLE || bot->CanUseItem(proto) != EQUIP_ERR_OK)
            return "";

        std::string const kind = ItemUsageValue::GetConsumableType(proto, IsManaUser(bot));
        return kind == FOOD_KIND || kind == DRINK_KIND ? kind : "";
    }

    SupplyCounts CountSupplies(Player* bot)
    {
        SupplyCounts counts;
        ForEachBagItem(bot, [&](Item* item)
        {
            std::string const kind = SupplyKindOf(bot, item->GetTemplate());
            if (kind == FOOD_KIND)
                counts.food += item->GetCount();
            else if (kind == DRINK_KIND)
                counts.drink += item->GetCount();
        });
        return counts;
    }

    // A mage that has learned Conjure Refreshment makes its own (the "no food" trigger of the mage strategy).
    bool ConjuresOwnSupplies(PlayerbotAI* botAI, Player* bot)
    {
        return bot->getClass() == CLASS_MAGE &&
               botAI->GetAiObjectContext()->GetValue<uint32>("spell id", CONJURE_SPELL)->Get() != 0;
    }

    // Fewer than the low water of food, or of drink for a mana user; a conjuring mage never runs low.
    bool SuppliesLow(PlayerbotAI* botAI, Player* bot)
    {
        if (ConjuresOwnSupplies(botAI, bot))
            return false;

        SupplyCounts const counts = CountSupplies(bot);
        return SupplyLow(counts.food) || (IsManaUser(bot) && SupplyLow(counts.drink));
    }

    uint64 SellValueOf(std::vector<Item*> const& items)
    {
        uint64 value = 0;
        for (Item const* item : items)
            value += uint64(item->GetTemplate()->SellPrice) * item->GetCount();
        return value;
    }

    // The best food or drink of that kind the vendor sells for plain gold that the bot can use: the highest required
    // level, then item level.
    bool FindOffer(Player* bot, Creature* vendor, char const* kind, SupplyOffer& best)
    {
        VendorItemData const* items = vendor->GetVendorItems();
        if (!items)
            return false;

        for (uint32 slot = 0; slot < items->GetItemCount(); ++slot)
        {
            VendorItem const* entry = items->GetItem(slot);
            if (!entry || entry->Type != ITEM_VENDOR_TYPE_ITEM || entry->ExtendedCost)
                continue;

            ItemTemplate const* proto = sObjectMgr->GetItemTemplate(entry->item);
            if (!proto || !proto->BuyPrice || proto->RequiredReputationFaction || SupplyKindOf(bot, proto) != kind)
                continue;

            if (entry->maxcount && vendor->GetVendorItemCurrentCount(entry) < proto->BuyCount)
                continue;

            if (best.proto && (proto->RequiredLevel < best.proto->RequiredLevel ||
                               (proto->RequiredLevel == best.proto->RequiredLevel && proto->ItemLevel <= best.proto->ItemLevel)))
                continue;

            best.slot = slot;
            best.proto = proto;
        }
        return best.proto != nullptr;
    }

    bool VendorSellsSupplies(Player* bot, Creature* vendor)
    {
        SupplyOffer offer;
        if (FindOffer(bot, vendor, FOOD_KIND, offer))
            return true;
        return IsManaUser(bot) && FindOffer(bot, vendor, DRINK_KIND, offer);
    }

    // Buys single items of one offer until the bot holds the target, the money above the reserve ends or the vendor
    // refuses. The count is measured from the bag, so a lot that holds several items is not overbought.
    // moneyCap: the purse this kind may plan with (FirstKindMoney), never more than the bot holds.
    uint32 BuyOffer(Player* bot, Creature* vendor, SupplyOffer const& offer, uint32 held, uint64 reserve, uint64 moneyCap)
    {
        uint32 const unitPrice =
            uint32(floor(offer.proto->BuyPrice * bot->GetReputationPriceDiscount(vendor)));
        // Planned once from what the bot held on arrival: planned again after each item, the low water rule stopped
        // the purchase at SUPPLY_LOW_WATER items (BT-177). The plan already keeps the reserve.
        uint64 const money = std::min<uint64>(moneyCap, bot->GetMoney());
        uint32 const toBuy = SupplyUnitsToBuy(held, money, reserve, unitPrice);
        uint32 bought = 0;
        while (bought < toBuy && held + bought < SUPPLY_STACK_TARGET)
        {
            uint32 const before = bot->GetItemCount(offer.proto->ItemId, false);
            bot->BuyItemFromVendorSlot(vendor->GetGUID(), offer.slot, offer.proto->ItemId, 1, NULL_BAG, NULL_SLOT);
            uint32 const gained = bot->GetItemCount(offer.proto->ItemId, false) - before;
            if (!gained)
                break;

            bought += gained;
        }
        return bought;
    }

    // The trip decision; cannotPay is set when the bot is low on supplies and selling would not pay for them either.
    bool EvaluateTripWanted(PlayerbotAI* botAI, Player* bot, bool& cannotPay)
    {
        cannotPay = false;
        uint64 const money = bot->GetMoney();
        uint64 const reserve = PurchaseReserve(bot->getLevel());
        bool const low = SuppliesLow(botAI, bot);
        if (low && CanAffordSupplies(money, reserve))
            return true;

        uint8 const bagSpace = botAI->GetAiObjectContext()->GetValue<uint8>("bag space")->Get();
        bool const bagsFull = BagsAlmostFull(bagSpace);
        if (!low && !bagsFull)
            return false;

        // Short of money for food or drink, or bags almost full: what the bot could sell decides.
        std::vector<Item*> const toSell = BotSale::SelectItemsToSell(botAI, bot);
        if (bagsFull && !toSell.empty())
            return true;

        SupplyPlan const plan = PlanSupplies(low, money, reserve, SellValueOf(toSell));
        cannotPay = plan == SupplyPlan::CannotPay;
        return SupplyTripWanted(plan);
    }
}

bool BotSale::IsEligible(PlayerbotAI* botAI, Player* bot)
{
    if (!sRandomPlayerbotMgr->IsRandomBot(bot) || !bot->IsInWorld() || !bot->IsAlive() || bot->IsInCombat() ||
        bot->IsBeingTeleported() || bot->InBattleground() || bot->InArena() || bot->GetMap()->Instanceable())
        return false;

    // A bot that follows a real player goes where the player goes.
    return !botAI->HasRealPlayerMaster();
}

bool BotSale::TripWanted(PlayerbotAI* botAI, Player* bot)
{
    if (!IsEligible(botAI, bot))
        return false;

    uint32 const now = getMSTime();
    {
        std::lock_guard<std::mutex> guard(sLock);
        TripState& state = sTrips[bot->GetGUID()];
        if (state.tripStartMs)
        {
            if (getMSTimeDiff(state.tripStartMs, now) < TRIP_TIMEOUT_MS)
                return true;

            state = TripState{ 0, now, false, now, UNPRODUCTIVE_COOLDOWN_MS, state.lastLogMs, state.logged };
            return false;
        }

        if (state.blockedForMs && getMSTimeDiff(state.blockedAtMs, now) < state.blockedForMs)
            return false;

        if (state.lastEvalMs && getMSTimeDiff(state.lastEvalMs, now) < TRIP_EVAL_INTERVAL_MS)
            return state.wanted;
    }

    bool cannotPay = false;
    bool const wanted = EvaluateTripWanted(botAI, bot, cannotPay);

    std::lock_guard<std::mutex> guard(sLock);
    TripState& state = sTrips[bot->GetGUID()];
    state.lastEvalMs = now;
    state.wanted = wanted;
    if (cannotPay)
    {
        state.blockedAtMs = now;
        state.blockedForMs = CANNOT_PAY_RECHECK_MS;
    }
    return wanted;
}

Creature* BotSale::FindNearbyVendor(PlayerbotAI* botAI, Player* bot)
{
    uint32 const now = getMSTime();
    {
        std::lock_guard<std::mutex> guard(sLock);
        auto const itr = sVendors.find(bot->GetGUID());
        if (itr != sVendors.end() && getMSTimeDiff(itr->second.time, now) < VENDOR_CACHE_MS)
        {
            Creature* vendor = itr->second.vendor ? botAI->GetCreature(BotGuid(itr->second.vendor)) : nullptr;
            return vendor && vendor->IsAlive() ? vendor : nullptr;
        }
    }

    // The vendors' lists are read only by a bot that needs food or drink. BT-213: money is not asked here, since a bot
    // short of it sells first, and a vendor of food buys junk as well as any other.
    bool const needsSupplies = SuppliesLow(botAI, bot);

    Creature* nearest = nullptr;
    float nearestDist = VENDOR_RANGE;
    Creature* nearestSupplier = nullptr;
    float nearestSupplierDist = VENDOR_RANGE;
    for (BotGuid const guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest npcs")->Get())
    {
        Creature* creature = botAI->GetCreature(guid);
        if (!creature || !creature->IsAlive() || !creature->IsVendor() || bot->IsHostileTo(creature))
            continue;

        float const dist = bot->GetDistance(creature);
        if (dist < nearestDist)
        {
            nearest = creature;
            nearestDist = dist;
        }

        if (needsSupplies && dist < nearestSupplierDist && VendorSellsSupplies(bot, creature))
        {
            nearestSupplier = creature;
            nearestSupplierDist = dist;
        }
    }

    Creature* const chosen = nearestSupplier ? nearestSupplier : nearest;
    std::lock_guard<std::mutex> guard(sLock);
    sVendors[bot->GetGUID()] = { now, chosen ? uint64(chosen->GetGUID()) : 0 };
    return chosen;
}

uint32 BotSale::SupplyHeld(Player* bot, ItemTemplate const* proto)
{
    std::string const kind = SupplyKindOf(bot, proto);
    if (kind.empty())
        return 0;

    SupplyCounts const counts = CountSupplies(bot);
    return kind == FOOD_KIND ? counts.food : counts.drink;
}

uint32 BotSale::ProfessionsOf(Player* bot)
{
    return ::ProfessionsOf(bot);
}

ItemFacts BotSale::FactsOf(Player* bot, Item* item, uint32 professions)
{
    ItemTemplate const* proto = item->GetTemplate();
    MaterialKind const kind = MaterialKindOf(proto);

    ItemFacts facts;
    facts.equipped = item->IsEquipped();
    facts.bag = proto->Class == ITEM_CLASS_CONTAINER;
    facts.questItem = proto->Class == ITEM_CLASS_QUEST || proto->StartQuest;
    facts.professionTool = IsProfessionTool(proto);
    facts.reagent = proto->Class == ITEM_CLASS_REAGENT;
    facts.precious = proto->Quality > ITEM_QUALITY_EPIC;
    facts.hasSellPrice = proto->SellPrice > 0;
    facts.poorQuality = proto->Quality == ITEM_QUALITY_POOR;
    facts.gatheredMaterial = IsGatheredMaterial(kind);
    facts.unwantedLoot = IsUnwantedLootClass(proto);
    facts.professionMaterial = ProfessionUses(professions, kind);
    facts.usableFoodOrDrink = !SupplyKindOf(bot, proto).empty();
    return facts;
}

bool BotSale::UsageKeeps(PlayerbotAI* botAI, ItemTemplate const* proto)
{
    // The usage value is not free (it creates a test item for gear), so it is asked only for what would be sold.
    ItemUsage const usage =
        botAI->GetAiObjectContext()->GetValue<ItemUsage>("item usage", std::to_string(proto->ItemId))->Get();
    if (usage == ITEM_USAGE_QUEST)
        return true;

    if ((proto->Class == ITEM_CLASS_ARMOR || proto->Class == ITEM_CLASS_WEAPON) &&
        (usage == ITEM_USAGE_EQUIP || usage == ITEM_USAGE_REPLACE || usage == ITEM_USAGE_BROKEN_EQUIP))
        return true;

    return proto->Class == ITEM_CLASS_RECIPE &&
           (usage == ITEM_USAGE_SKILL || usage == ITEM_USAGE_USE || usage == ITEM_USAGE_KEEP);
}

BotAuctionPolicy::MaterialUse BotSale::MaterialUseOf(Player* bot, ItemTemplate const* proto)
{
    return BotAuctionPolicy::MaterialUseOf(RecipeUsesOf(proto->ItemId), MaterialKindOf(proto),
                                           [bot](uint32 skill) { return HasTradeSkill(bot, skill) ? uint32(bot->GetSkillValue(skill)) : 0u; },
                                           [bot](uint32 spell) { return bot->HasSpell(spell); });
}

bool BotSale::IsUsableSupply(Player* bot, ItemTemplate const* proto)
{
    return !SupplyKindOf(bot, proto).empty();
}

std::vector<Item*> BotSale::SelectItemsToSell(PlayerbotAI* botAI, Player* bot)
{
    uint32 const professions = ProfessionsOf(bot);
    // How many of an entry the bot already keeps in this pass, for the caps on materials and food.
    std::unordered_map<uint32, uint32> keptByEntry;
    std::vector<Item*> toSell;

    ForEachBagItem(bot, [&](Item* item)
    {
        ItemTemplate const* proto = item->GetTemplate();
        uint32& kept = keptByEntry[proto->ItemId];

        ItemFacts facts = FactsOf(bot, item, professions);
        facts.professionMaterial = facts.professionMaterial && KeepStack(kept, PROFESSION_MATERIAL_KEEP_MAX);
        facts.usableFoodOrDrink = facts.usableFoodOrDrink && KeepStack(kept, FOOD_DRINK_KEEP_MAX);

        if (facts.professionMaterial || facts.usableFoodOrDrink)
            kept += item->GetCount();

        if (!ShouldSell(facts) || UsageKeeps(botAI, proto))
            return;

        toSell.push_back(item);
    });

    return toSell;
}

uint32 BotSale::BuyTools(Player* bot, Creature* vendor)
{
    std::vector<uint32> const wanted = BotToolPolicy::ToolsToBuy([bot](uint32 skill) { return bot->HasSkill(skill); },
                                                                 [bot](uint32 item) { return bot->HasItemCount(item, 1, true); });
    VendorItemData const* items = wanted.empty() ? nullptr : vendor->GetVendorItems();
    if (!items)
        return 0;

    uint64 const reserve = PurchaseReserve(bot->getLevel());
    uint32 bought = 0;
    for (uint32 itemId : wanted)
    {
        for (uint32 slot = 0; slot < items->GetItemCount(); ++slot)
        {
            VendorItem const* entry = items->GetItem(slot);
            if (!entry || entry->item != itemId || entry->Type != ITEM_VENDOR_TYPE_ITEM || entry->ExtendedCost)
                continue;

            ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
            if (!proto)
                break;

            // Paid from the money above the repair reserve, as food and drink are.
            uint64 const price = uint64(floor(proto->BuyPrice * bot->GetReputationPriceDiscount(vendor)));
            if (bot->GetMoney() < reserve + price)
                break;

            bot->BuyItemFromVendorSlot(vendor->GetGUID(), slot, itemId, 1, NULL_BAG, NULL_SLOT);
            if (bot->HasItemCount(itemId, 1, true))
            {
                ++bought;
                SF_LOG_INFO("playerbots", "[TOOL] bot=%s bought item %u from %s for " UI64FMTD, bot->GetName().c_str(),
                            itemId, vendor->GetName().c_str(), price);
            }
            break;
        }
    }
    return bought;
}

uint32 BotSale::BuySupplies(PlayerbotAI* botAI, Player* bot, Creature* vendor)
{
    if (!IsEligible(botAI, bot) || ConjuresOwnSupplies(botAI, bot))
        return 0;

    uint64 const reserve = PurchaseReserve(bot->getLevel());
    SupplyCounts const counts = CountSupplies(bot);
    uint64 const moneyBefore = bot->GetMoney();
    uint32 bought = 0;

    SupplyOffer food;
    bool const sellsFood = FindOffer(bot, vendor, FOOD_KIND, food);

    // BT-213: a mana user buys its drink first, with half of the spare money when it needs food too; the food gets
    // what is left.
    SupplyOffer drink;
    if (IsManaUser(bot) && FindOffer(bot, vendor, DRINK_KIND, drink))
    {
        bool const foodLow = sellsFood && SupplyLow(counts.food);
        bought += BuyOffer(bot, vendor, drink, counts.drink, reserve, FirstKindMoney(bot->GetMoney(), reserve, foodLow));
    }

    if (sellsFood)
        bought += BuyOffer(bot, vendor, food, counts.food, reserve, bot->GetMoney());

    if (bought && moneyBefore > bot->GetMoney())
        SF_LOG_INFO("playerbots", "[SUPPLY] bot=%s items=%u money=" UI64FMTD " drink=%u", bot->GetName().c_str(), bought,
                    moneyBefore - bot->GetMoney(), CountSupplies(bot).drink);

    return bought;
}

void BotSale::BeginTrip(Player* bot)
{
    std::lock_guard<std::mutex> guard(sLock);
    TripState& state = sTrips[bot->GetGUID()];
    if (!state.tripStartMs)
        state.tripStartMs = getMSTime();
}

void BotSale::EndTrip(Player* bot, bool productive)
{
    std::lock_guard<std::mutex> guard(sLock);
    TripState& state = sTrips[bot->GetGUID()];
    state.tripStartMs = 0;
    state.wanted = false;
    state.lastEvalMs = getMSTime();
    state.blockedAtMs = state.lastEvalMs;
    state.blockedForMs = productive ? PRODUCTIVE_COOLDOWN_MS : UNPRODUCTIVE_COOLDOWN_MS;
    sVendors.erase(bot->GetGUID());
}

void BotSale::LogSale(Player* bot, uint32 items, uint64 money)
{
    if (!items)
        return;

    uint32 const now = getMSTime();
    {
        std::lock_guard<std::mutex> guard(sLock);
        TripState& state = sTrips[bot->GetGUID()];
        if (!ShouldLogTrip(getMSTimeDiff(state.lastLogMs, now), state.logged))
            return;

        state.lastLogMs = now;
        state.logged = true;
    }

    SF_LOG_INFO("playerbots", "[SELL] bot=%s items=%u money=" UI64FMTD, bot->GetName().c_str(), items, money);
}

BotSalePolicy::FinderSupply BotSale::FinderSupplyOf(PlayerbotAI* botAI, Player* bot, uint32& drinkHeld)
{
    drinkHeld = 0;
    if (!IsManaUser(bot) || ConjuresOwnSupplies(botAI, bot))
        return FinderSupply::Ready;

    drinkHeld = CountSupplies(bot).drink;
    bool onTrip = false;
    {
        std::lock_guard<std::mutex> guard(sLock);
        auto const itr = sTrips.find(bot->GetGUID());
        onTrip = itr != sTrips.end() && itr->second.tripStartMs &&
                 getMSTimeDiff(itr->second.tripStartMs, getMSTime()) < TRIP_TIMEOUT_MS;
    }

    bool const canAfford = CanAffordSupplies(bot->GetMoney(), PurchaseReserve(bot->getLevel()));
    return BotSalePolicy::FinderSupplyOf(true, false, drinkHeld, canAfford, onTrip);
}

void BotSale::LogFinderSupply(Player* bot, FinderSupply supply, uint32 drinkHeld, char const* what)
{
    // A bot held back is seen again every finder pass, so that line is rate limited; a join is written every time.
    uint32 const now = getMSTime();
    if (supply == FinderSupply::Shopping)
    {
        std::lock_guard<std::mutex> guard(sLock);
        TripState& state = sTrips[bot->GetGUID()];
        if (!ShouldLogTrip(getMSTimeDiff(state.finderLogMs, now), state.finderLogged))
            return;

        state.finderLogMs = now;
        state.finderLogged = true;
    }

    SF_LOG_INFO("playerbots", "[SUPPLY] bot=%s finder=%s drink=%u reason=%s", bot->GetName().c_str(), what, drinkHeld,
                FinderSupplyReason(supply));
}
