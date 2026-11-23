/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#ifndef _PLAYERBOT_HUNTERTRIGGERS_H
#define _PLAYERBOT_HUNTERTRIGGERS_H

#include "CureTriggers.h"
#include "GenericTriggers.h"
#include "PlayerbotAI.h"
#include "SharedDefines.h"
#include "Trigger.h"
#include <set>

class PlayerbotAI;

// Ported from AC mod-playerbots Ai/Class/Hunter/HunterTriggers.h (#149)

class NoTrackTrigger : public BuffTrigger
{
public:
    NoTrackTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "no track") {}
    bool IsActive() override;
};

class ConcussiveShotOnSnareTargetTrigger : public SnareTargetTrigger
{
public:
    ConcussiveShotOnSnareTargetTrigger(PlayerbotAI* botAI) : SnareTargetTrigger(botAI, "concussive shot") {}
};

class HunterNoStingsActiveTrigger : public DebuffTrigger
{
public:
    HunterNoStingsActiveTrigger(PlayerbotAI* botAI) : DebuffTrigger(botAI, "no stings") {}
    bool IsActive() override;
};

// tranquilizing shot dispels enrage/magic off the current target; MoP's TargetAuraDispelTrigger
// (strategy/triggers/CureTriggers.h) already provides the same "current target has a dispellable
// aura of this type" check AC's TargetAuraDispelTrigger provides.
class TargetRemoveEnrageTrigger : public TargetAuraDispelTrigger
{
public:
    TargetRemoveEnrageTrigger(PlayerbotAI* botAI) : TargetAuraDispelTrigger(botAI, "tranquilizing shot", DISPEL_ENRAGE) {}
};

class TargetRemoveMagicTrigger : public TargetAuraDispelTrigger
{
public:
    TargetRemoveMagicTrigger(PlayerbotAI* botAI) : TargetAuraDispelTrigger(botAI, "tranquilizing shot", DISPEL_MAGIC) {}
};

BEGIN_TRIGGER(HuntersPetDeadTrigger, Trigger)
END_TRIGGER()

BEGIN_TRIGGER(HuntersPetMediumHealthTrigger, Trigger)
END_TRIGGER()

// ---------------------------------------------------------------------------
// #357 slice 3: 1:1 port of AC mod-playerbots Ai/Class/Hunter/HunterTriggers.h.
//
// AC's HunterTriggers.h declares 35 trigger classes but AC's HunterAiObjectContext.cpp:66-98
// registers only 33 of them. The two AC classes that are correctly left unregistered are
// BestialWrathTrigger (AC HunterTriggers.h:90-94) and AutoShotTrigger (AC HunterTriggers.h:185-189)
// - AC has no creator entry for either, so they are NOT ported here. Every other AC class below has
// an AC creator and is registered in the fork's HunterAiObjectContext.cpp.
//
// Every base class used here already exists in this fork with AC's signature:
// strategy/triggers/GenericTriggers.h -- BuffTrigger:312, BoostTrigger:438, DebuffTrigger:386,
// DebuffOnAttackerTrigger:412, HasCcTargetTrigger:935,
// BuffOnMainTankTrigger:919, SpellNoCooldownTrigger:185, InterruptSpellTrigger:203, SpellTrigger:161;
// strategy/triggers/CureTriggers.h:24 -- TargetAuraDispelTrigger (already used by the #149 port above).
// ---------------------------------------------------------------------------

// Buff and Out of Combat Triggers

class HunterAspectOfThePackTrigger : public BuffTrigger
{
public:
    HunterAspectOfThePackTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "aspect of the pack") {}

    bool IsActive() override;
};

class TrueshotAuraTrigger : public BuffTrigger
{
public:
    TrueshotAuraTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "trueshot aura") {}
};

// Cooldown Triggers

class RapidFireTrigger : public BoostTrigger
{
public:
    RapidFireTrigger(PlayerbotAI* botAI) : BoostTrigger(botAI, "rapid fire") {}
};

class IntimidationTrigger : public BuffTrigger
{
public:
    IntimidationTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "intimidation") {}
};

class KillCommandTrigger : public BuffTrigger
{
public:
    KillCommandTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "kill command") {}

    bool IsActive() override;
};

// AC HunterTriggers.h:109-118. 5.4.8 divergence: AC keeps IsActive() inline here, but this fork's
// PlayerbotAI::HasAura(std::string, Unit*) cannot bind a Player* in a header TU - Player and Unit are
// unrelated types until Player.h is complete, which it is not in every TU that includes this header.
// So the body moves to HunterTriggers.cpp (where the full types are visible), the same shape every other
// trigger in this file already uses (cf. NoTrackTrigger above).
class LockAndLoadTrigger : public BuffTrigger
{
public:
    LockAndLoadTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "lock and load") {}

    bool IsActive() override;
};

// CC Triggers

