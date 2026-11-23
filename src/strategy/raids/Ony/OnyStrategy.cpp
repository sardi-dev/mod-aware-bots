/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "OnyStrategy.h"
#include "Playerbots.h"

// AC Ai/Raid/Ony/OnyStrategy.cpp:10. 5.4.8: TriggerNode takes a NextAction::array (strategy/Trigger.h:45); AC's empty
// InitMultipliers override is dropped, Strategy has a no-op default (strategy/Strategy.h:61).
void RaidOnyxiaStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // ----------- Phase 1 (100% - 60%) -----------

    triggers.push_back(new TriggerNode(
        "ony near tail", NextAction::array(0, new NextAction("ony move to side", ACTION_RAID + 2), nullptr)));

    triggers.push_back(new TriggerNode(
        "ony avoid eggs", NextAction::array(0, new NextAction("ony avoid eggs move", ACTION_EMERGENCY + 5), nullptr)));

    // ----------- Phase 2 (60% - 40%) -----------

    triggers.push_back(new TriggerNode(
        "ony deep breath warning",
        NextAction::array(0, new NextAction("ony move to safe zone", ACTION_EMERGENCY + 5), nullptr)));

    triggers.push_back(new TriggerNode(
        "ony fireball splash incoming",
        NextAction::array(0, new NextAction("ony spread out", ACTION_EMERGENCY + 2), nullptr)));

    triggers.push_back(new TriggerNode(
        "ony whelps spawn", NextAction::array(0, new NextAction("ony kill whelps", ACTION_RAID + 1), nullptr)));
}
