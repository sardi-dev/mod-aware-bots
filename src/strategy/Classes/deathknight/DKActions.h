/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#ifndef _PLAYERBOT_DKACTIONS_H
#define _PLAYERBOT_DKACTIONS_H

#include "Event.h"
#include "GenericSpellActions.h"

class PlayerbotAI;

class CastBloodPresenceAction : public CastBuffSpellAction
{
public:
    CastBloodPresenceAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "blood presence") {}
};

class CastFrostPresenceAction : public CastBuffSpellAction
{
public:
    CastFrostPresenceAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "frost presence") {}
};

class CastUnholyPresenceAction : public CastBuffSpellAction
{
public:
    CastUnholyPresenceAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "unholy presence") {}
};

class CastDarkCommandAction : public CastSpellAction
{
public:
    CastDarkCommandAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "dark command") {}
    // #835: never taunt a player (BG lose-aggro edge can see the old target, then dps assist switches to a player).
    bool isUseful() override { Unit* t = GetTarget(); return t && !(t->GetTypeId() == TypeID::TYPEID_PLAYER) && CastSpellAction::isUseful(); }
};

BEGIN_RANGED_SPELL_ACTION(CastDeathGripAction, "death grip")
END_SPELL_ACTION()

// Unholy presence
class CastUnholyMeleeSpellAction : public CastMeleeSpellAction
{
public:
    CastUnholyMeleeSpellAction(PlayerbotAI* botAI, std::string const spell) : CastMeleeSpellAction(botAI, spell) {}

    NextAction** getPrerequisites() override;
};

// Frost presence
class CastFrostMeleeSpellAction : public CastMeleeSpellAction
{
public:
    CastFrostMeleeSpellAction(PlayerbotAI* botAI, std::string const spell) : CastMeleeSpellAction(botAI, spell) {}

    NextAction** getPrerequisites() override;
};

// Blood presence
class CastBloodMeleeSpellAction : public CastMeleeSpellAction
{
public:
    CastBloodMeleeSpellAction(PlayerbotAI* botAI, std::string const spell) : CastMeleeSpellAction(botAI, spell) {}

    NextAction** getPrerequisites() override;
};

class CastRuneStrikeAction : public CastMeleeSpellAction
{
public:
    CastRuneStrikeAction(PlayerbotAI* botAI) : CastMeleeSpellAction(botAI, "rune strike") {}
};

// debuff
//  BEGIN_DEBUFF_ACTION(CastPestilenceAction, "pestilence")
//  END_SPELL_ACTION()

class CastPestilenceAction : public CastSpellAction
{
public:
    CastPestilenceAction(PlayerbotAI* ai) : CastSpellAction(ai, "pestilence") {}
    ActionThreatType getThreatType() override { return ActionThreatType::None; }
    // Pestilence only spreads the diseases already on the target, so it needs one of the bot's own.
    bool isUseful() override;
};

// debuff
//  BEGIN_DEBUFF_ACTION(CastHowlingBlastAction, "howling blast")
//  END_SPELL_ACTION()

class CastHowlingBlastAction : public CastSpellAction
{
public:
    CastHowlingBlastAction(PlayerbotAI* ai) : CastSpellAction(ai, "howling blast") {}
};

// debuff it
//  BEGIN_DEBUFF_ACTION(CastIcyTouchAction, "icy touch")
//  END_SPELL_ACTION()

class CastIcyTouchAction : public CastSpellAction
{
public:
    CastIcyTouchAction(PlayerbotAI* ai) : CastSpellAction(ai, "icy touch") {}
};

class CastIcyTouchOnAttackerAction : public CastDebuffSpellOnAttackerAction
{
public:
    CastIcyTouchOnAttackerAction(PlayerbotAI* botAI)
        : CastDebuffSpellOnAttackerAction(botAI, "icy touch", true, .0f)
    {
    }
};

// debuff ps

class CastPlagueStrikeAction : public CastSpellAction
{
public:
    CastPlagueStrikeAction(PlayerbotAI* ai) : CastSpellAction(ai, "plague strike") {}
};
// BEGIN_DEBUFF_ACTION(CastPlagueStrikeAction, "plague strike")
// END_SPELL_ACTION()

class CastPlagueStrikeOnAttackerAction : public CastDebuffSpellOnMeleeAttackerAction
{
public:
    CastPlagueStrikeOnAttackerAction(PlayerbotAI* botAI)
        : CastDebuffSpellOnMeleeAttackerAction(botAI, "plague strike", true, .0f)
    {
    }
};

// rot-task-2 (#857): Mark of Blood is NOT-LEARNABLE on 5.4.8 (forkonly_verdicts.txt) and had no other reference, removed.

class CastUnholyBlightAction : public CastBuffSpellAction
{
public:
    CastUnholyBlightAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "unholy blight") {}
};

class CastSummonGargoyleAction : public CastSpellAction
{
public:
    CastSummonGargoyleAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "summon gargoyle") {}
};

