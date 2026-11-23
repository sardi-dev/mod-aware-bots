/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#ifndef _PLAYERBOT_MAGETRIGGERS_H
#define _PLAYERBOT_MAGETRIGGERS_H

#include "CureTriggers.h"
#include "GenericTriggers.h"
#include "SharedDefines.h"
#include <unordered_set>

class ArcaneBrillianceTrigger : public BuffTrigger
{
public:
    ArcaneBrillianceTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "arcane brilliance", 2 * 2000) {}
    bool IsActive() override;
};

// 5.4.8: AC's "arcane intellect on party" ported against MoP's single raid buff spell, "arcane brilliance"
// (Arcane Intellect was removed in Cataclysm; Arcane Brilliance is now the only Mage int buff).
class ArcaneBrillianceOnPartyTrigger : public BuffOnPartyTrigger
{
public:
    ArcaneBrillianceOnPartyTrigger(PlayerbotAI* botAI) : BuffOnPartyTrigger(botAI, "arcane brilliance", 4 * 2000) {}
};

class MageArmorTrigger : public BuffTrigger
{
public:
    MageArmorTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "mage armor", 5 * 2000) {}
    bool IsActive() override;
};

class FrostArmorTrigger : public BuffTrigger
{
public:
    FrostArmorTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "frost armor", 5 * 2000) {}
    bool IsActive() override;
};

class MoltenArmorTrigger : public BuffTrigger
{
public:
    MoltenArmorTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "molten armor", 5 * 2000) {}
    bool IsActive() override;
};

class CombustionTrigger : public BoostTrigger
{
public:
    CombustionTrigger(PlayerbotAI* botAI) : BoostTrigger(botAI, "combustion") {}
};

class MirrorImageTrigger : public BoostTrigger
{
public:
    MirrorImageTrigger(PlayerbotAI* botAI) : BoostTrigger(botAI, "mirror image") {}
};

class CounterspellInterruptSpellTrigger : public InterruptSpellTrigger
{
public:
    CounterspellInterruptSpellTrigger(PlayerbotAI* botAI) : InterruptSpellTrigger(botAI, "counterspell") {}
};

class SpellstealTrigger : public TargetAuraDispelTrigger
{
public:
    SpellstealTrigger(PlayerbotAI* botAI) : TargetAuraDispelTrigger(botAI, "spellsteal", DISPEL_MAGIC) {}
};
class LivingBombTrigger : public DebuffTrigger
{
public:
    LivingBombTrigger(PlayerbotAI* botAI) : DebuffTrigger(botAI, "living bomb", 1, true) {}
};

class LivingBombOnAttackersTrigger : public DebuffOnAttackerTrigger
{
public:
    LivingBombOnAttackersTrigger(PlayerbotAI* botAI) : DebuffOnAttackerTrigger(botAI, "living bomb", true) {}
    bool IsActive() override { return BuffTrigger::IsActive(); }
};

class MissileBarrageTrigger : public HasAuraTrigger
{
public:
    // #602 (AC MageTriggers.h MissileBarrageTrigger, aura "missile barrage"): 5.4.8 has no Missile Barrage (44401);
    // its successor is the "Arcane Missiles!" proc, aura 79683 (Wowhead mop-classic tooltip spell 79683, 2026-10-05).
    // HasAura matches the whole aura name, so the "!" is required.
    MissileBarrageTrigger(PlayerbotAI* botAI) : HasAuraTrigger(botAI, "arcane missiles!") {}
};

class FingersOfFrostSingleTrigger : public HasAuraStackTrigger
{
public:
    FingersOfFrostSingleTrigger(PlayerbotAI* ai) : HasAuraStackTrigger(ai, "fingers of frost", 1, 1) {}
    bool IsActive() override;
};

class FingersOfFrostDoubleTrigger : public HasAuraStackTrigger
{
public:
    FingersOfFrostDoubleTrigger(PlayerbotAI* ai) : HasAuraStackTrigger(ai, "fingers of frost", 2, 1) {}
};

