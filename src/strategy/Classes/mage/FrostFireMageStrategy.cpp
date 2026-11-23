/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license; you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option) any later version.
 */

#include "FrostFireMageStrategy.h"

#include "Playerbots.h"

FrostFireMageStrategy::FrostFireMageStrategy(PlayerbotAI* botAI) : GenericMageStrategy(botAI)
{
    // No custom ActionNodeFactory needed
}

// ===== Default Actions =====
// MoP base-class form: this fork's Strategy::getDefaultActions() returns NextAction** (Strategy.h:49),
// not AC's std::vector<NextAction>. Values are AC's, restated on MoP's ACTION_DEFAULT scale.
NextAction** FrostFireMageStrategy::getDefaultActions()
{
    return NextAction::array(0,
        new NextAction("frostfire bolt", ACTION_DEFAULT + 0.2f),
        new NextAction("fire blast", ACTION_DEFAULT + 0.1f),  // cast during movement
        new NextAction("shoot", ACTION_DEFAULT), nullptr);
}

// ===== Trigger Initialization =====
void FrostFireMageStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    GenericMageStrategy::InitTriggers(triggers);

    // Debuff Triggers
    triggers.push_back(
        new TriggerNode("living bomb",
            NextAction::array(0,
                new NextAction("living bomb", 18.5f), nullptr)));

    triggers.push_back(
        new TriggerNode("enemy too close for spell",
            NextAction::array(0,
                new NextAction("dragon's breath", ACTION_INTERRUPT + 1), nullptr)));
}