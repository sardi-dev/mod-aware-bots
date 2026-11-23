/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * Alt bots buy their gear upgrades like a player (BotShopping.h).
 */

#include "BotShopping.h"

#include "BotGuid.h"
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
    return bot->GetMoney() < moneyBefore;
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
