/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#include "ShadowPriestStrategy.h"

#include "Multiplier.h"
#include "Playerbots.h"
#include "PvpShadowPriest.h"
#include "ShadowPriestStrategyActionNodeFactory.h"

ShadowPriestStrategy::ShadowPriestStrategy(PlayerbotAI* botAI) : GenericPriestStrategy(botAI)
{
    actionNodeFactories.Add(new ShadowPriestStrategyActionNodeFactory());
}

NextAction** ShadowPriestStrategy::getDefaultActions()
{
    // Shadow priest sheet: gate spells by what this bot knows and put Death before the filler.
    Player* bot = botAI->GetBot();
    if (bot->HasSpell(121135))
        return NextAction::array(0, new NextAction("cascade", ACTION_DEFAULT + 0.4f),
            new NextAction("mind blast", ACTION_DEFAULT + 0.3f), new NextAction("shadow word: death", ACTION_DEFAULT + 0.25f),
            new NextAction("mind flay", ACTION_DEFAULT + 0.2f), new NextAction("shoot", ACTION_DEFAULT), nullptr);
    if (bot->HasSpell(32379))
        return NextAction::array(0, new NextAction("mind blast", ACTION_DEFAULT + 0.3f),
            new NextAction("shadow word: death", ACTION_DEFAULT + 0.25f), new NextAction("mind flay", ACTION_DEFAULT + 0.2f),
            new NextAction("shoot", ACTION_DEFAULT), nullptr);
    if (bot->HasSpell(8092))
        return NextAction::array(0, new NextAction("mind blast", ACTION_DEFAULT + 0.3f),
            new NextAction("mind flay", ACTION_DEFAULT + 0.2f), new NextAction("shoot", ACTION_DEFAULT), nullptr);
    return NextAction::array(0, new NextAction("smite", ACTION_DEFAULT + 0.2f), new NextAction("shoot", ACTION_DEFAULT), nullptr);
}

namespace
{
// BT-688: against players Vampiric Embrace goes with the burst or a hurt team, not on cooldown (PvP/PvpShadowPriest.cpp). PvE and
// casual bots keep the older edge.
class PvpShadowMultiplier : public Multiplier
{
public:
    PvpShadowMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "pvp shadow") {}

    float GetValue(Action* action) override { return PvpShadowPriestGlue::Holds(botAI, bot, action) ? 0.0f : 1.0f; }
};
}  // namespace

void ShadowPriestStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    GenericPriestStrategy::InitMultipliers(multipliers);
    multipliers.push_back(new PvpShadowMultiplier(botAI));
}

void ShadowPriestStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    GenericPriestStrategy::InitTriggers(triggers);

    // BT-688 to BT-690 (PvP class rules, Shadow package; every edge answers only in a fight against players): Leap of Faith on a
    // stunned or trapped ally, and Mass Dispel (with its glyph) on a magic immunity of the target.
    triggers.push_back(new TriggerNode("pvp leap of faith", NextAction::array(0, new NextAction("pvp leap of faith", ACTION_EMERGENCY - 1), nullptr)));
    triggers.push_back(new TriggerNode("pvp mass dispel", NextAction::array(0, new NextAction("pvp mass dispel", ACTION_HIGH + 4), nullptr)));

    // triggers.push_back(new TriggerNode("enemy out of spell", NextAction::array(0, new NextAction("reach spell",
    // ACTION_MOVE + 9), nullptr)));
    triggers.push_back(
        new TriggerNode("shadowform", NextAction::array(0, new NextAction("shadowform", ACTION_HIGH), nullptr)));
    // Shadow sheet line 14 (expert): end a Mind Flay after two ticks for a ready Mind Blast (the default list casts it
    // next, above Mind Flay).
    triggers.push_back(
        new TriggerNode("shadow mind flay clip", NextAction::array(0, new NextAction("cancel channel", ACTION_HIGH + 5), nullptr)));
    if (botAI->GetBot()->HasSpell(47585))
    {
    triggers.push_back(new TriggerNode("low mana", NextAction::array(0, new NextAction("dispersion", ACTION_HIGH
        + 5), nullptr)));
    triggers.push_back(new TriggerNode("critical health", NextAction::array(0, new NextAction("dispersion", ACTION_HIGH
        + 5), nullptr)));
    }
    // #524 documented divergence: AC has Vampiric Embrace as a non-combat buff (PriestNonCombatStrategy); in 5.x it is a
    // 3 min cooldown (Wowhead mop-classic spell 15286, 2026-10-04), so MoP keeps it in combat here.
    triggers.push_back(
        new TriggerNode("vampiric embrace", NextAction::array(0, new NextAction("vampiric embrace", 16.0f), nullptr)));
    // BT-164: keep the group edge available when the spell is learned after strategy setup.
    triggers.push_back(new TriggerNode("group pve enemy caster",
        NextAction::array(0, new NextAction("silence on enemy caster", ACTION_INTERRUPT + 0.5f), nullptr)));
    if (botAI->GetBot()->HasSpell(15487))
    {
    triggers.push_back(
        new TriggerNode("silence", NextAction::array(0, new NextAction("silence", ACTION_INTERRUPT + 1), nullptr)));
    triggers.push_back(
        new TriggerNode("silence on enemy healer",
                        NextAction::array(0, new NextAction("silence on enemy healer", ACTION_INTERRUPT), nullptr)));
    // Task 8 / #863 (MoP rotation row 14/15, shadow, group 10/2-10/1): Silence any casting enemy (pvp maps).
    triggers.push_back(new TriggerNode(
        "enemy caster in range",
        NextAction::array(0, new NextAction("silence on enemy caster", ACTION_INTERRUPT + 0.5f), nullptr)));
    }
    // Task 8 / #863 (MoP rotation row 25802): Void Tendrils on an enemy player within melee range (pvp maps,
    // not already CC'd - guarded in the action). 25803 Cascade is in the default actions.
    triggers.push_back(new TriggerNode(
        "enemy player in melee range", NextAction::array(0, new NextAction("void tendrils", ACTION_HIGH + 3), nullptr)));
    // Task E (#863, MoP rotation row 25802 prio 0/10, no pvp condition): Void Tendrils on a creature that attacks the bot as well.
    triggers.push_back(new TriggerNode(
        "void tendrils target close", NextAction::array(0, new NextAction("void tendrils", ACTION_HIGH + 3), nullptr)));
    // Shadow priest sheet: preserve the self shield and use Renew below 45 percent.
    triggers.push_back(new TriggerNode(
        "priest self health 60", NextAction::array(0, new NextAction("power word: shield", ACTION_HIGH), nullptr)));
    if (botAI->GetBot()->HasSpell(15286))
    triggers.push_back(new TriggerNode(
        "priest self health 40", NextAction::array(0, new NextAction("vampiric embrace", ACTION_HIGH), nullptr)));
    if (botAI->GetBot()->HasSpell(139))
    triggers.push_back(new TriggerNode(
        "priest self health 45", NextAction::array(0, new NextAction("renew", ACTION_HIGH), nullptr)));
    triggers.push_back(new TriggerNode("shadow solo flash heal", NextAction::array(0, new NextAction("flash heal", ACTION_HIGH), nullptr)));
    // triggers.push_back(new TriggerNode("shadowfiend", NextAction::array(0, new NextAction("shadowfiend",
    // ACTION_HIGH), nullptr))); triggers.push_back(new TriggerNode("medium mana", NextAction::array(0, new
    // NextAction("shadowfiend", ACTION_HIGH), nullptr))); triggers.push_back(new TriggerNode("low mana",
    // NextAction::array(0, new NextAction("mana burn", ACTION_HIGH), nullptr)));
}

void ShadowPriestAoeStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode(
        "shadow word: pain on attacker",
        NextAction::array(0, new NextAction("shadow word: pain on attacker", ACTION_NORMAL + 5), nullptr)));
    triggers.push_back(new TriggerNode(
        "vampiric touch on attacker",
        NextAction::array(0, new NextAction("vampiric touch on attacker", ACTION_NORMAL + 4), nullptr)));
    // #524 (AC ShadowPriestStrategy.cpp:68 ShadowPriestAoeStrategy, trigger "mind sear channel check"): cancel a
    // running Mind Sear channel when too few enemies remain (CancelChannelAction, #423).
    triggers.push_back(new TriggerNode(
        "mind sear channel check", NextAction::array(0, new NextAction("cancel channel", ACTION_HIGH + 5), nullptr)));
    triggers.push_back(
        new TriggerNode("shadow mind sear", NextAction::array(0, new NextAction("mind sear", ACTION_NORMAL + 3), nullptr)));
}

void ShadowPriestDebuffStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // #524 (AC ShadowPriestStrategy.cpp:110-132): AC order is vampiric touch HIGH+3, devouring plague HIGH+2,
    // shadow word: pain HIGH+1 (the fork had plague/touch swapped).
    triggers.push_back(new TriggerNode(
        "vampiric touch", NextAction::array(0, new NextAction("vampiric touch", ACTION_HIGH + 3), nullptr)));
    // #735: 5.4.8 Devouring Plague spends 3 Shadow Orbs - fired from "shadow orbs 3 available" (the plain debuff
    // action still skips a target that has our plague), as AC fires Rupture from "combo points 5 available" (AC
    // CombatRogueStrategy.cpp:118-126). The "devouring plague" debuff trigger stays registered (AC surface) but unwired:
    // at 0 orbs it fired a cast that failed SPELL_FAILED_NO_POWER on every try.
    triggers.push_back(new TriggerNode(
        "shadow orbs 3 available", NextAction::array(0, new NextAction("devouring plague", ACTION_HIGH + 2), nullptr)));
    triggers.push_back(new TriggerNode(
        "shadow word: pain", NextAction::array(0, new NextAction("shadow word: pain", ACTION_HIGH + 1), nullptr)));
    // triggers.push_back(new TriggerNode("feedback", NextAction::array(0, new NextAction("feedback", 80.0f),
    // nullptr))); triggers.push_back(new TriggerNode("hex of weakness", NextAction::array(0, new NextAction("hex of
    // weakness", 10.0f), nullptr)));
}