class ArcanePowerTrigger : public BuffTrigger
{
public:
    ArcanePowerTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "arcane power") {}
};

class PresenceOfMindTrigger : public BuffTrigger
{
public:
    PresenceOfMindTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "presence of mind") {}
};

// -- forst triggers
class FrostNovaOnTargetTrigger : public DebuffTrigger
{
public:
    FrostNovaOnTargetTrigger(PlayerbotAI* botAI) : DebuffTrigger(botAI, "frost nova", 1, false) {}
    bool IsActive() override;
};

class IcyVeinsTrigger : public BuffTrigger
{
public:
    IcyVeinsTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "icy veins") {}
    bool IsActive() override;
};

class FrostBombTrigger : public DebuffTrigger
{
public:
    FrostBombTrigger(PlayerbotAI* botAI) : DebuffTrigger(botAI, "frost bomb", 1, true) {}
};

// -- arcane triggers
class ArcaneBlastTrigger : public BuffTrigger
{
public:
    ArcaneBlastTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "arcane blast") {}
};

class ArcaneBlastStackTrigger : public HasAuraStackTrigger
{
public:
    ArcaneBlastStackTrigger(PlayerbotAI* botAI) : HasAuraStackTrigger(botAI, "arcane charge", 4, 1) {}
    bool IsActive() override;
    // #602: 5.4.8 Arcane Blast stacks as Arcane Charge 36032 (Wowhead mop-classic, 2026-10-05); no "Arcane Blast" aura.
};

class ArcaneMissilesTrigger : public HasAuraStackTrigger
{
public:
    ArcaneMissilesTrigger(PlayerbotAI* ai) : HasAuraStackTrigger(ai, "arcane missiles", 1, 1) {}
};

class BrainFreezeTrigger : public HasAuraStackTrigger
{
public:
    BrainFreezeTrigger(PlayerbotAI* ai) : HasAuraStackTrigger(ai, "brain freeze", 1, 1) {}
};

class ArcaneChargeTrigger : public HasAuraStackTrigger
{
public:
    ArcaneChargeTrigger(PlayerbotAI* ai) : HasAuraStackTrigger(ai, "arcane charge", 1, 1) {}
};

class SlowNotOnTargetTrigger : public DebuffTrigger
{
public:
    SlowNotOnTargetTrigger(PlayerbotAI* botAI) : DebuffTrigger(botAI, "slow", 1, false) {}
    bool IsActive() override;
};

class NetherTempestTrigger : public DebuffTrigger
{
public:
    NetherTempestTrigger(PlayerbotAI* botAI) : DebuffTrigger(botAI, "nether tempest", 1, true) {}
};

class RuneOfPowerTrigger : public BuffTrigger
{
public:
    RuneOfPowerTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "rune of power", 5 * 2000) {}
    bool IsActive() override;
};

class HeatingUpTrigger : public HasAuraTrigger
{
public:
    HeatingUpTrigger(PlayerbotAI* botAI) : HasAuraTrigger(botAI, "heating up", 1) {}
    bool IsActive() override;
};

class PyroblastTrigger : public HasAuraTrigger
{
public:
    PyroblastTrigger(PlayerbotAI* botAI) : HasAuraTrigger(botAI, "pyroblast!", 1) {}
};

// ---------------------------------------------------------------------------
// AC WotLK parity (#357 slice 8) - ported 1:1 from
// modules/mod-playerbots/src/Ai/Class/Mage/MageTriggers.h in the AC WotLK tree.
//
// 5.4.8 note: every IsActive() body lives in MageTriggers.cpp - PlayerbotAI::HasAura
// (std::string, Unit*) cannot bind a Player* from a header translation unit here.
// ---------------------------------------------------------------------------

// Buff and Out of Combat Triggers

class IceBarrierTrigger : public BuffTrigger
{
public:
    IceBarrierTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "ice barrier") {}
};

