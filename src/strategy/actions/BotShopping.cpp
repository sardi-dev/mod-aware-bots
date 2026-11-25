/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * Alt bots buy their gear upgrades like a player (BotShopping.h).
 */

#include "BotShopping.h"

#include "BotGuid.h"
#include "BotAuction.h"
#include "AuctionVisitPolicy.h"
#include "RecipePurchasePolicy.h"
#include "GearEarnPolicy.h"
#include "ReservePolicy.h"
#include "PlayerbotAIConfig.h"
#include "UseItemAction.h"
#include "BotMountPolicy.h"
#include "BotSalePolicy.h"
#include "BudgetValues.h"
#include "Config.h"
#include "Creature.h"
#include "DB2Stores.h"
#include "DBCStores.h"
#include "Event.h"
#include "ItemVisitors.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RandomPlayerbotMgr.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"

#include <cmath>
#include <ctime>
#include <mutex>
#include <string>
#include <unordered_map>

namespace
{
    // The answer of FindNearbyVendor is kept this long per bot: the trigger asks every tick while the bot walks.
    constexpr uint32 VENDOR_CACHE_MS = 5000;

    struct CachedVendor
    {
        uint32 time;
        uint64 vendor;
    };

    // Bots update on the map threads, so the cache is shared and locked.
    std::mutex sLock;
    std::unordered_map<uint64, CachedVendor> sCache;

    std::unordered_map<uint64, CachedVendor> sMountVendorCache;


    // A mount item: its second spell is the mount it teaches (the first is the learning spell, 55884 on 5.4.8).
    uint32 MountSpellOf(ItemTemplate const* proto)
    {
        for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
        {
            int32 const spellId = proto->Spells[i].SpellId;
            if (spellId <= 0)
                continue;
            SpellInfo const* info = sSpellMgr->GetSpellInfo(uint32(spellId));
            if (info && info->HasAura(SPELL_AURA_MOUNTED))
                return uint32(spellId);
        }
        return 0;
    }

    bool KnowsMount(Player* bot)
    {
        for (auto const& [spellId, spell] : bot->GetSpellMap())
        {
            if (!spell || spell->state == PLAYERSPELL_REMOVED || !spell->active)
                continue;
            SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
            if (info && info->HasAura(SPELL_AURA_MOUNTED))
                return true;
        }
        return false;
    }

    // The mount items this vendor sells for plain gold that the bot may use, with their prices after the discount.
    void MountOffers(Player* bot, Creature* vendor, std::vector<uint32>& slots, std::vector<uint64>& prices)
    {
        VendorItemData const* items = vendor->GetVendorItems();
        if (!items)
            return;

        for (uint32 slot = 0; slot < items->GetItemCount(); ++slot)
        {
            VendorItem const* entry = items->GetItem(slot);
            if (!entry || entry->Type != ITEM_VENDOR_TYPE_ITEM || entry->ExtendedCost)
                continue;

            ItemTemplate const* proto = sObjectMgr->GetItemTemplate(entry->item);
            if (!proto || !proto->BuyPrice || !MountSpellOf(proto) || bot->CanUseItem(proto) != EQUIP_ERR_OK)
                continue;
            if (proto->RequiredReputationFaction &&
                uint32(bot->GetReputationRank(proto->RequiredReputationFaction)) < proto->RequiredReputationRank)
                continue;

            slots.push_back(slot);
            prices.push_back(uint64(std::floor(proto->BuyPrice * bot->GetReputationPriceDiscount(vendor))));
        }
    }

    bool IsGear(ItemTemplate const* proto)
    {
        return proto->Class == ITEM_CLASS_WEAPON || proto->Class == ITEM_CLASS_ARMOR;
    }

