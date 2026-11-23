/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#include "MeleeDruidStrategy.h"

#include "Playerbots.h"

MeleeDruidStrategy::MeleeDruidStrategy(PlayerbotAI* botAI) : CombatStrategy(botAI) {}

NextAction** MeleeDruidStrategy::getDefaultActions()
{
    return NextAction::array(0, new NextAction("faerie fire", ACTION_DEFAULT + 0.1f),
                             new NextAction("melee", ACTION_DEFAULT), nullptr);
}

void MeleeDruidStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // BT-146: Omen of Clarity is passive in 5.4.8 and cannot be cast.

    CombatStrategy::InitTriggers(triggers);
}
