/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#ifndef _PLAYERBOT_HUNTERACTIONS_H
#define _PLAYERBOT_HUNTERACTIONS_H

#include "AiObject.h"
#include "GenericSpellActions.h"

class PlayerbotAI;
class Unit;

// BEGIN_RANGED_SPELL_ACTION(CastHuntersMarkAction, "hunter's mark")
// END_SPELL_ACTION()

class CastHuntersMarkAction : public CastDebuffSpellAction
{
public:
    CastHuntersMarkAction(PlayerbotAI* botAI) : CastDebuffSpellAction(botAI, "hunter's mark") {}
    bool isUseful() override;
};
class CastAutoShotAction : public CastSpellAction
{
public:
    CastAutoShotAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "auto shot") {}
    ActionThreatType getThreatType() override { return ActionThreatType::None; }
    bool isUseful() override;
};

class CastArcaneShotAction : public CastSpellAction
{
public:
    CastArcaneShotAction(PlayerbotAI* ai) : CastSpellAction(ai, "arcane shot") {}
    bool isUseful() override;
};

class CastExplosiveShotAction : public CastDebuffSpellAction
{
public:
    CastExplosiveShotAction(PlayerbotAI* botAI) : CastDebuffSpellAction(botAI, "explosive shot", true, 0.0f) {}
};

// MoP has one learnable Survival specialization spell (53301), rather than the four WotLK ranks.
// Keep AC's four action names for its rank fallback chain, but have each cast/check that spell.
class CastExplosiveShotRank1Action : public CastDebuffSpellAction
{
public:
    CastExplosiveShotRank1Action(PlayerbotAI* botAI) : CastDebuffSpellAction(botAI, "explosive shot", true, 0.0f) {}
    bool Execute(Event /*event*/) override;
    bool isUseful() override;
};
class CastExplosiveShotRank2Action : public CastDebuffSpellAction
{
public:
    CastExplosiveShotRank2Action(PlayerbotAI* botAI) : CastDebuffSpellAction(botAI, "explosive shot", true, 0.0f) {}
    bool Execute(Event /*event*/) override;
    bool isUseful() override;
};
class CastExplosiveShotRank3Action : public CastDebuffSpellAction
{
public:
    CastExplosiveShotRank3Action(PlayerbotAI* botAI) : CastDebuffSpellAction(botAI, "explosive shot", true, 0.0f) {}
    bool Execute(Event /*event*/) override;
    bool isUseful() override;
};
class CastExplosiveShotRank4Action : public CastDebuffSpellAction
{
public:
    CastExplosiveShotRank4Action(PlayerbotAI* botAI) : CastDebuffSpellAction(botAI, "explosive shot", true, 0.0f) {}
    bool Execute(Event /*event*/) override;
    bool isUseful() override;
};

// Intimidation (AC: CastIntimidationAction, HunterActions.h:155)
class CastIntimidationAction : public CastBuffSpellAction
{
public:
    CastIntimidationAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "intimidation", false, 5000) {}
    std::string const GetTargetName() override { return "pet target"; }
};

class CastAimedShotAction : public CastSpellAction
{
public:
    CastAimedShotAction(PlayerbotAI* ai) : CastSpellAction(ai, "aimed shot") {}
    bool isUseful() override;
};

BEGIN_RANGED_SPELL_ACTION(CastChimeraShotAction, "chimera shot")
END_SPELL_ACTION()

class CastConcussiveShotAction : public CastSnareSpellAction
{
public:
    CastConcussiveShotAction(PlayerbotAI* botAI) : CastSnareSpellAction(botAI, "concussive shot") {}
};

BEGIN_RANGED_SPELL_ACTION(CastDistractingShotAction, "distracting shot")
END_SPELL_ACTION()

class CastMultiShotAction : public CastSpellAction
{
public:
    CastMultiShotAction(PlayerbotAI* ai) : CastSpellAction(ai, "multi-shot") {}
    bool isUseful() override;
};

DEBUFF_CHECKISOWNER_ACTION(CastSerpentStingAction, "serpent sting");

BEGIN_RANGED_SPELL_ACTION(CastWyvernStingAction, "wyvern sting")
END_SPELL_ACTION()

