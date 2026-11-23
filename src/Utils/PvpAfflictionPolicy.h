/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_PVPAFFLICTIONPOLICY_H
#define _PLAYERBOT_PVPAFFLICTIONPOLICY_H

// BT-663 to BT-668: the Affliction Warlock package of the PvP class rules (design decision 0032, owner decision 2026-11-19, rule R21 of
// docs/pvp). The shared layers decide the control chain, the burst window, the interrupts and the defensives of the bot; this file
// adds what only an Affliction Warlock does inside them:
//   1. Shadowfury (the ladder's tier 30 pick, an area stun) opens the healer lock chain and Fear follows; the chain itself is the
//      shared kit (PvpTeamPolicy.h), this file only holds the splash rule (never into a team sheep).
//   2. Howl of Terror on a melee player in reach, as a peel: not into an enemy a teammate holds in a breakable control.
//   3. Fel Flame, an instant fire spell, when the shadow school is locked out (the DoTs are shut, the fire filler is not).
//   4. Haunt is a window spell against a player: it costs a shard and its damage frees a feared target, so it waits for the burst
//      window, a hurt target or a held one.
//   5. Dark Regeneration and the Healthstone together: the regeneration runs for 6 s, the stone heals at once.
//
// Every decision returns a reason and a deterministic integer score (design decision 0014 and 0026). Tier gating (fundamentals sheet 14): an
// expert plays all of it, a good bot the simple half, a casual bot keeps the older rotation.
//
// Free of game types, so tests/ can run it (tests/test_pvp_affliction.cpp).

#include "BotSkillMath.h"

#include <cstdint>

