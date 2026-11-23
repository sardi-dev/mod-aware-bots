/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#include "FuryWarriorStrategy.h"

#include "Playerbots.h"

class FuryWarriorStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
public:
    FuryWarriorStrategyActionNodeFactory()
    {
        creators["raging blow"] = &raging_blow;
        creators["bloodthirst"] = &bloodthirst;
        creators["wild strike"] = &wild_strike;
        creators["skull banner"] = &skull_banner;
    }

private:
    ACTION_NODE(raging_blow, "raging blow");
    ACTION_NODE(bloodthirst, "bloodthirst");
    ACTION_NODE(wild_strike, "wild strike");
    ACTION_NODE(skull_banner, "skull banner");
};

FuryWarriorStrategy::FuryWarriorStrategy(PlayerbotAI* botAI) : GenericWarriorStrategy(botAI)
{
    actionNodeFactories.Add(new FuryWarriorStrategyActionNodeFactory());
}

NextAction** FuryWarriorStrategy::getDefaultActions()
{
    return NextAction::array(0,
        new NextAction("recklessness", ACTION_DEFAULT + 0.7f),
        new NextAction("bloodbath", ACTION_DEFAULT + 0.6f),
        new NextAction("bloodthirst", ACTION_DEFAULT + 0.5f),
        // Task 11 (#866): "Dragon Roar" (class row, 0/7: below Bloodthirst 0/8, above Raging
        // Blow 0/6), target within melee. Talent: IMPOSSIBLE (silent) when not taken. It was only in the aoe strategy.
        new NextAction("dragon roar", ACTION_DEFAULT + 0.45f),
        new NextAction("wild strike", ACTION_DEFAULT + 0.35f),
        new NextAction("sunder armor", ACTION_DEFAULT + 0.05f),
        new NextAction("execute", ACTION_DEFAULT + 0.2f),
        new NextAction("melee", ACTION_DEFAULT), NULL);
}

void FuryWarriorStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    GenericWarriorStrategy::InitTriggers(triggers);

    // Damage sheet: throw only after Charge and Leap are unavailable.
    triggers.push_back(new TriggerNode("warrior expert throw", NextAction::array(0, new NextAction("heroic throw", ACTION_MOVE + 8), nullptr)));

    // BT-032: the stance first, as Arms and Protection do; the stance actions pick one (grouped Battle, solo Berserker)
    triggers.push_back(new TriggerNode("battle stance", NextAction::array(0, new NextAction("battle stance", ACTION_HIGH + 10), nullptr)));
    triggers.push_back(new TriggerNode("berserker stance", NextAction::array(0, new NextAction("berserker stance", ACTION_HIGH + 10), nullptr)));
    triggers.push_back(new TriggerNode("battle shout", NextAction::array(0, new NextAction("battle shout", ACTION_HIGH + 9), nullptr)));
    // Task 11 (#866): Bloodbath 12292 (prio 5/2, "always").
    triggers.push_back(new TriggerNode("bloodbath buff", NextAction::array(0, new NextAction("bloodbath", ACTION_HIGH), nullptr)));
    // Fury sheet: preserve two Raging Blow uses, then keep Bloodthirst on cooldown.
    triggers.push_back(new TriggerNode("raging blow two stacks", NextAction::array(0, new NextAction("raging blow", ACTION_HIGH + 10.25f), nullptr)));
    triggers.push_back(new TriggerNode("fury pack whirlwind", NextAction::array(0, new NextAction("whirlwind", ACTION_HIGH + 10.5f), nullptr)));
    triggers.push_back(new TriggerNode("bloodthirst ready", NextAction::array(0, new NextAction("bloodthirst", ACTION_HIGH + 10), nullptr)));
    triggers.push_back(new TriggerNode("raging blow", NextAction::array(0, new NextAction("raging blow", ACTION_HIGH + 9), nullptr)));
    triggers.push_back(new TriggerNode("meat cleaver", NextAction::array(0, new NextAction("raging blow", ACTION_HIGH + 9), nullptr)));
    triggers.push_back(new TriggerNode("bloodsurge", NextAction::array(0, new NextAction("wild strike", ACTION_HIGH + 9), nullptr)));

    // #522 (AC FuryWarriorStrategy.cpp:406-413): interrupt the enemy healer with Pummel 6552 (L24).
    triggers.push_back(new TriggerNode("pummel on enemy healer", NextAction::array(0, new NextAction("pummel on enemy healer", ACTION_INTERRUPT), nullptr)));
    // Fury sheet: save 30 rage for Wild Strike and dump at 60.
    triggers.push_back(new TriggerNode("high rage available", NextAction::array(0, new NextAction("heroic strike", ACTION_DEFAULT + 0.4f), nullptr)));
    // #522 (AC :487-494): Enraged Regeneration 55694 (talent).
    // Keep an emergency heal for all skill tiers.
    triggers.push_back(new TriggerNode("critical health", NextAction::array(0, new NextAction("enraged regeneration", ACTION_INTERRUPT - 2), nullptr)));
    triggers.push_back(new TriggerNode("tank enraged regeneration health", NextAction::array(0, new NextAction("enraged regeneration", ACTION_INTERRUPT - 2), nullptr)));

    // Fury sheet: snare escapees, interrupt pack casters and keep a panic button.
    triggers.push_back(new TriggerNode("hamstring", NextAction::array(0, new NextAction("hamstring", ACTION_HIGH), nullptr)));
    triggers.push_back(new TriggerNode("enemy caster in range", NextAction::array(0, new NextAction("pummel on pack caster", ACTION_INTERRUPT), new NextAction("disrupting shout on enemy caster", ACTION_INTERRUPT - 1), nullptr)));
    triggers.push_back(new TriggerNode("critical health", NextAction::array(0, new NextAction("intimidating shout", ACTION_EMERGENCY), nullptr)));

    // #522 DIVERGENCES: AC "instant slam -> slam" (:446-453) is already the 5.x "bloodsurge -> wild strike" above;
    // "bloodrage" and "death wish" (:454-478) are not learnable in 5.4.8 (cls.py 2026-10-04).
}
