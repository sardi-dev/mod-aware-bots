/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#include "CasterShamanStrategy.h"

#include "AiFactory.h"
#include "Multiplier.h"
#include "Playerbots.h"
#include "PvpElemental.h"
#include <algorithm>

namespace
{
// BT-680: against a player Ascendance, the Fire Elemental and Elemental Mastery are the burst of the window, and Ancestral Swiftness
// waits for the Hex on the healer (PvP/PvpElemental.cpp). PvE and casual bots keep the older gates.
class PvpElementalMultiplier : public Multiplier
{
public:
    PvpElementalMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "pvp elemental") {}

    float GetValue(Action* action) override { return PvpElementalGlue::Holds(botAI, bot, action) ? 0.0f : 1.0f; }
};
}  // namespace

CasterShamanStrategy::CasterShamanStrategy(PlayerbotAI* botAI) : GenericShamanStrategy(botAI) {}

void CasterShamanStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    GenericShamanStrategy::InitMultipliers(multipliers);
    multipliers.push_back(new PvpElementalMultiplier(botAI));
}

NextAction** CasterShamanStrategy::getDefaultActions()
{
    // Elemental sheet: avoid resolving spells before they are learned.
    Player* bot = botAI->GetBot();
    std::vector<NextAction*> actions;
    if (bot->HasSpell(73680)) actions.push_back(new NextAction("unleash elements", 5.25f));
    if (bot->HasSpell(51505)) actions.push_back(new NextAction("lava burst", 5.2f));
    if (bot->HasSpell(117014)) actions.push_back(new NextAction("elemental blast", 5.15f));
    actions.push_back(new NextAction("lightning bolt", 5.1f));
    actions.push_back(nullptr);
    NextAction** result = new NextAction*[actions.size()];
    std::copy(actions.begin(), actions.end(), result);
    return result;
}

void CasterShamanStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    GenericShamanStrategy::InitTriggers(triggers);
    
    // #382 (AC ElementalShamanStrategy.cpp:29-35 "call of the elements" 60): DIVERGENCE, edge removed. AC's spell is
    // 66842 "Simultaneously places up to 4 totems specified in the Totem Bar" (Wowhead mop-classic tooltip 66842,
    // 2026-10-05); the totem bar was removed in 5.0.4 and no player knows 66842 in 5.4.8 (wave 9: has=0 why=not-known
    // x7-11 per leg). The name now resolves to the 5.x talent 108285 (3 min cooldown reset of short totems, tooltip
    // 108285), which is a different effect, so nothing replaces it. The "call of the elements" trigger/action stay registered.
    // #382: Stoneclaw Totem no longer exists in 5.4.8 (cls.py shaman: NO-SUCH-NAME); Stone Bulwark Totem (108270,
    // Wowhead mop-classic tooltip 108270, 2026-10-04) is the earth-slot absorb-shield totem that replaces it.
    triggers.push_back(
        new TriggerNode("stone bulwark low", NextAction::array(0, new NextAction("stone bulwark totem", 40.0f), nullptr)));
    // Rotation task 3 (#858), MoP defensives at health <= 50 % (low health = config lowHealth): Healing Tide Totem,
    // Ancestral Guidance; Healing Surge on self at the same point (BT-021: at critical health a levelling shaman, who
    // knows only Healing Surge of the three, healed too late to live).
    triggers.push_back(
        new TriggerNode("healing tide low", NextAction::array(0, new NextAction("healing tide totem", 39.0f), nullptr)));
    triggers.push_back(
        new TriggerNode("ancestral guidance low", NextAction::array(0, new NextAction("ancestral guidance", 38.0f), nullptr)));
    triggers.push_back(
        new TriggerNode("low health", NextAction::array(0, new NextAction("healing surge", 37.0f), nullptr)));
    // Rotation task 3 (#858), Earthquake (the top of the spec list: >= 4 enemies
    // within 8 yd of the target). "high aoe" is AC's >= 4 attackers trigger; priority above Earth Shock execute 5.5.
    triggers.push_back(
        new TriggerNode("elemental earthquake aoe", NextAction::array(0, new NextAction("earthquake", 5.8f), nullptr)));
    // #427 (AC parity, AC ElementalShamanStrategy.cpp:57-65).
    triggers.push_back(
        new TriggerNode("earth shock execute", NextAction::array(0, new NextAction("earth shock", 5.5f), nullptr)));

    // triggers.push_back(new TriggerNode("enemy out of spell", NextAction::array(0, new NextAction("reach spell",
    // ACTION_NORMAL + 9), nullptr))); triggers.push_back(new TriggerNode("shaman weapon", NextAction::array(0, new
    // NextAction("flametongue weapon", 23.0f), nullptr)));
    // #382 (AC ElementalShamanStrategy.cpp:21-103 has none of these, rotdiff "MoP-only triggers"): the fork-only
    // edges "enough mana -> chain lightning" (ACTION_DEFAULT+0.1 = 5.1 > Lightning Bolt 5.0, so single-target
    // Lightning Bolt could never be reached), "main hand weapon no imbue" (AC imbues out of combat only, see
    // ShamanNonCombatStrategy.cpp), "frost shock snare", "no fire totem -> searing totem" (AC has no totem edge in
    // the elemental strategy; the "totems" strategy owns the fire slot), "fire elemental totem" (AC casts it from the
    // "boost" strategy at 23, GenericShamanStrategy.cpp ShamanBoostStrategy), "medium mana -> thunderstorm" and
    // "enemy too close for spell -> flee" are removed. AC's chain lightning comes from the "aoe" strategy
    // ("chain lightning no cd" 5.6).

    // #382 (AC ElementalShamanStrategy.cpp:44-52, 57-65): elemental mastery 29, flame shock 5.3 (below earth shock
    // execute 5.5, above Lava Burst 5.2 / Lightning Bolt 5.0).
    triggers.push_back(
        new TriggerNode("flame shock", NextAction::array(0, new NextAction("flame shock", 5.3f), nullptr)));
    triggers.push_back(new TriggerNode("elemental mastery",
                                       NextAction::array(0, new NextAction("elemental mastery", 29.0f), nullptr)));

    // #427 (AC parity, AC ElementalShamanStrategy.cpp:75-91).
    // A solo Restoration bot can also use caster; its water shield must remain intact.
    if (AiFactory::GetPlayerSpecTab(botAI->GetBot()) == Specializations::SPEC_SHAMAN_ELEMENTAL)
        triggers.push_back(new TriggerNode("lightning shield", NextAction::array(0, new NextAction("lightning shield", 19.5f), nullptr)));
    // Elemental sheet: keep Lightning Shield and spend charges at three.
    triggers.push_back(new TriggerNode("lightning shield charges", NextAction::array(0, new NextAction("earth shock", 5.5f), nullptr)));
    triggers.push_back(new TriggerNode("safe thunderstorm", NextAction::array(0, new NextAction("thunderstorm", 19.0f), nullptr)));
    triggers.push_back(new TriggerNode("astral shift low", NextAction::array(0, new NextAction("astral shift", 40.5f), nullptr)));
    triggers.push_back(new TriggerNode("shamanistic rage critical mana", NextAction::array(0, new NextAction("shamanistic rage", 23.0f), nullptr)));

    // BT-677 to BT-681 (PvP class rules, Elemental package; every edge answers only against enemy players): Ancestral Swiftness above
    // the control chain (ACTION_INTERRUPT + 10) so the Hex goes out instant, Grounding Totem (and the reset that brings it back) on an
    // incoming control cast, Spiritwalker's Grace while running with casts to make.
    triggers.push_back(new TriggerNode("pvp swiftness hex", NextAction::array(0, new NextAction("pvp swiftness hex", ACTION_INTERRUPT + 11), nullptr)));
    triggers.push_back(new TriggerNode("pvp grounding", NextAction::array(0, new NextAction("pvp grounding", ACTION_INTERRUPT + 12), nullptr)));
    triggers.push_back(new TriggerNode("pvp call of the elements",
        NextAction::array(0, new NextAction("pvp call of the elements", ACTION_INTERRUPT + 12), nullptr)));
    triggers.push_back(new TriggerNode("pvp spiritwalkers grace",
        NextAction::array(0, new NextAction("pvp spiritwalkers grace", ACTION_HIGH + 2), nullptr)));

}

void CasterAoeShamanStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(
        new TriggerNode("elemental chain aoe", NextAction::array(0, new NextAction("chain lightning", 5.19f), nullptr)));
}