class CastGhoulFrenzyAction : public CastBuffSpellAction
{
public:
    // #601: Ghoul Frenzy was removed in 4.0.1 (Wowpedia "Ghoul Frenzy"); its spell id 63560 is Dark Transformation in
    // 5.4.8 (cls.py dk: YES 63560/spec252/L70; Wowhead mop-classic tooltip 63560, read 2026-10-05). The action key
    // stays "ghoul frenzy" (AC parity), the spell name is the 5.x one.
    CastGhoulFrenzyAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "dark transformation", false, 5000) {}
    std::string const GetTargetName() override { return "pet target"; }
};

BEGIN_MELEE_SPELL_ACTION(CastCorpseExplosionAction, "corpse explosion")
END_SPELL_ACTION()

// rot-task-2 (#857): the 5.4.8 spell name is "Anti-Magic Shell" (hyphen); SpellIdValue matches names exactly, so the
// old "anti magic shell" string never resolved (silent no-op). Self buff, MoP rotation row 25014/25117/25218.
class CastAntiMagicShellAction : public CastBuffSpellAction
{
public:
    CastAntiMagicShellAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "anti-magic shell") {}
};

// BT-146: SpellIdValue needs the hyphen in the 5.4.8 spell name.
BEGIN_MELEE_SPELL_ACTION(CastAntiMagicZoneAction, "anti-magic zone")
END_SPELL_ACTION()

class CastChainsOfIceAction : public CastSpellAction
{
public:
    CastChainsOfIceAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "chains of ice") {}
};

class CastHeartStrikeAction : public CastMeleeSpellAction
{
public:
    CastHeartStrikeAction(PlayerbotAI* botAI) : CastMeleeSpellAction(botAI, "heart strike") {}
};

class CastBloodStrikeAction : public CastMeleeSpellAction
{
public:
    CastBloodStrikeAction(PlayerbotAI* botAI) : CastMeleeSpellAction(botAI, "blood strike") {}
};

class CastFrostStrikeAction : public CastMeleeSpellAction
{
public:
    CastFrostStrikeAction(PlayerbotAI* botAI) : CastMeleeSpellAction(botAI, "frost strike") {}
};

class CastObliterateAction : public CastMeleeSpellAction
{
public:
    CastObliterateAction(PlayerbotAI* botAI) : CastMeleeSpellAction(botAI, "obliterate") {}
};

class CastDeathStrikeAction : public CastMeleeSpellAction
{
public:
    CastDeathStrikeAction(PlayerbotAI* botAI) : CastMeleeSpellAction(botAI, "death strike") {}
};

class CastScourgeStrikeAction : public CastMeleeSpellAction
{
public:
    CastScourgeStrikeAction(PlayerbotAI* botAI) : CastMeleeSpellAction(botAI, "scourge strike") {}
};

class CastDeathCoilAction : public CastSpellAction
{
public:
    CastDeathCoilAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "death coil") {}
};

class CastBloodBoilAction : public CastMeleeSpellAction
{
public:
    CastBloodBoilAction(PlayerbotAI* botAI) : CastMeleeSpellAction(botAI, "blood boil") {}
};

class CastDeathAndDecayAction : public CastSpellAction
{
public:
    CastDeathAndDecayAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "death and decay") {}
    // ActionThreatType getThreatType() override { return ActionThreatType::Aoe; }
};

class CastHornOfWinterAction : public CastSpellAction
{
public:
    CastHornOfWinterAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "horn of winter") {}
    // BT-074: a self buff; with the enemy as target it could not be kept up out of combat
    std::string const GetTargetName() override { return "self target"; }
};

class CastBoneShieldAction : public CastBuffSpellAction
{
public:
    CastBoneShieldAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "bone shield") {}
};

class CastDeathPactAction : public CastBuffSpellAction
{
public:
    CastDeathPactAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "death pact") {}
};

class CastDancingRuneWeaponAction : public CastSpellAction
{
public:
    CastDancingRuneWeaponAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "dancing rune weapon") {}
};

class CastEmpowerRuneWeaponAction : public CastBuffSpellAction
{
public:
    CastEmpowerRuneWeaponAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "empower rune weapon") {}
};

class CastArmyOfTheDeadAction : public CastBuffSpellAction
{
public:
    CastArmyOfTheDeadAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "army of the dead") {}
};

class CastRaiseDeadAction : public CastBuffSpellAction
{
public:
    CastRaiseDeadAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "raise dead") {}
    virtual bool Execute(Event event) override;
};

class CastKillingMachineAction : public CastBuffSpellAction
{
public:
    CastKillingMachineAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "killing machine") {}
};

class CastIceboundFortitudeAction : public CastBuffSpellAction
{
public:
    CastIceboundFortitudeAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "icebound fortitude") {}
};

// #601: Pillar of Frost 51271, the 5.x successor of Unbreakable Armor (Wowhead mop-classic tooltip, 2026-10-05).
class CastPillarOfFrostAction : public CastBuffSpellAction
{
public:
    CastPillarOfFrostAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "pillar of frost") {}
};