BEGIN_RANGED_SPELL_ACTION(CastSteadyShotAction, "steady shot")
END_SPELL_ACTION()

BEGIN_RANGED_SPELL_ACTION(CastKillShotAction, "kill shot")
END_SPELL_ACTION()

BEGIN_RANGED_SPELL_ACTION(CastSilencingShotAction, "silencing shot")
END_SPELL_ACTION()

BEGIN_RANGED_SPELL_ACTION(CastTranquilizingShotAction, "tranquilizing shot")
END_SPELL_ACTION()

// Hunter sheets: these talent spells are self-targeted and unavailable until learned.
class CastIronHawkAction : public CastBuffSpellAction
{
public:
    CastIronHawkAction(PlayerbotAI* ai) : CastBuffSpellAction(ai, "aspect of the iron hawk") {}
};
class CastFervorAction : public CastSpellAction
{
public:
    CastFervorAction(PlayerbotAI* ai) : CastSpellAction(ai, "fervor") {}
    std::string const GetTargetName() override { return "self target"; }
};
class CastExhilarationAction : public CastSpellAction
{
public:
    CastExhilarationAction(PlayerbotAI* ai) : CastSpellAction(ai, "exhilaration") {}
    std::string const GetTargetName() override { return "self target"; }
};
class CastFocusFireAction : public CastSpellAction
{
public:
    CastFocusFireAction(PlayerbotAI* ai) : CastSpellAction(ai, "focus fire") {}
    std::string const GetTargetName() override { return "self target"; }
};

class HunterRabidAction : public Action
{
public:
    HunterRabidAction(PlayerbotAI* ai) : Action(ai, "hunter rabid") {}
    bool isUseful() override;
    bool Execute(Event event) override;
};

class HunterCancelTravelAspectAction : public Action
{
public:
    HunterCancelTravelAspectAction(PlayerbotAI* ai) : Action(ai, "cancel hunter travel aspect") {}
    bool Execute(Event event) override;
};

class CastAspectOfTheHawkAction : public CastBuffSpellAction
{
public:
    CastAspectOfTheHawkAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "aspect of the hawk") {}
    bool isUseful() override;
};

class CastAspectOfTheCheetahAction : public CastBuffSpellAction
{
public:
    CastAspectOfTheCheetahAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "aspect of the cheetah") {}

    bool isUseful() override;
};

class CastAspectOfThePackAction : public CastBuffSpellAction
{
public:
    CastAspectOfThePackAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "aspect of the pack") {}
};

class CastCallPetAction : public CastBuffSpellAction
{
public:
    CastCallPetAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "call pet 1") {}
};

class CastMendPetAction : public CastAuraSpellAction
{
public:
    CastMendPetAction(PlayerbotAI* botAI) : CastAuraSpellAction(botAI, "mend pet") {}

    std::string const GetTargetName() override { return "pet target"; }
};

class CastKillCommandAction : public CastAuraSpellAction
{
public:
    CastKillCommandAction(PlayerbotAI* botAI) : CastAuraSpellAction(botAI, "kill command") {}

    bool isUseful() override;

    // Round 3 (#380): 5.4.8 Kill Command 34026 targets the enemy (see HunterActions.cpp), AC/WotLK targeted the pet.
    std::string const GetTargetName() override { return "current target"; }
};

class CastRevivePetAction : public CastBuffSpellAction
{
public:
    CastRevivePetAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "revive pet") {}
};

class CastTrueshotAuraAction : public CastBuffSpellAction
{
public:
    CastTrueshotAuraAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "trueshot aura") {}
};

class CastFeignDeathAction : public CastBuffSpellAction
{
public:
    CastFeignDeathAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "feign death") {}
};

class CastRapidFireAction : public CastBuffSpellAction
{
public:
    CastRapidFireAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "rapid fire") {}
};

class CastDeterrenceAction : public CastBuffSpellAction
{
public:
    CastDeterrenceAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "deterrence") {}
};

class CastBlackArrow : public CastDebuffSpellAction
{
public:
    CastBlackArrow(PlayerbotAI* botAI) : CastDebuffSpellAction(botAI, "black arrow", true) {}
};

class CastFreezingTrap : public CastDebuffSpellAction
{
public:
    CastFreezingTrap(PlayerbotAI* botAI) : CastDebuffSpellAction(botAI, "freezing trap") {}

