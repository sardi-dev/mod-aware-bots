/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_PVPHOLYPALADINPOLICY_H
#define _PLAYERBOT_PVPHOLYPALADINPOLICY_H

// BT-683 to BT-687: the Holy Paladin package of the PvP class rules (design decision 0032, owner decision 2026-11-19, rules R3 and R18 of
// docs/pvp). The shared layers decide the control chain (Hammer of Justice and Repentance are rows of the kit), the burst window,
// the interrupts and the defensives of the paladin itself (Divine Protection and Divine Shield are in the catalog); this file adds
// what a Holy Paladin does for its team inside them:
//   1. Denounce in the downtime, with the Glyph of Denounce (70 percent of the ladder, taken into the set with this rule: BT-452).
//   2. Hand of Sacrifice before a control cast lands on a teammate, and in an emergency.
//   3. Hand of Protection on a teammate who is stunned or burst by melee, when he is not a melee damage dealer himself.
//   4. Devotion Aura (the silence and interrupt immunity, 180 s) when the paladin is silenced and the team needs heals.
//   5. Beacon of Light follows the teammate the enemy team is working on.
//
// Every decision returns a reason and a deterministic integer score where candidates compete (design decision 0014 and 0026). Tier gating
// (fundamentals sheet 14): an expert plays all of it, a good bot the simple half, a casual bot keeps the older chains.
//
// Free of game types, so tests/ can run it (tests/test_pvp_holy_paladin.cpp).

#include "BotSkillMath.h"

#include <cstdint>

