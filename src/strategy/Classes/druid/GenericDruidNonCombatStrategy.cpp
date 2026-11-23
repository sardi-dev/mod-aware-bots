/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#include "GenericDruidNonCombatStrategy.h"

#include "AiFactory.h"
#include "HealerTriggers.h"
#include "Playerbots.h"

class GenericDruidNonCombatStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
public:
    GenericDruidNonCombatStrategyActionNodeFactory()
    {
        creators["mark of the wild"] = &mark_of_the_wild;
        creators["mark of the wild on party"] = &mark_of_the_wild_on_party;
        // creators["innervate"] = &innervate;
        creators["regrowth_on_party"] = &regrowth_on_party;
        creators["rejuvenation on party"] = &rejuvenation_on_party;
        creators["remove curse on party"] = &remove_curse_on_party;
        creators["abolish poison on party"] = &abolish_poison_on_party;
        creators["revive"] = &revive;
        // #384 (AC GenericDruidNonCombatStrategy.cpp:25,96-102).
        creators["aquatic form"] = &aquatic_form;
        creators["prowl"] = &prowl;
    }

private:


    static ActionNode* prowl([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("prowl", NextAction::array(0, new NextAction("cat form"), nullptr), nullptr, nullptr);
    }

    static ActionNode* mark_of_the_wild(PlayerbotAI* botAI)
    {
        return new ActionNode("mark of the wild",
                              /*P*/ BotGetSpecialization(botAI->GetBot()) == SPEC_DRUID_BALANCE ? nullptr : NextAction::array(0, new NextAction("caster form"), nullptr),
                              /*A*/ nullptr,
                              /*C*/ nullptr);
    }

    static ActionNode* mark_of_the_wild_on_party(PlayerbotAI* botAI)
    {
        return new ActionNode("mark of the wild on party",
                              /*P*/ BotGetSpecialization(botAI->GetBot()) == SPEC_DRUID_BALANCE ? nullptr : NextAction::array(0, new NextAction("caster form"), nullptr),
                              /*A*/ nullptr,
                              /*C*/ nullptr);
    }
    static ActionNode* regrowth_on_party(PlayerbotAI* ai)
    {
        return new ActionNode("regrowth on party",
                              /*P*/ NextAction::array(0, new NextAction("caster form"), nullptr),
                              /*A*/ NULL,
                              /*C*/ NULL);
    }
    static ActionNode* rejuvenation_on_party(PlayerbotAI* botAI)
    {
        return new ActionNode("rejuvenation on party",
                              /*P*/ BotGetSpecialization(botAI->GetBot()) == SPEC_DRUID_BALANCE ? nullptr : NextAction::array(0, new NextAction("caster form"), nullptr),
                              /*A*/ NULL,
                              /*C*/ NULL);
    }
    static ActionNode* remove_curse_on_party(PlayerbotAI* botAI)
    {
        return new ActionNode("remove curse on party",
                              /*P*/ BotGetSpecialization(botAI->GetBot()) == SPEC_DRUID_BALANCE ? nullptr : NextAction::array(0, new NextAction("caster form"), nullptr),
                              /*A*/ NULL,
                              /*C*/ NULL);
    }
    static ActionNode* abolish_poison_on_party(PlayerbotAI* botAI)
    {
        return new ActionNode("abolish poison on party",
                              /*P*/ BotGetSpecialization(botAI->GetBot()) == SPEC_DRUID_BALANCE ? nullptr : NextAction::array(0, new NextAction("caster form"), nullptr),
                              /*A*/ NULL,
                              /*C*/ NULL);
    }
    static ActionNode* revive(PlayerbotAI* ai)
    {
        return new ActionNode("revive",
                              /*P*/ NextAction::array(0, new NextAction("caster form"), nullptr),
                              /*A*/ NULL,
                              /*C*/ NULL);
    }
    // #384 (AC GenericDruidNonCombatStrategy.cpp:96-102): shift out of any other form before Aquatic Form.
    static ActionNode* aquatic_form([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("aquatic form",
                              /*P*/ NextAction::array(0, new NextAction("caster form"), nullptr),
                              /*A*/ nullptr,
                              /*C*/ nullptr);
    }
    // static ActionNode* innervate([[maybe_unused]] PlayerbotAI* botAI)
    // {
    //     return new ActionNode ("innervate",
    //         /*P*/ nullptr,
    //         /*A*/ NextAction::array(0, new NextAction("drink"), nullptr),
    //         /*C*/ nullptr);
    // }
};

GenericDruidNonCombatStrategy::GenericDruidNonCombatStrategy(PlayerbotAI* botAI) : NonCombatStrategy(botAI)
{
    actionNodeFactories.Add(new GenericDruidNonCombatStrategyActionNodeFactory());
}

void GenericDruidNonCombatStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    NonCombatStrategy::InitTriggers(triggers);

    if (BotGetSpecialization(botAI->GetBot()) == SPEC_DRUID_RESTORATION)
    {
        // BT-155: a healer outside combat must still reach a hurt group member.
        triggers.push_back(new TriggerNode("party member to heal out of spell range",
            NextAction::array(0, new NextAction("reach party member to heal", ACTION_CRITICAL_HEAL + 1), nullptr)));

        // BT-157: the tank's Rejuvenation and Lifebloom before the pull and while only the tank fights (the triggers hold
        // the mana guardrail, the pre-pull window and the skill tiers), and the proactive top-up and the walk back to the tank.
        triggers.push_back(new TriggerNode(
            "rejuvenation on main tank",
            NextAction::array(0, new NextAction("rejuvenation on main tank", ACTION_MEDIUM_HEAL + 0.9f), nullptr)));
        triggers.push_back(new TriggerNode(
            "lifebloom on main tank",
            NextAction::array(0, new NextAction("lifebloom on main tank", ACTION_MEDIUM_HEAL + 0.8f), nullptr)));
        triggers.push_back(new TriggerNode(
            "rejuvenation proactive",
            NextAction::array(0, new NextAction("rejuvenation proactive", ACTION_LIGHT_HEAL + HealerRelevance::PROACTIVE_HOT_OFFSET_OVER_LIGHT_HEAL),
                              nullptr)));
        triggers.push_back(new TriggerNode(
            "healer tank out of sight",
            NextAction::array(0, new NextAction("reach main tank to heal", ACTION_NORMAL + HealerRelevance::REACH_TANK_OFFSET_OVER_NORMAL),
                              nullptr)));
    }

    // #163 S8 / #147 (AC parity, GenericDruidNonCombatStrategy.cpp:151): apply the configured stance
    // to a freshly summoned treant.
    triggers.push_back(
        new TriggerNode("new pet", NextAction::array(0, new NextAction("set pet stance", 60.0f), nullptr)));

    if (botAI->GetBot()->getLevel() >= 62)
        triggers.push_back(
            new TriggerNode("mark of the wild", NextAction::array(0, new NextAction("mark of the wild", 14.0f), nullptr)));
    // Feral druid sheet: the solo stealth opener must run outside combat.
    if (BotGetSpecialization(botAI->GetBot()) == SPEC_DRUID_FERAL && !botAI->GetBot()->GetGroup())
        triggers.push_back(new TriggerNode("prowl", NextAction::array(0, new NextAction("prowl", 29.5f), nullptr)));
    triggers.push_back(new TriggerNode("moonkin noncombat", NextAction::array(0, new NextAction("moonkin form", ACTION_NORMAL), nullptr)));
    triggers.push_back(new TriggerNode("cat noncombat", NextAction::array(0, new NextAction("cat form", ACTION_NORMAL), nullptr)));
    // Class sheet, guardian druid (BT-131): a tank in a group is back in Bear Form before the next pull. ACTION_NORMAL is
    // under every edge that needs caster form (buffs 13 and 14, heals from 12 up, dispels, revive 40); the trigger also
    // stays silent for eating, drinking and the travel forms. Silent for every other spec and for a solo druid.
    triggers.push_back(
        new TriggerNode("tank bear form", NextAction::array(0, new NextAction("bear form", ACTION_NORMAL), nullptr)));
    // triggers.push_back(new TriggerNode("thorns", NextAction::array(0, new NextAction("thorns", 12.0f), nullptr)));
    // triggers.push_back(new TriggerNode("cure poison", NextAction::array(0, new NextAction("abolish poison", 21.0f),
    // nullptr)));
    triggers.push_back(new TriggerNode(
        "party member cure poison", NextAction::array(0, new NextAction("abolish poison on party", 20.0f), nullptr)));
    triggers.push_back(new TriggerNode(
        "party member dead", NextAction::array(0, new NextAction("revive", ACTION_CRITICAL_HEAL + 10), nullptr)));
    // triggers.push_back(new TriggerNode("low mana", NextAction::array(0, new NextAction("innervate", ACTION_EMERGENCY
    // + 5), nullptr))); triggers.push_back(new TriggerNode("swimming", NextAction::array(0, new NextAction("aquatic
    // form", 1.0f), nullptr)));
    // #434 (AC GenericDruidNonCombatStrategy.cpp:118): wizard/mana oils still exist in 5.4.8 (capped by
    // weapon item level), so this edge is ported 1:1 (ImbueAction.cpp).
    triggers.push_back(new TriggerNode("often", NextAction::array(0, new NextAction("apply oil", 1.0f), nullptr)));

    triggers.push_back(
        new TriggerNode("party member critical health",
                        NextAction::array(0,
                                          new NextAction("wild growth on party", ACTION_MEDIUM_HEAL + 7),
                                          new NextAction("regrowth on party", ACTION_MEDIUM_HEAL + 6),
                                          new NextAction("rejuvenation on party", ACTION_MEDIUM_HEAL + 5),
                                          nullptr)));

    triggers.push_back(
        new TriggerNode("party member low health",
                        NextAction::array(0, 
                                          new NextAction("wild growth on party", ACTION_MEDIUM_HEAL + 5),
                                          new NextAction("regrowth on party", ACTION_MEDIUM_HEAL + 4),
                                          new NextAction("rejuvenation on party", ACTION_MEDIUM_HEAL + 3),
                                          nullptr)));

    triggers.push_back(
        new TriggerNode("party member medium health",
                        NextAction::array(0, new NextAction("wild growth on party", ACTION_MEDIUM_HEAL + 3),
                                          new NextAction("regrowth on party", ACTION_MEDIUM_HEAL + 2),
                                          new NextAction("rejuvenation on party", ACTION_MEDIUM_HEAL + 1),
                                          nullptr)));

    triggers.push_back(
        new TriggerNode("party member almost full health",
                        NextAction::array(0, new NextAction("wild growth on party", ACTION_LIGHT_HEAL + 3), new NextAction("rejuvenation on party", ACTION_LIGHT_HEAL + 2), NULL)));

    triggers.push_back(
        new TriggerNode("party member remove curse",
                        NextAction::array(0, new NextAction("remove curse on party", ACTION_DISPEL + 7), nullptr)));

    // #384 (AC GenericDruidNonCombatStrategy.cpp:176): Aquatic Form (1066, level 18) is still learnable in 5.4.8
    // (cls.py druid). The "aquatic form" trigger (AquaticFormTrigger) and action are already registered.
    triggers.push_back(new TriggerNode("aquatic form", NextAction::array(0, new NextAction("aquatic form", 10.0f), nullptr)));

    // #434 (AC GenericDruidNonCombatStrategy.cpp:178-182): spec-dependent weapon imbue. AC's tabs
    // Balance/Restoration -> oil, Feral -> stone; on 5.4.8 Guardian is its own spec and, like AC's
    // Feral tab (bear + cat), gets the stone. Evaluated at strategy init, as on AC.
    int specTab = int(AiFactory::GetPlayerSpecTab(botAI->GetBot()));
    if (specTab == SPEC_DRUID_BALANCE || specTab == SPEC_DRUID_RESTORATION)
        triggers.push_back(new TriggerNode("often", NextAction::array(0, new NextAction("apply oil", 1.0f), nullptr)));
    if (specTab == SPEC_DRUID_FERAL || specTab == SPEC_DRUID_GUARDIAN)
        triggers.push_back(new TriggerNode("often", NextAction::array(0, new NextAction("apply stone", 1.0f), nullptr)));
}

GenericDruidBuffStrategy::GenericDruidBuffStrategy(PlayerbotAI* botAI) : NonCombatStrategy(botAI)
{
    actionNodeFactories.Add(new GenericDruidNonCombatStrategyActionNodeFactory());
}

void GenericDruidBuffStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    NonCombatStrategy::InitTriggers(triggers);

    if (botAI->GetBot()->getLevel() >= 62)
        triggers.push_back(
        new TriggerNode("mark of the wild on party",
                        NextAction::array(0, new NextAction("mark of the wild on party", 13.0f), nullptr)));
    // #384: AC's "thorns on main tank" buff edge (AC GenericDruidNonCombatStrategy.cpp:187-190) is dropped: Thorns is
    // NOT-LEARNABLE in 5.4.8 (cls.py druid: ids 15438/21335/..., no class skill-line row) and has no 5.x successor
    // (the 5.x druid has no thorns-type buff). The thorns actions/triggers stay registered but unused.
}
