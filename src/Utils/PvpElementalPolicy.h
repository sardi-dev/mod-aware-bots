/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_PVPELEMENTALPOLICY_H
#define _PLAYERBOT_PVPELEMENTALPOLICY_H

// BT-677 to BT-681: the Elemental Shaman package of the PvP class rules (design decision 0032, owner decision 2026-11-19, rules R7 and R16 of
// docs/pvp). The shared layers decide the control chain (Hex on the enemy healer is a row of the kit), the burst window, the
// interrupts and the defensives; this file adds what an Elemental Shaman does inside them:
//   1. Ancestral Swiftness before the Hex on the healer (an instant Hex cannot be kicked) and Swiftness held for it.
//   2. Grounding Totem on an incoming control cast, and Call of the Elements to bring it back for the next one.
//   3. Ascendance, the Fire Elemental and Elemental Mastery are the burst: held for the window, not cast on cooldown.
//   4. Spiritwalker's Grace while running with casts to make.
//
// Every decision returns a reason (and a score where candidates compete), deterministic (design decision 0014 and 0026). Tier gating
// (fundamentals sheet 14): an expert plays all of it, a good bot the simple half, a casual bot keeps the older rotation.
//
// Free of game types, so tests/ can run it (tests/test_pvp_elemental.cpp).

#include "BotSkillMath.h"

#include <cstdint>