namespace PvpHolyPaladin
{
using Tier = BotSkillMath::Tier;

constexpr std::uint32_t SPELL_DENOUNCE = 2812;
constexpr std::uint32_t SPELL_HAND_OF_SACRIFICE = 6940;
constexpr std::uint32_t SPELL_HAND_OF_PROTECTION = 1022;
constexpr std::uint32_t SPELL_DEVOTION_AURA = 31821;
constexpr std::uint32_t SPELL_BEACON_OF_LIGHT = 53563;

constexpr float HEAL_RANGE_YD = 40.0f;
constexpr float DENOUNCE_RANGE_YD = 30.0f;

// ---------------------------------------------------------------------------------------------------------------------
// 1. Denounce in the downtime
// ---------------------------------------------------------------------------------------------------------------------
enum class DenounceReason : std::uint8_t
{
    NO,
    NOT_KNOWN,
    CASUAL_SKIPS,
    NOT_A_PLAYER,
    OUT_OF_REACH,
    MOVING,             // a cast cannot be made on the run
    ALLY_NEEDS_HEAL,    // the paladin heals first
    BREAKABLE_CC,       // the damage would free a sheep or a fear
    KICK_RISK,          // an expert does not offer a holy school lock to a kick that is not spent
    DOWNTIME
};

constexpr float DENOUNCE_QUIET_EXPERT_PCT = 80.0f;
constexpr float DENOUNCE_QUIET_GOOD_PCT = 90.0f;

struct DenounceInputs
{
    bool known = false;
    bool targetIsPlayer = false;
    float targetDistance = 0.0f;
    bool moving = false;
    float lowestAllyPercent = 100.0f;
    bool targetBreakableCc = false;
    bool enemyKickNotSpent = false;     // the paladin's own book
};

inline DenounceReason Denounce(DenounceInputs const& in, Tier tier)
{
    if (!in.known)
        return DenounceReason::NOT_KNOWN;
    if (tier == Tier::CASUAL)
        return DenounceReason::CASUAL_SKIPS;
    if (!in.targetIsPlayer)
        return DenounceReason::NOT_A_PLAYER;
    if (in.targetDistance > DENOUNCE_RANGE_YD)
        return DenounceReason::OUT_OF_REACH;
    if (in.moving)
        return DenounceReason::MOVING;
    float const line = tier == Tier::EXPERT ? DENOUNCE_QUIET_EXPERT_PCT : DENOUNCE_QUIET_GOOD_PCT;
    if (in.lowestAllyPercent < line)
        return DenounceReason::ALLY_NEEDS_HEAL;
    if (in.targetBreakableCc)
        return DenounceReason::BREAKABLE_CC;
    if (tier == Tier::EXPERT && in.enemyKickNotSpent)
        return DenounceReason::KICK_RISK;
    return DenounceReason::DOWNTIME;
}

// ---------------------------------------------------------------------------------------------------------------------
// 2. Hand of Sacrifice
// ---------------------------------------------------------------------------------------------------------------------
enum class SacrificeReason : std::uint8_t
{
    NO,
    NOT_READY,
    CASUAL_SKIPS,
    ON_SELF,                // the hand cannot be put on the paladin
    ALREADY_SACRIFICED,
    NO_THREAT,
    BEFORE_CONTROL,         // a control cast is on its way to the ally
    EMERGENCY               // the ally is low and a player is hitting him
};

constexpr std::uint32_t SACRIFICE_MIN_LEFT_MS = 300;
constexpr float SACRIFICE_EMERGENCY_EXPERT_PCT = 40.0f;
constexpr float SACRIFICE_EMERGENCY_GOOD_PCT = 35.0f;
// The hand sends part of the damage to the paladin: it is not cast with the paladin below this.
constexpr float SACRIFICE_PALADIN_MIN_PCT = 60.0f;
constexpr std::int32_t SCORE_SACRIFICE_BEFORE_CONTROL = 80;
constexpr std::int32_t SCORE_SACRIFICE_EMERGENCY = 60;

struct SacrificeInputs
{
    bool ready = false;                       // known, off cooldown
    bool allyIsSelf = false;
    bool allyHasSacrifice = false;
    std::uint32_t controlLeftMs = 0;          // a control cast aimed at the ally: what is left of it (0: none)
    std::int32_t playerAttackers = 0;
    float allyHealthPercent = 100.0f;
    float paladinHealthPercent = 100.0f;
};

struct SacrificeVerdict
{
    bool cast = false;
    SacrificeReason reason = SacrificeReason::NO;
    std::int32_t score = 0;
};

inline SacrificeVerdict HandOfSacrifice(SacrificeInputs const& in, Tier tier)
{
    if (!in.ready)
        return { false, SacrificeReason::NOT_READY, 0 };
    if (tier == Tier::CASUAL)
        return { false, SacrificeReason::CASUAL_SKIPS, 0 };
    if (in.allyIsSelf)
        return { false, SacrificeReason::ON_SELF, 0 };
    if (in.allyHasSacrifice)
        return { false, SacrificeReason::ALREADY_SACRIFICED, 0 };
    if (tier == Tier::EXPERT && in.controlLeftMs >= SACRIFICE_MIN_LEFT_MS)
        return { true, SacrificeReason::BEFORE_CONTROL, SCORE_SACRIFICE_BEFORE_CONTROL + std::int32_t(100.0f - in.allyHealthPercent) };
    if (in.playerAttackers <= 0)
        return { false, SacrificeReason::NO_THREAT, 0 };
    float const line = tier == Tier::EXPERT ? SACRIFICE_EMERGENCY_EXPERT_PCT : SACRIFICE_EMERGENCY_GOOD_PCT;
    if (in.allyHealthPercent <= line && in.paladinHealthPercent >= SACRIFICE_PALADIN_MIN_PCT)
        return { true, SacrificeReason::EMERGENCY, SCORE_SACRIFICE_EMERGENCY + std::int32_t(100.0f - in.allyHealthPercent) };
    return { false, SacrificeReason::NO_THREAT, 0 };
}

// ---------------------------------------------------------------------------------------------------------------------
// 3. Hand of Protection
// ---------------------------------------------------------------------------------------------------------------------
enum class ProtectionReason : std::uint8_t
{
    NO,
    NOT_READY,
    CASUAL_SKIPS,
    ALLY_HITS_HARD,         // a melee damage dealer is better left to hit (the bubble stops his attacks)
    NO_PHYSICAL,            // nobody is hitting him with melee
    ALREADY_PROTECTED,
    STUNNED,                // stunned in an enemy go
    PHYSICAL_BURST,
    EMERGENCY
};

constexpr float PROTECTION_STUN_PCT = 70.0f;
constexpr float PROTECTION_BURST_PCT = 60.0f;
constexpr float PROTECTION_EMERGENCY_PCT = 35.0f;
constexpr std::int32_t SCORE_PROTECTION_BASE = 60;
constexpr std::int32_t SCORE_PROTECTION_STUN = 15;
constexpr std::int32_t SCORE_PROTECTION_BURST = 10;

struct ProtectionInputs
{
    bool ready = false;
    bool allyHasProtection = false;
    bool allyIsMeleeDamage = false;           // a melee damage dealer: the bubble would stop his damage
    bool allyStunned = false;
    std::int32_t meleeAttackers = 0;          // melee players hitting him
    bool enemyBurstSeen = false;
    float allyHealthPercent = 100.0f;
};

struct ProtectionVerdict
{
    bool cast = false;
    ProtectionReason reason = ProtectionReason::NO;
    std::int32_t score = 0;
};

inline ProtectionVerdict HandOfProtection(ProtectionInputs const& in, Tier tier)
{
    if (!in.ready)
        return { false, ProtectionReason::NOT_READY, 0 };
    if (tier == Tier::CASUAL)
        return { false, ProtectionReason::CASUAL_SKIPS, 0 };
    if (in.allyHasProtection)
        return { false, ProtectionReason::ALREADY_PROTECTED, 0 };
    if (in.meleeAttackers <= 0)
        return { false, ProtectionReason::NO_PHYSICAL, 0 };
    if (in.allyIsMeleeDamage && !in.allyStunned)
        return { false, ProtectionReason::ALLY_HITS_HARD, 0 };

    std::int32_t const base = SCORE_PROTECTION_BASE + std::int32_t(100.0f - in.allyHealthPercent);
    if (tier == Tier::EXPERT && in.allyStunned && in.allyHealthPercent <= PROTECTION_STUN_PCT)
        return { true, ProtectionReason::STUNNED, base + SCORE_PROTECTION_STUN };
    if (tier == Tier::EXPERT && in.enemyBurstSeen && in.allyHealthPercent <= PROTECTION_BURST_PCT)
        return { true, ProtectionReason::PHYSICAL_BURST, base + SCORE_PROTECTION_BURST };
    if (in.allyHealthPercent <= PROTECTION_EMERGENCY_PCT)
        return { true, ProtectionReason::EMERGENCY, base };
    return { false, ProtectionReason::NO, 0 };
}

// ---------------------------------------------------------------------------------------------------------------------
// 4. Devotion Aura when silenced
// ---------------------------------------------------------------------------------------------------------------------
enum class AuraReason : std::uint8_t
{
    NO,
    NOT_READY,
    TIER_SKIPS,
    NOT_SILENCED,
    TEAM_SAFE,          // nobody needs heals badly enough to spend the cooldown
    PRESS
};

constexpr float AURA_EXPERT_PCT = 75.0f;
constexpr float AURA_GOOD_PCT = 50.0f;

struct AuraInputs
{
    bool ready = false;                 // known, off cooldown
    bool silenced = false;
    float lowestAllyPercent = 100.0f;
};

inline AuraReason DevotionAuraNow(AuraInputs const& in, Tier tier)
{
    if (!in.ready)
        return AuraReason::NOT_READY;
    if (tier == Tier::CASUAL)
        return AuraReason::TIER_SKIPS;
    if (!in.silenced)
        return AuraReason::NOT_SILENCED;
    float const line = tier == Tier::EXPERT ? AURA_EXPERT_PCT : AURA_GOOD_PCT;
    return in.lowestAllyPercent <= line ? AuraReason::PRESS : AuraReason::TEAM_SAFE;
}

// ---------------------------------------------------------------------------------------------------------------------
// 5. Beacon of Light follows the focus
// ---------------------------------------------------------------------------------------------------------------------
enum class BeaconReason : std::uint8_t
{
    NO,
    NOT_KNOWN,
    CASUAL_SKIPS,       // the main tank rule stands for a casual bot
    NO_FOCUS,           // nobody is under attack by a player
    ALREADY_ON_FOCUS,
    PLACE,              // no beacon is up
    MOVE                // the beacon sits on an ally nobody is hitting while another one is under fire
};

constexpr float BEACON_FOCUS_PCT = 90.0f;

struct BeaconInputs
{
    bool known = false;
    bool beaconUp = false;                    // the paladin's beacon is on some ally
    std::int32_t beaconHolderAttackers = 0;   // players hitting the ally who holds it
    bool candidateIsHolder = false;           // the most pressed ally already holds it
    std::int32_t candidateAttackers = 0;
    float candidateHealthPercent = 100.0f;
};

inline BeaconReason Beacon(BeaconInputs const& in, Tier tier)
{
    if (!in.known)
        return BeaconReason::NOT_KNOWN;
    if (tier == Tier::CASUAL)
        return BeaconReason::CASUAL_SKIPS;
    if (in.candidateAttackers <= 0 || in.candidateHealthPercent > BEACON_FOCUS_PCT)
        return BeaconReason::NO_FOCUS;
    if (in.candidateIsHolder)
        return BeaconReason::ALREADY_ON_FOCUS;
    if (!in.beaconUp)
        return BeaconReason::PLACE;
    // Moving it costs a cast of its own: only an expert follows the focus, and only off an ally nobody hits.
    if (tier == Tier::EXPERT && in.beaconHolderAttackers == 0)
        return BeaconReason::MOVE;
    return BeaconReason::NO;
}

inline bool BeaconDue(BeaconReason reason)
{
    return reason == BeaconReason::PLACE || reason == BeaconReason::MOVE;
}
}  // namespace PvpHolyPaladin

#endif
