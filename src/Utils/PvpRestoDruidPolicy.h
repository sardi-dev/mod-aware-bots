/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_PVPRESTODRUIDPOLICY_H
#define _PLAYERBOT_PVPRESTODRUIDPOLICY_H

// BT-668 to BT-672: the Restoration Druid package of the PvP class rules (design decision 0032, owner decision 2026-11-19, rule R19 and the
// external defenses of R3 of docs/pvp). The shared layers decide the healer control chain (Cyclone on the enemy healer is a row
// of the kit), the defensives of the druid itself and the interrupts; this file adds what a Restoration Druid does for its team
// inside them. Four of the six picks of the ladder's set (Cenarion Ward, Displacer Beast, Typhoon, the Cyclone glyph) were idle
// in a fight against players:
//   1. Cenarion Ward on the ally the enemy team is working on, and in a silence (it works while silenced).
//   2. Ironbark early on that ally when an enemy burst cooldown is up (the first reaction to a go, used high on health).
//   3. Nature's Swiftness then Cyclone on a bursting enemy damage dealer (an instant Cyclone that cannot be kicked).
//   4. Displacer Beast as the escape from a melee player that has connected, and Typhoon as the peel when it is not ready.
//
// Every decision is a candidate with a reason and a deterministic integer score (design decision 0014 and 0026). Tier gating (fundamentals
// sheet 14): an expert plays all of it, a good bot the simple half, a casual bot keeps the older chains (the main tank rules).
//
// Free of game types, so tests/ can run it (tests/test_pvp_resto_druid.cpp).

#include "BotSkillMath.h"

#include <cstdint>