namespace PvpElemental
{
using Tier = BotSkillMath::Tier;

constexpr std::uint32_t SPELL_HEX = 51514;
constexpr std::uint32_t SPELL_ANCESTRAL_SWIFTNESS = 16188;
constexpr std::uint32_t SPELL_GROUNDING_TOTEM = 8177;
constexpr std::uint32_t SPELL_CALL_OF_THE_ELEMENTS = 108285;
constexpr std::uint32_t SPELL_SPIRITWALKERS_GRACE = 110806;
constexpr std::uint8_t SPIRITWALKERS_GRACE_LEVEL = 87;

// ---------------------------------------------------------------------------------------------------------------------
// 1. Ancestral Swiftness for the Hex
// ---------------------------------------------------------------------------------------------------------------------
enum class SwiftnessReason : std::uint8_t
{
    NO,
    NOT_READY,          // not learned, on cooldown, or already up
    TIER_SKIPS,         // only an expert plans the instant Hex
    HEX_NOT_DUE,        // the control chain does not name this shaman's Hex now
    NO_KICK_TO_FEAR,    // no enemy kick left unspent: a cast Hex is as good
    PRESS
};

struct SwiftnessInputs
{
    bool ready = false;                 // known, off cooldown, not up
    bool hexDue = false;                // the shared chain names this shaman's Hex on the healer now
    bool enemyKickNotSpent = false;     // the shaman's own book
};

inline SwiftnessReason SwiftnessForHex(SwiftnessInputs const& in, Tier tier)
{
    if (!in.ready)
        return SwiftnessReason::NOT_READY;
    if (tier != Tier::EXPERT)
        return SwiftnessReason::TIER_SKIPS;
    if (!in.hexDue)
        return SwiftnessReason::HEX_NOT_DUE;
    return in.enemyKickNotSpent ? SwiftnessReason::PRESS : SwiftnessReason::NO_KICK_TO_FEAR;
}

// The generic edge casts Swiftness on cooldown. In a team fight, with Hex known and ready, a non-casual bot keeps it for the Hex.
inline bool SwiftnessHeld(bool inTeamFight, bool hexKnown, bool hexReady, Tier tier)
{
    return tier != Tier::CASUAL && inTeamFight && hexKnown && hexReady;
}

// ---------------------------------------------------------------------------------------------------------------------
// 2. Grounding Totem on an incoming control
// ---------------------------------------------------------------------------------------------------------------------
enum class GroundReason : std::uint8_t
{
    NO,
    NOT_READY,          // not learned or on cooldown
    TIER_SKIPS,         // a casual bot does not react to casts
    NOTHING_INCOMING,   // no control cast on its way to the shaman or an ally
    NOT_FOR_ME,         // a good bot protects only itself
    TOO_LATE,           // the cast ends before the totem can take it
    GROUND
};

// The totem needs this long before the cast lands; the cast is read after the tier's reaction time already (BotSkill), so the
// numbers are what is left of the cast.
constexpr std::uint32_t GROUND_MIN_LEFT_EXPERT_MS = 350;
constexpr std::uint32_t GROUND_MIN_LEFT_GOOD_MS = 550;

struct GroundInputs
{
    bool ready = false;                 // known, off cooldown
    bool incomingControl = false;       // a hostile player's control cast is on its way
    bool atShaman = false;              // ... at the shaman (else at an ally within the totem's reach)
    std::uint32_t leftMs = 0;           // what is left of the cast
};

inline GroundReason Grounding(GroundInputs const& in, Tier tier)
{
    if (!in.ready)
        return GroundReason::NOT_READY;
    if (tier == Tier::CASUAL)
        return GroundReason::TIER_SKIPS;
    if (!in.incomingControl)
        return GroundReason::NOTHING_INCOMING;
    if (tier == Tier::GOOD && !in.atShaman)
        return GroundReason::NOT_FOR_ME;
    std::uint32_t const minLeft = tier == Tier::EXPERT ? GROUND_MIN_LEFT_EXPERT_MS : GROUND_MIN_LEFT_GOOD_MS;
    return in.leftMs >= minLeft ? GroundReason::GROUND : GroundReason::TOO_LATE;
}

// Call of the Elements gives the totems back (Grounding, Windwalk, Capacitor ...): pressed when the Grounding is spent and another
// control cast is on its way, an expert only.
inline bool CallOfTheElementsNow(bool groundingSpent, bool callReady, bool incomingControl, std::uint32_t leftMs, Tier tier)
{
    return tier == Tier::EXPERT && groundingSpent && callReady && incomingControl && leftMs >= 2 * GROUND_MIN_LEFT_EXPERT_MS;
}

// ---------------------------------------------------------------------------------------------------------------------
// 3. The burst held for the window
// ---------------------------------------------------------------------------------------------------------------------
enum class BurstReason : std::uint8_t
{
    NOT_A_PLAYER,       // the PvE rotation casts them as before
    CASUAL_FREE,
    WINDOW,
    HOLD
};

inline bool BurstAllowed(BurstReason reason)
{
    return reason != BurstReason::HOLD;
}

inline BurstReason Burst(bool targetIsPlayer, bool windowOpen, Tier tier)
{
    if (!targetIsPlayer)
        return BurstReason::NOT_A_PLAYER;
    if (tier == Tier::CASUAL)
        return BurstReason::CASUAL_FREE;
    return windowOpen ? BurstReason::WINDOW : BurstReason::HOLD;
}

// ---------------------------------------------------------------------------------------------------------------------
// 4. Spiritwalker's Grace
// ---------------------------------------------------------------------------------------------------------------------
enum class GraceReason : std::uint8_t
{
    NO,
    NOT_READY,          // not learned (level 87), on cooldown or up
    TIER_SKIPS,
    NOT_MOVING,
    NO_PRESSURE,        // nobody to run from
    PRESS
};

constexpr float GRACE_EXPERT_YD = 30.0f;
constexpr float GRACE_GOOD_YD = 15.0f;

struct GraceInputs
{
    bool ready = false;                 // known, off cooldown, not up
    bool moving = false;
    float nearestEnemyPlayerYd = 999.0f;
    bool hasTargetInReach = false;      // something to cast at
};

inline GraceReason SpiritwalkersGrace(GraceInputs const& in, Tier tier)
{
    if (!in.ready)
        return GraceReason::NOT_READY;
    if (tier == Tier::CASUAL)
        return GraceReason::TIER_SKIPS;
    if (!in.moving)
        return GraceReason::NOT_MOVING;
    float const reach = tier == Tier::EXPERT ? GRACE_EXPERT_YD : GRACE_GOOD_YD;
    if (!in.hasTargetInReach || in.nearestEnemyPlayerYd > reach)
        return GraceReason::NO_PRESSURE;
    return GraceReason::PRESS;
}
}  // namespace PvpElemental

#endif