class NoManaGemTrigger : public Trigger
{
public:
    NoManaGemTrigger(PlayerbotAI* botAI) : Trigger(botAI, "no mana gem") {}
    bool IsActive() override;
};

// #385/#521: "being attacked" (frost ice barrier edge) is the shared BeingAttackedTrigger in
// strategy/triggers/TriggerContext.h (AC TriggerContext.h:127).

// Proc and Boost Triggers

class ArcaneBlast4StacksAndMissileBarrageTrigger : public TwoTriggers
{
public:
    ArcaneBlast4StacksAndMissileBarrageTrigger(PlayerbotAI* botAI)
        : TwoTriggers(botAI, "arcane blast stack", "missile barrage") {}
};

class IcyVeinsCooldownTrigger : public SpellCooldownTrigger
{
public:
    IcyVeinsCooldownTrigger(PlayerbotAI* botAI) : SpellCooldownTrigger(botAI, "icy veins") {}
};

// 5.4.8 INERT: Deep Freeze became the Water Elemental's Freeze in Cataclysm; spell 44572 is gone.
class DeepFreezeCooldownTrigger : public SpellCooldownTrigger
{
public:
    DeepFreezeCooldownTrigger(PlayerbotAI* botAI) : SpellCooldownTrigger(botAI, "deep freeze") {}

    bool IsActive() override;
};

class ColdSnapTrigger : public Trigger
{
public:
    ColdSnapTrigger(PlayerbotAI* botAI) : Trigger(botAI, "cold snap", 1) {}
    bool IsActive() override;
};

// CC, Interrupt, and Dispel Triggers

class PolymorphTrigger : public HasCcTargetTrigger
{
public:
    PolymorphTrigger(PlayerbotAI* botAI) : HasCcTargetTrigger(botAI, "polymorph") {}
};

class RemoveCurseTrigger : public NeedCureTrigger
{
public:
    RemoveCurseTrigger(PlayerbotAI* botAI) : NeedCureTrigger(botAI, "remove curse", DISPEL_CURSE) {}
};

class PartyMemberRemoveCurseTrigger : public PartyMemberNeedCureTrigger
{
public:
    PartyMemberRemoveCurseTrigger(PlayerbotAI* botAI) : PartyMemberNeedCureTrigger(botAI, "remove curse", DISPEL_CURSE) {}
};

class CounterspellEnemyHealerTrigger : public InterruptEnemyHealerTrigger
{
public:
    CounterspellEnemyHealerTrigger(PlayerbotAI* botAI) : InterruptEnemyHealerTrigger(botAI, "counterspell") {}
};

// Damage and Debuff Triggers

class FireballTrigger : public DebuffTrigger
{
public:
    FireballTrigger(PlayerbotAI* botAI) : DebuffTrigger(botAI, "fireball", 1, true) {}
};

// AC parity; no strategy uses this trigger on either tree (#385 task M: Frostfire Bolt 44614 exists on 5.4.8).
class FrostfireBoltTrigger : public DebuffTrigger
{
public:
    FrostfireBoltTrigger(PlayerbotAI* botAI) : DebuffTrigger(botAI, "frostfire bolt", 1, true) {}
};

class FingersOfFrostTrigger : public HasAuraTrigger
{
public:
    FingersOfFrostTrigger(PlayerbotAI* botAI) : HasAuraTrigger(botAI, "fingers of frost") {}
};

class FlamestrikeNearbyTrigger : public Trigger
{
public:
    FlamestrikeNearbyTrigger(PlayerbotAI* botAI, float radius = 30.0f) : Trigger(botAI, "flamestrike nearby"), radius(radius) {}
    bool IsActive() override;

protected:
    float radius;
    static const std::unordered_set<uint32> FLAMESTRIKE_SPELL_IDS;
};

class FlamestrikeBlizzardTrigger : public TwoTriggers
{
public:
    FlamestrikeBlizzardTrigger(PlayerbotAI* botAI) : TwoTriggers(botAI, "flamestrike nearby", "medium aoe") {}
};