    Value<Unit*>* GetTargetValue() override;
};

// #521 (AC HunterActions.h:100-105): Disengage 781 (L14, cls.py hunter 2026-10-04).
class CastDisengageAction : public CastSpellAction
{
public:
    CastDisengageAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "disengage") {}
    bool Execute(Event event) override;
};

// Hunter sheets (traps): the Trap Launcher toggle; a second cast would turn it off, which the aura check prevents.
class CastTrapLauncherAction : public CastBuffSpellAction
{
public:
    CastTrapLauncherAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "trap launcher") {}
};

// #521 (AC HunterActions.h:362-367): Explosive Trap 13813 (L38, cls.py hunter 2026-10-04).
class CastExplosiveTrapAction : public CastSpellAction
{
public:
    CastExplosiveTrapAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "explosive trap") {}
};

class CastSerpentStingOnAttackerAction : public CastDebuffSpellOnAttackerAction
{
public:
    CastSerpentStingOnAttackerAction(PlayerbotAI* botAI) : CastDebuffSpellOnAttackerAction(botAI, "serpent sting", true)
    {
    }
};

class CastBestialWrathAction : public CastBuffSpellAction
{
public:
    CastBestialWrathAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "bestial wrath") {}
    std::string const GetTargetName() override { return "pet target"; }
};

class CastScareBeastAction : public CastSpellAction
{
public:
    CastScareBeastAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "scare beast") {}
};

class CastScareBeastCcAction : public CastSpellAction
{
public:
    // BT-146: Keep the action key while checking the actual spell name.
    CastScareBeastCcAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "scare beast on cc") { spell = "scare beast"; }

    Value<Unit*>* GetTargetValue() override;
    bool Execute(Event event) override;
};

class CastMisdirectionOnMainTankAction : public BuffOnMainTankAction
{
public:
    CastMisdirectionOnMainTankAction(PlayerbotAI* ai) : BuffOnMainTankAction(ai, "misdirection", true) {}
};

// ===== #856 rotation task 1 (epic #854): MoP hunter kit, spell ids verified in the 5.4.8 Spell.dbc =====
// Cobra Shot 77767 (BM/SV filler, prio 0/3 and 0/1)
BEGIN_RANGED_SPELL_ACTION(CastCobraShotAction, "cobra shot")
END_SPELL_ACTION()

// Glaive Toss 117050
BEGIN_RANGED_SPELL_ACTION(CastGlaiveTossAction, "glaive toss")
END_SPELL_ACTION()

// Binding Shot 109248 (target 15-30 yd, see HunterBindingShotRangeTrigger)
BEGIN_RANGED_SPELL_ACTION(CastBindingShotAction, "binding shot")
END_SPELL_ACTION()

// Dire Beast 120679
BEGIN_RANGED_SPELL_ACTION(CastDireBeastAction, "dire beast")
END_SPELL_ACTION()

// Stampede 121818
BEGIN_RANGED_SPELL_ACTION(CastStampedeAction, "stampede")
END_SPELL_ACTION()

// A Murder of Crows 131894 (a DoT on the target; own-aura check stops a recast)
class CastMurderOfCrowsAction : public CastDebuffSpellAction
{
public:
    CastMurderOfCrowsAction(PlayerbotAI* botAI) : CastDebuffSpellAction(botAI, "a murder of crows", true) {}

    // Rotation fix task A (#856): pools focus for the 60-focus cost (body in HunterActions.cpp).
    bool Execute(Event event) override;
};

// "Hunter counter shot victim": Counter Shot 147362 on the current target
BEGIN_RANGED_SPELL_ACTION(CastCounterShotAction, "counter shot")
END_SPELL_ACTION()

// "Hunter counter shot random": Counter Shot on any casting enemy in range. Uses task 0's
// CastSpellOnEnemyCasterAction (#855). Gated on a battleground/arena map (beyond-AC behaviour stays out of PvE).
class CastCounterShotOnEnemyCasterAction : public CastSpellOnEnemyCasterAction
{
public:
    CastCounterShotOnEnemyCasterAction(PlayerbotAI* botAI) : CastSpellOnEnemyCasterAction(botAI, "counter shot") {}
    bool isUseful() override;
};

#endif
