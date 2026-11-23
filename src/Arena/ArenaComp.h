/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_ARENACOMP_H
#define _PLAYERBOT_ARENACOMP_H

// BT-154: which two bots make a good 2v2 team. Each specialisation has a profile (its role, what crowd control and
// interrupts it learns and at which level, its defensives and how much burst it brings), filled from the class sheets
// in docs/class-sheets/ ("What the specialisation knows" tables, 5.4.8 data). The pair score adds the profiles up; the
// team former accepts a pair that reaches ArenaTeams.MinPairScore. Free of game types so that tests/ can run it; the
// specialisation is the id of the core's Specializations enum (ChrSpecialization).

#include "ArenaMath.h"
#include "BotSkillMath.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <vector>

namespace ArenaComp
{
// Crowd control categories a team can lock an enemy down with. Diminishing returns work per category, so a team with
// many categories can chain.
constexpr std::uint8_t LOCK_STUN = 1;
constexpr std::uint8_t LOCK_INCAPACITATE = 2;
constexpr std::uint8_t LOCK_DISORIENT = 4;  // fear, horror and other disorients
constexpr std::uint8_t LOCK_ROOT = 8;
constexpr std::uint8_t LOCK_SILENCE = 16;   // silence and interrupt
constexpr std::uint32_t LOCK_CATEGORY_COUNT = 5;
constexpr std::uint8_t LOCK_NO_CHAIN_MASK = LOCK_STUN | LOCK_INCAPACITATE | LOCK_DISORIENT;

enum class Role : std::uint8_t
{
    DAMAGE,
    HEALER,
    TANK  // never in an arena team of the pilot: slow killers
};

// Class ids of the core (SharedDefines.h Classes).
constexpr std::uint8_t CLASS_ID_WARRIOR = 1;
constexpr std::uint8_t CLASS_ID_PALADIN = 2;
constexpr std::uint8_t CLASS_ID_HUNTER = 3;
constexpr std::uint8_t CLASS_ID_ROGUE = 4;
constexpr std::uint8_t CLASS_ID_PRIEST = 5;
constexpr std::uint8_t CLASS_ID_DEATH_KNIGHT = 6;
constexpr std::uint8_t CLASS_ID_SHAMAN = 7;
constexpr std::uint8_t CLASS_ID_MAGE = 8;
constexpr std::uint8_t CLASS_ID_WARLOCK = 9;
constexpr std::uint8_t CLASS_ID_MONK = 10;
constexpr std::uint8_t CLASS_ID_DRUID = 11;

struct Ability
{
    std::uint8_t lockMask;  // 0 for an empty slot
    std::uint8_t level;     // the level it is learned at
};

constexpr std::uint32_t MAX_LOCK_ABILITIES = 6;

struct SpecProfile
{
    std::uint16_t specId;
    std::uint8_t classId;
    Role role;
    bool melee;
    std::uint8_t burst;           // 0 to 3: how much a burst window of the spec adds
    std::uint8_t interruptLevel;  // 0: none
    std::uint8_t selfDefensive;   // level of the first big self defensive; 0: none
    std::uint8_t external;        // level of a defensive cast on the partner (Pain Suppression ...); 0: none
    Ability locks[MAX_LOCK_ABILITIES];
};

// The ids are the core's Specializations (SharedDefines.h). Learn levels are from the class sheets; abilities that are
// only talents are left out, because a team must not count on them.
inline constexpr SpecProfile PROFILES[] = {
    // Mage: Frost Nova 3, Counterspell 8, Polymorph 14, Ice Block 26, Dragon's Breath 62, Deep Freeze 66
    { 62, CLASS_ID_MAGE, Role::DAMAGE, false, 2, 8, 26, 0, { { LOCK_ROOT, 3 }, { LOCK_INCAPACITATE, 14 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
    { 63, CLASS_ID_MAGE, Role::DAMAGE, false, 3, 8, 26, 0, { { LOCK_ROOT, 3 }, { LOCK_INCAPACITATE, 14 }, { LOCK_DISORIENT, 62 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
    { 64, CLASS_ID_MAGE, Role::DAMAGE, false, 2, 8, 26, 0, { { LOCK_ROOT, 3 }, { LOCK_INCAPACITATE, 14 }, { LOCK_STUN, 66 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
    // Paladin: Hammer of Justice 7, Divine Shield 18, Rebuke 36, Hand of Protection 48, Blinding Light 87
    { 65, CLASS_ID_PALADIN, Role::HEALER, false, 1, 36, 18, 48, { { LOCK_STUN, 7 }, { LOCK_DISORIENT, 87 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
    { 66, CLASS_ID_PALADIN, Role::TANK, true, 1, 36, 18, 48, { { LOCK_STUN, 7 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
    { 70, CLASS_ID_PALADIN, Role::DAMAGE, true, 3, 36, 18, 48, { { LOCK_STUN, 7 }, { LOCK_DISORIENT, 87 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
    // Warrior: Pummel 24, Intimidating Shout 52, Spell Reflection 66
    { 71, CLASS_ID_WARRIOR, Role::DAMAGE, true, 3, 24, 66, 0, { { LOCK_DISORIENT, 52 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
    { 72, CLASS_ID_WARRIOR, Role::DAMAGE, true, 3, 24, 66, 0, { { LOCK_DISORIENT, 52 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
    { 73, CLASS_ID_WARRIOR, Role::TANK, true, 1, 24, 48, 0, { { LOCK_DISORIENT, 52 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
    // Druid: Entangling Roots 10, Solar Beam 28, Barkskin 44, Skull Bash 64, Cyclone 78
    { 102, CLASS_ID_DRUID, Role::DAMAGE, false, 2, 28, 44, 0, { { LOCK_ROOT, 10 }, { LOCK_DISORIENT, 78 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
    { 103, CLASS_ID_DRUID, Role::DAMAGE, true, 2, 64, 44, 0, { { LOCK_ROOT, 10 }, { LOCK_DISORIENT, 78 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
    { 104, CLASS_ID_DRUID, Role::TANK, true, 1, 64, 44, 0, { { LOCK_ROOT, 10 }, { LOCK_DISORIENT, 78 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
    { 105, CLASS_ID_DRUID, Role::HEALER, false, 0, 0, 44, 0, { { LOCK_ROOT, 10 }, { LOCK_DISORIENT, 78 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
    // Death knight: Mind Freeze 57, Strangulate 58, Icebound Fortitude 62, Anti-Magic Shell 68
    { 250, CLASS_ID_DEATH_KNIGHT, Role::TANK, true, 1, 57, 62, 0, { { LOCK_SILENCE, 58 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
    { 251, CLASS_ID_DEATH_KNIGHT, Role::DAMAGE, true, 3, 57, 62, 0, { { LOCK_SILENCE, 58 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
    { 252, CLASS_ID_DEATH_KNIGHT, Role::DAMAGE, true, 2, 57, 62, 0, { { LOCK_SILENCE, 58 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
    // Hunter: Scatter Shot 15, Freezing Trap 28, Deterrence 78 (no baseline interrupt)
    { 253, CLASS_ID_HUNTER, Role::DAMAGE, false, 3, 0, 78, 0, { { LOCK_DISORIENT, 15 }, { LOCK_INCAPACITATE, 28 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
    { 254, CLASS_ID_HUNTER, Role::DAMAGE, false, 3, 0, 78, 0, { { LOCK_DISORIENT, 15 }, { LOCK_INCAPACITATE, 28 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
    { 255, CLASS_ID_HUNTER, Role::DAMAGE, false, 2, 0, 78, 0, { { LOCK_DISORIENT, 15 }, { LOCK_INCAPACITATE, 28 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
    // Priest: Psychic Scream 12, Silence 52 (shadow), Dispersion 60 (shadow), Pain Suppression 58, Guardian Spirit 70
    { 256, CLASS_ID_PRIEST, Role::HEALER, false, 0, 0, 0, 58, { { LOCK_DISORIENT, 12 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
    { 257, CLASS_ID_PRIEST, Role::HEALER, false, 0, 0, 0, 70, { { LOCK_DISORIENT, 12 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
    { 258, CLASS_ID_PRIEST, Role::DAMAGE, false, 2, 52, 60, 0, { { LOCK_DISORIENT, 12 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
    // Rogue: Sap 12, Kick 18, Gouge 22, Cheap Shot 30, Blind 38, Kidney Shot 40, Cloak of Shadows 58
    { 259, CLASS_ID_ROGUE, Role::DAMAGE, true, 2, 18, 58, 0, { { LOCK_INCAPACITATE, 12 }, { LOCK_STUN, 30 }, { LOCK_DISORIENT, 38 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
    { 260, CLASS_ID_ROGUE, Role::DAMAGE, true, 3, 18, 58, 0, { { LOCK_INCAPACITATE, 12 }, { LOCK_STUN, 30 }, { LOCK_DISORIENT, 38 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
    { 261, CLASS_ID_ROGUE, Role::DAMAGE, true, 3, 18, 58, 0, { { LOCK_INCAPACITATE, 12 }, { LOCK_STUN, 30 }, { LOCK_DISORIENT, 38 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
    // Shaman: Wind Shear 16, Capacitor Totem 63, Hex 75 (no baseline self defensive)
    { 262, CLASS_ID_SHAMAN, Role::DAMAGE, false, 2, 16, 0, 0, { { LOCK_STUN, 63 }, { LOCK_INCAPACITATE, 75 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
    { 263, CLASS_ID_SHAMAN, Role::DAMAGE, true, 3, 16, 0, 0, { { LOCK_STUN, 63 }, { LOCK_INCAPACITATE, 75 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
    { 264, CLASS_ID_SHAMAN, Role::HEALER, false, 0, 16, 0, 0, { { LOCK_STUN, 63 }, { LOCK_INCAPACITATE, 75 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
    // Warlock: Fear 14, Banish 32, Unending Resolve 64 (the pet's interrupt is not counted)
    { 265, CLASS_ID_WARLOCK, Role::DAMAGE, false, 2, 0, 64, 0, { { LOCK_DISORIENT, 14 }, { LOCK_INCAPACITATE, 32 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
    { 266, CLASS_ID_WARLOCK, Role::DAMAGE, false, 3, 0, 64, 0, { { LOCK_DISORIENT, 14 }, { LOCK_INCAPACITATE, 32 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
    { 267, CLASS_ID_WARLOCK, Role::DAMAGE, false, 3, 0, 64, 0, { { LOCK_DISORIENT, 14 }, { LOCK_INCAPACITATE, 32 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
    // Monk: Fortifying Brew 24, Spear Hand Strike 32, Paralysis 44, Life Cocoon 50
    { 268, CLASS_ID_MONK, Role::TANK, true, 1, 32, 24, 0, { { LOCK_INCAPACITATE, 44 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
    { 269, CLASS_ID_MONK, Role::DAMAGE, true, 3, 32, 24, 0, { { LOCK_INCAPACITATE, 44 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
    { 270, CLASS_ID_MONK, Role::HEALER, false, 0, 32, 24, 50, { { LOCK_INCAPACITATE, 44 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
};

inline SpecProfile const* FindProfile(std::uint16_t specId)
{
    for (SpecProfile const& profile : PROFILES)
        if (profile.specId == specId)
            return &profile;
    return nullptr;
}

// A bot of the pool: its specialisation and level are what the score looks at.
struct Member
{
    std::uint16_t specId;
    std::uint8_t level;
};

// Pair score weights (named so that a tuning pass touches one place).
constexpr std::int32_t INELIGIBLE = -100000;
constexpr std::int32_t SCORE_ONE_HEALER = 40;
constexpr std::int32_t SCORE_TWO_HEALERS = -90;  // cannot kill, mana stalemate
constexpr std::int32_t SCORE_BOTH_SELF_DEFENSIVE_NO_HEALER = 10;
constexpr std::int32_t SCORE_PER_LOCK_CATEGORY = 8;
constexpr std::int32_t SCORE_NO_HARD_CONTROL = -30;
constexpr std::int32_t SCORE_BOTH_INTERRUPT = 10;
constexpr std::int32_t SCORE_RANGED_AND_MELEE = 10;
constexpr std::int32_t SCORE_PER_BURST_POINT = 5;
constexpr std::int32_t SCORE_EXTERNAL_DEFENSIVE = 10;
constexpr std::int32_t DEFAULT_MIN_PAIR_SCORE = 60;

// What a member can do at its level: the union of the categories of the abilities it has learned, interrupts included.
inline std::uint8_t LocksAt(SpecProfile const& profile, std::uint8_t level)
{
    std::uint8_t mask = 0;
    for (Ability const& ability : profile.locks)
        if (ability.lockMask && ability.level <= level)
            mask = std::uint8_t(mask | ability.lockMask);
    if (profile.interruptLevel && profile.interruptLevel <= level)
        mask = std::uint8_t(mask | LOCK_SILENCE);
    return mask;
}

inline std::uint32_t CountBits(std::uint8_t mask)
{
    std::uint32_t count = 0;
    for (; mask; mask = std::uint8_t(mask >> 1))
        count += mask & 1u;
    return count;
}

inline bool Has(std::uint8_t level, std::uint8_t learnedAt)
{
    return learnedAt != 0 && learnedAt <= level;
}

// A tank, an unknown specialisation or a spec without a profile cannot join a pilot team.
inline bool Eligible(Member const& member)
{
    SpecProfile const* profile = FindProfile(member.specId);
    return profile && profile->role != Role::TANK;
}

inline std::int32_t PairScore(Member const& first, Member const& second)
{
    SpecProfile const* a = FindProfile(first.specId);
    SpecProfile const* b = FindProfile(second.specId);
    if (!a || !b || a->role == Role::TANK || b->role == Role::TANK)
        return INELIGIBLE;

    std::int32_t score = 0;

    std::uint32_t const healers = (a->role == Role::HEALER ? 1u : 0u) + (b->role == Role::HEALER ? 1u : 0u);
    if (healers == 1)
        score += SCORE_ONE_HEALER;
    else if (healers == 2)
        score += SCORE_TWO_HEALERS;
    else if (Has(first.level, a->selfDefensive) && Has(second.level, b->selfDefensive))
        score += SCORE_BOTH_SELF_DEFENSIVE_NO_HEALER;

    std::uint8_t const locks = std::uint8_t(LocksAt(*a, first.level) | LocksAt(*b, second.level));
    score += SCORE_PER_LOCK_CATEGORY * std::int32_t(std::min(LOCK_CATEGORY_COUNT, CountBits(locks)));
    if (!(locks & LOCK_NO_CHAIN_MASK))
        score += SCORE_NO_HARD_CONTROL;

    if (Has(first.level, a->interruptLevel) && Has(second.level, b->interruptLevel))
        score += SCORE_BOTH_INTERRUPT;

    if (a->melee != b->melee)
        score += SCORE_RANGED_AND_MELEE;

    score += SCORE_PER_BURST_POINT * std::int32_t(a->burst + b->burst);

    // The level that counts is the caster's: a priest of level 58 brings Pain Suppression for its partner.
    if (Has(first.level, a->external) || Has(second.level, b->external))
        score += SCORE_EXTERNAL_DEFENSIVE;

    return score;
}

// ---------------------------------------------------------------------------------------------------------------
// Team forming. The pool is sorted by guid so that forming is reproducible. The next team is the best pair of the
// tier that is furthest below its target share (the mix), and a pair of mixed tiers only when no same-tier pair
// reaches the minimum score. The tier of the team is the weaker one of its members.
// ---------------------------------------------------------------------------------------------------------------
struct Candidate
{
    std::uint32_t guid;
    std::uint16_t specId;
    std::uint8_t level;
    BotSkillMath::Tier tier;
};

struct FormedPair
{
    std::uint32_t first = 0;
    std::uint32_t second = 0;
    std::int32_t score = 0;
    BotSkillMath::Tier tier = BotSkillMath::Tier::GOOD;
    bool sameTier = false;
};

// mix: the target number of teams per tier (expert, good, casual); have: how many there already are.
struct TierMix
{
    std::uint32_t target[3] = { 1, 3, 2 };
};

inline std::optional<FormedPair> FormNextTeam(std::vector<Candidate> pool, std::int32_t minScore, TierMix const& mix,
    std::uint32_t const (&have)[3])
{
    pool.erase(std::remove_if(pool.begin(), pool.end(), [](Candidate const& c) { return !Eligible({ c.specId, c.level }); }),
        pool.end());
    std::sort(pool.begin(), pool.end(), [](Candidate const& x, Candidate const& y) { return x.guid < y.guid; });

    auto better = [](FormedPair const& x, FormedPair const& y)
    {
        if (x.score != y.score)
            return x.score > y.score;
        if (x.first != y.first)
            return x.first < y.first;
        return x.second < y.second;
    };

    std::optional<FormedPair> bestSame[3];
    std::optional<FormedPair> bestMixed;
    for (std::size_t i = 0; i < pool.size(); ++i)
    {
        for (std::size_t j = i + 1; j < pool.size(); ++j)
        {
            std::int32_t const score = PairScore({ pool[i].specId, pool[i].level }, { pool[j].specId, pool[j].level });
            if (score < minScore)
                continue;

            FormedPair pair;
            pair.first = pool[i].guid;
            pair.second = pool[j].guid;
            pair.score = score;
            pair.sameTier = pool[i].tier == pool[j].tier;
            pair.tier = ArenaMath::WeakestTier(pool[i].tier, pool[j].tier);

            std::optional<FormedPair>& slot = pair.sameTier ? bestSame[std::uint8_t(pair.tier)] : bestMixed;
            if (!slot || better(pair, *slot))
                slot = pair;
        }
    }

    // The tier with the largest shortfall against its target goes first; ties go to the better tier.
    std::optional<FormedPair> chosen;
    std::int64_t chosenShortfall = 0;
    for (std::uint32_t tier = 0; tier < 3; ++tier)
    {
        if (!bestSame[tier])
            continue;

        std::int64_t const shortfall = std::int64_t(mix.target[tier]) - std::int64_t(have[tier]);
        if (!chosen || shortfall > chosenShortfall)
        {
            chosen = bestSame[tier];
            chosenShortfall = shortfall;
        }
    }

    return chosen ? chosen : bestMixed;
}
}  // namespace ArenaComp

#endif
