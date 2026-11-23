/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_PVPARMSPOLICY_H
#define _PLAYERBOT_PVPARMSPOLICY_H

// BT-673 to BT-676: the Arms Warrior package of the PvP class rules (design decision 0032, owner decision 2026-11-19, rule R23 of docs/pvp).
// The shared layers decide the burst window, the interrupts and the defensives; this file adds what an Arms Warrior does inside
// them. Two of the ladder's set picks (Storm Bolt, Shockwave) and the Disarm of the class had no rule against a player:
//   1. Recklessness, Skull Banner and Avatar are one go: spent together inside the burst window with rage pooled for the Slam
//      spam, not on cooldown the moment the fight starts.
//   2. The stun chain: after a gap closer, or inside the window, one stun on a target the diminishing returns still let a full
//      stun land on (Storm Bolt first, Shockwave when it hits more than the target or Storm Bolt is spent).
//   3. Disarm on a melee player that is stunned or shows a physical burst, so the disarm cannot be dodged.
//   4. Touch of Karma is an untouchable target (the shared immunity read), so the burst window closes and the team looks elsewhere.
//
// Every decision returns a reason and a deterministic integer score (design decision 0014 and 0026). Tier gating (fundamentals sheet 14): an
// expert plays all of it, a good bot the simple half, a casual bot keeps the older rotation.
//
// Free of game types, so tests/ can run it (tests/test_pvp_arms.cpp).

#include "BotSkillMath.h"

#include <cstdint>

