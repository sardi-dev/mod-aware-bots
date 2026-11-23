/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#include "HealPaladinStrategy.h"

#include "HealerTriggers.h"
#include "Playerbots.h"
#include "Strategy.h"

HealPaladinStrategy::HealPaladinStrategy(PlayerbotAI* botAI) : GenericPaladinStrategy(botAI)
{
}

// Class sheet, holy paladin (BT-132): the holy chain in InitTriggers replaces the shared party heal lines.
void HealPaladinStrategy::InitPartyHealTriggers([[maybe_unused]] std::vector<TriggerNode*>& triggers)
{
}

NextAction** HealPaladinStrategy::getDefaultActions()
{
    // 5.4.8: Judgement of Light does not exist; Judgement is a single unified ability in MoP, so this
    // now casts the generic "judgement" action.
    return NextAction::array(0, new NextAction("judgement", ACTION_DEFAULT), nullptr);
}

void HealPaladinStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    GenericPaladinStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode("seal", NextAction::array(0, new NextAction("seal of insight", ACTION_HIGH), nullptr)));
    // #383 (AC HealPaladinStrategy.cpp "seal" -> seal of wisdom): already mapped to Seal of Insight (20165,
    // the 5.x healer seal: "+5% healing, melee hits heal, Holy restores 6% base mana", Wowhead 2026-10-04).
    // rotation task 6 (#861): Divine Plea at mana <= 30% (the "low mana" edge)
    triggers.push_back(new TriggerNode("low mana", NextAction::array(0, new NextAction("divine plea", ACTION_HIGH + 2),
                                                                      new NextAction("divine favor", ACTION_HIGH + 1), nullptr)));
    // rotation task 6 (#861): Divine Favor, Guardian of Ancient Kings and Avenging Wrath when
    // >= 3 group members are below 50% (within 40 yd)
    triggers.push_back(new TriggerNode("paladin 3 hurt group", NextAction::array(0,
        new NextAction("guardian of ancient kings", ACTION_HIGH + 4),
        new NextAction("avenging wrath", ACTION_HIGH + 3),
        new NextAction("divine favor", ACTION_HIGH + 2), nullptr)));
    // rotation task 6 (#861): Holy Radiance, 3+ members < 50% within 10 yd of the target. Above Flash of
    // Light / Divine Light / Holy Light (rows 6511-6513), below Holy Shock / Eternal Flame (rows 6507-6509).
    triggers.push_back(new TriggerNode("paladin holy radiance group",
        NextAction::array(0, new NextAction("holy radiance on party", ACTION_CRITICAL_HEAL + 4.8f), nullptr)));
    triggers.push_back(new TriggerNode("party member to heal out of spell range", NextAction::array(0, new NextAction("reach party member to heal", ACTION_EMERGENCY + 3), nullptr)));
    // BT-293 (left for later there): Hand of Sacrifice on a bot main tank whose danger reading calls for an external,
    // the same rule and rank as the other healers' externals.
    triggers.push_back(new TriggerNode("main tank in danger",
        NextAction::array(0, new NextAction("hand of sacrifice on main tank", ACTION_CRITICAL_HEAL + 4.5f), nullptr)));

    // BT-683 to BT-687 (PvP class rules, Holy package; every edge answers only in a fight against players): the hands above the
    // heals, Devotion Aura in a silence, the beacon on the focused ally, and Denounce in the downtime just above the default actions.
    triggers.push_back(new TriggerNode("pvp hand of sacrifice",
        NextAction::array(0, new NextAction("pvp hand of sacrifice", ACTION_EMERGENCY - 1), nullptr)));
    triggers.push_back(new TriggerNode("pvp hand of protection",
        NextAction::array(0, new NextAction("pvp hand of protection", ACTION_EMERGENCY - 2), nullptr)));
    triggers.push_back(new TriggerNode("pvp devotion aura",
        NextAction::array(0, new NextAction("pvp devotion aura", ACTION_EMERGENCY - 3), nullptr)));
    triggers.push_back(new TriggerNode("pvp beacon", NextAction::array(0, new NextAction("pvp beacon", ACTION_MEDIUM_HEAL + 6), nullptr)));
    triggers.push_back(new TriggerNode("pvp denounce", NextAction::array(0, new NextAction("pvp denounce", ACTION_NORMAL), nullptr)));

    triggers.push_back(
        new TriggerNode("medium group heal setting", NextAction::array(0,
            new NextAction("avenging wrath", ACTION_HIGH + 4),
            nullptr)));

    // Class sheet, holy paladin (BT-132): the single target chain is ordered by cost. The health bands are cumulative
    // (a member at 20 percent fires all four), so each action below is listed once at the band where it starts to apply
    // and the highest number among everything that fires wins:
    //   Eternal Flame / Word of Glory at 3 Holy Power (26.2 / 26.1, under 65 percent), Holy Shock on cooldown (25.9, under
    //   85 percent: 8 percent mana, instant), Divine Light only under 45 percent (25.5), Holy Light (12.6 percent mana) in
    //   every band, and Flash of Light (37.8 percent mana) only under 25 percent and as the fallback where Holy Light is
    //   not learned yet (below level 34 it is the main heal; the engine skips an unknown spell and takes the next line).
    // The shared lines of GenericPaladinStrategy (Word of Glory at 1 Holy Power, Flash of Light under 45 percent) are not
    // used here, see InitPartyHealTriggers.
    triggers.push_back(
        new TriggerNode("party member critical health", NextAction::array(0,
            new NextAction("holy shock on party", ACTION_CRITICAL_HEAL + 6),
            new NextAction("eternal flame on party", ACTION_CRITICAL_HEAL + 5),
            new NextAction("flash of light on party", ACTION_CRITICAL_HEAL + 4.5f),
            new NextAction("divine light on party", ACTION_CRITICAL_HEAL + 4.2f),
            new NextAction("holy light on party", ACTION_CRITICAL_HEAL + 4), nullptr)));

    // Word of Glory spends one or two Holy Power only for a member below the critical threshold.
    triggers.push_back(new TriggerNode("party member critical health and holy power 1 available",
        NextAction::array(0, new NextAction("word of glory on party", ACTION_CRITICAL_HEAL + 4.9f), nullptr)));

    triggers.push_back(
        new TriggerNode("party member low health",
                        NextAction::array(0, new NextAction("divine light on party", ACTION_MEDIUM_HEAL + 5.5f),
                                          new NextAction("holy light on party", ACTION_MEDIUM_HEAL + 5),
                                          new NextAction("flash of light on party", ACTION_MEDIUM_HEAL + 4.5f),
                                          nullptr)));

    // One rule for both Holy Power heals: the lowest member under 65 percent and 3 Holy Power (Eternal Flame also takes a
    // Divine Purpose proc, its action checks that). Eternal Flame replaces Word of Glory once talented, so only one of the
    // two is ever castable.
    triggers.push_back(
        new TriggerNode("party member medium health",
                        NextAction::array(0, new NextAction("eternal flame on party", ACTION_MEDIUM_HEAL + 6.2f),
                                          new NextAction("execution sentence on party", ACTION_LIGHT_HEAL + 11),
                                          new NextAction("holy light on party", ACTION_LIGHT_HEAL + 9),
                                          new NextAction("flash of light on party", ACTION_LIGHT_HEAL + 8), nullptr)));
    triggers.push_back(new TriggerNode(
        "party member medium health and holy power 3 available",
        NextAction::array(0, new NextAction("word of glory on party", ACTION_MEDIUM_HEAL + 6.1f), nullptr)));

    triggers.push_back(new TriggerNode(
        "party member almost full health",
        NextAction::array(0, new NextAction("holy shock on party", ACTION_MEDIUM_HEAL + 5.9f),
                          new NextAction("holy light on party", ACTION_LIGHT_HEAL + 3),
                          new NextAction("flash of light on party", ACTION_LIGHT_HEAL + 2), nullptr)));

    triggers.push_back(new TriggerNode(
        "beacon of light on main tank",
        NextAction::array(0, new NextAction("beacon of light on main tank", ACTION_CRITICAL_HEAL + 7), nullptr)));

    triggers.push_back(new TriggerNode(
        "sacred shield on main tank",
        NextAction::array(0, new NextAction("sacred shield on main tank", ACTION_CRITICAL_HEAL + 6), nullptr)));

    // BT-157: the paladin has no heal over time, so the proactive top-up is Holy Shock (8 percent mana, instant, gives Holy
    // Power) on the member who is a little hurt, under the same guardrails; both upkeep triggers above hold the tank guardrail.
    triggers.push_back(new TriggerNode(
        "holy shock proactive",
        NextAction::array(0, new NextAction("holy shock proactive", ACTION_LIGHT_HEAL + HealerRelevance::PROACTIVE_HOT_OFFSET_OVER_LIGHT_HEAL),
                          nullptr)));
    triggers.push_back(new TriggerNode(
        "healer tank out of sight",
        NextAction::array(0, new NextAction("reach main tank to heal", ACTION_NORMAL + HealerRelevance::REACH_TANK_OFFSET_OVER_NORMAL),
                          nullptr)));
}
