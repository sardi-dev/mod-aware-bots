/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "OSStrategy.h"

#include "OSMultipliers.h"
#include "Playerbots.h"

// AC Ai/Raid/OS/OSStrategy.cpp:10. 5.4.8: TriggerNode takes a NextAction::array (strategy/Trigger.h:45). AC's exit
// portal trigger is left out: the 5.4.8 script takes players out of the twilight realm itself when the acolyte dies
// (boss_sartharion.cpp, SPELL_TWILIGHT_SHIFT_REMOVAL_ALL) and spawns no Normal Portal.
void RaidOsStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode(
        "sartharion tank", NextAction::array(0, new NextAction("sartharion tank position", ACTION_MOVE), nullptr)));
    triggers.push_back(new TriggerNode(
        "twilight fissure", NextAction::array(0, new NextAction("avoid twilight fissure", ACTION_RAID + 2), nullptr)));
    triggers.push_back(new TriggerNode(
        "flame tsunami", NextAction::array(0, new NextAction("avoid flame tsunami", ACTION_RAID + 1), nullptr)));
    triggers.push_back(new TriggerNode(
        "sartharion dps", NextAction::array(0, new NextAction("sartharion attack priority", ACTION_RAID), nullptr)));
    // Flank dragon positioning
    triggers.push_back(new TriggerNode(
        "sartharion melee positioning", NextAction::array(0, new NextAction("rear flank", ACTION_MOVE + 4), nullptr)));

    triggers.push_back(new TriggerNode(
        "twilight portal enter", NextAction::array(0, new NextAction("enter twilight portal", ACTION_RAID + 1), nullptr)));
}

void RaidOsStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new SartharionMultiplier(botAI));
}
