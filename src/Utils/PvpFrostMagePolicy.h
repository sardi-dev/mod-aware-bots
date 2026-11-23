/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_PVPFROSTMAGEPOLICY_H
#define _PLAYERBOT_PVPFROSTMAGEPOLICY_H

// BT-650 to BT-655: the Frost Mage package of the PvP class rules (design decision 0032, owner decision 2026-11-19, rule R15 of docs/pvp).
// The shared layers already decide the healer lock chain (PvpTeamPolicy.h), the burst window (PvpBurstPolicy.h) and the defensives;
// this file adds what only a Frost Mage does inside them:
//   1. Alter Time as the go of the burst: pressed inside the window with fuel on the bar (Icy Veins, Fingers of Frost, Brain Freeze),
//      never while Hypothermia is on the mage (the rewind would bring it back).
//   2. Frozen Orb is a damage cooldown of the window against a player, not a filler on cooldown, and not into a target that a
//      sheep holds (the orb breaks it).
//   3. The Water Elemental's Freeze goes before Deep Freeze (Deep Freeze needs a frozen target) and banks a Fingers of Frost charge.
//   4. Deep Freeze is kept for the enemy healer while a healer is near and free (the lock chain casts it); the kill target gets it
//      once the healer is held.
//   5. Presence of Mind is spent on an instant control (the same route as Fire, PvpBurstPolicy.h).
//   6. Incanter's Ward (the ladder's tier 90 pick, cast by nobody before) goes up before the burst and when a player hits the mage.
// Ring of Frost as an armed chain link lives in the shared control kit (PvpTeamPolicy.h, armMs).
//
// Every decision returns a reason and a deterministic integer score, so a later selector may pick among them (design decision 0014 and 0026).
// Tier gating (fundamentals sheet 14): expert uses all of it, good bots the simple half, casual bots keep the old rotation.
//
// Free of game types, so tests/ can run it (tests/test_pvp_frost_mage.cpp).

#include "BotSkillMath.h"
#include "PvpTeamPolicy.h"

#include <cstdint>

