/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "EoEStrategy.h"

#include "EoEMultipliers.h"
#include "Playerbots.h"

// AC Ai/Raid/EoE/EoEStrategy.cpp:10. "group flying" and "drake combat" are the Oculus triggers
// (strategy/dungeons/OC/OCTriggerContext.h), as in AC.
void RaidEoEStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode(
        "malygos", NextAction::array(0, new NextAction("malygos position", ACTION_MOVE), nullptr)));
    triggers.push_back(new TriggerNode(
        "malygos", NextAction::array(0, new NextAction("malygos target", ACTION_RAID + 1), nullptr)));

    triggers.push_back(new TriggerNode(
        "group flying", NextAction::array(0, new NextAction("eoe fly drake", ACTION_NORMAL + 1), nullptr)));
    triggers.push_back(new TriggerNode(
        "drake combat", NextAction::array(0, new NextAction("eoe drake attack", ACTION_NORMAL + 5), nullptr)));
}

void RaidEoEStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new MalygosMultiplier(botAI));
}