class BlizzardChannelCheckTrigger : public Trigger
{
public:
    BlizzardChannelCheckTrigger(PlayerbotAI* botAI, uint32 minEnemies = 2)
        : Trigger(botAI, "blizzard channel check"), minEnemies(minEnemies) {}

    bool IsActive() override;

protected:
    uint32 minEnemies;
    static const std::unordered_set<uint32> BLIZZARD_SPELL_IDS;
};

// Rotation task 10 (#865, epic #854). PvP gates are class-local
// triggers over the lane-0 contract names ("in pvp instance" value, "enemy player in melee range" and
// "enemy caster in range" triggers), so open world / dungeons stay on AC behaviour.
// "mage pvp melee enemy player": in a BG/arena and an enemy player is within 5 yd.
// Frost Nova (<= 5 yd), Blink (<= 3 yd and target is a player).
class MagePvpEnemyPlayerInMeleeRangeTrigger : public Trigger
{
public:
    MagePvpEnemyPlayerInMeleeRangeTrigger(PlayerbotAI* botAI) : Trigger(botAI, "mage pvp melee enemy player", 1) {}
    bool IsActive() override;
};

// "mage pvp enemy caster": in a BG/arena and any enemy caster (interruptible cast/channel) is in range.
// Counterspell (each valid enemy around; AC's healer-only edge stays alongside).
class MagePvpEnemyCasterInRangeTrigger : public Trigger
{
public:
    MagePvpEnemyCasterInRangeTrigger(PlayerbotAI* botAI) : Trigger(botAI, "mage pvp enemy caster", 1) {}
    bool IsActive() override;
};

// "mage ice block health": in a BG/arena Ice Block at <= 10 % and in a group;
// everywhere else AC's "critical health" threshold (GenericMageStrategy.cpp:100).
class MageIceBlockHealthTrigger : public Trigger
{
public:
    MageIceBlockHealthTrigger(PlayerbotAI* botAI) : Trigger(botAI, "mage ice block health", 1) {}
    bool IsActive() override;
};

// Rotation fix task H (duel combat #868, w73 duel logs): two AC edges that are wrong in a 1v1 duel / BG fight because the
// opponent follows and the MoP rotation has neither row (no Invisibility, no fleeing; it casts, Frost Novas and Blinks).
// "mage medium threat": AC "medium threat" (2+ attackers: the duel opponent plus its pet / our Mirror Image) -> Invisibility
// (AC GenericMageStrategy.cpp:97), only outside a pvp instance / running duel (Invisibility drops the target: "invalid target").
class MageMediumThreatTrigger : public Trigger
{
public:
    MageMediumThreatTrigger(PlayerbotAI* botAI) : Trigger(botAI, "mage medium threat", 1) {}
    bool IsActive() override;
};

// "mage flee close": AC "enemy too close for spell" -> flee (RangedCombatStrategy.cpp:14-15), only outside a pvp instance /
// running duel. In PvP the Frost Nova (50) / Blink back (35) / Blazing Speed (34) edges handle a close enemy, then the mage
// keeps casting instead of running (w73 Nancey: 5 casts in 56 s).
class MageFleeCloseTrigger : public Trigger
{
public:
    MageFleeCloseTrigger(PlayerbotAI* botAI) : Trigger(botAI, "mage flee close", 1) {}
    bool IsActive() override;
};

