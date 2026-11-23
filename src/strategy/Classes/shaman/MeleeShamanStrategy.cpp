/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#include "MeleeShamanStrategy.h"

#include "Playerbots.h"
#include <algorithm>

class MeleeShamanStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
public:
    MeleeShamanStrategyActionNodeFactory()
    {
        creators["stormstrike"] = &stormstrike;
        creators["lava lash"] = &lava_lash;
        creators["magma totem"] = &magma_totem;
    }

private:
    static ActionNode* stormstrike([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("stormstrike",
                              /*P*/ nullptr,
                              // /*A*/ NextAction::array(0, new NextAction("lava lash"), nullptr),
                              nullptr,
                              /*C*/ nullptr);
    }

    static ActionNode* lava_lash([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("lava lash",
                              /*P*/ nullptr,
                              /*A*/ NextAction::array(0, new NextAction("melee"), nullptr),
                              /*C*/ nullptr);
    }

    static ActionNode* magma_totem([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("magma totem",
                              /*P*/ NULL,
                              /*A*/ NextAction::array(0, new NextAction("searing totem"), NULL),
                              /*C*/ NULL);
    }
};

MeleeShamanStrategy::MeleeShamanStrategy(PlayerbotAI* botAI) : GenericShamanStrategy(botAI)
{
    actionNodeFactories.Add(new MeleeShamanStrategyActionNodeFactory());
}

NextAction** MeleeShamanStrategy::getDefaultActions()
{
    // Enhancement sheet: check the spellbook as the bot levels or changes talents.
    Player* bot = botAI->GetBot();
    std::vector<NextAction*> actions;
    if (bot->HasSpell(117014)) actions.push_back(new NextAction("elemental blast", 5.6f));
    if (bot->HasSpell(73680)) actions.push_back(new NextAction("unleash elements", 5.55f));
    if (bot->HasSpell(17364)) actions.push_back(new NextAction("stormstrike", 5.5f));
    else if (bot->HasSpell(73899)) actions.push_back(new NextAction("primal strike", 5.5f));
    if (bot->HasSpell(51533)) actions.push_back(new NextAction("feral spirit", 5.4f));
    if (bot->HasSpell(60103)) actions.push_back(new NextAction("lava lash", 5.2f));
    if (bot->HasSpell(8042)) actions.push_back(new NextAction("earth shock", 5.1f));
    actions.push_back(new NextAction("melee", ACTION_DEFAULT));
    actions.push_back(nullptr);
    NextAction** result = new NextAction*[actions.size()];
    std::copy(actions.begin(), actions.end(), result);
    return result;
}

void MeleeShamanStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    GenericShamanStrategy::InitTriggers(triggers);
    // Enhancement sheet: spend Maelstrom on an emergency solo heal before damage.
    triggers.push_back(new TriggerNode("maelstrom solo heal", NextAction::array(0, new NextAction("healing surge", 41.0f), nullptr)));
    triggers.push_back(new TriggerNode("astral shift low", NextAction::array(0, new NextAction("astral shift", 40.5f), nullptr)));
    triggers.push_back(new TriggerNode("enhancement magma aoe", NextAction::array(0, new NextAction("magma totem", 8.0f), nullptr)));

    // #382 (AC EnhancementShamanStrategy.cpp:54-61 "call of the elements and enemy within melee" 60): DIVERGENCE,
    // edge removed - AC's spell 66842 places the Totem Bar totems, the bar and the spell are gone for players in
    // 5.4.8 (wave 9: has=0 why=not-known x11; Wowhead mop-classic tooltip 66842 / 108285, 2026-10-05; the name now
    // resolves to the unrelated 5.x talent 108285, a cooldown reset). Trigger/action stay registered.
    // #427 (AC parity, AC EnhancementShamanStrategy.cpp:63-71): "spirit walk" edge.
    triggers.push_back(
        new TriggerNode("spirit walk ready", NextAction::array(0, new NextAction("spirit walk", 50.0f), nullptr)));
    // Rotation fix C (#858, wave 73): Elemental Mastery is a class-wide 5.4.8 haste cooldown, the MoP rotation casts it whenever
    // ready (enhancement and restoration, condition "always"); AC has it only in the elemental strategy.
    triggers.push_back(new TriggerNode("elemental mastery",
                                       NextAction::array(0, new NextAction("elemental mastery", 29.0f), nullptr)));

    // #382 (AC EnhancementShamanStrategy.cpp:46-130 has no imbue edges): the fork-only combat copies of
    // "main hand weapon no imbue -> windfury weapon main hand" / "off hand ... -> flametongue weapon off hand" are
    // removed; AC imbues out of combat only and ShamanNonCombatStrategy.cpp keeps both per-hand edges.
    // triggers.push_back(new TriggerNode("searing totem", NextAction::array(0, new NextAction("reach melee", 22.0f),
    // new NextAction("searing totem", 22.0f), nullptr)));
    triggers.push_back(
        new TriggerNode("flame shock", NextAction::array(0, new NextAction("flame shock", 19.0f), nullptr)));  // #382 (AC EnhancementShamanStrategy.cpp:103: 19)
    // #427 (AC parity, AC EnhancementShamanStrategy.cpp:108-114).
    triggers.push_back(
        new TriggerNode("lightning shield", NextAction::array(0, new NextAction("lightning shield", 18.5f), nullptr)));
    // #427 (AC parity, AC EnhancementShamanStrategy.cpp:83-98): the 5-stack overload edge was missing.
    triggers.push_back(
        new TriggerNode("maelstrom weapon 5", NextAction::array(0, new NextAction("lightning bolt", 20.0f), nullptr)));

    // #382: the "medium aoe -> strength of earth totem" edge is removed - Strength of Earth Totem is NOT-LEARNABLE
    // in 5.4.8 (cls.py shaman, removed 5.0.4) and the earth slot has no same-role successor.
    // #382 (AC EnhancementShamanStrategy.cpp:73-79): reach melee 40.
    triggers.push_back(new TriggerNode(
        "enemy out of melee", NextAction::array(0, new NextAction("reach melee", 40.0f), nullptr)));

    // #382: fork-only "no fire totem -> reach melee, magma totem" and "fire elemental totem -> fire elemental totem
    // melee 32" are removed (not in AC's enhancement strategy; AC casts Fire Elemental from the "boost" strategy at 24
    // and drops fire totems via the "totems"-style strategies, here "totems").

    // #382: the "no air totem -> windfury totem" edge is removed - Windfury Totem is NOT-LEARNABLE in 5.4.8
    // (removed 5.0.4; the Windfury effect is the Windfury Weapon imbue above) and there is no standing air buff totem.

    triggers.push_back(
        new TriggerNode("shamanistic rage mana", NextAction::array(0, new NextAction("shamanistic rage", 23.0f), nullptr)));

    triggers.push_back(
        new TriggerNode("shamanistic rage low", NextAction::array(0, new NextAction("shamanistic rage", 23.0f), nullptr)));

    // Rotation task 3 (#858), MoP defensives at health <= 50 % (low health = config lowHealth): Healing Tide Totem,
    // Ancestral Guidance; Healing Surge on self at the same point (BT-021: at critical health a levelling shaman, who
    // knows only Healing Surge of the three, healed too late to live).
    triggers.push_back(
        new TriggerNode("healing tide low", NextAction::array(0, new NextAction("healing tide totem", 39.0f), nullptr)));
    triggers.push_back(
        new TriggerNode("ancestral guidance low", NextAction::array(0, new NextAction("ancestral guidance", 38.0f), nullptr)));
    triggers.push_back(
        new TriggerNode("low health", NextAction::array(0, new NextAction("healing surge", 37.0f), nullptr)));
}

void MeleeAoeShamanStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // triggers.push_back(new TriggerNode("magma totem", NextAction::array(0, new NextAction("magma totem", 26.0f),
    // nullptr)));
    triggers.push_back(
        new TriggerNode("medium aoe", NextAction::array(0, new NextAction("fire nova", 25.0f), nullptr)));
}