namespace PvpArms
{
using Tier = BotSkillMath::Tier;

constexpr std::uint32_t SPELL_RECKLESSNESS = 1719;
constexpr std::uint32_t SPELL_SKULL_BANNER = 114207;
constexpr std::uint32_t SPELL_AVATAR = 107574;
constexpr std::uint32_t SPELL_STORM_BOLT = 107570;
constexpr std::uint32_t SPELL_SHOCKWAVE = 46968;
constexpr std::uint32_t SPELL_DISARM = 676;
constexpr std::uint32_t SPELL_DISARM_ARMS = 118093;   // the level 20 row of the spec; a warrior knows one of the two
constexpr std::uint32_t SPELL_CHARGE = 100;
constexpr std::uint32_t SPELL_HEROIC_LEAP = 6544;
constexpr std::uint32_t AURA_TOUCH_OF_KARMA = 122470;

// ---------------------------------------------------------------------------------------------------------------------
// 1. The go: Recklessness with Skull Banner and Avatar
// ---------------------------------------------------------------------------------------------------------------------
// The rage an expert pools before the go: the window is spent on Slam and Mortal Strike, which want it.
constexpr std::uint32_t GO_RAGE = 80;
// A pool that has not filled this long after the window opened is spent as it is (the heroic strike dump keeps rage low).
constexpr std::uint32_t GO_POOL_TIMEOUT_MS = 5000;

enum class GoReason : std::uint8_t
{
    NOT_A_PLAYER,     // the PvE gates are untouched
    CASUAL_FREE,      // a casual bot spends them as before
    WINDOW_CLOSED,
    POOLING,          // the window is open and the expert still fills the rage
    POOL_TIMEOUT,
    GO
};

struct GoInputs
{
    bool targetIsPlayer = false;
    bool windowOpen = false;
    std::uint32_t rage = 0;            // the displayed rage (0 to 100)
    std::uint32_t windowOpenMs = 0;    // how long the window has been open for this bot
};

inline bool GoAllowed(GoReason reason)
{
    return reason == GoReason::NOT_A_PLAYER || reason == GoReason::CASUAL_FREE || reason == GoReason::POOL_TIMEOUT ||
        reason == GoReason::GO;
}

inline GoReason Go(GoInputs const& in, Tier tier)
{
    if (!in.targetIsPlayer)
        return GoReason::NOT_A_PLAYER;
    if (tier == Tier::CASUAL)
        return GoReason::CASUAL_FREE;
    if (!in.windowOpen)
        return GoReason::WINDOW_CLOSED;
    if (tier == Tier::GOOD)
        return GoReason::GO;
    if (in.rage >= GO_RAGE)
        return GoReason::GO;
    return in.windowOpenMs >= GO_POOL_TIMEOUT_MS ? GoReason::POOL_TIMEOUT : GoReason::POOLING;
}

// ---------------------------------------------------------------------------------------------------------------------
// 2. The stun chain
// ---------------------------------------------------------------------------------------------------------------------
enum class StunReason : std::uint8_t
{
    NO,
    CASUAL_SKIPS,
    NOT_A_PLAYER,
    NO_SETUP,           // neither a gap closer just landed nor the window open
    TARGET_HELD,        // already stunned, feared, sheeped or rooted: a stun would waste or break it
    DR_SPENT,           // the stun diminishing returns are above full (an expert reads them)
    NOT_READY,          // Storm Bolt and Shockwave are spent or out of reach
    STORM_BOLT,
    SHOCKWAVE
};

constexpr float STORM_BOLT_RANGE_YD = 30.0f;
constexpr float SHOCKWAVE_RANGE_YD = 10.0f;
constexpr std::int32_t SCORE_STUN_BASE = 60;
constexpr std::int32_t SCORE_STUN_AFTER_CLOSER = 20;
constexpr std::int32_t SCORE_STUN_WINDOW = 10;
constexpr std::int32_t SCORE_SHOCKWAVE_PER_EXTRA = 10;

struct StunInputs
{
    bool targetIsPlayer = false;
    bool windowOpen = false;
    bool afterGapCloser = false;       // Charge or Heroic Leap landed in the last moments
    bool targetHeld = false;
    std::uint32_t stunDrStep = 0;      // the bot's own reading (an expert); a good bot passes 0
    bool stormBoltReady = false;       // known and off cooldown
    bool shockwaveReady = false;
    float targetDistance = 0.0f;
    bool targetInFront = false;
    std::uint32_t enemiesInCone = 0;   // enemy players within the Shockwave cone, the target included
};

struct StunVerdict
{
    bool cast = false;
    StunReason reason = StunReason::NO;
    std::uint32_t spellId = 0;
    std::int32_t score = 0;
};

inline StunVerdict StunChain(StunInputs const& in, Tier tier)
{
    if (tier == Tier::CASUAL)
        return { false, StunReason::CASUAL_SKIPS, 0, 0 };
    if (!in.targetIsPlayer)
        return { false, StunReason::NOT_A_PLAYER, 0, 0 };
    if (!in.afterGapCloser && !in.windowOpen)
        return { false, StunReason::NO_SETUP, 0, 0 };
    if (in.targetHeld)
        return { false, StunReason::TARGET_HELD, 0, 0 };
    if (tier == Tier::EXPERT && in.stunDrStep > 0)
        return { false, StunReason::DR_SPENT, 0, 0 };

    std::int32_t const base = SCORE_STUN_BASE + (in.afterGapCloser ? SCORE_STUN_AFTER_CLOSER : 0) + (in.windowOpen ? SCORE_STUN_WINDOW : 0);
    bool const stormBolt = in.stormBoltReady && in.targetDistance <= STORM_BOLT_RANGE_YD;
    bool const shockwave = in.shockwaveReady && in.targetDistance <= SHOCKWAVE_RANGE_YD && in.targetInFront;
    // The cone is the pick when it catches another player too (the healer behind the target) or Storm Bolt is not there.
    if (shockwave && (!stormBolt || in.enemiesInCone >= 2))
    {
        std::int32_t const extra = in.enemiesInCone > 1 ? std::int32_t(in.enemiesInCone - 1) : 0;
        return { true, StunReason::SHOCKWAVE, SPELL_SHOCKWAVE, base + extra * SCORE_SHOCKWAVE_PER_EXTRA };
    }
    if (stormBolt)
        return { true, StunReason::STORM_BOLT, SPELL_STORM_BOLT, base };
    return { false, StunReason::NOT_READY, 0, 0 };
}

// ---------------------------------------------------------------------------------------------------------------------
// 3. Disarm after a stun
// ---------------------------------------------------------------------------------------------------------------------
enum class DisarmReason : std::uint8_t
{
    NO,
    NOT_READY,
    CASUAL_SKIPS,
    NOT_A_MELEE_PLAYER,
    ALREADY_DISARMED,
    OUT_OF_REACH,
    AFTER_STUN,         // the target is stunned: the disarm cannot be dodged
    PHYSICAL_BURST,     // a melee player shows a burst cooldown
    WINDOW              // a good bot disarms inside the window
};

constexpr float DISARM_RANGE_YD = 5.0f;

struct DisarmInputs
{
    bool ready = false;                 // known, off cooldown
    bool targetIsMeleePlayer = false;
    bool alreadyDisarmed = false;
    float targetDistance = 0.0f;
    bool targetStunned = false;
    bool targetBursting = false;
    bool windowOpen = false;
};

inline DisarmReason Disarm(DisarmInputs const& in, Tier tier)
{
    if (!in.ready)
        return DisarmReason::NOT_READY;
    if (tier == Tier::CASUAL)
        return DisarmReason::CASUAL_SKIPS;
    if (!in.targetIsMeleePlayer)
        return DisarmReason::NOT_A_MELEE_PLAYER;
    if (in.alreadyDisarmed)
        return DisarmReason::ALREADY_DISARMED;
    if (in.targetDistance > DISARM_RANGE_YD)
        return DisarmReason::OUT_OF_REACH;
    if (in.targetStunned)
        return DisarmReason::AFTER_STUN;
    if (tier == Tier::EXPERT && in.targetBursting)
        return DisarmReason::PHYSICAL_BURST;
    if (tier == Tier::GOOD && in.windowOpen)
        return DisarmReason::WINDOW;
    return DisarmReason::NO;
}

inline bool DisarmDue(DisarmReason reason)
{
    return reason == DisarmReason::AFTER_STUN || reason == DisarmReason::PHYSICAL_BURST || reason == DisarmReason::WINDOW;
}
}  // namespace PvpArms

#endif
