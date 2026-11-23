/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_PVPMISTWEAVERPOLICY_H
#define _PLAYERBOT_PVPMISTWEAVERPOLICY_H

// BT-657 to BT-662: the Mistweaver Monk package of the PvP class rules (design decision 0032, owner decision 2026-11-19, rule R22 and the
// healing part of R3 and R9 of docs/pvp). The shared layers decide interrupts, the defensives of the bot itself and the control
// chain; this file adds what a Mistweaver does for its team inside them:
//   1. Healing Sphere as a spot heal under a hurt ally (the ladder's Glyph of Enduring Healing Sphere had no rule before).
//   2. Mana Tea while the heals are locked out, and in quiet moments (it works while silenced).
//   3. Life Cocoon as a tempo tool at about half health on the ally the enemy team is working on, not at the critical line.
//   4. Thunder Focus Tea before a Surging Mist when an ally is in danger and an enemy kick is not spent (the instant heal cannot be
//      kicked).
//   5. Tiger's Lust on a rooted or slowed ally or self in a PvP fight (the ladder's tier 15 pick had no Mistweaver rule).
// The cheap control break (Nimble Brew before the medallion) is the shared break layer: its catalog row is added for every monk.
//
// Every decision is a candidate with a reason and a deterministic integer score (design decision 0014 and 0026). Tier gating (fundamentals
// sheet 14): an expert plays all of it, a good bot the simple half, a casual bot keeps the older healing chains (the group heals,
// the critical heals and the Life Cocoon of BT-135).
//
// Free of game types, so tests/ can run it (tests/test_pvp_mistweaver.cpp).

#include "BotSkillMath.h"

#include <cstdint>