class CastVampiricBloodAction : public CastBuffSpellAction
{
public:
    CastVampiricBloodAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "vampiric blood") {}
};

class CastMindFreezeAction : public CastMeleeSpellAction
{
public:
    CastMindFreezeAction(PlayerbotAI* botAI) : CastMeleeSpellAction(botAI, "mind freeze") {}
};

// rot-task-2 (#857): Strangulate 47476 has a 30 yd range (MoP rotation row 10/11), so it is a plain
// CastSpellAction (range check by the spell), not a melee-range action.
class CastStrangulateAction : public CastSpellAction
{
public:
    CastStrangulateAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "strangulate") {}
};

// rot-task-2 (#857): MoP rotation row 9 / 11 (prio group 10, "each valid enemy around") - interrupt ANY
// casting enemy in range, not only the current target / enemy healer. Targets the lane-0 value "enemy caster target".
class CastMindFreezeOnEnemyCasterAction : public CastSpellOnEnemyCasterAction
{
public:
    CastMindFreezeOnEnemyCasterAction(PlayerbotAI* botAI) : CastSpellOnEnemyCasterAction(botAI, "mind freeze") {}
};

class CastStrangulateOnEnemyCasterAction : public CastSpellOnEnemyCasterAction
{
public:
    CastStrangulateOnEnemyCasterAction(PlayerbotAI* botAI) : CastSpellOnEnemyCasterAction(botAI, "strangulate") {}
};

// Class sheet, blood death knight (BT-134, docs/class-sheets/deathknight-blood.md): Mind Freeze on any enemy caster in melee
// range, dungeons included (the action above is for PvP maps only). Modelled on the protection warrior's
// CastPummelOnPackCasterAction. A "good" line of the sheet; isUseful is in DKActions.cpp.
class CastMindFreezeOnPackCasterAction : public CastSpellOnEnemyCasterAction
{
public:
    CastMindFreezeOnPackCasterAction(PlayerbotAI* botAI) : CastSpellOnEnemyCasterAction(botAI, "mind freeze") {}

    std::string const getName() override { return "mind freeze on pack caster"; }
    bool isUseful() override;
};

// Class sheet, blood death knight (BT-134): Raise Ally (level 72, 30 Runic Power, 40 yd, 600 s), the death knight's combat
// resurrection. Modelled on the druid's Rebirth (DruidActions.h); the target is the "party member to resurrect" value, which
// lists the leader, then healers, then tanks, then the rest.
class CastRaiseAllyAction : public ResurrectPartyMemberAction
{
public:
    CastRaiseAllyAction(PlayerbotAI* botAI) : ResurrectPartyMemberAction(botAI, "raise ally") {}

    bool isUseful() override;
};

// rot-task-2 (#857): MoP-only rotation spells from the MoP 5.4.8 data (AC WotLK has none of them).
// Festering Strike 85948 (Unholy, MoP rotation row 25207/25208/25212).
class CastFesteringStrikeAction : public CastMeleeSpellAction
{
public:
    CastFesteringStrikeAction(PlayerbotAI* botAI) : CastMeleeSpellAction(botAI, "festering strike") {}
};

// Outbreak 77575 (all specs, 30 yd, applies Blood Plague + Frost Fever; MoP rotation row 25203/25204/25003/25004).
class CastOutbreakAction : public CastSpellAction
{
public:
    CastOutbreakAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "outbreak") {}
};

// Soul Reaper (spec ids 114866 Blood / 130735 Frost / 130736 Unholy; the name resolves to the one the bot knows;
// MoP rotation row 25009/25105/25213).
class CastSoulReaperAction : public CastMeleeSpellAction
{
public:
    CastSoulReaperAction(PlayerbotAI* botAI) : CastMeleeSpellAction(botAI, "soul reaper") {}
};

// Unholy Frenzy 49016 (Unholy, MoP rotation row 25219, cast on self).
class CastUnholyFrenzyAction : public CastBuffSpellAction
{
public:
    CastUnholyFrenzyAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "unholy frenzy") {}
};

class CastMindFreezeOnEnemyHealerAction : public CastSpellOnEnemyHealerAction
{
public:
    CastMindFreezeOnEnemyHealerAction(PlayerbotAI* botAI) : CastSpellOnEnemyHealerAction(botAI, "mind freeze") {}
};

// BT-075: Rune Tap heals the death knight; as a melee action it needed an enemy in melee range
class CastRuneTapAction : public CastSpellAction
{
public:
    CastRuneTapAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "rune tap") {}
    std::string const GetTargetName() override { return "self target"; }
};

class CastBloodTapAction : public CastSpellAction
{
public:
    CastBloodTapAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "blood tap") {}
    // A self spell: it must not wait for the current target to be in melee range.
    std::string const GetTargetName() override { return "self target"; }
};

#endif