namespace PvpFrostMage
{
using Tier = BotSkillMath::Tier;

constexpr std::uint32_t SPELL_ALTER_TIME = 108978;
constexpr std::uint32_t SPELL_HYPOTHERMIA = 41425;
constexpr std::uint32_t SPELL_FROZEN_ORB = 84714;
constexpr std::uint32_t SPELL_DEEP_FREEZE = 44572;
constexpr std::uint32_t SPELL_PET_FREEZE = 33395;   // the Water Elemental's ground freeze (root, banks a Fingers of Frost charge)
constexpr std::uint32_t SPELL_RING_OF_FROST = 113724;
constexpr std::uint32_t SPELL_INCANTERS_WARD = 1463;

constexpr std::uint8_t ALTER_TIME_LEVEL = 87;
// Fingers of Frost holds at most two charges; a third proc is lost.
constexpr std::uint32_t FINGERS_CAP = 2;
// The Water Elemental casts Freeze within this of the target.
constexpr float PET_FREEZE_RANGE_YD = 40.0f;

// ---------------------------------------------------------------------------------------------------------------------
// 1. Alter Time in the burst
// ---------------------------------------------------------------------------------------------------------------------
enum class AlterReason : std::uint8_t
{
    NONE,
    NOT_KNOWN,        // below level 87 or the spell is not learned
    CASUAL_SKIPS,     // a casual bot does not plan a rewind
    WINDOW_CLOSED,    // the burst window on the target is not open
    ALREADY_UP,       // the rewind is running
    HYPOTHERMIA,      // Hypothermia on the mage: the rewind would restore it (no Ice Block for 6 s more)
    NO_FUEL,          // nothing on the bar worth doubling
    GO
};

constexpr std::int32_t SCORE_ALTER_BASE = 70;
constexpr std::int32_t SCORE_ALTER_ICY_VEINS = 10;
constexpr std::int32_t SCORE_ALTER_PER_FINGER = 8;
constexpr std::int32_t SCORE_ALTER_BRAIN_FREEZE = 6;

struct AlterInputs
{
    bool knows = false;
    bool windowOpen = false;        // PvpBurst verdict on the kill target
    bool alterTimeUp = false;
    bool hypothermia = false;
    bool icyVeinsUp = false;
    std::uint32_t fingers = 0;
    bool brainFreeze = false;
};

struct AlterVerdict
{
    bool cast = false;
    AlterReason reason = AlterReason::NONE;
    std::int32_t score = 0;
};

inline char const* AlterReasonName(AlterReason reason)
{
    switch (reason)
    {
        case AlterReason::NOT_KNOWN: return "not-known";
        case AlterReason::CASUAL_SKIPS: return "casual-skips";
        case AlterReason::WINDOW_CLOSED: return "window-closed";
        case AlterReason::ALREADY_UP: return "already-up";
        case AlterReason::HYPOTHERMIA: return "hypothermia";
        case AlterReason::NO_FUEL: return "no-fuel";
        case AlterReason::GO: return "go";
        default: return "none";
    }
}

// An expert wants the bar loaded (Icy Veins and a charge or a proc to dump); a good bot presses it with Icy Veins or a charge.
inline AlterVerdict AlterTime(AlterInputs const& in, std::uint8_t level, Tier tier)
{
    if (!in.knows || level < ALTER_TIME_LEVEL)
        return { false, AlterReason::NOT_KNOWN, 0 };
    if (tier == Tier::CASUAL)
        return { false, AlterReason::CASUAL_SKIPS, 0 };
    if (!in.windowOpen)
        return { false, AlterReason::WINDOW_CLOSED, 0 };
    if (in.alterTimeUp)
        return { false, AlterReason::ALREADY_UP, 0 };
    if (in.hypothermia)
        return { false, AlterReason::HYPOTHERMIA, 0 };

    bool const procs = in.fingers >= 1 || in.brainFreeze;
    bool const loaded = tier == Tier::EXPERT ? (in.icyVeinsUp && procs) : (in.icyVeinsUp || procs);
    if (!loaded)
        return { false, AlterReason::NO_FUEL, 0 };

    std::int32_t score = SCORE_ALTER_BASE + (in.icyVeinsUp ? SCORE_ALTER_ICY_VEINS : 0) +
        std::int32_t(in.fingers > FINGERS_CAP ? FINGERS_CAP : in.fingers) * SCORE_ALTER_PER_FINGER +
        (in.brainFreeze ? SCORE_ALTER_BRAIN_FREEZE : 0);
    return { true, AlterReason::GO, score };
}

// ---------------------------------------------------------------------------------------------------------------------
// 2. Frozen Orb as a window cooldown
// ---------------------------------------------------------------------------------------------------------------------
enum class OrbReason : std::uint8_t
{
    NOT_A_PLAYER,     // not this file's business: the PvE rotation casts it on cooldown
    CASUAL_FILLER,    // a casual bot keeps casting it on cooldown
    WINDOW,           // the window is open: spend it
    HOLD_FOR_WINDOW,  // good and expert bots wait for the go
    HOLD_FOR_SHEEP    // the orb would break a polymorph on the target
};

struct OrbInputs
{
    bool targetIsPlayer = false;
    bool windowOpen = false;
    bool targetBreakableCc = false;   // sheep, fear, sap ... on the target
};

inline bool OrbAllowed(OrbReason reason)
{
    return reason == OrbReason::NOT_A_PLAYER || reason == OrbReason::CASUAL_FILLER || reason == OrbReason::WINDOW;
}

inline OrbReason FrozenOrb(OrbInputs const& in, Tier tier)
{
    if (!in.targetIsPlayer)
        return OrbReason::NOT_A_PLAYER;
    if (tier == Tier::CASUAL)
        return OrbReason::CASUAL_FILLER;
    if (in.targetBreakableCc)
        return OrbReason::HOLD_FOR_SHEEP;
    return in.windowOpen ? OrbReason::WINDOW : OrbReason::HOLD_FOR_WINDOW;
}

// ---------------------------------------------------------------------------------------------------------------------
// 3. Water Elemental Freeze before Deep Freeze
// ---------------------------------------------------------------------------------------------------------------------
enum class FreezeReason : std::uint8_t
{
    NONE,
    CASUAL_SKIPS,        // a casual bot does not steer the pet
    NO_PET,              // no elemental, dead, or it does not know Freeze
    NOT_READY,           // on cooldown
    NOT_A_PLAYER,
    OUT_OF_RANGE,
    TARGET_HELD,         // already stunned, rooted or in a breakable control: a root would waste it (or break a sheep)
    DEEP_FREEZE_SETUP,   // Deep Freeze is ready and needs a frozen target
    FINGERS_BANK,        // a charge of Fingers of Frost is free to bank
    NOT_NEEDED
};

constexpr std::int32_t SCORE_FREEZE_SETUP = 80;
constexpr std::int32_t SCORE_FREEZE_BANK = 55;

struct FreezeInputs
{
    bool petReady = false;          // alive, knows Freeze, not on cooldown
    bool petPresent = false;
    bool targetIsPlayer = false;
    float targetDistance = 0.0f;    // from the pet
    bool targetHeld = false;        // stunned, rooted or breakable control on the target
    bool deepFreezeReady = false;   // known, ready (the bot's own spell book)
    std::uint32_t fingers = 0;
    bool windowOpen = false;
};

struct FreezeVerdict
{
    bool cast = false;
    FreezeReason reason = FreezeReason::NONE;
    std::int32_t score = 0;
};

// An expert sets up Deep Freeze, and banks a charge when the window is open; a good bot freezes a free player on cooldown while
// the bar has room, so the steer is rough but present.
inline FreezeVerdict PetFreeze(FreezeInputs const& in, Tier tier)
{
    if (tier == Tier::CASUAL)
        return { false, FreezeReason::CASUAL_SKIPS, 0 };
    if (!in.petPresent)
        return { false, FreezeReason::NO_PET, 0 };
    if (!in.petReady)
        return { false, FreezeReason::NOT_READY, 0 };
    if (!in.targetIsPlayer)
        return { false, FreezeReason::NOT_A_PLAYER, 0 };
    if (in.targetDistance > PET_FREEZE_RANGE_YD)
        return { false, FreezeReason::OUT_OF_RANGE, 0 };
    if (in.targetHeld)
        return { false, FreezeReason::TARGET_HELD, 0 };
    if (in.deepFreezeReady)
        return { true, FreezeReason::DEEP_FREEZE_SETUP, SCORE_FREEZE_SETUP };
    bool const room = in.fingers < FINGERS_CAP;
    if (room && (tier == Tier::GOOD || in.windowOpen))
        return { true, FreezeReason::FINGERS_BANK, SCORE_FREEZE_BANK };
    return { false, FreezeReason::NOT_NEEDED, 0 };
}

// ---------------------------------------------------------------------------------------------------------------------
// 4. Deep Freeze kept for the healer
// ---------------------------------------------------------------------------------------------------------------------
enum class DeepFreezeReason : std::uint8_t
{
    FREE,               // no reason to keep it
    CASUAL_FREE,        // a casual bot casts it as before
    TARGET_IS_HEALER,   // the healer is the target: that is the lock
    HEALER_HELD,        // the healer is already held: the kill target may take the stun
    NO_HEALER,          // nobody to keep it for
    KEEP_FOR_HEALER
};

struct DeepFreezeInputs
{
    bool teamContext = false;       // a battleground, an arena or a skirmish with the team play on
    bool healerPresent = false;
    bool targetIsHealer = false;
    std::uint32_t healerLockedMs = 0;
};

inline bool DeepFreezeAllowed(DeepFreezeReason reason)
{
    return reason != DeepFreezeReason::KEEP_FOR_HEALER;
}

inline DeepFreezeReason DeepFreezeUse(DeepFreezeInputs const& in, Tier tier)
{
    if (!in.teamContext)
        return DeepFreezeReason::FREE;
    if (tier == Tier::CASUAL)
        return DeepFreezeReason::CASUAL_FREE;
    if (!in.healerPresent)
        return DeepFreezeReason::NO_HEALER;
    if (in.targetIsHealer)
        return DeepFreezeReason::TARGET_IS_HEALER;
    if (PvpTeam::HealerHeld(in.healerLockedMs))
        return DeepFreezeReason::HEALER_HELD;
    return DeepFreezeReason::KEEP_FOR_HEALER;
}

// ---------------------------------------------------------------------------------------------------------------------
// 6. Incanter's Ward before the go
// ---------------------------------------------------------------------------------------------------------------------
enum class WardReason : std::uint8_t
{
    NO,
    NOT_READY,          // not learned, on cooldown or already up
    NOT_A_FIGHT,        // the target is not an enemy player
    BEFORE_THE_GO,      // the window is open: the shield breaks into spell damage during the dump
    UNDER_ATTACK,       // a player is hitting the mage and the health is below the line
    CASUAL_ATTACKED     // a casual bot shields when it is attacked, whatever its health
};

// The ward is up with this much health or less when a player attacks the mage (a full health mage has no need yet).
constexpr float WARD_ATTACKED_HP_PERCENT = 90.0f;

struct WardInputs
{
    bool knows = false;
    bool ready = false;             // off cooldown
    bool up = false;
    bool targetIsPlayer = false;
    bool windowOpen = false;
    bool attackedByPlayer = false;
    float healthPercent = 100.0f;
};

inline WardReason IncantersWard(WardInputs const& in, Tier tier)
{
    if (!in.knows || !in.ready || in.up)
        return WardReason::NOT_READY;
    if (!in.targetIsPlayer)
        return WardReason::NOT_A_FIGHT;
    if (tier == Tier::CASUAL)
        return in.attackedByPlayer ? WardReason::CASUAL_ATTACKED : WardReason::NO;
    if (in.windowOpen)
        return WardReason::BEFORE_THE_GO;
    if (in.attackedByPlayer && in.healthPercent <= WARD_ATTACKED_HP_PERCENT)
        return WardReason::UNDER_ATTACK;
    return WardReason::NO;
}

inline bool WardDue(WardReason reason)
{
    return reason == WardReason::BEFORE_THE_GO || reason == WardReason::UNDER_ATTACK || reason == WardReason::CASUAL_ATTACKED;
}
}  // namespace PvpFrostMage

#endif
