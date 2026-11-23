/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_NEWRPGSPAWNINDEX_H
#define _PLAYERBOT_NEWRPGSPAWNINDEX_H

#include "DBCStores.h"
#include "GameEventMgr.h"

#include <unordered_map>
#include <vector>

// Shared by the spawn indexes the rpg quest trips build once from the spawn stores: the quest ender index (BT-210) and
// the quest item source index (BT-211).

// Defined with the quest hub search (NewRpgBaseAction.cpp).
namespace QuestHubSearch
{
// The active game events, refreshed after the core's event hook, never per tick.
GameEventMgr::ActiveEvents ActiveEventsSnapshot();
// A spawn listed for a game event shows only while that event runs; a negative listing hides it meanwhile.
bool EventSpawnShown(std::vector<int32> const& events, GameEventMgr::ActiveEvents const& active);
}  // namespace QuestHubSearch

namespace RpgSpawnIndex
{
// SkyFire keeps an event's positive and negative spawn lists around the event count offset (as the quest hub search's
// index). GameEventMgr's list type is private, hence the template.
template <class Lists>
std::unordered_map<uint32, std::vector<int32>> EventSpawns(Lists const& lists)
{
    std::unordered_map<uint32, std::vector<int32>> spawns;
    int32 const eventCount = int32(sGameEventMgr->GetEventMap().size());
    for (size_t i = 0; i < lists.size(); ++i)
    {
        int32 const eventId = int32(i) - eventCount + 1;
        if (eventId)
            for (uint32 guid : lists[i])
                spawns[guid].push_back(eventId);
    }
    return spawns;
}

inline bool OpenWorld(uint32 mapId)
{
    MapEntry const* mapEntry = sMapStore.LookupEntry(mapId);
    return mapEntry && !mapEntry->Instanceable();
}
}  // namespace RpgSpawnIndex

#endif