class FreezingTrapTrigger : public HasCcTargetTrigger
{
public:
    FreezingTrapTrigger(PlayerbotAI* botAI) : HasCcTargetTrigger(botAI, "freezing trap") {}
};

class ScareBeastTrigger : public HasCcTargetTrigger
{
public:
    ScareBeastTrigger(PlayerbotAI* botAI) : HasCcTargetTrigger(botAI, "scare beast") {}
};

class SilencingShotTrigger : public InterruptSpellTrigger
{
public:
    SilencingShotTrigger(PlayerbotAI* botAI) : InterruptSpellTrigger(botAI, "silencing shot") {}
};

// DoT/Debuff Triggers

class HuntersMarkTrigger : public DebuffTrigger
{
public:
    HuntersMarkTrigger(PlayerbotAI* botAI) : DebuffTrigger(botAI, "hunter's mark", 1, true, 8.0f) {}

    bool IsActive() override { return DebuffTrigger::IsActive(); }
};

class ExplosiveShotTrigger : public DebuffTrigger
{
public:
    ExplosiveShotTrigger(PlayerbotAI* botAI) : DebuffTrigger(botAI, "explosive shot", 1, true) {}

    bool IsActive() override { return BuffTrigger::IsActive(); }
};

class BlackArrowTrigger : public DebuffTrigger
{
public:
    BlackArrowTrigger(PlayerbotAI* botAI) : DebuffTrigger(botAI, "black arrow", 1, true) {}

    bool IsActive() override;
};

class SerpentStingOnAttackerTrigger : public DebuffOnAttackerTrigger
{
public:
    SerpentStingOnAttackerTrigger(PlayerbotAI* botAI) : DebuffOnAttackerTrigger(botAI, "serpent sting", true) {}

    bool IsActive() override;
};

// Damage/Combat Triggers

class SwitchToRangedTrigger : public Trigger
{
public:
    SwitchToRangedTrigger(PlayerbotAI* botAI) : Trigger(botAI, "switch to ranged") {}

    bool IsActive() override;
};

class SwitchToMeleeTrigger : public Trigger
{
public:
    SwitchToMeleeTrigger(PlayerbotAI* botAI) : Trigger(botAI, "switch to melee") {}

    bool IsActive() override;
};

class MisdirectionOnMainTankTrigger : public BuffOnMainTankTrigger
{
public:
    MisdirectionOnMainTankTrigger(PlayerbotAI* botAI) : BuffOnMainTankTrigger(botAI, "misdirection", true) {}
};

BEGIN_TRIGGER(HuntersPetLowHealthTrigger, Trigger)
END_TRIGGER()

BEGIN_TRIGGER(HunterPetNotHappy, Trigger)
END_TRIGGER()

// ===== #856 rotation task 1 (epic #854): hunter triggers =====
// Counter Shot at the current target when it is casting an interruptible spell.
class CounterShotTrigger : public InterruptSpellTrigger
{
public:
    CounterShotTrigger(PlayerbotAI* botAI) : InterruptSpellTrigger(botAI, "counter shot") {}
};

// Binding Shot: current target 15-30 yd away.
class HunterBindingShotRangeTrigger : public Trigger
{
public:
    HunterBindingShotRangeTrigger(PlayerbotAI* botAI) : Trigger(botAI, "binding shot range") {}
    bool IsActive() override;
};

// Deterrence: health <= 30 % in combat (AC's "low health" edge is kept as well).
class HunterDeterrenceHealthTrigger : public Trigger
{
public:
    HunterDeterrenceHealthTrigger(PlayerbotAI* botAI) : Trigger(botAI, "deterrence health") {}
    bool IsActive() override;
};

// Rotation fix task H (#868): AC's "low health" (45 %, sPlayerbotAIConfig->lowHealth) -> Deterrence edge outside a pvp instance /
// running duel only. w73 duel logs: Antinic cast Deterrence at hp 46/43/40/37 %, the MoP rotation uses <= 30 %
// ("deterrence health" above); Deterrence also suppresses the hunter's own shots for its 5 s, so an early cast wastes the duel.
class HunterLowHealthNoPvpTrigger : public Trigger
{
public:
    HunterLowHealthNoPvpTrigger(PlayerbotAI* botAI) : Trigger(botAI, "hunter low health") {}
    bool IsActive() override;
};

// Rotation fix task H (#868): AC's "medium threat" (2+ attackers: duel opponent + its pet) -> Feign Death edge outside a pvp instance /
// running duel only (w73 Antinic cast Feign Death mid-duel; the MoP rotation has no Feign Death row and a player has no threat to drop).
class HunterMediumThreatNoPvpTrigger : public Trigger
{
public:
    HunterMediumThreatNoPvpTrigger(PlayerbotAI* botAI) : Trigger(botAI, "hunter medium threat") {}
    bool IsActive() override;
};