class MageImportantPullTrigger : public Trigger
{
public:
    MageImportantPullTrigger(PlayerbotAI* ai) : Trigger(ai, "mage important pull", 1) {}
    bool IsActive() override;
};
class MagePlayerNovaTrigger : public Trigger
{
public:
    MagePlayerNovaTrigger(PlayerbotAI* ai) : Trigger(ai, "mage player nova", 1) {}
    bool IsActive() override;
};
class MageMovingTrigger : public Trigger
{
public:
    MageMovingTrigger(PlayerbotAI* ai) : Trigger(ai, "mage moving", 1) {}
    bool IsActive() override;
};
class MagePlayerSlowTrigger : public Trigger
{
public:
    MagePlayerSlowTrigger(PlayerbotAI* ai) : Trigger(ai, "mage player slow", 1) {}
    bool IsActive() override;
};
class MageExplosionTrigger : public Trigger
{
public:
    MageExplosionTrigger(PlayerbotAI* ai) : Trigger(ai, "mage explosion", 1) {}
    bool IsActive() override;
};
// Fire sheet line 5: hard-cast Pyroblast as the opener of a fight the mage starts alone (DpsRotationMath::Mage).
class FirePyroblastOpenerTrigger : public Trigger
{
public:
    FirePyroblastOpenerTrigger(PlayerbotAI* ai) : Trigger(ai, "fire pyroblast opener", 1) {}
    bool IsActive() override;
};

// Fire sheet rotation item 5: Presence of Mind before a Pyroblast on a pack of elites, a boss or a player.
class FirePresenceOfMindTrigger : public Trigger
{
public:
    FirePresenceOfMindTrigger(PlayerbotAI* ai) : Trigger(ai, "fire presence of mind", 1) {}
    bool IsActive() override;
};

// The Presence of Mind aura is up: its instant goes to Pyroblast.
class PresenceOfMindActiveTrigger : public HasAuraTrigger
{
public:
    PresenceOfMindActiveTrigger(PlayerbotAI* ai) : HasAuraTrigger(ai, "presence of mind", 1) {}
    // BT-620: not against an enemy player (FirePomControlTrigger takes the aura there).
    bool IsActive() override;
};

// BT-620: the aura is up against an enemy player and the route says Frostjaw: the instant goes to the freeze setup.
class FirePomControlTrigger : public Trigger
{
public:
    FirePomControlTrigger(PlayerbotAI* ai) : Trigger(ai, "fire pom control", 1) {}
    bool IsActive() override;
};

// BT-650 to BT-655: the Frost package of the PvP class rules (Utils/PvpFrostMagePolicy.h, PvP/PvpFrostMage.h). Each trigger asks the
// game glue, which answers only for an enemy player target; PvE is untouched.
// Alter Time in the burst window with a loaded bar and no Hypothermia.
class FrostAlterTimeTrigger : public Trigger
{
public:
    FrostAlterTimeTrigger(PlayerbotAI* ai) : Trigger(ai, "frost alter time", 1) {}
    bool IsActive() override;
};

// The Water Elemental's Freeze goes before Deep Freeze (or banks a Fingers of Frost charge).
class FrostPetFreezeTrigger : public Trigger
{
public:
    FrostPetFreezeTrigger(PlayerbotAI* ai) : Trigger(ai, "frost pet freeze", 1) {}
    bool IsActive() override;
};

// The target is frozen by the mage's elemental: Deep Freeze has its frozen target.
class FrostPetFrozenTargetTrigger : public Trigger
{
public:
    FrostPetFrozenTargetTrigger(PlayerbotAI* ai) : Trigger(ai, "frost pet frozen target", 1) {}
    bool IsActive() override;
};

// Presence of Mind against a player goes to an instant control (Polymorph on the healer, Frostjaw on the kill target).
class FrostPresenceOfMindTrigger : public Trigger
{
public:
    FrostPresenceOfMindTrigger(PlayerbotAI* ai) : Trigger(ai, "frost presence of mind", 1) {}
    bool IsActive() override;
};

// Incanter's Ward before the burst window or when a player hits the mage (the ladder's tier 90 pick).
class FrostIncantersWardTrigger : public Trigger
{
public:
    FrostIncantersWardTrigger(PlayerbotAI* ai) : Trigger(ai, "frost incanters ward", 1) {}
    bool IsActive() override;
};

// Fire sheet level 87 line: Alter Time while a free Pyroblast is up.
class FireAlterTimeTrigger : public Trigger
{
public:
    FireAlterTimeTrigger(PlayerbotAI* ai) : Trigger(ai, "fire alter time", 1) {}
    bool IsActive() override;
};
#endif
