/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#include "HunterBuffStrategies.h"

#include "Playerbots.h"

HunterBuffDpsStrategy::HunterBuffDpsStrategy(PlayerbotAI* botAI) : NonCombatStrategy(botAI)
{
}

void HunterBuffDpsStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("iron hawk", NextAction::array(0, new NextAction("aspect of the iron hawk", 21.0f), nullptr)));
    triggers.push_back(
        new TriggerNode("aspect of the hawk", NextAction::array(0, new NextAction("aspect of the hawk", 20.0f), nullptr)));
}

void HunterBuffSpeedStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("aspect of the pack",
                                       NextAction::array(0, new NextAction("aspect of the pack", 20.0f), nullptr)));
}

