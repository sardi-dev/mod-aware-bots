/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#include "GenericPaladinNonCombatStrategy.h"

#include "AiFactory.h"
#include "GenericPaladinStrategyActionNodeFactory.h"
#include "HealerTriggers.h"
#include "Playerbots.h"

GenericPaladinNonCombatStrategy::GenericPaladinNonCombatStrategy(PlayerbotAI* botAI) : NonCombatStrategy(botAI)
{
    actionNodeFactories.Add(new GenericPaladinStrategyActionNodeFactory());
}

// #424: AC GenericPaladinNonCombatStrategy::InitTriggers edges that are NOT ported, and why
// (Wowpedia patch notes, read 2026-10-04):
// - "not sensing undead" -> "sense undead": Sense Undead was removed in patch 4.0.1, so the spell
//   does not exist in 5.4.8; the trigger and action are removed.
// - "greater blessing needed" -> "cast greater blessing assignment": Greater Blessings were removed
//   in patch 4.0.1; the fork has no ai::gbless subsystem (see PaladinAiObjectContext.cpp).
// - "often" -> "apply oil" (holy) / "apply stone" (prot, ret): NOT a divergence. Oils and stones
//   still exist in 5.4.8 (item-level capped). Ported under #434 (ImbueAction.cpp), see below.

void GenericPaladinNonCombatStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    NonCombatStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode("party member dead", NextAction::array(0, new NextAction("redemption", ACTION_CRITICAL_HEAL + 10), nullptr)));

    int specTab = int(AiFactory::GetPlayerSpecTab(botAI->GetBot()));

    if (specTab == SPEC_PALADIN_HOLY)
    {
        // BT-155: a healer outside combat must still reach a hurt group member.
        triggers.push_back(new TriggerNode("party member to heal out of spell range",
            NextAction::array(0, new NextAction("reach party member to heal", ACTION_CRITICAL_HEAL + 1), nullptr)));

        // BT-157: the tank's upkeep before the pull and while only the tank fights (the triggers hold the mana guardrail, the
        // pre-pull window and the skill tiers), the proactive top-up, and the walk back to the tank.
        triggers.push_back(new TriggerNode(
            "beacon of light on main tank",
            NextAction::array(0, new NextAction("beacon of light on main tank", ACTION_MEDIUM_HEAL + 1.9f), nullptr)));
        triggers.push_back(new TriggerNode(
            "sacred shield on main tank",
            NextAction::array(0, new NextAction("sacred shield on main tank", ACTION_MEDIUM_HEAL + 1.8f), nullptr)));
        triggers.push_back(new TriggerNode(
            "holy shock proactive",
            NextAction::array(0, new NextAction("holy shock proactive", ACTION_LIGHT_HEAL + HealerRelevance::PROACTIVE_HOT_OFFSET_OVER_LIGHT_HEAL),
                              nullptr)));
        triggers.push_back(new TriggerNode(
            "healer tank out of sight",
            NextAction::array(0, new NextAction("reach main tank to heal", ACTION_NORMAL + HealerRelevance::REACH_TANK_OFFSET_OVER_NORMAL),
                              nullptr)));

        // Class sheet, holy paladin (BT-132): top-ups out of combat with the cheap heals, never Flash of Light (37.8 percent
        // mana, spent right before the drink that has to give it back). Holy Shock (8 percent, from level 10) for anyone
        // under 85 percent, Holy Light (12.6 percent, level 34) after it, Word of Glory for free at 3 Holy Power (under
        // 65 percent) or at one Holy Power for a member under the critical threshold.
        triggers.push_back(new TriggerNode("party member almost full health", NextAction::array(0,
            new NextAction("holy shock on party", 26.0f), new NextAction("holy light on party", 25.0f), nullptr)));
        triggers.push_back(new TriggerNode("party member medium health and holy power 3 available", NextAction::array(0, new NextAction("word of glory on party", 27.0f), nullptr)));
        triggers.push_back(new TriggerNode("party member critical health and holy power 1 available", NextAction::array(0, new NextAction("word of glory on party", 28.0f), nullptr)));
    }
    else
    {
        // Retribution sheet: expensive Flash of Light starts at medium health.

        triggers.push_back(new TriggerNode("party member medium health", NextAction::array(0, new NextAction("flash of light on party", 26.0f), nullptr)));
        triggers.push_back(new TriggerNode("party member low health", NextAction::array(0, new NextAction("flash of light on party", 26.0f), nullptr)));
        triggers.push_back(new TriggerNode("party member critical health", NextAction::array(0, new NextAction("flash of light on party", 26.0f), nullptr)));
        // #735: Word of Glory (85673) costs 1-3 Holy Power - gated by the "holy power 1 available" trigger (AC's
        // TwoTriggers pattern, AC TriggerContext.h:125,375) instead of in the action.
        triggers.push_back(new TriggerNode("paladin word of glory on party", NextAction::array(0, new NextAction("word of glory on party", 27.0f), nullptr)));
        triggers.push_back(new TriggerNode("party member critical health and holy power 1 available", NextAction::array(0, new NextAction("word of glory on party", 28.0f), nullptr)));
    }

    // #434 (AC GenericPaladinNonCombatStrategy.cpp:28-32): spec-dependent weapon imbue, same
    // priority (ACTION_IDLE + 1.0f). Holy -> oil; Protection / Retribution -> stone.
    if (specTab == SPEC_PALADIN_HOLY)
        triggers.push_back(new TriggerNode("often", NextAction::array(0, new NextAction("apply oil", ACTION_IDLE + 1.0f), nullptr)));
    if (specTab == SPEC_PALADIN_PROTECTION || specTab == SPEC_PALADIN_RETRIBUTION)
        triggers.push_back(new TriggerNode("often", NextAction::array(0, new NextAction("apply stone", ACTION_IDLE + 1.0f), nullptr)));
}
