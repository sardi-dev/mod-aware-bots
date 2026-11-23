/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "VoAStrategy.h"

#include "Playerbots.h"

// AC Ai/Raid/VoA/VoAStrategy.cpp:12. 5.4.8: AC's Emalon nature and Koralon fire resistance auras are dropped (no
// paladin resistance auras or Aspect of the Wild in 5.4.8), and so is the fall-under-the-floor teleport (a player
// cannot teleport).
void RaidVoAStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // Emalon the Storm Watcher
    triggers.push_back(new TriggerNode(
        "emalon lighting nova trigger",
        NextAction::array(0, new NextAction("emalon lighting nova action", ACTION_RAID + 1), nullptr)));

    triggers.push_back(new TriggerNode(
        "emalon mark boss trigger",
        NextAction::array(0, new NextAction("emalon mark boss action", ACTION_RAID), nullptr)));

    triggers.push_back(new TriggerNode(
        "emalon overcharge trigger",
        NextAction::array(0, new NextAction("emalon overcharge action", ACTION_RAID), nullptr)));
}
