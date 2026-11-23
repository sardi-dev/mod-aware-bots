/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#ifndef _PLAYERBOT_WARRIORTRIGGERS_H
#define _PLAYERBOT_WARRIORTRIGGERS_H

#include "GenericTriggers.h"
#include "RangeTriggers.h"
#include "Playerbots.h"
#include "PlayerbotAI.h"

class BattleShoutTrigger : public BuffTrigger
{
public:
	BattleShoutTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "battle shout", 10) {}
};
class CommandingShoutTrigger : public BuffTrigger
{
public:
	CommandingShoutTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "commanding shout", 10) {}
};
BUFF_TRIGGER(BattleStanceTrigger, "battle stance");
BUFF_TRIGGER(DefensiveStanceTrigger, "defensive stance");
BUFF_TRIGGER(BerserkerStanceTrigger, "berserker stance");

CAN_CAST_TRIGGER(VictoryRushTrigger, "victory rush");
// Task 11 (#866): Thunder Clap 6343 applies Weakened Blows 115798 (not an aura named
// "thunder clap"), so the old name-based DEBUFF_TRIGGER never saw the debuff. Active when the spell is ready, the current
// target is within 7 yd and does not carry the bot's Weakened Blows.
class ThunderClapTrigger : public Trigger
{
public:
    ThunderClapTrigger(PlayerbotAI* botAI) : Trigger(botAI, "thunder clap", 1) {}

    bool IsActive() override;
};
DEBUFF_TRIGGER(SunderArmorTrigger, "sunder armor");
// #522 (AC pattern GenericTriggers: DKTriggers.h PlagueStrikeDebuffTrigger "blood plague", 1, true, .0f): Colossus
// Smash 86346 (20 s cd damage ability, Wowhead mop-classic tooltip 2026-10-05) is our own debuff, so check ownership
// and drop DebuffTrigger's default 8 s target-lifetime guard (target hp / group dps >= 8), which kept the edge silent
// for the whole 30-40 s dummy fight in wave 10 (no T:colossus smash, no attempt).
class ColossusSmashTrigger : public DebuffTrigger
{
public:
    ColossusSmashTrigger(PlayerbotAI* botAI) : DebuffTrigger(botAI, "colossus smash", 1, true, .0f) {}
};
INTERRUPT_HEALER_TRIGGER(PummelInterruptEnemyHealerSpellTrigger, "pummel");
INTERRUPT_TRIGGER(PummelInterruptSpellTrigger, "pummel");

// -- arms
CAN_CAST_TRIGGER(OverpowerAvailableTrigger, "overpower");
CAN_CAST_TRIGGER(MortalStrikeReadyTrigger, "mortal strike");
DEBUFF_CHECKISOWNER_TRIGGER(MortalWoundsDebuffTrigger, "mortal wounds");
HAS_AURA_TRIGGER(TasteForBloodTrigger, "taste for blood");

class WarriorExpertThrowTrigger : public EnemyOutOfMeleeTrigger
{
public:
    WarriorExpertThrowTrigger(PlayerbotAI* ai) : EnemyOutOfMeleeTrigger(ai) {}
    bool IsActive() override;
};
// Damage sheets: Enrage from 12880 suppresses routine Berserker Rage, while control breaks remain separate.
class WarriorEnrageMissingTrigger : public Trigger
{
public:
    WarriorEnrageMissingTrigger(PlayerbotAI* ai) : Trigger(ai, "warrior enrage missing", 1) {}
    bool IsActive() override;
};

// Arms sheet gap item 10: the Heroic Strike dump and Slam inside the Colossus Smash window (DpsRotationMath::Warrior).
class ArmsHeroicStrikeRageTrigger : public Trigger
{
public:
    ArmsHeroicStrikeRageTrigger(PlayerbotAI* ai) : Trigger(ai, "arms heroic strike rage", 1) {}
    bool IsActive() override;
};
// Whether this bot's own Colossus Smash is on its current target (also read by the Arms Execute action).
bool ArmsColossusSmashWindowUp(PlayerbotAI* botAI);

class ArmsColossusSmashWindowTrigger : public Trigger
{
public:
    ArmsColossusSmashWindowTrigger(PlayerbotAI* ai) : Trigger(ai, "arms colossus smash window", 1) {}
    bool IsActive() override;
};

