/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_PVPSHADOWPRIESTPOLICY_H
#define _PLAYERBOT_PVPSHADOWPRIESTPOLICY_H

// BT-688 to BT-690: the Shadow Priest package of the PvP class rules (design decision 0032, owner decision 2026-11-19, rule R17 of docs/pvp).
// The shared layers decide the control chain (Psychic Horror and Silence are rows of the kit), the burst window, the interrupts and
// the defensives (Dispersion is in the catalog); this file adds what a Shadow Priest does inside them:
//   1. Vampiric Embrace goes with the burst when allies are hurt, not on cooldown (the generic edge cast it on every fight).
//   2. Leap of Faith pulls a stunned or trapped ally out of an enemy go.
//   3. Mass Dispel on an enemy that holds a magic immunity (Ice Block, Divine Shield, Hand of Protection), with the glyph the set
//      holds (the glyph had no rule before).
//
// Every decision returns a reason (and a score where candidates compete), deterministic (design decision 0014 and 0026). Tier gating
// (fundamentals sheet 14): an expert plays all of it, a good bot the simple half, a casual bot keeps the older rotation.
//
// Free of game types, so tests/ can run it (tests/test_pvp_shadow_priest.cpp).

#include "BotSkillMath.h"

#include <cstdint>

namespace PvpShadowPriest
{
using Tier = BotSkillMath::Tier;

constexpr std::uint32_t SPELL_VAMPIRIC_EMBRACE = 15286;
constexpr std::uint32_t SPELL_LEAP_OF_FAITH = 73325;
constexpr std::uint32_t SPELL_MASS_DISPEL = 32375;
constexpr std::uint32_t SPELL_DIVINE_SHIELD = 642;
constexpr std::uint32_t SPELL_ICE_BLOCK = 45438;
constexpr std::uint32_t SPELL_HAND_OF_PROTECTION = 1022;
constexpr std::uint32_t SPELL_GLYPH_OF_MASS_DISPEL = 55691;

// ---------------------------------------------------------------------------------------------------------------------
// 1. Vampiric Embrace with the burst
// ---------------------------------------------------------------------------------------------------------------------
enum class EmbraceReason : std::uint8_t
{
    NOT_A_PVP_FIGHT,     // the PvE rotation keeps its edge
    CASUAL_FREE,
    SELF_LOW,            // the priest itself needs the healing
    TEAM_HURT,           // two allies or more are hurt
    BURST_WITH_HURT,     // the burst window is open and an ally is hurt
    HOLD
};

constexpr float EMBRACE_HURT_PCT = 80.0f;
constexpr float EMBRACE_SELF_PCT = 50.0f;

struct EmbraceInputs
{
    bool pvpFight = false;
    std::uint32_t alliesHurt = 0;       // allies (the priest included) under EMBRACE_HURT_PCT
    bool windowOpen = false;
    float selfHealthPercent = 100.0f;
};

inline bool EmbraceAllowed(EmbraceReason reason)
{
    return reason != EmbraceReason::HOLD;
}

inline EmbraceReason VampiricEmbrace(EmbraceInputs const& in, Tier tier)
{
    if (!in.pvpFight)
        return EmbraceReason::NOT_A_PVP_FIGHT;
    if (tier == Tier::CASUAL)
        return EmbraceReason::CASUAL_FREE;
    if (in.selfHealthPercent <= EMBRACE_SELF_PCT)
        return EmbraceReason::SELF_LOW;
    if (in.alliesHurt >= 2)
        return EmbraceReason::TEAM_HURT;
    if (in.windowOpen && in.alliesHurt >= 1)
        return EmbraceReason::BURST_WITH_HURT;
    return EmbraceReason::HOLD;
}

// ---------------------------------------------------------------------------------------------------------------------
// 2. Leap of Faith
// ---------------------------------------------------------------------------------------------------------------------
enum class LeapReason : std::uint8_t
{
    NO,
    NOT_READY,
    CASUAL_SKIPS,
    ON_SELF,
    NOT_HELD,            // the ally can act: no need to pull him
    NO_THREAT,
    TOO_CLOSE,           // he is next to the priest already
    TOO_HEALTHY,
    PULL
};

constexpr float LEAP_EXPERT_PCT = 65.0f;
constexpr float LEAP_GOOD_PCT = 40.0f;
constexpr float LEAP_MIN_DISTANCE_YD = 8.0f;
constexpr float LEAP_RANGE_YD = 40.0f;
constexpr std::int32_t SCORE_LEAP_BASE = 50;
constexpr std::int32_t SCORE_LEAP_PER_ATTACKER = 5;
constexpr std::int32_t LEAP_ATTACKERS_COUNTED = 3;

struct LeapInputs
{
    bool ready = false;
    bool allyIsSelf = false;
    bool allyHeld = false;               // stunned, rooted or in a hard control that holds him in place
    std::int32_t playerAttackers = 0;
    float allyHealthPercent = 100.0f;
    float allyDistance = 0.0f;
};

struct LeapVerdict
{
    bool cast = false;
    LeapReason reason = LeapReason::NO;
    std::int32_t score = 0;
};

inline LeapVerdict LeapOfFaith(LeapInputs const& in, Tier tier)
{
    if (!in.ready)
        return { false, LeapReason::NOT_READY, 0 };
    if (tier == Tier::CASUAL)
        return { false, LeapReason::CASUAL_SKIPS, 0 };
    if (in.allyIsSelf)
        return { false, LeapReason::ON_SELF, 0 };
    if (!in.allyHeld)
        return { false, LeapReason::NOT_HELD, 0 };
    if (in.playerAttackers <= 0)
        return { false, LeapReason::NO_THREAT, 0 };
    if (in.allyDistance < LEAP_MIN_DISTANCE_YD || in.allyDistance > LEAP_RANGE_YD)
        return { false, LeapReason::TOO_CLOSE, 0 };
    float const line = tier == Tier::EXPERT ? LEAP_EXPERT_PCT : LEAP_GOOD_PCT;
    if (in.allyHealthPercent > line)
        return { false, LeapReason::TOO_HEALTHY, 0 };
    std::int32_t const attackers = in.playerAttackers > LEAP_ATTACKERS_COUNTED ? LEAP_ATTACKERS_COUNTED : in.playerAttackers;
    return { true, LeapReason::PULL, SCORE_LEAP_BASE + std::int32_t(100.0f - in.allyHealthPercent) + attackers * SCORE_LEAP_PER_ATTACKER };
}

// ---------------------------------------------------------------------------------------------------------------------
// 3. Mass Dispel on a magic immunity
// ---------------------------------------------------------------------------------------------------------------------
enum class DispelReason : std::uint8_t
{
    NO,
    NOT_READY,
    CASUAL_SKIPS,
    NO_GLYPH,            // without the glyph Mass Dispel leaves the immunities alone
    NOT_IMMUNE,
    SILENCED,
    OUT_OF_RANGE,
    BREAK_IMMUNITY
};

constexpr float MASS_DISPEL_RANGE_YD = 30.0f;

struct DispelInputs
{
    bool ready = false;                  // known, off cooldown
    bool glyphed = false;
    bool targetImmune = false;           // Ice Block, Divine Shield or Hand of Protection on the target
    bool silenced = false;
    float targetDistance = 0.0f;
};

inline DispelReason MassDispel(DispelInputs const& in, Tier tier)
{
    if (!in.ready)
        return DispelReason::NOT_READY;
    if (tier == Tier::CASUAL)
        return DispelReason::CASUAL_SKIPS;
    if (!in.glyphed)
        return DispelReason::NO_GLYPH;
    if (!in.targetImmune)
        return DispelReason::NOT_IMMUNE;
    if (in.silenced)
        return DispelReason::SILENCED;
    if (in.targetDistance > MASS_DISPEL_RANGE_YD)
        return DispelReason::OUT_OF_RANGE;
    return DispelReason::BREAK_IMMUNITY;
}
}  // namespace PvpShadowPriest

#endif