    // The checks Player::BuyItemFromVendorSlot makes, so the bot never walks to an item it cannot buy.
    bool CanPayFor(PlayerbotAI* botAI, Player* bot, Creature* vendor, VendorItem const* vendorItem,
                   ItemTemplate const* proto)
    {
        if (vendorItem->maxcount && vendor->GetVendorItemCurrentCount(vendorItem) < proto->BuyCount)
            return false;

        if (proto->RequiredReputationFaction &&
            uint32(bot->GetReputationRank(proto->RequiredReputationFaction)) < proto->RequiredReputationRank)
            return false;

        if (vendorItem->ExtendedCost)
        {
            ItemExtendedCostEntry const* cost = sItemExtendedCostStore.LookupEntry(vendorItem->ExtendedCost);
            if (!cost)
                return false;

            for (uint8 i = 0; i < MAX_ITEM_EXT_COST_ITEMS; ++i)
                if (cost->RequiredItem[i] && !bot->HasItemCount(cost->RequiredItem[i], cost->RequiredItemCount[i]))
                    return false;

            for (uint8 i = 0; i < MAX_ITEM_EXT_COST_CURRENCIES; ++i)
            {
                if (!cost->RequiredCurrency[i])
                    continue;
                // A season-earned requirement (conquest caps and the like) is left to the owner.
                if (cost->RequirementFlags & (ITEM_EXT_COST_CURRENCY_REQ_IS_SEASON_EARNED_1 << i))
                    return false;
                if (!bot->HasCurrency(cost->RequiredCurrency[i], cost->RequiredCurrencyCount[i]))
                    return false;
            }

            if (cost->RequiredPersonalArenaRating &&
                bot->GetMaxPersonalArenaRatingRequirement(cost->RequiredArenaSlot) < cost->RequiredPersonalArenaRating)
                return false;

            if (cost->RequiredFactionId &&
                uint32(bot->GetReputationRank(cost->RequiredFactionId)) < cost->RequiredFactionStanding)
                return false;

            if ((cost->RequirementFlags & ITEM_EXT_COST_FLAG_REQUIRE_GUILD) && !bot->GetGuildId())
                return false;
        }

        if (!vendorItem->IsGoldRequired(proto) || proto->BuyPrice <= 0)
            return true;

        // BuyAction pays only out of the gear budget, so the bot must not walk for more than that.
        uint32 const price = uint32(std::floor(proto->BuyPrice * bot->GetReputationPriceDiscount(vendor)));
        return botAI->GetAiObjectContext()->GetValue<uint32>("free money for", uint32(NeedMoneyFor::gear))->Get() >= price;
    }
}

bool BotShopping::Enabled()
{
    // Read once on first use, like the other local settings (BotTraining.cpp), to keep PlayerbotAIConfig.h unchanged.
    static bool const enabled = sConfigMgr->GetBoolDefault("AiPlayerbot.AltBotsShopForGear", true);
    return enabled;
}

bool BotShopping::RandomBotsEnabled()
{
    static bool const enabled = sConfigMgr->GetBoolDefault("AiPlayerbot.RandomBotsShopForGear", true);
    return enabled;
}

bool BotShopping::BuyUpgradesAt(PlayerbotAI* botAI, Player* bot, Creature* vendor)
{
    if (!RandomBotsEnabled() || !CountUpgradesAt(botAI, bot, vendor))
        return false;

    uint64 const moneyBefore = bot->GetMoney();
    botAI->DoSpecificAction("buy", Event("supply trip", "vendor gear"), true);
    Forget(bot);
    uint64 const moneyAfter = bot->GetMoney();
    if (moneyAfter >= moneyBefore)
        return false;

    // One line per purchase, so a soak can count how often bots really buy and wear vendor upgrades.
    SF_LOG_INFO("playerbots", "[BUYGEAR] bot=%s level=%u vendor=%u spent=%u money=%u", bot->GetName().c_str(),
                uint32(bot->getLevel()), vendor->GetEntry(), uint32(moneyBefore - moneyAfter), uint32(moneyAfter));
    return true;
}

