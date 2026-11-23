/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_ARENAPLANMATH_H
#define _PLAYERBOT_ARENAPLANMATH_H

// BT-154: the scoring of the team play in a ladder arena match (play v1). Every in-match decision is a list of
// candidate options with a deterministic integer score; the best one is taken, ties go to the option kind order, then
// to the lower actor and target id. No random numbers. A later selector can choose among the candidates that lie within
// PICK_MARGIN of the best. Free of game types so that tests/ can run it; the game side
// (ArenaPlan.cpp) fills the inputs from public unit state and applies the result.

#include "BotSkillMath.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace ArenaPlanMath
{
using BotSkillMath::Tier;

enum class OptionKind : std::uint8_t
{
    FOCUS,
    DEFENSIVE,
    LOS
};

struct ArenaOption
{
    OptionKind kind;
    std::uint32_t actor;
    std::uint32_t target;
    std::uint32_t spell;
    std::int32_t score;
};

constexpr std::int32_t PICK_MARGIN = 10;  // a later selector may pick any option this close to the best

inline bool OptionLess(ArenaOption const& a, ArenaOption const& b)  // true when a ranks before b
{
    if (a.score != b.score)
        return a.score > b.score;
    if (a.kind != b.kind)
        return std::uint8_t(a.kind) < std::uint8_t(b.kind);
    if (a.actor != b.actor)
        return a.actor < b.actor;
    return a.target < b.target;
}

// The best option, or nullptr for an empty list. The input order does not matter.
inline ArenaOption const* PickBest(std::vector<ArenaOption> const& options)
{
    ArenaOption const* best = nullptr;
    for (ArenaOption const& option : options)
        if (!best || OptionLess(option, *best))
            best = &option;
    return best;
}

// ---------------------------------------------------------------------------------------------------------------
// Skill tier profiles (the arena table of the pilot design): how often a team re-plans, how long it holds a focus,
// how far another target must lead before it swaps, which hp percentage the defensives use and which scoring terms the
// tier understands.
// ---------------------------------------------------------------------------------------------------------------
struct TierProfile
{
    std::uint32_t planIntervalMs;
    std::uint32_t minFocusHoldMs;
    std::int32_t swapMargin;
    std::int32_t defensiveThreshold;  // hp percent below which the defensive score starts to count
    bool usesRole;                    // healer first
    bool usesImmunity;                // does not hit an immune or crowd controlled target
    bool usesDefensives;              // sees a big defensive on the target
    bool usesKillWindow;              // goes for the kill when the own cooldowns are up
    bool losRetreat;                  // walks out of the enemy casters' line of sight when low
    bool losPreemptive;               // the healer breaks line of sight before it is low
};

inline TierProfile ProfileOf(Tier tier)
{
    switch (tier)
    {
        case Tier::EXPERT:
            return { 500, 2000, 8, 55, true, true, true, true, true, true };
        case Tier::GOOD:
            return { 1000, 4000, 15, 40, true, true, false, false, true, false };
        case Tier::CASUAL:
            return { 2000, 8000, 25, 25, false, false, false, false, false, false };
    }
    return ProfileOf(Tier::GOOD);
}

// ---------------------------------------------------------------------------------------------------------------
// Focus target.
// ---------------------------------------------------------------------------------------------------------------
constexpr std::int32_t FOCUS_ROLE_HEALER = 35;
constexpr std::int32_t FOCUS_ROLE_CASTER = 20;
constexpr std::int32_t FOCUS_ROLE_MELEE = 15;
constexpr std::int32_t FOCUS_ROLE_OTHER = 10;
constexpr std::int32_t FOCUS_IN_RANGE_OF_BOTH = 25;
constexpr std::int32_t FOCUS_LOS_SHARE = 30;  // times the share of own members with line of sight
constexpr std::int32_t FOCUS_STICKINESS = 20;
constexpr std::int32_t FOCUS_BURST_WINDOW = 15;
constexpr std::int32_t FOCUS_BURST_HP_BELOW = 60;
constexpr std::int32_t FOCUS_IMMUNE = -60;
constexpr std::int32_t FOCUS_HEAVY_DEFENSIVE = -40;
constexpr std::int32_t FOCUS_CC_BREAKS = -50;
constexpr std::int32_t FOCUS_HEALER_CCD = 25;

struct FocusInput
{
    std::uint32_t id = 0;          // the enemy's guid (low part)
    std::int32_t hpPercent = 100;  // 0 to 100
    bool healer = false;
    bool casterDamage = false;
    bool meleeDamage = false;
    bool inRangeOfBoth = false;    // every own member can cast on or reach it
    std::int32_t losSharePercent = 0;
    bool isCurrentFocus = false;
    bool burstWindow = false;      // the team's big cooldowns are up
    bool immune = false;           // Ice Block, Divine Shield, ...
    bool heavyDefensive = false;   // Pain Suppression, Guardian Spirit, ...
    bool ccBreaksOnDamage = false; // polymorphed, sapped, feared ...: a hit frees it
    bool healerCcd = false;        // the enemy healer is crowd controlled by the own team (and this one is not the healer)
};

inline std::int32_t FocusScore(FocusInput const& in, TierProfile const& profile)
{
    std::int32_t const hp = std::clamp(in.hpPercent, 0, 100);
    std::int32_t score = 100 - hp;

    if (profile.usesRole)
        score += in.healer ? FOCUS_ROLE_HEALER : in.casterDamage ? FOCUS_ROLE_CASTER : in.meleeDamage ? FOCUS_ROLE_MELEE : FOCUS_ROLE_OTHER;

    if (in.inRangeOfBoth)
        score += FOCUS_IN_RANGE_OF_BOTH;

    // The share of own members that see the target; only the good and expert tiers (the role terms) weigh it.
    if (profile.usesRole)
        score += FOCUS_LOS_SHARE * std::clamp(in.losSharePercent, 0, 100) / 100;

    if (in.isCurrentFocus)
        score += FOCUS_STICKINESS;

    if (profile.usesKillWindow && in.burstWindow && hp < FOCUS_BURST_HP_BELOW)
        score += FOCUS_BURST_WINDOW;

    if (profile.usesImmunity)
    {
        if (in.immune)
            score += FOCUS_IMMUNE;
        if (in.ccBreaksOnDamage)
            score += FOCUS_CC_BREAKS;
    }

    if (profile.usesDefensives && in.heavyDefensive)
        score += FOCUS_HEAVY_DEFENSIVE;

    if (profile.usesKillWindow && in.healerCcd && !in.healer)
        score += FOCUS_HEALER_CCD;

    return score;
}

struct FocusDecision
{
    bool hasTarget = false;
    std::uint32_t id = 0;
    bool changed = false;  // the focus differs from the current one
};

// Keeps the current focus until another target leads by the swap margin and the focus has been held long enough; a
// current focus that is gone from the list (dead, out of reach) or immune is dropped at once. A tier that understands
// immunity never picks an immune target while a target that is not immune exists.
inline FocusDecision ChooseFocus(std::vector<FocusInput> const& enemies, std::uint32_t currentId, std::uint32_t heldMs,
    TierProfile const& profile)
{
    FocusDecision decision;
    if (enemies.empty())
        return decision;

    bool anyOpen = false;
    for (FocusInput const& enemy : enemies)
        anyOpen = anyOpen || !enemy.immune;

    FocusInput const* best = nullptr;
    std::int32_t bestScore = 0;
    FocusInput const* current = nullptr;
    std::int32_t currentScore = 0;
    for (FocusInput const& enemy : enemies)
    {
        FocusInput scored = enemy;
        scored.isCurrentFocus = currentId != 0 && enemy.id == currentId;
        std::int32_t const score = FocusScore(scored, profile);
        if (scored.isCurrentFocus)
        {
            current = &enemy;
            currentScore = score;
        }

        if (profile.usesImmunity && anyOpen && enemy.immune)
            continue;

        if (!best || score > bestScore || (score == bestScore && enemy.id < best->id))
        {
            best = &enemy;
            bestScore = score;
        }
    }

    decision.hasTarget = true;
    bool const currentUsable = current && !(profile.usesImmunity && anyOpen && current->immune);
    if (currentUsable)
    {
        bool const swap = best->id != current->id && bestScore - currentScore >= profile.swapMargin &&
            heldMs >= profile.minFocusHoldMs;
        decision.id = swap ? best->id : current->id;
    }
    else
        decision.id = best->id;

    decision.changed = decision.id != currentId;
    return decision;
}

// ---------------------------------------------------------------------------------------------------------------
// Defensive cooldowns.
// ---------------------------------------------------------------------------------------------------------------
constexpr std::int32_t DEFENSIVE_PRESSURE = 30;
constexpr std::int32_t DEFENSIVE_NO_HEALER_REACH = 20;
constexpr std::int32_t DEFENSIVE_RECENTLY_USED = -40;
constexpr std::int32_t DEFENSIVE_FIRE_SCORE = 50;
constexpr std::uint32_t DEFENSIVE_RECENT_MS = 30000;
constexpr std::int32_t PREDICTED_LETHAL_PERCENT = 80;  // expert: lost >= 80 percent of the current hp within 3 seconds

inline std::int32_t DefensiveScore(std::int32_t hpPercent, std::int32_t threshold, bool underPressure, bool noHealerCanReach,
    bool usedRecently)
{
    double const gap = threshold > 0 ? double(threshold - std::clamp(hpPercent, 0, 100)) / double(threshold) : 0.0;
    std::int32_t score = std::int32_t(std::lround(100.0 * std::clamp(gap, 0.0, 1.0)));
    if (underPressure)
        score += DEFENSIVE_PRESSURE;
    if (noHealerCanReach)
        score += DEFENSIVE_NO_HEALER_REACH;
    if (usedRecently)
        score += DEFENSIVE_RECENTLY_USED;
    return score;
}

inline bool PredictedLethal(std::uint32_t currentHp, std::uint32_t lostInThreeSeconds)
{
    return currentHp > 0 && std::uint64_t(lostInThreeSeconds) * 100 >= std::uint64_t(currentHp) * PREDICTED_LETHAL_PERCENT;
}

inline bool ShouldUseDefensive(std::int32_t score, bool predictedLethal, TierProfile const& profile)
{
    return score >= DEFENSIVE_FIRE_SCORE || (profile.usesKillWindow && predictedLethal);
}

// ---------------------------------------------------------------------------------------------------------------
// Line of sight retreat. Candidate points on rings around the bot; the game side checks sight and paths.
// ---------------------------------------------------------------------------------------------------------------
constexpr std::int32_t LOS_BREAKS_ENEMY_SIGHT = 60;
constexpr std::int32_t LOS_KEEPS_PARTNER_SIGHT = 30;
constexpr std::int32_t LOS_OUT_OF_MELEE_REACH = 20;
constexpr std::int32_t LOS_PER_PATH_YARD = -2;
constexpr std::int32_t LOS_DEAD_END = -25;
constexpr std::int32_t LOS_HAZARD = -40;
constexpr std::int32_t LOS_MIN_SCORE = 50;       // a point that does not break enemy sight is not worth walking to
constexpr double LOS_PATH_FACTOR = 1.6;          // the walk may be at most this much longer than the straight line
constexpr std::uint32_t LOS_RING_POINTS = 8;
constexpr std::uint32_t LOS_RING_COUNT = 3;
constexpr float LOS_RING_RADIUS[LOS_RING_COUNT] = { 8.0f, 14.0f, 20.0f };
constexpr std::int32_t LOS_RETREAT_HP_BELOW = 40;
constexpr std::int32_t LOS_HEALER_PREEMPTIVE_HP_BELOW = 70;

struct LosCandidate
{
    float x = 0.0f;
    float y = 0.0f;
    bool breaksEnemySight = false;
    bool keepsPartnerSight = false;
    bool outOfMeleeReach = false;
    float pathLength = 0.0f;
    bool deadEnd = false;
    bool hazard = false;  // the Dalaran Sewers waterfall zone, a pillar that falls ...
};

inline std::int32_t LosScore(LosCandidate const& c)
{
    std::int32_t score = 0;
    if (c.breaksEnemySight)
        score += LOS_BREAKS_ENEMY_SIGHT;
    if (c.keepsPartnerSight)
        score += LOS_KEEPS_PARTNER_SIGHT;
    if (c.outOfMeleeReach)
        score += LOS_OUT_OF_MELEE_REACH;
    score += LOS_PER_PATH_YARD * std::int32_t(std::lround(c.pathLength));
    if (c.deadEnd)
        score += LOS_DEAD_END;
    if (c.hazard)
        score += LOS_HAZARD;
    return score;
}

inline bool PathAcceptable(float pathLength, float straightDistance)
{
    return pathLength > 0.0f && pathLength <= LOS_PATH_FACTOR * straightDistance;
}

// The point `index` of ring `ring` around (cx, cy); the rings are turned against each other so that no two points line up.
inline void RingPoint(float cx, float cy, std::uint32_t ring, std::uint32_t index, float& x, float& y)
{
    constexpr double TWO_PI = 6.283185307179586;
    double const angle = TWO_PI * (double(index) + 0.5 * double(ring % 2)) / double(LOS_RING_POINTS);
    float const radius = LOS_RING_RADIUS[ring % LOS_RING_COUNT];
    x = cx + radius * float(std::cos(angle));
    y = cy + radius * float(std::sin(angle));
}

// Whether the bot should look for cover now. Casual bots never do; good bots when low and hit by casters; the expert
// healer also before it is low.
inline bool WantsCover(TierProfile const& profile, std::int32_t hpPercent, bool focusedByCasters, bool isHealer)
{
    if (!profile.losRetreat || !focusedByCasters)
        return false;
    if (hpPercent < LOS_RETREAT_HP_BELOW)
        return true;
    return profile.losPreemptive && isHealer && hpPercent < LOS_HEALER_PREEMPTIVE_HP_BELOW;
}

// The best candidate that breaks enemy sight and reaches LOS_MIN_SCORE; ties by the lower index (ring order).
inline int BestCover(std::vector<LosCandidate> const& candidates)
{
    int best = -1;
    std::int32_t bestScore = LOS_MIN_SCORE - 1;
    for (std::size_t i = 0; i < candidates.size(); ++i)
    {
        if (!candidates[i].breaksEnemySight)
            continue;
        std::int32_t const score = LosScore(candidates[i]);
        if (score > bestScore)
        {
            best = int(i);
            bestScore = score;
        }
    }
    return best;
}
}  // namespace ArenaPlanMath

#endif
