/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#include "GenericDKNonCombatStrategy.h"

#include "Playerbots.h"

class GenericDKNonCombatStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
public:
    GenericDKNonCombatStrategyActionNodeFactory()
    {
        creators["bone shield"] = &bone_shield;
        creators["horn of winter"] = &horn_of_winter;
    }

private:
    static ActionNode* bone_shield([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("bone shield",
                              /*P*/ nullptr,
                              /*A*/ nullptr,
                              /*C*/ nullptr);
    }

    static ActionNode* horn_of_winter([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("horn of winter",
                              /*P*/ nullptr,
                              /*A*/ nullptr,
                              /*C*/ nullptr);
    }
};

GenericDKNonCombatStrategy::GenericDKNonCombatStrategy(PlayerbotAI* botAI) : NonCombatStrategy(botAI)
{
    actionNodeFactories.Add(new GenericDKNonCombatStrategyActionNodeFactory());
}

void GenericDKNonCombatStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    NonCombatStrategy::InitTriggers(triggers);

    // #434 (AC GenericDKNonCombatStrategy.cpp:18): sharpening stone / weightstone on the weapons, 1:1.
    triggers.push_back(new TriggerNode("often", NextAction::array(0, new NextAction("apply stone", 1.0f), nullptr)));
    // #128 (AC GenericDKNonCombatStrategy.cpp:14-23): AC raises no ghoul and toggles no pet spell out of
    // combat; the MoP "no pet -> raise dead" / "has pet -> toggle pet spell" lines here looped Raise Dead.
    triggers.push_back(
        new TriggerNode("horn of winter", NextAction::array(0, new NextAction("horn of winter", 21.0f), nullptr)));
    triggers.push_back(
        new TriggerNode("bone shield", NextAction::array(0, new NextAction("bone shield", 21.0f), nullptr)));
    // Frost and Unholy sheets: start the pull in the damage presence.
    triggers.push_back(new TriggerNode("dk dps frost presence", NextAction::array(0, new NextAction("frost presence", ACTION_HIGH + 2), nullptr)));
    triggers.push_back(new TriggerNode("dk dps unholy presence", NextAction::array(0, new NextAction("unholy presence", ACTION_HIGH + 2), nullptr)));
    // Frost and Unholy sheets (expert): Army of the Dead before a boss pull, after the presence.
    triggers.push_back(new TriggerNode("dk pre-pull army", NextAction::array(0, new NextAction("army of the dead", ACTION_HIGH + 1), nullptr)));
    // Class sheet, blood death knight (BT-134): the tank takes Blood Presence out of combat (Frost Presence at levels 55 and
    // 56, before Blood Presence is learned), so the pull and a fight after a resurrection do not start without it. Both
    // triggers are silent for every other spec, for a solo death knight and while mounted. Same shape as the warrior's
    // "tank defensive stance".
    triggers.push_back(new TriggerNode(
        "tank blood presence", NextAction::array(0, new NextAction("blood presence", ACTION_HIGH + 2), nullptr)));
    triggers.push_back(new TriggerNode(
        "tank frost presence", NextAction::array(0, new NextAction("frost presence", ACTION_HIGH + 2), nullptr)));
}

void DKBuffDpsStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // triggers.push_back(new TriggerNode("improved icy talons", NextAction::array(0, new NextAction("improved icy
    // talons", 19.0f), nullptr)));
}
