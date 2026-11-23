/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "BotRepair.h"

#include "BotGuid.h"
#include "Creature.h"
#include "Item.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Timer.h"

#include <mutex>
#include <unordered_map>

namespace
{
    // Below this share of its full durability the equipped gear needs a repair.
    constexpr float WORN_GEAR_RATIO = 0.30f;

    // How far a bot walks to a repair NPC it can see, in yards.
    constexpr float REPAIRER_RANGE = 60.0f;

    // The answer of FindNearbyRepairer is kept this long per bot.
    constexpr uint32 REPAIRER_CACHE_MS = 5000;

    struct CachedRepairer
    {
        uint32 time;
        uint64 repairer;
    };

    // Bots update on the map threads, so the cache is shared and locked.
    std::mutex sLock;
    std::unordered_map<uint64, CachedRepairer> sCache;
}

bool BotRepair::NeedsRepair(Player* bot)
{
    uint32 current = 0;
    uint32 maximum = 0;
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!item)
            continue;

        uint32 const itemMax = item->GetUInt32Value(ITEM_FIELD_MAX_DURABILITY);
        if (!itemMax)
            continue;

        uint32 const itemCurrent = item->GetUInt32Value(ITEM_FIELD_DURABILITY);
        if (!itemCurrent)
            return true;

        current += itemCurrent;
        maximum += itemMax;
    }

    return maximum && float(current) < float(maximum) * WORN_GEAR_RATIO;
}

Creature* BotRepair::FindNearbyRepairer(PlayerbotAI* botAI, Player* bot)
{
    uint32 const now = getMSTime();
    {
        std::lock_guard<std::mutex> guard(sLock);
        auto const itr = sCache.find(bot->GetGUID());
        if (itr != sCache.end() && getMSTimeDiff(itr->second.time, now) < REPAIRER_CACHE_MS)
        {
            Creature* repairer = itr->second.repairer ? botAI->GetCreature(BotGuid(itr->second.repairer)) : nullptr;
            return repairer && repairer->IsAlive() ? repairer : nullptr;
        }
    }

    Creature* nearest = nullptr;
    float nearestDist = REPAIRER_RANGE;
    for (BotGuid const guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest npcs")->Get())
    {
        Creature* creature = botAI->GetCreature(guid);
        if (!creature || !creature->IsAlive() || !creature->IsArmorer() || bot->IsHostileTo(creature))
            continue;

        float const dist = bot->GetDistance(creature);
        if (dist > nearestDist)
            continue;

        nearest = creature;
        nearestDist = dist;
    }

    std::lock_guard<std::mutex> guard(sLock);
    sCache[bot->GetGUID()] = { now, nearest ? uint64(nearest->GetGUID()) : 0 };
    return nearest;
}

void BotRepair::Forget(Player* bot)
{
    std::lock_guard<std::mutex> guard(sLock);
    sCache.erase(bot->GetGUID());
}
