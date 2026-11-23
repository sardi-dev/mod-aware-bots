/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#include "PaladinBuffStrategies.h"

#include "Playerbots.h"

void PaladinBuffHealthStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(
        new TriggerNode("blessing of kings on party",
                        NextAction::array(0, new NextAction("blessing of kings on party", 11.0f), nullptr)));
    // triggers.push_back(new TriggerNode("blessing", NextAction::array(0, new NextAction("blessing of kings",
    // ACTION_HIGH + 8), nullptr)));
}

void PaladinBuffDpsStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(
        new TriggerNode("blessing of might on party",
                        NextAction::array(0, new NextAction("blessing of might on party", 11.0f), nullptr)));
    // triggers.push_back(new TriggerNode("blessing", NextAction::array(0, new NextAction("blessing of might",
    // ACTION_HIGH + 8), nullptr)));
}

void PaladinBuffArmorStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // #383 (AC PaladinBuffStrategies.cpp:49-53 devotion aura -> devotion aura, ACTION_NORMAL): edge removed.
    // In 5.4.8 Devotion Aura (31821, L60) is a 3 min raid cooldown (10 s party/raid DR + immunity to
    // silence/interrupt), not a persistent keep-up aura (Wowhead mop-classic tooltip 31821, 2026-10-05:
    // "Instant, 3 min cooldown"); the BuffTrigger fired every tick and wave 9 retried it 29x with
    // why=cooldown. Strategy key "barmor" stays registered for key parity.
}

void PaladinBuffThreatStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode(
        "righteous fury", NextAction::array(0, new NextAction("righteous fury", ACTION_HIGH + 8), nullptr)));
}

void PaladinBuffStatsStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(
        new TriggerNode("blessing of kings on party",
                        NextAction::array(0, new NextAction("blessing of kings on party", 11.0f), nullptr)));
    // triggers.push_back(new TriggerNode("blessing", NextAction::array(0, new NextAction("blessing of kings",
    // ACTION_HIGH + 8), nullptr)));
}

void PaladinBuffMightSecondStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // One tick under the Kings line (11.0) so Kings always comes first when both are missing.
    triggers.push_back(new TriggerNode(
        "blessing of might second on party",
        NextAction::array(0, new NextAction("blessing of might on party", 10.9f), nullptr)));
}