// Disengage: an enemy within 5 yd that is attacking me. The MoP rotation has it on every map; here it is
// gated on a battleground/arena map (an enemy PLAYER), open-world melee mobs keep AC's "enemy too close for auto shot".
class HunterDisengagePvpTrigger : public Trigger
{
public:
    HunterDisengagePvpTrigger(PlayerbotAI* botAI) : Trigger(botAI, "disengage pvp") {}
    bool IsActive() override;
};

// (MM) Steady Shot while Steady Focus (53224) has <= 3000 ms left (refresh the buff).
class HunterSteadyFocusLowTrigger : public Trigger
{
public:
    HunterSteadyFocusLowTrigger(PlayerbotAI* botAI) : Trigger(botAI, "steady focus low") {}
    bool IsActive() override;
};

// Rotation fix task K (#868): AC's "enemy too close for auto shot" -> disengage 35 / flee 34 edge (AC GenericHunterStrategy.cpp:166-167)
// outside a pvp instance / started duel only. In PvP the Disengage row ("disengage pvp" above) is the only retreat;
// w76 t613 Merick (SV) ran "flee" after Disengage vs a duel opponent at 5.6 yd, then idled 14 s of a 39 s duel.
class HunterEnemyTooCloseTrigger : public Trigger
{
public:
    HunterEnemyTooCloseTrigger(PlayerbotAI* botAI) : Trigger(botAI, "hunter enemy too close", 1) {}
    bool IsActive() override;
};

// Rotation fix task K (#868, BEYOND AC): a started duel whose opponent is alive but the bot has no current target. The combat
// engine's "invalid target" -> "drop target" (CombatStrategy.cpp:14, ChooseTargetActions.cpp:92) also switches to the NON-COMBAT
// engine, where AC's DuelStrategy "no attackers" -> "attack duel opponent" never fires because AttackersValue.cpp:54-55 always lists
// the duel opponent (AC AttackersValue.cpp does the same: AC idles up to 18 s in duels). w76 Merick: target dropped at duel second 8,
// then 30+ ticks of "follow USELESS" while the opponent hit him. Drives "attack duel opponent" in the combat and non-combat engines.
class HunterDuelRetargetTrigger : public Trigger
{
public:
    HunterDuelRetargetTrigger(PlayerbotAI* botAI) : Trigger(botAI, "hunter duel retarget", 1) {}
    bool IsActive() override;
};

// The hunter sheets use cached attacker counts and explicit proc aura IDs.
BEGIN_TRIGGER(HunterRapidFirePackTrigger, Trigger)
END_TRIGGER()
BEGIN_TRIGGER(HunterFrenzyTrigger, Trigger)
END_TRIGGER()
BEGIN_TRIGGER(HunterMasterMarksmanTrigger, Trigger)
END_TRIGGER()
BEGIN_TRIGGER(HunterFervorTrigger, Trigger)
END_TRIGGER()
BEGIN_TRIGGER(HunterIronHawkTrigger, Trigger)
END_TRIGGER()

BEGIN_TRIGGER(HunterTravelAspectTrigger, Trigger)
END_TRIGGER()
BEGIN_TRIGGER(HunterCancelTravelAspectTrigger, Trigger)
END_TRIGGER()

// NOT PORTED, DELIBERATELY:
//  * The "trap weave" strategy (AC GenericHunterStrategy.cpp:100/186-189, "immolation trap no cd -> reach melee") -
//    #521: Immolation Trap (13795 etc.) is NOT-LEARNABLE for a 5.4.8 hunter per the DBC (cls.py hunter 2026-10-04),
//    and no 5.x trap-weave successor is documented (Wowhead mop-classic tooltips 13795/13813, 2026-10-04: Explosive
//    Trap is a separate L38 30s-cooldown fire trap, not a melee-weave mechanic), so nothing is registered rather than
//    inventing one.
//  * BestialWrathTrigger (AC HunterTriggers.h:90-94) and AutoShotTrigger (AC HunterTriggers.h:185-189)
//    - AC declares them but registers neither in AiObjectContext.cpp, so there is no AC parity to
//    reproduce. "bestial wrath" and "auto shot" already resolve as ACTIONS in this fork
//    (HunterAiObjectContext.cpp:125 / :88).
// Hunter sheets (traps): good and expert bots keep Trap Launcher on; an expert throws Explosive Trap into a pack.
class HunterTrapLauncherTrigger : public Trigger
{
public:
    HunterTrapLauncherTrigger(PlayerbotAI* ai) : Trigger(ai, "hunter trap launcher", 2) {}
    bool IsActive() override;
};

class HunterExplosiveTrapPackTrigger : public Trigger
{
public:
    HunterExplosiveTrapPackTrigger(PlayerbotAI* ai) : Trigger(ai, "hunter explosive trap pack", 1) {}
    bool IsActive() override;
};

#endif
