/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 *
 * Ported from the mod-playerbots module for AzerothCore (Ai/Base/Strategy/LfgStrategy.cpp) into the
 * Legends of Azeroth Pandaria fork (#210 S10 G11 registration). See LfgStrategy.h for what is/isn't
 * ported in this batch.
 */

#include "LfgStrategy.h"

#include "Playerbots.h"

void LfgStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // #142 (AC LfgStrategy.cpp:11-13): "random" -> "lfg join", "seldom" -> "lfg leave".
    triggers.push_back(new TriggerNode("random", NextAction::array(0, new NextAction("lfg join", relevance), nullptr)));
    triggers.push_back(new TriggerNode("seldom", NextAction::array(0, new NextAction("lfg leave", relevance), nullptr)));
    triggers.push_back(new TriggerNode(
        "unknown dungeon", NextAction::array(0, new NextAction("give leader in dungeon", relevance), nullptr)));
    // Beyond AC: finder manners in the group (greeting, ready, thanks, leaving after the last boss). Below the loot
    // and fight actions; the action itself waits for the fight and the loot before it leaves.
    triggers.push_back(new TriggerNode(
        "lfg manners", NextAction::array(0, new NextAction("lfg manners", ACTION_NORMAL + 5), nullptr)));
    // Beyond AC: a bot the dungeon never took in asks for the teleport again once it is out of the fight (LfgTeleportRetryAction).
    triggers.push_back(new TriggerNode(
        "lfg teleport due", NextAction::array(0, new NextAction("lfg teleport retry", ACTION_NORMAL + 6), nullptr)));
    // Beyond AC: the leader of a bot-only run walks the dungeon. Above the default "new rpg" action (11), so a leader that
    // waits for its group is not sent roaming; the action stands still itself while it waits.
    triggers.push_back(new TriggerNode(
        "lfg crawl", NextAction::array(0, new NextAction("lfg crawl", ACTION_NORMAL + 4), nullptr)));
}

LfgStrategy::LfgStrategy(PlayerbotAI* botAI) : PassTroughStrategy(botAI) {}

void LfgCombatLeaveStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // The trigger is active only for a leave the plan allows from combat (a group without a player, the fight over), so
    // it may outrank the combat actions: those of a bot nothing can reach fail or repeat (BT-202, run 7).
    triggers.push_back(new TriggerNode(
        "lfg manners", NextAction::array(0, new NextAction("lfg manners", ACTION_INTERRUPT), nullptr)));
}