bool BotShopping::BuyBagAt(PlayerbotAI* botAI, Player* bot, Creature* vendor)
{
    bool empty = false;
    for (uint8 slot = INVENTORY_SLOT_BAG_START; slot < INVENTORY_SLOT_BAG_END; ++slot)
        if (!bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
            empty = true;
    VendorItemData const* items = vendor->GetVendorItems();
    if (!empty || !items)
        return false;

    uint64 const budget = botAI->GetAiObjectContext()->GetValue<uint32>("free money for", uint32(NeedMoneyFor::gear))->Get();
    uint32 bestSlot = 0, bestSize = 0;
    uint64 bestPrice = 0;
    for (uint32 slot = 0; slot < items->GetItemCount(); ++slot)
    {
        VendorItem const* entry = items->GetItem(slot);
        if (!entry || entry->Type != ITEM_VENDOR_TYPE_ITEM || entry->ExtendedCost)
            continue;
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(entry->item);
        if (!proto || proto->Class != ITEM_CLASS_CONTAINER || proto->SubClass != ITEM_SUBCLASS_CONTAINER ||
            bot->CanUseItem(proto) != EQUIP_ERR_OK || !CanPayFor(botAI, bot, vendor, entry, proto))
            continue;
        uint64 const price = uint64(std::floor(proto->BuyPrice * bot->GetReputationPriceDiscount(vendor)));
        if (GearEarnPolicy::BetterBag(proto->ContainerSlots, price, budget, bestSize, bestPrice))
        {
            bestSlot = slot;
            bestSize = proto->ContainerSlots;
            bestPrice = price;
        }
    }
    if (!bestSize)
        return false;
    VendorItem const* entry = items->GetItem(bestSlot);
    uint32 const itemId = entry->item;
    uint32 const before = bot->GetItemCount(itemId, true);
    bot->BuyItemFromVendorSlot(vendor->GetGUID(), bestSlot, itemId, 1, NULL_BAG, NULL_SLOT);
    if (bot->GetItemCount(itemId, true) <= before)
        return false;
    botAI->DoSpecificAction("equip upgrades packet action");
    return true;
}

bool BotShopping::BuyRecipesAt(PlayerbotAI* botAI, Player* bot, Creature* vendor)
{
    if (!sConfigMgr->GetBoolDefault("AiPlayerbot.RecipeBuying.Enable", false) ||
        !sRandomPlayerbotMgr->IsRandomBot(bot) ||
        !AuctionVisitPolicy::NameAllowed(sPlayerbotAIConfig->auctionOnlyBots, bot->GetName()))
        return false;

    ReservePolicy::Facts facts;
    facts.level = bot->getLevel();
    for (uint32 skill : ReservePolicy::CRAFTING_SKILLS)
        if (bot->HasSkill(skill) && bot->GetSkillValue(skill))
            ++facts.craftingProfessionCount;
    VendorItemData const* items = vendor->GetVendorItems();
    if (!facts.craftingProfessionCount || !items)
        return false;

    ReservePolicy::Params params;
    params.goalSharePercent = sPlayerbotAIConfig->auctionReserveGoalSharePercent;
    params.recipeSharePercent = sPlayerbotAIConfig->auctionReserveRecipeSharePercent;
    uint64 const money = bot->GetMoney();
    uint64 budget = ReservePolicy::RecipeBudget(ReservePolicy::ComputeReserve(facts, params, money), money);
    uint32 const visitLimit = uint32(std::max(0, sConfigMgr->GetIntDefault("AiPlayerbot.RecipeBuying.PerVisit", 1)));
    uint32 const dayLimit = uint32(std::max(0, sConfigMgr->GetIntDefault("AiPlayerbot.RecipeBuying.PerDay", 3)));
    uint32 dayCount = sRandomPlayerbotMgr->GetValue(bot, "recipe_daily");
    uint32 const limit = std::min(visitLimit, dayCount < dayLimit ? dayLimit - dayCount : 0u);
    std::vector<RecipePurchasePolicy::Offer> offers;
    for (uint32 slot = 0; slot < items->GetItemCount(); ++slot)
    {
        VendorItem const* entry = items->GetItem(slot);
        if (!entry || entry->Type != ITEM_VENDOR_TYPE_ITEM || entry->ExtendedCost)
            continue;
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(entry->item);
        // The core's special learning item path casts spell 483 with slot 1 as the taught spell.
        if (!proto || proto->Class != ITEM_CLASS_RECIPE || proto->Spells[0].SpellId != 483 ||
            proto->Spells[1].SpellId <= 0 || proto->BuyPrice <= 0 || !sSpellMgr->GetSpellInfo(uint32(proto->Spells[1].SpellId)))
            continue;
        RecipePurchasePolicy::Offer offer;
        offer.item = proto->ItemId;
        offer.spell = uint32(proto->Spells[1].SpellId);
        offer.skill = proto->RequiredSkill;
        offer.requiredSkill = proto->RequiredSkillRank;
        offer.currentSkill = bot->GetSkillValue(offer.skill);
        offer.price = uint32(std::floor(proto->BuyPrice * bot->GetReputationPriceDiscount(vendor)));
        auto const abilities = sSpellMgr->GetSkillLineAbilityMapBounds(offer.spell);
        bool matchesSkill = false;
        for (auto it = abilities.first; it != abilities.second; ++it)
            if (it->second && it->second->skillId == offer.skill)
                matchesSkill = true;
        offer.unambiguous = matchesSkill;
        offer.craftingSkill = ReservePolicy::IsCraftingSkill(offer.skill);
        offer.known = bot->HasSpell(offer.spell);
        offer.owned = bot->HasItemCount(offer.item, 1, true);
        offer.available = (!entry->maxcount || vendor->GetVendorItemCurrentCount(entry) >= proto->BuyCount) &&
            (!proto->RequiredReputationFaction || uint32(bot->GetReputationRank(proto->RequiredReputationFaction)) >= proto->RequiredReputationRank);
        offer.goldOnly = entry->IsGoldRequired(proto);
        offer.usable = bot->CanUseItem(proto) == EQUIP_ERR_OK && !bot->IsNonMeleeSpellCasted(false) &&
                       botAI->CanCastSpell(483, bot, false);
        if (RecipePurchasePolicy::Eligible(offer))
            offers.push_back(offer);
    }
    auto const plan = RecipePurchasePolicy::PlanPurchases(offers, budget, limit);
    std::sort(offers.begin(), offers.end(), [](RecipePurchasePolicy::Offer const& a, RecipePurchasePolicy::Offer const& b) {
        return a.price != b.price ? a.price < b.price : a.item < b.item;
    });
    std::vector<RecipePurchasePolicy::Offer> counted;
    uint64 remaining = budget;
    uint32 skippedBudget = 0, skippedLimit = 0;
    for (auto const& offer : offers)
    {
        bool duplicate = false;
        for (auto const& previous : counted)
            if (previous.item == offer.item || previous.spell == offer.spell)
                duplicate = true;
        if (duplicate) continue;
        if (counted.size() >= limit) { ++skippedLimit; continue; }
        if (offer.price > remaining) { ++skippedBudget; continue; }
        counted.push_back(offer);
        remaining -= offer.price;
    }
    if (skippedBudget || skippedLimit)
        BotAuction::RecordRecipe(0, 0, skippedBudget, skippedLimit);

    bool bought = false;
    for (auto const& offer : plan)
    {
        if (offer.price > budget || bot->HasSpell(offer.spell) || bot->HasItemCount(offer.item, 1, true))
            continue;
        for (uint32 slot = 0; slot < items->GetItemCount(); ++slot)
        {
            VendorItem const* entry = items->GetItem(slot);
            if (!entry || entry->item != offer.item || entry->ExtendedCost || entry->Type != ITEM_VENDOR_TYPE_ITEM)
                continue;
            uint64 const before = bot->GetMoney();
            bot->BuyItemFromVendorSlot(vendor->GetGUID(), slot, offer.item, 1, NULL_BAG, NULL_SLOT);
            uint64 const after = bot->GetMoney();
            if (after >= before || !bot->HasItemCount(offer.item, 1))
                break;
            budget = before - after >= budget ? 0 : budget - (before - after);
            time_t const now = std::time(nullptr);
            uint32 const secondsLeft = uint32(24 * 60 * 60 - now % (24 * 60 * 60));
            sRandomPlayerbotMgr->SetRecipePurchaseCount(bot, ++dayCount, secondsLeft);
            Item* item = bot->GetItemByEntry(offer.item);
            if (item)
                UseItemAction(botAI, "recipe").UseQuestSourceItem(item);
            bool const learned = bot->HasSpell(offer.spell);
            BotAuction::RecordRecipe(1, learned ? 1 : 0, 0, 0);
            SF_LOG_INFO("playerbots", "[RECIPE] bot=%s vendor=%u item=%u spell=%u price=%llu money=%llu learned=%u",
                        bot->GetName().c_str(), vendor->GetEntry(), offer.item, offer.spell,
                        static_cast<unsigned long long>(before - after), static_cast<unsigned long long>(after), uint32(learned));
            bought = true;
            break;
        }
    }
    return bought;
}

bool BotShopping::KnowsMount(Player* bot)
{
    return ::KnowsMount(bot);
}

bool BotShopping::WantsMount(Player* bot)
{
    return RandomBotsEnabled() && sRandomPlayerbotMgr->IsRandomBot(bot) &&
           BotMountPolicy::ShouldBuyMount(bot->GetSkillValue(SKILL_RIDING), KnowsMount(bot));
}

Creature* BotShopping::FindMountVendor(PlayerbotAI* botAI, Player* bot)
{
    uint32 const now = getMSTime();
    {
        std::lock_guard<std::mutex> guard(sLock);
        auto const itr = sMountVendorCache.find(bot->GetGUID());
        if (itr != sMountVendorCache.end() && getMSTimeDiff(itr->second.time, now) < VENDOR_CACHE_MS)
        {
            Creature* vendor = itr->second.vendor ? botAI->GetCreature(BotGuid(itr->second.vendor)) : nullptr;
            return vendor && vendor->IsAlive() ? vendor : nullptr;
        }
    }

    Creature* nearest = nullptr;
    if (WantsMount(bot))
    {
        uint64 const reserve = BotSalePolicy::PurchaseReserve(bot->getLevel());
        float nearestDist = Range();
        for (BotGuid const guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest npcs")->Get())
        {
            Creature* creature = botAI->GetCreature(guid);
            if (!creature || !creature->IsAlive() || !creature->IsVendor() || bot->IsHostileTo(creature))
                continue;

            float const dist = bot->GetDistance(creature);
            if (dist > nearestDist)
                continue;

            std::vector<uint32> slots;
            std::vector<uint64> prices;
            MountOffers(bot, creature, slots, prices);
            if (BotMountPolicy::CheapestAffordable(prices, bot->GetMoney(), reserve) < 0)
                continue;

            nearest = creature;
            nearestDist = dist;
        }
    }

    std::lock_guard<std::mutex> guard(sLock);
    sMountVendorCache[bot->GetGUID()] = { now, nearest ? uint64(nearest->GetGUID()) : 0 };
    return nearest;
}

bool BotShopping::BuyMountAt(Player* bot, Creature* vendor)
{
    {
        std::lock_guard<std::mutex> guard(sLock);
        sMountVendorCache.erase(bot->GetGUID());
    }

    std::vector<uint32> slots;
    std::vector<uint64> prices;
    MountOffers(bot, vendor, slots, prices);
    int const pick = BotMountPolicy::CheapestAffordable(prices, bot->GetMoney(),
                                                        BotSalePolicy::PurchaseReserve(bot->getLevel()));
    if (pick < 0)
        return false;

    VendorItem const* entry = vendor->GetVendorItems()->GetItem(slots[pick]);
    uint32 const itemId = entry->item;
    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
    uint32 const mountSpell = proto ? MountSpellOf(proto) : 0;
    if (!mountSpell)
        return false;

    bot->BuyItemFromVendorSlot(vendor->GetGUID(), slots[pick], itemId, 1, NULL_BAG, NULL_SLOT);
    if (!bot->HasItemCount(itemId, 1))
        return false;

    // Using the item teaches the mount and consumes it (its learning spell); the same outcome, done directly.
    bot->learnSpell(mountSpell, false);
    bot->DestroyItemCount(itemId, 1, true);
    SF_LOG_INFO("playerbots", "[MOUNT] %s bought item %u (mount %u) from %s for " UI64FMTD, bot->GetName().c_str(), itemId,
                mountSpell, vendor->GetName().c_str(), prices[pick]);
    return true;
}

float BotShopping::Range()
{
    static float const range = sConfigMgr->GetFloatDefault("AiPlayerbot.AltBotVendorRange", 60.0f);
    return range;
}

uint32 BotShopping::CountUpgradesAt(PlayerbotAI* botAI, Player* bot, Creature* vendor)
{
    VendorItemData const* items = vendor->GetVendorItems();
    if (!items)
        return 0;

    uint32 count = 0;
    for (VendorItem const* vendorItem : items->m_items)
    {
        if (!vendorItem || vendorItem->Type != ITEM_VENDOR_TYPE_ITEM)
            continue;

        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(vendorItem->item);
        if (!proto || !IsGear(proto))
            continue;

        ItemUsage const usage =
            botAI->GetAiObjectContext()->GetValue<ItemUsage>("item usage", std::to_string(proto->ItemId))->Get();
        if (usage != ITEM_USAGE_EQUIP && usage != ITEM_USAGE_REPLACE)
            continue;

        if (CanPayFor(botAI, bot, vendor, vendorItem, proto))
            ++count;
    }
    return count;
}

Creature* BotShopping::FindNearbyVendor(PlayerbotAI* botAI, Player* bot)
{
    uint32 const now = getMSTime();
    {
        std::lock_guard<std::mutex> guard(sLock);
        auto const itr = sCache.find(bot->GetGUID());
        if (itr != sCache.end() && getMSTimeDiff(itr->second.time, now) < VENDOR_CACHE_MS)
        {
            Creature* vendor = itr->second.vendor ? botAI->GetCreature(BotGuid(itr->second.vendor)) : nullptr;
            return vendor && vendor->IsAlive() ? vendor : nullptr;
        }
    }

    Creature* nearest = nullptr;
    float nearestDist = Range();
    for (BotGuid const guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest npcs")->Get())
    {
        Creature* creature = botAI->GetCreature(guid);
        if (!creature || !creature->IsAlive() || !creature->IsVendor() || bot->IsHostileTo(creature))
            continue;

        float const dist = bot->GetDistance(creature);
        if (dist > nearestDist || !CountUpgradesAt(botAI, bot, creature))
            continue;

        nearest = creature;
        nearestDist = dist;
    }

    std::lock_guard<std::mutex> guard(sLock);
    sCache[bot->GetGUID()] = { now, nearest ? uint64(nearest->GetGUID()) : 0 };
    return nearest;
}

void BotShopping::Forget(Player* bot)
{
    std::lock_guard<std::mutex> guard(sLock);
    sCache.erase(bot->GetGUID());
}