namespace PvpAffliction
{
using Tier = BotSkillMath::Tier;

constexpr std::uint32_t SPELL_SHADOWFURY = 30283;
constexpr std::uint32_t SPELL_HOWL_OF_TERROR = 5484;
constexpr std::uint32_t SPELL_HAUNT = 48181;
constexpr std::uint32_t SPELL_FEL_FLAME = 77799;
constexpr std::uint32_t SPELL_DARK_REGENERATION = 108359;
constexpr std::uint32_t SPELL_CORRUPTION = 172;

// Shadowfury and Howl of Terror strike everyone within this of their centre.
constexpr float AREA_RADIUS_YD = 8.0f;

// ---------------------------------------------------------------------------------------------------------------------
// 1. Splash: an area control on the healer must not wake a sheep of the team
// ---------------------------------------------------------------------------------------------------------------------
// A control that strikes an area is cast only when no enemy within the radius of the centre is held in a breakable control (the
// stun is a hit, and a hit frees a sheep, a fear or a sleep).
inline bool SplashAllowed(std::uint32_t breakableHeldInRadius)
{
    return breakableHeldInRadius == 0;
}

// ---------------------------------------------------------------------------------------------------------------------
// 2. Howl of Terror as a peel
// ---------------------------------------------------------------------------------------------------------------------
enum class HowlReason : std::uint8_t
{
    NO,
    NOT_READY,          // not learned or on cooldown
    NO_MELEE,           // nobody to peel
    TEAM_CONTROL,       // a teammate holds an enemy within the radius in a breakable control
    HEALTHY_CASUAL,     // a casual bot uses it as a last resort only
    HEALTHY_GOOD,       // a good bot peels once it is hurt
    PEEL
};

constexpr float HOWL_CASUAL_HP_PERCENT = 55.0f;
constexpr float HOWL_GOOD_HP_PERCENT = 85.0f;
constexpr std::int32_t SCORE_HOWL_BASE = 50;
constexpr std::int32_t SCORE_HOWL_PER_MELEE = 10;
constexpr std::int32_t MELEE_COUNTED = 3;

struct HowlInputs
{
    bool ready = false;                       // known, off cooldown
    std::uint32_t meleePlayersInRadius = 0;   // melee players within the radius that are attacking the warlock
    std::uint32_t breakableHeldInRadius = 0;
    float healthPercent = 100.0f;
};

struct HowlVerdict
{
    bool cast = false;
    HowlReason reason = HowlReason::NO;
    std::int32_t score = 0;
};

inline HowlVerdict HowlOfTerror(HowlInputs const& in, Tier tier)
{
    if (!in.ready)
        return { false, HowlReason::NOT_READY, 0 };
    if (in.meleePlayersInRadius == 0)
        return { false, HowlReason::NO_MELEE, 0 };
    if (tier != Tier::CASUAL && !SplashAllowed(in.breakableHeldInRadius))
        return { false, HowlReason::TEAM_CONTROL, 0 };
    if (tier == Tier::CASUAL && in.healthPercent > HOWL_CASUAL_HP_PERCENT)
        return { false, HowlReason::HEALTHY_CASUAL, 0 };
    if (tier == Tier::GOOD && in.healthPercent > HOWL_GOOD_HP_PERCENT)
        return { false, HowlReason::HEALTHY_GOOD, 0 };
    std::uint32_t const melee = in.meleePlayersInRadius > std::uint32_t(MELEE_COUNTED) ? std::uint32_t(MELEE_COUNTED) : in.meleePlayersInRadius;
    std::int32_t const score = SCORE_HOWL_BASE + std::int32_t(melee) * SCORE_HOWL_PER_MELEE +
        std::int32_t((100.0f - in.healthPercent) / 2.0f);
    return { true, HowlReason::PEEL, score };
}

// ---------------------------------------------------------------------------------------------------------------------
// 3. Fel Flame while the shadow school is locked
// ---------------------------------------------------------------------------------------------------------------------
enum class FelFlameReason : std::uint8_t
{
    NO,
    NOT_READY,
    CASUAL_SKIPS,
    SHADOW_OPEN,        // the DoTs and the filler work: no need
    NOT_A_FIGHT,        // the target is not an enemy player
    SHADOW_LOCKED
};

struct FelFlameInputs
{
    bool known = false;
    bool shadowLocked = false;      // a shadow spell of the warlock is on a lockout cooldown (Corruption has none of its own)
    bool targetIsPlayer = false;
};

inline FelFlameReason FelFlame(FelFlameInputs const& in, Tier tier)
{
    if (!in.known)
        return FelFlameReason::NOT_READY;
    if (tier == Tier::CASUAL)
        return FelFlameReason::CASUAL_SKIPS;
    if (!in.shadowLocked)
        return FelFlameReason::SHADOW_OPEN;
    return in.targetIsPlayer ? FelFlameReason::SHADOW_LOCKED : FelFlameReason::NOT_A_FIGHT;
}

inline bool FelFlameDue(FelFlameReason reason)
{
    return reason == FelFlameReason::SHADOW_LOCKED;
}

// ---------------------------------------------------------------------------------------------------------------------
// 4. Haunt in the window
// ---------------------------------------------------------------------------------------------------------------------
enum class HauntReason : std::uint8_t
{
    NOT_A_PLAYER,       // not this file's business: the PvE rotation keeps Haunt up
    CASUAL_KEEPS,       // a casual bot keeps it up as before
    BREAKABLE_CC,       // the damage would free a feared or sheeped target
    WINDOW,
    TARGET_LOW,
    TARGET_HELD,
    HOLD
};

constexpr float HAUNT_LOW_HP_PERCENT = 30.0f;

struct HauntInputs
{
    bool targetIsPlayer = false;
    bool windowOpen = false;
    bool targetBreakableCc = false;
    bool targetHeld = false;            // stunned or rooted
    float targetHealthPercent = 100.0f;
};

inline bool HauntAllowed(HauntReason reason)
{
    return reason == HauntReason::NOT_A_PLAYER || reason == HauntReason::CASUAL_KEEPS || reason == HauntReason::WINDOW ||
        reason == HauntReason::TARGET_LOW || reason == HauntReason::TARGET_HELD;
}

inline HauntReason Haunt(HauntInputs const& in, Tier tier)
{
    if (!in.targetIsPlayer)
        return HauntReason::NOT_A_PLAYER;
    if (tier == Tier::CASUAL)
        return HauntReason::CASUAL_KEEPS;
    if (in.targetBreakableCc)
        return HauntReason::BREAKABLE_CC;
    if (in.windowOpen)
        return HauntReason::WINDOW;
    if (in.targetHealthPercent <= HAUNT_LOW_HP_PERCENT)
        return HauntReason::TARGET_LOW;
    // Only an expert reads a held target as a free window for the shard.
    if (tier == Tier::EXPERT && in.targetHeld)
        return HauntReason::TARGET_HELD;
    return HauntReason::HOLD;
}

// ---------------------------------------------------------------------------------------------------------------------
// 5. Dark Regeneration with the Healthstone
// ---------------------------------------------------------------------------------------------------------------------
enum class StoneReason : std::uint8_t
{
    NO,
    CASUAL_SKIPS,
    NO_REGENERATION,    // the regeneration is not running: the older health lines decide
    NO_STONE,
    HEALTHY,
    PAIR
};

constexpr float STONE_PAIR_HP_PERCENT = 50.0f;

struct StoneInputs
{
    bool regenerationRunning = false;   // Dark Regeneration aura on the warlock
    bool stoneUsable = false;           // in the bags and off cooldown
    float healthPercent = 100.0f;
};

inline StoneReason RegenerationStone(StoneInputs const& in, Tier tier)
{
    if (tier == Tier::CASUAL)
        return StoneReason::CASUAL_SKIPS;
    if (!in.regenerationRunning)
        return StoneReason::NO_REGENERATION;
    if (!in.stoneUsable)
        return StoneReason::NO_STONE;
    if (in.healthPercent > STONE_PAIR_HP_PERCENT)
        return StoneReason::HEALTHY;
    return StoneReason::PAIR;
}

inline bool StoneDue(StoneReason reason)
{
    return reason == StoneReason::PAIR;
}
}  // namespace PvpAffliction

#endif
