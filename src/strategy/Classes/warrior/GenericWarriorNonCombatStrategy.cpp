#include "GenericWarriorNonCombatStrategy.h"

#include "Playerbots.h"

class GenericWarriorNonCombatStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
public:
    GenericWarriorNonCombatStrategyActionNodeFactory()
    {
    }

private:
};

GenericWarriorNonCombatStrategy::GenericWarriorNonCombatStrategy(PlayerbotAI* botAI) : NonCombatStrategy(botAI)
{
    actionNodeFactories.Add(new GenericWarriorNonCombatStrategyActionNodeFactory());
}

void GenericWarriorNonCombatStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    NonCombatStrategy::InitTriggers(triggers);

    // #522 (AC GenericWarriorNonCombatStrategy.cpp:36-39). "apply stone" exists since #434.
    triggers.push_back(new TriggerNode("often", NextAction::array(0, new NextAction("apply stone", 1.0f), nullptr)));
    // Arms and Fury sheets: apply the shout before combat when it is missing.
    triggers.push_back(new TriggerNode("battle shout", NextAction::array(0, new NextAction("battle shout", ACTION_NORMAL), nullptr)));
    // Vigilance 114030 (5.4.8 talent, Wowhead tooltip 2026-10-04); target pick/trigger fixed for #196.
    triggers.push_back(new TriggerNode("vigilance", NextAction::array(0, new NextAction("vigilance", 10.0f), nullptr)));
    // Class sheet, protection warrior (BT-125): Defensive Stance before the pull. The trigger is silent for every other
    // spec, for a solo warrior and while mounted.
    triggers.push_back(new TriggerNode("tank defensive stance", NextAction::array(0, new NextAction("defensive stance", ACTION_NORMAL), nullptr)));
    // AC's berserker rage node (P: "berserker stance", AC :20) is dropped: Berserker Rage needs no stance on 5.4.8.
    triggers.push_back(new TriggerNode("fear sleep sap", NextAction::array(0, new NextAction("berserker rage", ACTION_EMERGENCY + 1), nullptr)));
}