// -- fury
CAN_CAST_TRIGGER(BloodthirstReadyTrigger, "bloodthirst");
class FuryPackWhirlwindTrigger : public MediumAoeTrigger
{
public:
    FuryPackWhirlwindTrigger(PlayerbotAI* ai) : MediumAoeTrigger(ai) {}
    bool IsActive() override;
};
class RagingBlowTwoStacksTrigger : public Trigger
{
public:
    RagingBlowTwoStacksTrigger(PlayerbotAI* ai) : Trigger(ai, "raging blow two stacks", 1) {}
    bool IsActive() override;
};
CAN_CAST_TRIGGER(RagingBlowAvailableTrigger, "raging blow");
class MeatCleaverTrigger : public HasAuraTrigger
{
public:
	MeatCleaverTrigger(PlayerbotAI* ai) : HasAuraTrigger(ai, "meat cleaver") {}
};
class BloodSurgeTrigger : public HasAuraTrigger
{
public:
	BloodSurgeTrigger(PlayerbotAI* ai) : HasAuraTrigger(ai, "bloodsurge") {}
};

// -- prot
class ShieldBarrierTrigger : public BuffTrigger
{
public:
	ShieldBarrierTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "shield barrier", 10) {}
	bool IsActive() override;
};
BUFF_TRIGGER(ShieldBlockTrigger, "shield block");
DEBUFF_TRIGGER(DisarmDebuffTrigger, "disarm");
CAN_CAST_TRIGGER(RevengeAvailableTrigger, "revenge");
class VigilanceTrigger : public BuffOnPartyTrigger
{
public:
	VigilanceTrigger(PlayerbotAI* botAI) : BuffOnPartyTrigger(botAI, "vigilance") {}

	bool IsActive() override;
};
// #163 S8 (AC parity, Ai/Class/Warrior/WarriorTriggers.h:47).
SNARE_TRIGGER(TauntSnareTrigger, "taunt");

// ---------------------------------------------------------------------------
// Issue #357 slice 10 - 1:1 port of AC Ai/Class/Warrior/WarriorTriggers.h:
// the trigger classes this fork was missing. Only classes AC declares are
// added here. Every macro used below exists with AC's exact signature in
// strategy/AiObject.h (BUFF_TRIGGER, BUFF_TRIGGER_A, DEBUFF_TRIGGER,
// SNARE_TRIGGER, CD_TRIGGER, BOOST_TRIGGER, HAS_AURA_TRIGGER, DEFLECT_TRIGGER,
// INTERRUPT_TRIGGER, INTERRUPT_HEALER_TRIGGER) and every base class comes from
// GenericTriggers.h. The two classes that need a body declare
// `bool IsActive() override;` here and define it in WarriorTriggers.cpp: on
// this core PlayerbotAI::HasAura(std::string, Unit*) will not bind a Player*
// from a header TU, and AI_VALUE2 expands to the AiObject `context` member.
// Registered names: see WarriorTriggerFactoryInternal (AC
// WarriorAiObjectContext.cpp:55-102).
SNARE_TRIGGER(HamstringTrigger, "hamstring");
BOOST_TRIGGER(DeathWishTrigger, "death wish");
BOOST_TRIGGER(RecklessnessTrigger, "recklessness");

BUFF_TRIGGER_A(BloodrageBuffTrigger, "bloodrage");
HAS_AURA_TRIGGER(SwordAndBoardTrigger, "sword and board");
DEBUFF_TRIGGER(MortalStrikeDebuffTrigger, "mortal strike");
SNARE_TRIGGER(ThunderClapSnareTrigger, "thunder clap");
BUFF_TRIGGER(BloodthirstBuffTrigger, "bloodthirst");
BUFF_TRIGGER(WhirlwindTrigger, "whirlwind");
BUFF_TRIGGER(BerserkerRageBuffTrigger, "berserker rage");
SNARE_TRIGGER(ShockwaveSnareTrigger, "shockwave");
DEBUFF_TRIGGER(ShockwaveTrigger, "shockwave");
DEFLECT_TRIGGER(SpellReflectionTrigger, "spell reflection");
HAS_AURA_TRIGGER(SuddenDeathTrigger, "sudden death");
HAS_AURA_TRIGGER(SlamInstantTrigger, "slam!");

// #522 (AC Ai/Base/Trigger/GenericTriggers.h:753-758): "fear sleep sap" is not registered in this fork's
// shared TriggerContext; same body as AC's FearSleepSapTrigger, registered by the warrior context.
class WarriorFearSleepSapTrigger : public Trigger
{
public:
    WarriorFearSleepSapTrigger(PlayerbotAI* botAI) : Trigger(botAI, "fear sleep sap", 1) {}

    bool IsActive() override;
};

class ShatteringThrowTrigger : public Trigger
{
public:
    ShatteringThrowTrigger(PlayerbotAI* botAI) : Trigger(botAI, "shattering throw trigger") {}

    bool IsActive() override;
};