namespace PvpRestoDruid
{
using Tier = BotSkillMath::Tier;

constexpr std::uint32_t SPELL_CENARION_WARD = 102351;
constexpr std::uint32_t SPELL_IRONBARK = 102342;
constexpr std::uint32_t SPELL_NATURES_SWIFTNESS = 132158;
constexpr std::uint32_t SPELL_CYCLONE = 33786;
constexpr std::uint32_t SPELL_DISPLACER_BEAST = 102280;
constexpr std::uint32_t SPELL_TYPHOON = 132469;

constexpr float HEAL_RANGE_YD = 40.0f;
// The melee reach in which a player counts as on the druid.
constexpr float MELEE_REACH_YD = 10.0f;
constexpr float CYCLONE_RANGE_YD = 35.0f;

// ---------------------------------------------------------------------------------------------------------------------
// 1. Cenarion Ward
// ---------------------------------------------------------------------------------------------------------------------
enum class WardReason : std::uint8_t
{
    NO,
    NOT_READY,          // not learned, on cooldown, or the ally already has it
    CASUAL_SKIPS,       // a casual bot keeps the main tank rule of the talent policy
    NOT_FOCUSED,        // no player is hitting the ally and the druid is not silenced
    TOO_HEALTHY,
    ON_FOCUS,           // the ally is under attack by a player
    WHILE_SILENCED      // the heals are shut, the ward is not
};

constexpr float WARD_EXPERT_HP_PERCENT = 95.0f;
constexpr float WARD_GOOD_HP_PERCENT = 80.0f;
constexpr float WARD_SILENCED_HP_PERCENT = 90.0f;
constexpr std::int32_t SCORE_WARD_BASE = 60;
constexpr std::int32_t SCORE_WARD_SILENCED = 15;
constexpr std::int32_t SCORE_WARD_PER_ATTACKER = 5;
constexpr std::int32_t ATTACKERS_COUNTED = 3;

struct WardInputs
{
    bool ready = false;                  // known, off cooldown
    bool allyHasWard = false;
    float allyHealthPercent = 100.0f;
    std::int32_t playerAttackers = 0;
    bool druidSilenced = false;
};

struct WardVerdict
{
    bool cast = false;
    WardReason reason = WardReason::NO;
    std::int32_t score = 0;
};

inline std::int32_t CountedAttackers(std::int32_t attackers)
{
    return attackers > ATTACKERS_COUNTED ? ATTACKERS_COUNTED : attackers;
}

inline WardVerdict CenarionWard(WardInputs const& in, Tier tier)
{
    if (!in.ready || in.allyHasWard)
        return { false, WardReason::NOT_READY, 0 };
    if (tier == Tier::CASUAL)
        return { false, WardReason::CASUAL_SKIPS, 0 };
    bool const silencedCast = in.druidSilenced && in.allyHealthPercent <= WARD_SILENCED_HP_PERCENT;
    if (in.playerAttackers <= 0 && !silencedCast)
        return { false, WardReason::NOT_FOCUSED, 0 };
    float const line = tier == Tier::EXPERT ? WARD_EXPERT_HP_PERCENT : WARD_GOOD_HP_PERCENT;
    if (in.playerAttackers > 0 && in.allyHealthPercent <= line)
    {
        std::int32_t const score = SCORE_WARD_BASE + std::int32_t(100.0f - in.allyHealthPercent) +
            CountedAttackers(in.playerAttackers) * SCORE_WARD_PER_ATTACKER + (in.druidSilenced ? SCORE_WARD_SILENCED : 0);
        return { true, WardReason::ON_FOCUS, score };
    }
    if (silencedCast)
        return { true, WardReason::WHILE_SILENCED, SCORE_WARD_BASE + std::int32_t(100.0f - in.allyHealthPercent) + SCORE_WARD_SILENCED };
    return { false, WardReason::TOO_HEALTHY, 0 };
}

// ---------------------------------------------------------------------------------------------------------------------
// 2. Ironbark early
// ---------------------------------------------------------------------------------------------------------------------
enum class IronbarkReason : std::uint8_t
{
    NO,
    NOT_READY,
    CASUAL_SKIPS,       // the main tank edges and BT-293 stand for a casual bot
    NOT_FOCUSED,
    TOO_HEALTHY,
    BURST_SEEN,         // an enemy burst cooldown is up on a player hitting the ally
    UNDER_FIRE
};

constexpr float IRONBARK_BURST_HP_PERCENT = 80.0f;
constexpr float IRONBARK_EXPERT_HP_PERCENT = 55.0f;
constexpr float IRONBARK_GOOD_HP_PERCENT = 45.0f;
constexpr std::int32_t SCORE_IRONBARK_BASE = 50;
constexpr std::int32_t SCORE_IRONBARK_BURST = 20;

struct IronbarkInputs
{
    bool ready = false;
    bool allyHasIronbark = false;
    float allyHealthPercent = 100.0f;
    std::int32_t playerAttackers = 0;
    bool enemyBurstSeen = false;
};

struct IronbarkVerdict
{
    bool cast = false;
    IronbarkReason reason = IronbarkReason::NO;
    std::int32_t score = 0;
};

inline IronbarkVerdict Ironbark(IronbarkInputs const& in, Tier tier)
{
    if (!in.ready || in.allyHasIronbark)
        return { false, IronbarkReason::NOT_READY, 0 };
    if (tier == Tier::CASUAL)
        return { false, IronbarkReason::CASUAL_SKIPS, 0 };
    if (in.playerAttackers <= 0)
        return { false, IronbarkReason::NOT_FOCUSED, 0 };
    bool const burst = tier == Tier::EXPERT && in.enemyBurstSeen && in.allyHealthPercent <= IRONBARK_BURST_HP_PERCENT;
    float const line = tier == Tier::EXPERT ? IRONBARK_EXPERT_HP_PERCENT : IRONBARK_GOOD_HP_PERCENT;
    if (!burst && in.allyHealthPercent > line)
        return { false, IronbarkReason::TOO_HEALTHY, 0 };
    std::int32_t const score = SCORE_IRONBARK_BASE + std::int32_t(100.0f - in.allyHealthPercent) +
        (burst ? SCORE_IRONBARK_BURST : 0) + CountedAttackers(in.playerAttackers) * SCORE_WARD_PER_ATTACKER;
    return { true, burst ? IronbarkReason::BURST_SEEN : IronbarkReason::UNDER_FIRE, score };
}

// ---------------------------------------------------------------------------------------------------------------------
// 3. Nature's Swiftness Cyclone on a bursting enemy damage dealer
// ---------------------------------------------------------------------------------------------------------------------
enum class CycloneReason : std::uint8_t
{
    NO,
    NOT_KNOWN,
    TIER_SKIPS,         // only an expert plays the instant Cyclone
    NOT_A_PLAYER,
    IS_HEALER,          // the healer control chain of the shared kit casts that Cyclone
    NOT_BURSTING,
    NO_THREAT,          // the enemy is not hitting an ally
    ALREADY_HELD,       // already in a control or cycloned
    DR_SPENT,           // the diminishing returns on Cyclone are at immune
    HARD_CAST_KICKED,   // no Nature's Swiftness and a kick may stop the cast
    SWIFTNESS_FIRST,    // press Nature's Swiftness, then Cyclone
    INSTANT,            // Nature's Swiftness is up: Cyclone now
    HARD_CAST           // no Nature's Swiftness, no kick to fear: a hard cast
};

constexpr std::int32_t SCORE_CYCLONE_BASE = 70;
constexpr std::int32_t SCORE_CYCLONE_INSTANT = 15;
constexpr std::int32_t SCORE_CYCLONE_PER_ATTACKER = 5;
constexpr std::uint32_t DR_IMMUNE_STEP = 3;

struct CycloneInputs
{
    bool known = false;
    bool targetIsPlayer = false;
    bool targetIsHealer = false;
    bool targetBursting = false;        // a burst aura on it
    std::int32_t allyHitByTarget = 0;   // allies it is hitting (0 or 1 in practice)
    bool targetHeld = false;            // hard control or already cycloned
    std::uint32_t drStep = 0;           // the druid's own reading of the Cyclone category
    bool swiftnessReady = false;
    bool swiftnessUp = false;
    bool enemyKickNotSpent = false;     // the druid's own book
    float targetDistance = 0.0f;
};

struct CycloneVerdict
{
    bool cast = false;               // Cyclone itself, now
    bool pressSwiftness = false;     // Nature's Swiftness first
    CycloneReason reason = CycloneReason::NO;
    std::int32_t score = 0;
};

inline CycloneVerdict BurstCyclone(CycloneInputs const& in, Tier tier)
{
    if (!in.known)
        return { false, false, CycloneReason::NOT_KNOWN, 0 };
    if (tier != Tier::EXPERT)
        return { false, false, CycloneReason::TIER_SKIPS, 0 };
    if (!in.targetIsPlayer || in.targetDistance > CYCLONE_RANGE_YD)
        return { false, false, CycloneReason::NOT_A_PLAYER, 0 };
    if (in.targetIsHealer)
        return { false, false, CycloneReason::IS_HEALER, 0 };
    if (!in.targetBursting)
        return { false, false, CycloneReason::NOT_BURSTING, 0 };
    if (in.allyHitByTarget <= 0)
        return { false, false, CycloneReason::NO_THREAT, 0 };
    if (in.targetHeld)
        return { false, false, CycloneReason::ALREADY_HELD, 0 };
    if (in.drStep >= DR_IMMUNE_STEP)
        return { false, false, CycloneReason::DR_SPENT, 0 };

    std::int32_t const base = SCORE_CYCLONE_BASE + in.allyHitByTarget * SCORE_CYCLONE_PER_ATTACKER;
    if (in.swiftnessUp)
        return { true, false, CycloneReason::INSTANT, base + SCORE_CYCLONE_INSTANT };
    if (in.swiftnessReady)
        return { false, true, CycloneReason::SWIFTNESS_FIRST, base + SCORE_CYCLONE_INSTANT };
    if (in.enemyKickNotSpent)
        return { false, false, CycloneReason::HARD_CAST_KICKED, 0 };
    return { true, false, CycloneReason::HARD_CAST, base };
}

// ---------------------------------------------------------------------------------------------------------------------
// 4. Displacer Beast and Typhoon against a melee player that has connected
// ---------------------------------------------------------------------------------------------------------------------
enum class EscapeReason : std::uint8_t
{
    NO,
    NOT_PVP,
    NO_MELEE,           // nobody on the druid
    DISPLACER,
    TYPHOON,
    TEAM_CONTROL        // a teammate holds an enemy near the druid in a breakable control: Typhoon would free it
};

constexpr float ESCAPE_EXPERT_HP_PERCENT = 60.0f;
constexpr float ESCAPE_GOOD_HP_PERCENT = 45.0f;
constexpr float ESCAPE_CASUAL_HP_PERCENT = 30.0f;
constexpr float PEEL_EXPERT_HP_PERCENT = 75.0f;
constexpr std::int32_t SCORE_ESCAPE_BASE = 60;

struct EscapeInputs
{
    bool inPvpFight = false;
    std::uint32_t meleeAttackers = 0;       // melee players within the reach that are hitting the druid
    float healthPercent = 100.0f;
    bool displacerReady = false;            // known, off cooldown
    bool typhoonReady = false;
    std::uint32_t breakableHeldNear = 0;    // enemies a teammate holds in a breakable control near the druid
};

struct EscapeVerdict
{
    EscapeReason reason = EscapeReason::NO;
    std::int32_t score = 0;
};

inline EscapeVerdict MeleeEscape(EscapeInputs const& in, Tier tier)
{
    if (!in.inPvpFight)
        return { EscapeReason::NOT_PVP, 0 };
    if (in.meleeAttackers == 0)
        return { EscapeReason::NO_MELEE, 0 };
    float const line = tier == Tier::EXPERT ? ESCAPE_EXPERT_HP_PERCENT : tier == Tier::GOOD ? ESCAPE_GOOD_HP_PERCENT : ESCAPE_CASUAL_HP_PERCENT;
    std::int32_t const score = SCORE_ESCAPE_BASE + std::int32_t(100.0f - in.healthPercent) + std::int32_t(in.meleeAttackers) * 5;
    // The escape is spent after the melee has connected (it is hitting the druid) and the health says it matters; a trade a
    // human makes late, never at the first touch.
    if (in.displacerReady && in.healthPercent <= line)
        return { EscapeReason::DISPLACER, score };
    // Typhoon is the peel while the escape is not there; a good or casual bot has no peel plan.
    if (tier == Tier::EXPERT && in.typhoonReady && in.healthPercent <= PEEL_EXPERT_HP_PERCENT)
    {
        if (in.breakableHeldNear > 0)
            return { EscapeReason::TEAM_CONTROL, 0 };
        return { EscapeReason::TYPHOON, score - 10 };
    }
    return { EscapeReason::NO, 0 };
}
}  // namespace PvpRestoDruid

#endif
