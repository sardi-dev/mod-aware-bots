/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#include "BloodDKStrategy.h"

#include "Playerbots.h"

class BloodDKStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
public:
    BloodDKStrategyActionNodeFactory()
    {
        // creators["melee"] = &melee;
        // creators["blood strike"] = &blood_strike;
        creators["rune strike"] = &rune_strike;
        creators["heart strike"] = &heart_strike;
        creators["death strike"] = &death_strike;
        // creators["death grip"] = &death_grip;
        // creators["plague strike"] = &plague_strike;
        // creators["pestilence"] = &pestilence;
        creators["icy touch"] = &icy_touch;
        // creators["obliterate"] = &obliterate;
        // creators["blood boil"] = &blood_boil;
        // creators["mark of_blood"] = &mark_of_blood;
        // creators["blood presence"] = &blood_presence;
        // creators["rune tap"] = &rune_tap;
        // creators["vampiric blood"] = &vampiric_blood;
        // creators["death pact"] = &death_pact;
        // creators["death rune_mastery"] = &death_rune_mastery;
        // creators["dancing weapon"] = &dancing_weapon;
        creators["dark command"] = &dark_command;
        creators["taunt spell"] = &dark_command;
    }

private:
    // #601 MoP-only divergence (AC BloodDKStrategy.cpp:29,40,51,65,76 use "frost presence" - WotLK tanks used
    // Frost Presence): in 5.4.8 Blood Presence 48263 is the tank presence (armor/stamina, -10% damage taken,
    // "threat generation is also significantly increased"), Frost Presence 48266 is the Frost dps presence and
    // Unholy Presence 48265 the Unholy dps presence (Wowhead mop-classic tooltips 48263/48266/48265 and Wowpedia
    // "Presence", read 2026-10-05). So every Blood prerequisite below is "blood presence".
    static ActionNode* rune_strike([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("rune strike",
                              /*P*/ NextAction::array(0, new NextAction("blood presence"), nullptr),
                              /*A*/ nullptr,
                              /*C*/ nullptr);
    }
    static ActionNode* icy_touch([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("icy touch",
                              /*P*/ NextAction::array(0, new NextAction("blood presence"), nullptr),
                              /*A*/ nullptr,
                              /*C*/ nullptr);
    }
    static ActionNode* heart_strike([[maybe_unused]] PlayerbotAI* botAI)
    {
        // #601 (AC BloodDKStrategy.cpp:53-55): alternative = blood strike when Heart Strike cannot be cast.
        return new ActionNode("heart strike",
                              /*P*/ NextAction::array(0, new NextAction("blood presence"), nullptr),
                              /*A*/ NextAction::array(0, new NextAction("blood strike"), nullptr),
                              /*C*/ nullptr);
    }

    static ActionNode* death_strike([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("death strike",
                              /*P*/ NextAction::array(0, new NextAction("blood presence"), nullptr),
                              /*A*/ nullptr,
                              /*C*/ nullptr);
    }
    static ActionNode* dark_command([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("dark command",
                              /*P*/ NextAction::array(0, new NextAction("blood presence"), NULL),
                              /*A*/ NextAction::array(0, new NextAction("death grip"), NULL),
                              /*C*/ NULL);
    }
};

BloodDKStrategy::BloodDKStrategy(PlayerbotAI* botAI) : GenericDKStrategy(botAI)
{
    actionNodeFactories.Add(new BloodDKStrategyActionNodeFactory());
}

NextAction** BloodDKStrategy::getDefaultActions()
{
    // #601 (AC BloodDKStrategy.cpp:94-100): AC's defaults and numbers. The fork-only "blood strike" (.5) and
    // "plague strike" (.2) defaults are dropped: blood strike is now heart strike's alternative (AC :53-55) and
    // plague strike has its own "plague strike" trigger edge (HIGH+2) as in AC.
    return NextAction::array(
        // w75: Heart Strike (MoP rotation row 25010, prio 6) sat at .4 under Rune Strike (.6, MoP prio 5) and Icy Touch (.5, not in
        // MoP's Blood list at all), so on every GCD-ready tick one of those two cast first and Heart Strike only ever
        // saw GCD ticks (28 CAST rejects result=70 NOT_READY, 0 casts; the Blood rune sat unused). Order now follows
        // MoP priority: Soul Reaper 7 > Heart Strike 6 > Rune Strike 5, Icy Touch (AC filler) last of the three.
        // Class sheet, blood death knight (BT-134): the Icy Touch default (5.5) is gone. It took the frost rune that Death
        // Strike needs; Icy Touch stays as the Frost Fever edge ("icy touch" trigger below) and as the pull.
        0, new NextAction("rune strike", ACTION_DEFAULT + 0.6f),
        new NextAction("heart strike", ACTION_DEFAULT + 0.65f),
        // rot-task-2 (#857): MoP rotation row 25002 Dancing Rune Weapon is prio 11, the highest Blood rotation entry
        // (AC had it at DEFAULT+0.3); 25009 Soul Reaper (prio 7) sits just above Heart Strike (prio 6).
        new NextAction("dancing rune weapon", ACTION_DEFAULT + 0.9f),
        new NextAction("soul reaper", ACTION_DEFAULT + 0.7f),
        new NextAction("death coil", ACTION_DEFAULT + 0.2f),
        new NextAction("horn of winter", ACTION_DEFAULT + 0.1f), new NextAction("melee", ACTION_DEFAULT), NULL);
}

void BloodDKStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    GenericDKStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode(
        "rune strike", NextAction::array(0, new NextAction("rune strike", ACTION_NORMAL + 3), nullptr)));
    triggers.push_back(
        new TriggerNode("blood tap", NextAction::array(0, new NextAction("blood tap", ACTION_HIGH + 5), nullptr)));
    triggers.push_back(
        new TriggerNode("lose aggro", NextAction::array(0, new NextAction("dark command", ACTION_HIGH + 3), nullptr)));
    // Class sheet, blood death knight (BT-134): Army of the Dead (a 4 second channel) is no longer the 45 percent emergency
    // edge, and no opener casts it in a group: its ghouls scatter and the cast taunts. It stays unused in dungeon groups.
    triggers.push_back(
        new TriggerNode("low health", NextAction::array(0, new NextAction("death strike", ACTION_HIGH + 3), nullptr)));
    triggers.push_back(
        new TriggerNode("critical health", NextAction::array(0, new NextAction("vampiric blood", ACTION_HIGH + 5), nullptr)));
    // Class sheet, blood death knight (BT-134): the tank's defensive cooldowns at 50 (Icebound Fortitude), 60 (Rune Tap) and 60
    // percent (Vampiric Blood), named in DKTriggers.h. Vampiric Blood no longer fires under 90 percent, which kept it on
    // cooldown from the first scratch. The generic strategy leaves its "dk health 40" edge out for this strategy.
    triggers.push_back(new TriggerNode(
        "dk tank icebound fortitude health",
        NextAction::array(0, new NextAction("icebound fortitude", ACTION_HIGH + 5), nullptr)));
    triggers.push_back(new TriggerNode(
        "dk tank vampiric blood health",
        NextAction::array(0, new NextAction("vampiric blood", ACTION_HIGH + 4.5f), nullptr)));
    triggers.push_back(new TriggerNode(
        "dk tank rune tap health", NextAction::array(0, new NextAction("rune tap", ACTION_HIGH + 4), nullptr)));
    // Class sheet, blood death knight (BT-134): Mind Freeze on any casting enemy in melee range, not only the target and
    // an enemy healer ("good" tier; the shared interrupt edges of the generic strategy stay).
    triggers.push_back(new TriggerNode(
        "enemy caster in range",
        NextAction::array(0, new NextAction("mind freeze on pack caster", ACTION_INTERRUPT), nullptr)));
    triggers.push_back(
        new TriggerNode("icy touch", NextAction::array(0, new NextAction("icy touch", ACTION_HIGH + 2), nullptr)));
    triggers.push_back(new TriggerNode(
        "plague strike", NextAction::array(0, new NextAction("plague strike", ACTION_HIGH + 2), nullptr)));
    // #523 (AC BloodDKStrategy.cpp:173-180): spend surplus unholy runes on Death Strike (49998, learnable
    // in 5.4.8 per cls.py dk). AC's "hysteria no cd -> hysteria" (:108-114) is not ported: Hysteria is not
    // learnable in 5.4.8 (Spell.dbc only, no class skill-line / talent / spec row).
    // Class sheet, blood death knight (BT-134): "high unholy rune" fired on the unholy runes alone, with no frost rune to pay
    // the other half. Death Strike goes when a frost and an unholy rune (or Death Runes) are ready, under the disease edges
    // (22) so a missing Frost Fever or Blood Plague still gets the rune first.
    triggers.push_back(new TriggerNode(
        "dk death strike runes", NextAction::array(0, new NextAction("death strike", ACTION_HIGH + 1), nullptr)));
    // #601 (AC UnholyDKStrategy.cpp:165-171 "bone shield" -> HIGH+3): in 5.4.8 Bone Shield 49222 is a Blood-spec
    // spell only (cls.py dk: spec250 L78; Wowhead mop-classic tooltip 49222, read 2026-10-05), Frost/Unholy cannot
    // learn it, so the edge moves from Unholy to Blood at AC's number.
    triggers.push_back(
        new TriggerNode("bone shield", NextAction::array(0, new NextAction("bone shield", ACTION_HIGH + 3), nullptr)));

    // rot-task-2 (#857): MoP Blood priority list.
    // 25003/25004 Outbreak when a disease is missing (prio 10, above Blood Boil / Death Strike).
    triggers.push_back(
        new TriggerNode("dk outbreak", NextAction::array(0, new NextAction("outbreak", ACTION_HIGH + 3), nullptr)));
    // 25005/25006/25007 Blood Boil on a disease with <= 3 s left, or with a Crimson Scourge proc (prio 9).
    triggers.push_back(new TriggerNode(
        "dk blood boil", NextAction::array(0, new NextAction("blood boil", ACTION_DEFAULT + 0.85f), nullptr)));
    // 25016 Vampiric Blood at health <= 90 (group 5 cooldown): replaced by "dk tank vampiric blood health" above (class sheet
    // BT-134).
    // 25020 Raise Dead (in combat, group 5). 5.4.8 Raise Dead is a 1-min ghoul with a 3 min cooldown (the action
    // stamps its own cooldown, DKActions.cpp), so this fires once per fight; the out-of-combat "no pet" loop
    // (#128) stays Unholy-only.
    triggers.push_back(
        new TriggerNode("raise dead", NextAction::array(0, new NextAction("raise dead", ACTION_NORMAL + 1), nullptr)));
}