// ---------------------------------------------------------------------------
// Task 11 (#866, epic #854): MoP warrior rotation port.
// ---------------------------------------------------------------------------
// Exact MoP health thresholds (Shield Wall and Rallying Cry/Demoralizing Banner/Last Stand at
// 20 %, Die by the Sword at 40 %, Demoralizing Shout and Shield Barrier at 50 %). AC's "low health"
// / "critical health" values are config driven and do not match, so the warrior reads the percentage directly.
class WarriorSelfHealthTrigger : public Trigger
{
public:
    WarriorSelfHealthTrigger(PlayerbotAI* botAI, std::string const name, uint8 pct)
        : Trigger(botAI, name, 1), pct(pct)
    {
    }

    bool IsActive() override;

private:
    uint8 pct;
};
class WarriorHealth20Trigger : public WarriorSelfHealthTrigger
{
public:
    WarriorHealth20Trigger(PlayerbotAI* botAI) : WarriorSelfHealthTrigger(botAI, "warrior health 20", 20) {}
};
class WarriorHealth40Trigger : public WarriorSelfHealthTrigger
{
public:
    WarriorHealth40Trigger(PlayerbotAI* botAI) : WarriorSelfHealthTrigger(botAI, "warrior health 40", 40) {}
};
class WarriorHealth50Trigger : public WarriorSelfHealthTrigger
{
public:
    WarriorHealth50Trigger(PlayerbotAI* botAI) : WarriorSelfHealthTrigger(botAI, "warrior health 50", 50) {}
};
// Avatar 107574, Bloodbath 12292, Skull Banner 114207: "always" in combat.
// Not BoostTrigger: the MoP rotation has no balance gate, only the cooldown.
BUFF_TRIGGER(AvatarTrigger, "avatar");
BUFF_TRIGGER(BloodbathBuffTrigger, "bloodbath");
CAN_CAST_TRIGGER(SkullBannerCanCastTrigger, "skull banner");

// ---------------------------------------------------------------------------
// Class sheet, protection warrior (BT-125, docs/class-sheets/warrior-protection.md): IsActive bodies in
// WarriorTriggers.cpp.
// ---------------------------------------------------------------------------
// Defensive cooldown thresholds of the tank, in percent of its own health. The MoP rotation port above presses them at
// 20 to 25 percent, which is too late for a dungeon tank: the sheet puts Shield Wall at 40, Last Stand at 35 and Enraged
// Regeneration at 50.
constexpr uint8 TANK_ENRAGED_REGENERATION_HEALTH_PCT = 50;
constexpr uint8 TANK_SHIELD_WALL_HEALTH_PCT = 40;
constexpr uint8 TANK_LAST_STAND_HEALTH_PCT = 35;

// Shield Wall at the tank threshold while the spell is ready (the first of the two big cooldowns).
class TankShieldWallHealthTrigger : public WarriorSelfHealthTrigger
{
public:
    TankShieldWallHealthTrigger(PlayerbotAI* botAI)
        : WarriorSelfHealthTrigger(botAI, "tank shield wall health", TANK_SHIELD_WALL_HEALTH_PCT)
    {
    }

    bool IsActive() override;
};

// Last Stand at the tank threshold once Shield Wall cannot be used (not learned yet, or already on its cooldown): the
// two big cooldowns do not go off together.
class TankLastStandTrigger : public Trigger
{
public:
    TankLastStandTrigger(PlayerbotAI* botAI) : Trigger(botAI, "tank last stand", 1) {}

    bool IsActive() override;
};

// Enraged Regeneration at the tank threshold (a "good" line of the sheet; the critical health edge keeps it for every tier).
class TankEnragedRegenerationHealthTrigger : public WarriorSelfHealthTrigger
{
public:
    TankEnragedRegenerationHealthTrigger(PlayerbotAI* botAI)
        : WarriorSelfHealthTrigger(botAI, "tank enraged regeneration health", TANK_ENRAGED_REGENERATION_HEALTH_PCT)
    {
    }

    bool IsActive() override;
};

// Defensive Stance out of combat for a tank in a group, so the first combat tick is not spent swapping stance.
class TankDefensiveStanceTrigger : public BuffTrigger
{
public:
    TankDefensiveStanceTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "defensive stance") {}

    bool IsActive() override;
};

// BT-673 to BT-676: the Arms package of the PvP class rules (Utils/PvpArmsPolicy.h, PvP/PvpArms.h). The glue answers for an enemy
// player target only; PvE is untouched.
// The stun chain: Storm Bolt or Shockwave after a gap closer or inside the burst window.
class ArmsPvpStunTrigger : public Trigger
{
public:
    ArmsPvpStunTrigger(PlayerbotAI* botAI) : Trigger(botAI, "arms pvp stun", 1) {}

    bool IsActive() override;
};

// Disarm on a melee player that is stunned or shows a physical burst.
class ArmsPvpDisarmTrigger : public Trigger
{
public:
    ArmsPvpDisarmTrigger(PlayerbotAI* botAI) : Trigger(botAI, "arms pvp disarm", 1) {}

    bool IsActive() override;
};

#endif