namespace PvpMistweaver
{
using Tier = BotSkillMath::Tier;

constexpr std::uint32_t SPELL_HEALING_SPHERE = 115460;
constexpr std::uint32_t SPELL_MANA_TEA = 115294;
constexpr std::uint32_t AURA_MANA_TEA_STACKS = 115867;
constexpr std::uint32_t SPELL_LIFE_COCOON = 116849;
constexpr std::uint32_t SPELL_THUNDER_FOCUS_TEA = 116680;
constexpr std::uint32_t SPELL_TIGERS_LUST = 116841;
constexpr std::uint32_t SPELL_SURGING_MIST = 116694;

// Healing Sphere is a ground cast: the heal goes to whoever steps into it within the minute. Range as the spell's.
constexpr float SPHERE_RANGE_YD = 40.0f;
// At most three spheres stand; casting a fourth pops the oldest. The expert keeps one slot free.
constexpr std::int32_t SPHERES_MAX = 3;

// ---------------------------------------------------------------------------------------------------------------------
// 1. Healing Sphere under a hurt ally
// ---------------------------------------------------------------------------------------------------------------------
enum class SphereReason : std::uint8_t
{
    NO,
    NOT_KNOWN,
    CASUAL_SKIPS,       // a casual bot does not place ground heals
    OUT_OF_RANGE,
    TOO_HEALTHY,        // above the spot heal line
    TOO_HURT,           // the critical chain (Surging, Enveloping Mist) takes it
    TOO_MANY_SPHERES,
    SPOT_HEAL
};

// The spot heal line: a sphere tops an ally off; below the critical line the direct heals take over.
constexpr float SPHERE_LINE_EXPERT_PCT = 80.0f;
constexpr float SPHERE_LINE_GOOD_PCT = 70.0f;
constexpr float SPHERE_CRITICAL_PCT = 30.0f;
constexpr std::int32_t SCORE_SPHERE_BASE = 40;
constexpr std::int32_t SCORE_SPHERE_ATTACKED = 15;

struct SphereInputs
{
    bool known = false;
    float allyHealthPercent = 100.0f;
    float allyDistance = 0.0f;
    std::int32_t spheresOut = 0;
    bool allyAttackedByPlayer = false;   // the sphere is worth more under an ally the enemy team is hitting
};

struct SphereVerdict
{
    bool cast = false;
    SphereReason reason = SphereReason::NO;
    std::int32_t score = 0;
};

inline SphereVerdict HealingSphere(SphereInputs const& in, Tier tier)
{
    if (!in.known)
        return { false, SphereReason::NOT_KNOWN, 0 };
    if (tier == Tier::CASUAL)
        return { false, SphereReason::CASUAL_SKIPS, 0 };
    if (in.allyDistance > SPHERE_RANGE_YD)
        return { false, SphereReason::OUT_OF_RANGE, 0 };
    float const line = tier == Tier::EXPERT ? SPHERE_LINE_EXPERT_PCT : SPHERE_LINE_GOOD_PCT;
    if (in.allyHealthPercent > line)
        return { false, SphereReason::TOO_HEALTHY, 0 };
    if (in.allyHealthPercent <= SPHERE_CRITICAL_PCT)
        return { false, SphereReason::TOO_HURT, 0 };
    std::int32_t const room = tier == Tier::EXPERT ? SPHERES_MAX - 1 : SPHERES_MAX;
    if (in.spheresOut >= room)
        return { false, SphereReason::TOO_MANY_SPHERES, 0 };
    std::int32_t const score = SCORE_SPHERE_BASE + std::int32_t(100.0f - in.allyHealthPercent) +
        (in.allyAttackedByPlayer ? SCORE_SPHERE_ATTACKED : 0);
    return { true, SphereReason::SPOT_HEAL, score };
}

// ---------------------------------------------------------------------------------------------------------------------
// 2. Mana Tea
// ---------------------------------------------------------------------------------------------------------------------
enum class TeaReason : std::uint8_t
{
    NO,
    NO_STACKS,       // the spell consumes up to two stacks: with fewer there is nothing worth a global cooldown
    CASUAL_OLD_RULE, // a casual bot keeps the mana edge of BT-135
    LOCKOUT,         // silenced: the heals are shut, the tea is not
    QUIET            // nobody needs healing and the mana is below the tier's line
};

constexpr std::uint32_t TEA_MIN_STACKS = 2;
constexpr float TEA_LOCKOUT_MANA_PCT = 90.0f;
constexpr float TEA_QUIET_EXPERT_PCT = 80.0f;
constexpr float TEA_QUIET_GOOD_PCT = 65.0f;
// "Nobody needs healing" means every ally is above this.
constexpr float TEA_ALLY_SAFE_PCT = 70.0f;

struct TeaInputs
{
    std::uint32_t stacks = 0;
    float manaPercent = 100.0f;
    bool silenced = false;
    float lowestAllyPercent = 100.0f;
    bool attackedByPlayer = false;     // a player is on the monk: the cast would not finish
};

inline TeaReason ManaTea(TeaInputs const& in, Tier tier)
{
    if (in.stacks < TEA_MIN_STACKS)
        return TeaReason::NO_STACKS;
    if (tier == Tier::CASUAL)
        return TeaReason::CASUAL_OLD_RULE;
    if (in.silenced && in.manaPercent <= TEA_LOCKOUT_MANA_PCT)
        return TeaReason::LOCKOUT;
    float const line = tier == Tier::EXPERT ? TEA_QUIET_EXPERT_PCT : TEA_QUIET_GOOD_PCT;
    if (!in.attackedByPlayer && in.lowestAllyPercent > TEA_ALLY_SAFE_PCT && in.manaPercent <= line)
        return TeaReason::QUIET;
    return TeaReason::NO;
}

inline bool TeaDue(TeaReason reason)
{
    return reason == TeaReason::LOCKOUT || reason == TeaReason::QUIET;
}

// ---------------------------------------------------------------------------------------------------------------------
// 3. Life Cocoon as a tempo tool
// ---------------------------------------------------------------------------------------------------------------------
enum class CocoonReason : std::uint8_t
{
    NO,
    NOT_READY,          // not learned, on cooldown, or the ally already has it
    CASUAL_SKIPS,       // the BT-135 emergency edge covers a casual bot
    NOT_FOCUSED,        // no player is hitting the ally
    TOO_HEALTHY,
    BURST_SEEN,         // an enemy burst cooldown is up on a player hitting the ally
    UNDER_FIRE          // the ally is at the tempo line and focused
};

// The expert cocoons at the tempo line when an enemy burst is seen, or lower without one; a good bot waits for the lower line.
constexpr float COCOON_TEMPO_PCT = 55.0f;
constexpr float COCOON_LOW_PCT = 45.0f;
constexpr float COCOON_GOOD_PCT = 40.0f;
constexpr std::int32_t SCORE_COCOON_BASE = 50;
constexpr std::int32_t SCORE_COCOON_BURST = 20;
constexpr std::int32_t SCORE_COCOON_PER_ATTACKER = 5;
constexpr std::int32_t COCOON_ATTACKERS_COUNTED = 3;

struct CocoonInputs
{
    bool ready = false;                  // known, off cooldown
    bool allyHasCocoon = false;
    float allyHealthPercent = 100.0f;
    std::int32_t playerAttackers = 0;    // players hitting the ally
    bool enemyBurstSeen = false;         // a burst aura on one of them
};

struct CocoonVerdict
{
    bool cast = false;
    CocoonReason reason = CocoonReason::NO;
    std::int32_t score = 0;
};

inline CocoonVerdict LifeCocoon(CocoonInputs const& in, Tier tier)
{
    if (!in.ready || in.allyHasCocoon)
        return { false, CocoonReason::NOT_READY, 0 };
    if (tier == Tier::CASUAL)
        return { false, CocoonReason::CASUAL_SKIPS, 0 };
    if (in.playerAttackers <= 0)
        return { false, CocoonReason::NOT_FOCUSED, 0 };

    bool const burst = tier == Tier::EXPERT && in.enemyBurstSeen && in.allyHealthPercent <= COCOON_TEMPO_PCT;
    float const line = tier == Tier::EXPERT ? COCOON_LOW_PCT : COCOON_GOOD_PCT;
    if (!burst && in.allyHealthPercent > line)
        return { false, CocoonReason::TOO_HEALTHY, 0 };

    std::int32_t const attackers = in.playerAttackers > COCOON_ATTACKERS_COUNTED ? COCOON_ATTACKERS_COUNTED : in.playerAttackers;
    std::int32_t const score = SCORE_COCOON_BASE + std::int32_t(100.0f - in.allyHealthPercent) + (burst ? SCORE_COCOON_BURST : 0) +
        attackers * SCORE_COCOON_PER_ATTACKER;
    return { true, burst ? CocoonReason::BURST_SEEN : CocoonReason::UNDER_FIRE, score };
}

// ---------------------------------------------------------------------------------------------------------------------
// 4. Thunder Focus Tea then Surging Mist
// ---------------------------------------------------------------------------------------------------------------------
enum class TftReason : std::uint8_t
{
    NO,
    NOT_READY,
    CASUAL_SKIPS,
    NOT_THREATENED,   // nobody is in danger
    NO_KICK_RISK,     // an expert waits for a kick to matter (the instant heal beats it)
    CAST_TEA,         // press the tea
    SURGE_NOW         // the tea is up: the instant Surging Mist goes to the ally in danger
};

constexpr float TFT_EXPERT_PCT = 60.0f;
constexpr float TFT_GOOD_PCT = 50.0f;
constexpr float TFT_SURGE_PCT = 75.0f;

struct TftInputs
{
    bool known = false;
    bool ready = false;                 // off cooldown
    bool teaUp = false;                 // the empowered Surging Mist is available
    float lowestAllyPercent = 100.0f;
    bool allyAttackedByPlayer = false;
    bool enemyKickNotSpent = false;     // the bot's own book: a kick that can stop a cast is not seen used
};

inline TftReason ThunderFocusTea(TftInputs const& in, Tier tier)
{
    if (in.teaUp && in.lowestAllyPercent <= TFT_SURGE_PCT)
        return TftReason::SURGE_NOW;
    if (!in.known || !in.ready || in.teaUp)
        return TftReason::NOT_READY;
    if (tier == Tier::CASUAL)
        return TftReason::CASUAL_SKIPS;
    float const line = tier == Tier::EXPERT ? TFT_EXPERT_PCT : TFT_GOOD_PCT;
    if (in.lowestAllyPercent > line || !in.allyAttackedByPlayer)
        return TftReason::NOT_THREATENED;
    if (tier == Tier::EXPERT && !in.enemyKickNotSpent)
        return TftReason::NO_KICK_RISK;
    return TftReason::CAST_TEA;
}

// ---------------------------------------------------------------------------------------------------------------------
// 5. Tiger's Lust in a PvP fight
// ---------------------------------------------------------------------------------------------------------------------
enum class LustReason : std::uint8_t
{
    NO,
    NOT_PVP,        // the dungeon and open world chains are untouched
    CASUAL_SKIPS,
    FREE_SELF,      // the monk is rooted or slowed
    FREE_ALLY       // an ally is
};

inline LustReason TigersLust(bool inPvpFight, bool selfImpaired, bool allyImpaired, Tier tier)
{
    if (!inPvpFight)
        return LustReason::NOT_PVP;
    if (tier == Tier::CASUAL)
        return LustReason::CASUAL_SKIPS;
    if (selfImpaired)
        return LustReason::FREE_SELF;
    return allyImpaired ? LustReason::FREE_ALLY : LustReason::NO;
}

inline bool LustDue(LustReason reason)
{
    return reason == LustReason::FREE_SELF || reason == LustReason::FREE_ALLY;
}
}  // namespace PvpMistweaver

#endif
