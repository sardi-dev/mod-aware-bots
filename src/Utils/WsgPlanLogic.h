/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_WSGPLANLOGIC_H
#define _PLAYERBOT_WSGPLANLOGIC_H

// BT-153: the Warsong Gulch playbook (docs/bg-sheets/warsong-gulch.md) as pure logic, free of game types so that
// tests/ can run it: the plan draw, role counts and role assignment, the flag phases, the score and clock rules, the
// objective options with their scores, the graveyard rule, the call-out limiter and the reading of a human's call.
//
// Every decision is a list of candidate options with a deterministic score, so the rules stay the fallback and an AI
// layer (design decision 0014, 0026) can later pick among the same options. Each rule says its tier: core rules are played by every
// bot, good rules by good and expert bots, expert rules by expert bots only (BotSkillMath.h, owner 2026-11-12).

#include "BgPlanCommon.h"
#include "BotSkillMath.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace WsgLogic
{
using Tier = BotSkillMath::Tier;

// BT-156: the pieces that are not specific to Warsong Gulch live in Utils/BgPlanCommon.h and are shared with Arathi
// Basin and the later maps. The names stay in this namespace so that the Warsong code and its tests did not change.
using BgPlan::DrawPlan;
using BgPlan::Plan;
using BgPlan::AdoptLagMs;
using BgPlan::CallChancePercent;
using BgPlan::CallDelayMs;
using BgPlan::NearestN;
using BgPlan::ClassId;
using BgPlan::CLS_WARRIOR;
using BgPlan::CLS_PALADIN;
using BgPlan::CLS_HUNTER;
using BgPlan::CLS_ROGUE;
using BgPlan::CLS_PRIEST;
using BgPlan::CLS_DEATH_KNIGHT;
using BgPlan::CLS_SHAMAN;
using BgPlan::CLS_MAGE;
using BgPlan::CLS_WARLOCK;
using BgPlan::CLS_MONK;
using BgPlan::CLS_DRUID;
using BgPlan::CALL_TEAM_GAP_MS;
using BgPlan::CALL_BOT_GAP_MS;
using BgPlan::CALL_WINDOW_MS;
using BgPlan::CALL_MAX_PER_WINDOW;
using BgPlan::ADOPT_LAG_EXPERT_MS;
using BgPlan::ADOPT_LAG_GOOD_MS;
using BgPlan::ADOPT_LAG_CASUAL_MS;
using BgPlan::PLAN_OFFENSE_PERCENT;
using BgPlan::PLAN_BALANCED_PERCENT;

// ---------------------------------------------------------------------------------------------------------------------
// Numbers from the core (BattlegroundWS.cpp) and the owner's decisions of 2026-11-12.
// ---------------------------------------------------------------------------------------------------------------------

// The match ends when the instance clock reaches 27 minutes (BattlegroundWS.cpp:52); the clock also runs during the
// 2 minute preparation (Battleground.cpp:247).
constexpr std::uint32_t MATCH_LIMIT_MS = 27u * 60u * 1000u;

// A team of this many players or more plays the table of the sheet; smaller teams use the small-team rule.
constexpr std::uint32_t FULL_TEAM = 10;
constexpr std::uint32_t SMALL_TEAM_MAX = 6;

// Low brackets (levels below this) have no mounts and short spell lists: three escorts (owner).
constexpr std::uint32_t LOW_BRACKET_BELOW_LEVEL = 30;
constexpr std::uint32_t LOW_BRACKET_ESCORTS = 3;

// At 2-0 at most this many bots hold the enemy tunnel exit (owner).
constexpr std::uint32_t LEAD_TUNNEL_HOLDERS = 3;

// Score and clock thresholds of section 6 of the sheet.
constexpr std::int32_t ALL_IN_LEFT_MS = 5 * 60 * 1000;
constexpr std::int32_t LEVEL_LATE_LEFT_MS = 4 * 60 * 1000;
constexpr std::int32_t LEAD1_LATE_LEFT_MS = 3 * 60 * 1000;
constexpr std::int32_t RUN_ONLY_LEFT_MS = 150 * 1000;

// Both flags carried: the debuffs start at 10 and 15 minutes (BattlegroundWS.h BG_WS_SPELL_FORCE_TIME, _BRUTAL_TIME),
// so the stalemate breaker starts hunting at 5 minutes and sends the carriers home at 9. These clock numbers are the
// fallback of the stack rule below: they hold on a core that gives its first debuff at 10 minutes.
constexpr std::uint32_t STALEMATE_HUNT_MS = 5u * 60u * 1000u;
constexpr std::uint32_t STALEMATE_RUN_MS = 9u * 60u * 1000u;

// BT-472 (Twin Peaks sheet G4): the carriers' own debuff is read live, so the rule is right before and after the core
// change that adds stacking debuffs from the first minute (BT-462, owner answer 3a, 2026-11-18). Focused Assault 46392
// and Brutal Assault 46393 are +10 percent damage taken each stack (Brutal also caps the run speed); the reference core
// gives Focused Assault up to 4 stacks and Brutal Assault from the fifth, up to 10; the core of today casts each once.
constexpr std::uint32_t FOCUSED_ASSAULT_SPELL = 46392;
constexpr std::uint32_t BRUTAL_ASSAULT_SPELL = 46393;
constexpr std::uint32_t BRUTAL_MIN_LEVEL = 5;
// The first stack on either carrier sends the hunt; two stacks on the own carrier send it home when nobody is on the enemy
// carrier (sheet decision point D8).
constexpr std::uint32_t STALEMATE_HUNT_LEVEL = 1;
constexpr std::uint32_t STALEMATE_RUN_LEVEL = 2;
// BT-706 (Twin Peaks, both flags carried): live run of build 30, 13 minutes of both flags out and a debuff of 10 on both
// carriers: the hunt was one bot per team (the backup) against a carrier with its healer and guards at its own stand, so no
// carrier ever died. After this long with both flags held the guards and the first escort leave their posts and hunt too (the
// plan sets the flag for Twin Peaks only, so the Warsong Gulch sheet is unchanged). Both teams do it: whoever kills the carrier
// first returns its own flag, and its own carrier at its stand captures at once.
constexpr std::uint32_t STALEMATE_ALL_IN_MS = 4u * 60u * 1000u;
// The enemy carrier is "under attack" when this many of our players stand within this distance of it.
constexpr float EFC_PRESSURE_RADIUS = 40.0f;
constexpr std::uint32_t EFC_PRESSURE_MIN = 2;

// Opening: the first minute and a half after the gates open.
constexpr std::uint32_t OPENING_MS = 90u * 1000u;
// A strike group that is not together by this time goes anyway.
constexpr std::uint32_t GATHER_GIVE_UP_MS = 75u * 1000u;
constexpr float GATHER_RADIUS = 25.0f;
constexpr std::uint32_t GATHER_MIN_MEMBERS = 3;

// Home guards chase the enemy carrier only inside this distance of our flag stand (sheet role table).
constexpr float HOME_CHASE_LIMIT = 40.0f;
// Escorts stay within this distance of the carrier (good rule).
constexpr float ESCORT_DISTANCE = 12.0f;
// The nearest bots to a dropped flag answer it.
constexpr std::uint32_t DROP_RESPONDERS = 2;
constexpr std::uint32_t EFC_HUNTERS = 2;

// Graveyard rule (owner): no bot stays at the enemy main graveyard longer than this unless the enemy carries our flag
// on that lane.
constexpr std::uint32_t GY_DWELL_LIMIT_MS = 20u * 1000u;
constexpr float GY_ZONE_RADIUS = 45.0f;

// Call-out limits (sheet section 8): the team limits are BgPlan's; the carrier position call has its own gap.
constexpr std::uint32_t CALL_KIND_GAP_MOVED_MS = 10000;
// The leader's stance call (BT-481, the same number as BgLeader::STANCE_CALL_GAP_MS).
constexpr std::uint32_t CALL_KIND_GAP_STANCE_MS = 15000;

// ---------------------------------------------------------------------------------------------------------------------
// Plan, roles, counts.
// ---------------------------------------------------------------------------------------------------------------------

// Slot order is also the fill order.
enum class Role : std::uint8_t
{
    TAKER = 0,
    BACKUP = 1,
    HOME = 2,
    ESCORT = 3,
    MID = 4,
    NONE = 5
};
constexpr std::size_t ROLE_COUNT = 5;

inline char const* RoleName(Role role)
{
    switch (role)
    {
        case Role::TAKER: return "taker";
        case Role::BACKUP: return "backup";
        case Role::HOME: return "home";
        case Role::ESCORT: return "escort";
        case Role::MID: return "mid";
        case Role::NONE: break;
    }
    return "none";
}

inline bool IsStrikeRole(Role role)
{
    return role == Role::TAKER || role == Role::BACKUP || role == Role::ESCORT;
}

// The sheet sends the whole strike group to fight at the stand. Until then, keep moving unless
// too hurt or held: a rooted bot cannot move, and multiple melee attackers can pin it down.
constexpr float STRIKE_STAND_FIGHT_RADIUS = 30.0f;
constexpr float STRIKE_MIN_HEALTH_PCT = 40.0f;
constexpr float STRIKE_MIN_HEALTH_CASUAL_PCT = 25.0f;
inline bool StrikeRunOverridesCombat(Role role, bool planIsTakeFlag, bool alive, float healthPct,
                                     float distToEnemyStand, bool rooted, std::uint32_t meleeAttackersOnMe, Tier tier)
{
    return IsStrikeRole(role) && planIsTakeFlag && alive && !rooted && meleeAttackersOnMe <= 1 &&
           healthPct >= (tier == Tier::CASUAL ? STRIKE_MIN_HEALTH_CASUAL_PCT : STRIKE_MIN_HEALTH_PCT) &&
           distToEnemyStand > STRIKE_STAND_FIGHT_RADIUS;
}

struct RoleCounts
{
    std::uint32_t n[ROLE_COUNT] = { 0, 0, 0, 0, 0 };

    std::uint32_t& operator[](Role role) { return n[static_cast<std::size_t>(role)]; }
    std::uint32_t operator[](Role role) const { return n[static_cast<std::size_t>(role)]; }

    std::uint32_t Total() const
    {
        std::uint32_t total = 0;
        for (std::uint32_t value : n)
            total += value;
        return total;
    }
};

// Make the counts add up to size: too many are cut from the roles that matter least (mid, escort, backup, home,
// taker), too few go to mid.
inline void FitCounts(RoleCounts& counts, std::uint32_t size)
{
    static Role const trimOrder[ROLE_COUNT] = { Role::MID, Role::ESCORT, Role::BACKUP, Role::HOME, Role::TAKER };
    for (Role role : trimOrder)
    {
        while (counts.Total() > size && counts[role] > 0)
            --counts[role];
    }
    if (counts.Total() < size)
        counts[Role::MID] += size - counts.Total();
}

// The role table of section 4 for a full team of 10, scaled to the team size. Teams of 6 or fewer: one home guard, one
// mid, the rest strike (sheet section 7).
inline RoleCounts BaseCounts(Plan plan, std::uint32_t size)
{
    RoleCounts counts;
    if (size == 0)
        return counts;

    if (size <= SMALL_TEAM_MAX)
    {
        counts[Role::TAKER] = 1;
        counts[Role::HOME] = size >= 2 ? 1 : 0;
        counts[Role::MID] = size >= 3 ? 1 : 0;
        counts[Role::BACKUP] = size >= 4 ? 1 : 0;
        counts[Role::ESCORT] = size - counts.Total();
        return counts;
    }

    // Per plan, in Role order (taker, backup, home, escort, mid) for 10 players: balanced 1/1/3/2/3, offense 1/1/1/4/3,
    // defense 1/0/5/2/2.
    static std::uint32_t const table[3][ROLE_COUNT] = {
        { 1, 1, 3, 2, 3 },
        { 1, 1, 1, 4, 3 },
        { 1, 0, 5, 2, 2 }
    };
    std::uint32_t const* row = table[static_cast<std::size_t>(plan)];
    for (std::size_t i = 0; i < ROLE_COUNT; ++i)
        counts.n[i] = row[i];

    if (size >= FULL_TEAM)
    {
        counts[Role::MID] += size - FULL_TEAM;
        return counts;
    }

    // 7 to 9 players: scale the roles after the taker by the largest remainder, then fit.
    std::uint32_t const rest = size - 1;
    std::uint32_t const tableRest = FULL_TEAM - 1;
    std::uint32_t scaled[ROLE_COUNT] = { 1, 0, 0, 0, 0 };
    std::uint32_t remainder[ROLE_COUNT] = { 0, 0, 0, 0, 0 };
    std::uint32_t used = 1;
    for (std::size_t i = 1; i < ROLE_COUNT; ++i)
    {
        scaled[i] = row[i] * rest / tableRest;
        remainder[i] = row[i] * rest % tableRest;
        used += scaled[i];
    }
    // Largest remainders first; ties by the fill order.
    while (used < size)
    {
        std::size_t best = 1;
        for (std::size_t i = 2; i < ROLE_COUNT; ++i)
            if (remainder[i] > remainder[best])
                best = i;
        ++scaled[best];
        remainder[best] = 0;
        ++used;
    }
    for (std::size_t i = 0; i < ROLE_COUNT; ++i)
        counts.n[i] = scaled[i];
    return counts;
}

// Score and clock situations of section 6.
enum class Situation : std::uint8_t
{
    NORMAL,
    LEAD2,        // ahead by two with two captures: defense plan, no spawn camping
    ALL_IN,       // behind with under 5 minutes left
    LEVEL_LATE,   // level with under 4 minutes left: the last capture wins at the timeout
    LEAD1_LATE    // ahead by one with under 3 minutes left
};

inline char const* SituationName(Situation situation)
{
    switch (situation)
    {
        case Situation::NORMAL: return "normal";
        case Situation::LEAD2: return "lead2";
        case Situation::ALL_IN: return "allin";
        case Situation::LEVEL_LATE: return "level-late";
        case Situation::LEAD1_LATE: return "lead1-late";
    }
    return "normal";
}

// How heavy a carrier's debuff is, 0 for none: its Focused Assault stacks, or its Brutal Assault stacks counted as at
// least the fifth level (a core that casts Brutal Assault once, at 15 minutes, is already at that level).
inline std::uint32_t DebuffLevel(std::uint32_t focusedStacks, std::uint32_t brutalStacks)
{
    if (brutalStacks)
        return std::max(brutalStacks, BRUTAL_MIN_LEVEL);
    return focusedStacks;
}

// Both flags are carried: whether the hunt of the enemy carrier has begun. The clock is the fallback, the first stack on
// either carrier is the live signal.
inline bool StalemateHunts(std::uint32_t bothHeldMs, std::uint32_t ownLevel, std::uint32_t enemyLevel)
{
    return bothHeldMs >= STALEMATE_HUNT_MS || std::max(ownLevel, enemyLevel) >= STALEMATE_HUNT_LEVEL;
}

// Both flags are carried: whether our carrier stops hiding and runs. At two stacks only when nobody is on the enemy
// carrier (efcPressed false): a hunt that is working keeps the carrier where it is.
inline bool StalemateRuns(std::uint32_t bothHeldMs, std::uint32_t ownLevel, bool efcPressed)
{
    return bothHeldMs >= STALEMATE_RUN_MS || (ownLevel >= STALEMATE_RUN_LEVEL && !efcPressed);
}

inline bool StalemateAllIn(std::uint32_t bothHeldMs)
{
    return bothHeldMs >= STALEMATE_ALL_IN_MS;
}

inline std::int32_t LeftMs(std::uint32_t elapsedMs)
{
    return static_cast<std::int32_t>(MATCH_LIMIT_MS) - static_cast<std::int32_t>(elapsedMs);
}

inline Situation SituationOf(std::uint32_t ours, std::uint32_t theirs, std::int32_t leftMs)
{
    if (ours >= 2 && ours >= theirs + 2)
        return Situation::LEAD2;
    if (theirs > ours && leftMs <= ALL_IN_LEFT_MS)
        return Situation::ALL_IN;
    if (ours == theirs && leftMs <= LEVEL_LATE_LEFT_MS)
        return Situation::LEVEL_LATE;
    if (ours == theirs + 1 && leftMs <= LEAD1_LATE_LEFT_MS)
        return Situation::LEAD1_LATE;
    return Situation::NORMAL;
}

struct TeamMix
{
    std::uint32_t size = 0;      // everybody on the team, humans included (a human takes a slot, owner)
    std::uint32_t humans = 0;
    bool hasHealer = false;
    bool lowBracket = false;
};

// The counts the bots of a team fill: the plan's table, changed by the situation, the bracket and the healers, minus the
// slots the humans take (mid first, then home, then escort; never the flag takers).
inline RoleCounts CountsFor(Plan plan, Situation situation, TeamMix const& mix)
{
    std::uint32_t const size = mix.size;
    RoleCounts counts = BaseCounts(situation == Situation::LEAD2 ? Plan::DEFENSE : plan, size);

    if (size >= 7)
    {
        switch (situation)
        {
            case Situation::ALL_IN:
                // Home down to one; the freed bots strike.
                if (counts[Role::HOME] > 1)
                {
                    counts[Role::ESCORT] += counts[Role::HOME] - 1;
                    counts[Role::HOME] = 1;
                }
                break;
            case Situation::LEVEL_LATE:
                // Strike group of five, home three, the rest mid.
                counts = RoleCounts();
                counts[Role::TAKER] = 1;
                counts[Role::BACKUP] = 1;
                counts[Role::ESCORT] = 3;
                counts[Role::HOME] = 3;
                counts[Role::MID] = 2;
                FitCounts(counts, size);
                break;
            case Situation::LEAD1_LATE:
                // Home five, mid holds the enemy tunnel, a strike group of two only to deny their flag.
                counts = RoleCounts();
                counts[Role::TAKER] = 1;
                counts[Role::BACKUP] = 1;
                counts[Role::HOME] = 5;
                counts[Role::MID] = 3;
                FitCounts(counts, size);
                break;
            default:
                break;
        }

        // Low brackets and teams without a healer: three escorts (melee bodies), taken from mid, then home.
        if ((mix.lowBracket || !mix.hasHealer) && situation != Situation::LEAD1_LATE &&
            counts[Role::ESCORT] < LOW_BRACKET_ESCORTS)
        {
            std::uint32_t need = LOW_BRACKET_ESCORTS - counts[Role::ESCORT];
            std::uint32_t const takeMid = std::min(need, counts[Role::MID]);
            counts[Role::MID] -= takeMid;
            need -= takeMid;
            std::uint32_t const takeHome = std::min(need, counts[Role::HOME] > 1 ? counts[Role::HOME] - 1 : 0u);
            counts[Role::HOME] -= takeHome;
            counts[Role::ESCORT] += takeMid + takeHome;
        }
    }

    static Role const humanOrder[3] = { Role::MID, Role::HOME, Role::ESCORT };
    std::uint32_t humans = std::min(mix.humans, size > 0 ? size - 1 : 0u);
    for (Role role : humanOrder)
    {
        std::uint32_t const take = std::min(humans, counts[role]);
        counts[role] -= take;
        humans -= take;
    }
    return counts;
}

// ---------------------------------------------------------------------------------------------------------------------
// Role assignment.
// ---------------------------------------------------------------------------------------------------------------------

// The fields every map's candidate has are BgPlan::CandidateBase; Warsong adds the role the bot has and the flag.
struct Candidate : BgPlan::CandidateBase
{
    Role previous = Role::NONE;
    bool carrying = false;
};

struct ScoreContext
{
    std::uint8_t minLevel = 0;
    std::uint8_t maxLevel = 0;
    std::uint32_t escortHealers = 0;   // healers already in the escort slots
};

// A stealth class takes the flag-taker slots whatever else is true (owner).
constexpr int STEALTH_BONUS = 1000;
constexpr int LEVEL_STEP = 20;          // per level over the lowest in the team: level beats class fit
constexpr int CARRIER_BONUS = 10000;    // a carrier is always the taker
constexpr int STICKY_BONUS = 40;        // keeping the role a bot has: no re-roll, only events change roles
constexpr int SECOND_HEALER_PENALTY = -200;  // a second healer is not an escort: it stays with mid
constexpr int PROMOTE_BONUS = 20;       // the next in line takes over: backup becomes taker, escort becomes backup

using BgPlan::IsStealthClass;

// Class and spec fit of a role, 0 to 30.
inline int ClassFit(Candidate const& c, Role role, ScoreContext const& ctx)
{
    switch (role)
    {
        case Role::TAKER:
        case Role::BACKUP:
        {
            if (c.healer)
                return 2;
            int base = 8;
            switch (c.cls)
            {
                case CLS_ROGUE: base = 30; break;
                case CLS_DRUID: base = c.feral ? 30 : 12; break;
                case CLS_WARRIOR: base = 20; break;
                case CLS_MONK: base = 20; break;
                case CLS_PALADIN: base = 18; break;
                case CLS_HUNTER: base = 16; break;
                case CLS_SHAMAN: base = 14; break;
                case CLS_DEATH_KNIGHT: base = 14; break;
                case CLS_MAGE: base = 8; break;
                case CLS_WARLOCK: base = 6; break;
                case CLS_PRIEST: base = 4; break;
                default: break;
            }
            return base + (c.tank ? 4 : 0);
        }
        case Role::ESCORT:
        {
            // The first healer heals the carrier; a second one stays with mid (sheet section 7).
            if (c.healer)
                return ctx.escortHealers == 0 ? 40 : SECOND_HEALER_PENALTY;
            int base = 10;
            switch (c.cls)
            {
                case CLS_PALADIN: base = 20; break;
                case CLS_WARRIOR: base = 18; break;
                case CLS_MONK: base = 18; break;
                case CLS_DEATH_KNIGHT: base = 16; break;
                case CLS_ROGUE: base = 14; break;
                case CLS_DRUID: base = 14; break;
                case CLS_SHAMAN: base = 12; break;
                case CLS_HUNTER: base = 8; break;
                case CLS_MAGE: base = 6; break;
                default: break;
            }
            return base + (c.melee ? 4 : 0);
        }
        case Role::MID:
        {
            if (c.healer)
                return 10 + (ctx.escortHealers > 0 ? 10 : 0);
            switch (c.cls)
            {
                case CLS_MAGE: return 26;
                case CLS_HUNTER: return 26;
                case CLS_SHAMAN: return 22;
                case CLS_WARLOCK: return 20;
                case CLS_PRIEST: return 18;
                case CLS_DRUID: return 18;
                case CLS_WARRIOR: return 14;
                case CLS_DEATH_KNIGHT: return 14;
                case CLS_ROGUE: return 12;
                case CLS_PALADIN: return 12;
                case CLS_MONK: return 12;
                default: return 10;
            }
        }
        case Role::HOME:
        {
            if (c.healer)
                return 16;
            switch (c.cls)
            {
                case CLS_MAGE: return 26;
                case CLS_WARLOCK: return 26;
                case CLS_HUNTER: return 22;
                case CLS_PRIEST: return 20;
                case CLS_SHAMAN: return 18;
                case CLS_DRUID: return 16;
                case CLS_PALADIN: return 14;
                default: return 10;
            }
        }
        case Role::NONE:
            break;
    }
    return 0;
}

inline int TierFit(Tier tier, Role role)
{
    switch (role)
    {
        case Role::TAKER:
        case Role::BACKUP:
        case Role::MID:
            // Expert bots react at once: they take the roles where a late reaction costs most.
            return BgPlan::TierFitCritical(tier);
        case Role::HOME:
        case Role::ESCORT:
            // Casual bots hurt least at the posts.
            return BgPlan::TierFitPost(tier);
        case Role::NONE:
            break;
    }
    return 0;
}

inline int Score(Candidate const& c, Role role, ScoreContext const& ctx)
{
    int score = ClassFit(c, role, ctx) + TierFit(c.tier, role);
    int const level = static_cast<int>(c.level) - static_cast<int>(ctx.minLevel);
    int const levelFromTop = static_cast<int>(ctx.maxLevel) - static_cast<int>(c.level);

    switch (role)
    {
        case Role::TAKER:
        case Role::BACKUP:
            // Rogues and feral druids first, then the highest level in the bracket.
            if (IsStealthClass(c))
                score += STEALTH_BONUS;
            score += level * LEVEL_STEP;
            if (c.carrying)
                score += CARRIER_BONUS;
            break;
        case Role::MID:
            score += level * (LEVEL_STEP / 2);
            break;
        case Role::HOME:
        case Role::ESCORT:
            // The lowest levels stand next to the healer or at the posts, not against higher-level guards.
            score += levelFromTop * (LEVEL_STEP / 2);
            break;
        case Role::NONE:
            break;
    }

    if (c.previous == role)
        score += STICKY_BONUS;
    else if ((role == Role::TAKER && c.previous == Role::BACKUP) || (role == Role::BACKUP && c.previous == Role::ESCORT))
        score += PROMOTE_BONUS;

    return score;
}

struct Assignment
{
    std::uint32_t guidLow = 0;
    Role role = Role::NONE;
};

// Fill TAKER, BACKUP, ESCORT, HOME, MID by best score; ties by the GUID hash (stable, like the skill tiers) and then the
// GUID. Candidates beyond the counts become mid. The result lists every candidate once.
inline std::vector<Assignment> Assign(std::vector<Candidate> const& candidates, RoleCounts const& counts)
{
    ScoreContext ctx;
    BgPlan::LevelSpan const span = BgPlan::SpanOf(candidates);
    ctx.minLevel = span.minLevel;
    ctx.maxLevel = span.maxLevel;

    std::vector<bool> taken(candidates.size(), false);
    std::vector<Assignment> result;
    result.reserve(candidates.size());

    // The escorts are filled before the home guards so that the first healer goes with the carrier (sheet section 4).
    static Role const fillOrder[ROLE_COUNT] = { Role::TAKER, Role::BACKUP, Role::ESCORT, Role::HOME, Role::MID };
    for (Role role : fillOrder)
    {
        for (std::uint32_t slot = 0; slot < counts[role]; ++slot)
        {
            std::size_t const best = BgPlan::PickBest(candidates, taken,
                [&](Candidate const& c) { return Score(c, role, ctx); });
            if (best == candidates.size())
                break;

            taken[best] = true;
            result.push_back({ candidates[best].guidLow, role });
            if (role == Role::ESCORT && candidates[best].healer)
                ++ctx.escortHealers;
        }
    }

    for (std::size_t i = 0; i < candidates.size(); ++i)
        if (!taken[i])
            result.push_back({ candidates[i].guidLow, Role::MID });

    return result;
}

// ---------------------------------------------------------------------------------------------------------------------
// Flag phases (sheet section 6, states A to F). Flag state numbers are the core's BG_WS_FlagState.
// ---------------------------------------------------------------------------------------------------------------------

constexpr std::uint8_t FLAG_ON_BASE = 0;
constexpr std::uint8_t FLAG_WAIT_RESPAWN = 1;
constexpr std::uint8_t FLAG_ON_PLAYER = 2;
constexpr std::uint8_t FLAG_ON_GROUND = 3;

enum class Phase : std::uint8_t
{
    BOTH_HOME,     // A
    WE_CARRY,      // B
    THEY_CARRY,    // C
    BOTH_CARRIED,  // D
    FLAG_DOWN,     // E
    POST_CAPTURE   // F
};

inline char const* PhaseName(Phase phase)
{
    switch (phase)
    {
        case Phase::BOTH_HOME: return "A";
        case Phase::WE_CARRY: return "B";
        case Phase::THEY_CARRY: return "C";
        case Phase::BOTH_CARRIED: return "D";
        case Phase::FLAG_DOWN: return "E";
        case Phase::POST_CAPTURE: return "F";
    }
    return "A";
}

// ourFlag = state of OUR flag (is the enemy carrying it), theirFlag = state of THEIR flag (do we carry it).
inline Phase PhaseOf(std::uint8_t ourFlag, std::uint8_t theirFlag)
{
    if (ourFlag == FLAG_WAIT_RESPAWN || theirFlag == FLAG_WAIT_RESPAWN)
        return Phase::POST_CAPTURE;
    if (ourFlag == FLAG_ON_PLAYER && theirFlag == FLAG_ON_PLAYER)
        return Phase::BOTH_CARRIED;
    if (ourFlag == FLAG_ON_GROUND || theirFlag == FLAG_ON_GROUND)
        return Phase::FLAG_DOWN;
    if (ourFlag == FLAG_ON_PLAYER)
        return Phase::THEY_CARRY;
    if (theirFlag == FLAG_ON_PLAYER)
        return Phase::WE_CARRY;
    return Phase::BOTH_HOME;
}

// How long both flags have been carried (the debuff clock). The caller keeps the start; zero start means not carried.
inline std::uint32_t BothHeldMs(std::uint8_t ourFlag, std::uint8_t theirFlag, std::uint32_t nowMs, std::uint32_t& startMs)
{
    if (ourFlag == FLAG_ON_PLAYER && theirFlag == FLAG_ON_PLAYER)
    {
        if (startMs == 0)
            startMs = nowMs ? nowMs : 1;
        return nowMs - startMs;
    }
    startMs = 0;
    return 0;
}

// ---------------------------------------------------------------------------------------------------------------------
// Objective options.
// ---------------------------------------------------------------------------------------------------------------------

enum class Kind : std::uint8_t
{
    CARRY_HOME,
    CARRY_HIDE,
    RETURN_OWN_FLAG,
    PICKUP_DROPPED_FLAG,
    BLOCK_STEAL,
    ANSWER_CALL,
    HUNT_EFC,
    CUT_EFC_LANE,
    ESCORT_CARRIER,
    COVER_CARRIER_REAR,
    TAKE_ENEMY_FLAG,
    GATHER_STRIKE,
    HOLD_HOME_POST,
    HOLD_HOME_LOOSE,
    HOLD_MID,
    HOLD_ENEMY_EXIT,
    TAKE_POWERUP,
    HEAL_CARRIER,
    REGROUP_LANE,     // Twin Peaks state G: the wiped group gathers at the own lane post before it goes again
    JOIN_MID_FIGHT,   // Twin Peaks, BT-694: the middle fight is locked for 60 s, the idle home guards join it
    VIA_LANE,         // Twin Peaks, BT-695: after a kill in a locked fight the strike group walks round by the other lane
    ESCORT_TAKER      // Twin Peaks, BT-695: a guard that joined walks with the taker for that run instead of going home
};

inline char const* KindName(Kind kind)
{
    switch (kind)
    {
        case Kind::CARRY_HOME: return "carry-home";
        case Kind::CARRY_HIDE: return "carry-hide";
        case Kind::RETURN_OWN_FLAG: return "return-flag";
        case Kind::PICKUP_DROPPED_FLAG: return "pickup-dropped";
        case Kind::BLOCK_STEAL: return "block-steal";
        case Kind::ANSWER_CALL: return "answer-call";
        case Kind::HUNT_EFC: return "hunt-efc";
        case Kind::CUT_EFC_LANE: return "cut-lane";
        case Kind::ESCORT_CARRIER: return "escort";
        case Kind::COVER_CARRIER_REAR: return "cover-rear";
        case Kind::TAKE_ENEMY_FLAG: return "take-flag";
        case Kind::GATHER_STRIKE: return "gather";
        case Kind::HOLD_HOME_POST: return "home-post";
        case Kind::HOLD_HOME_LOOSE: return "home-loose";
        case Kind::HOLD_MID: return "hold-mid";
        case Kind::HOLD_ENEMY_EXIT: return "hold-enemy-exit";
        case Kind::TAKE_POWERUP: return "powerup";
        case Kind::HEAL_CARRIER: return "heal-carrier";
        case Kind::REGROUP_LANE: return "regroup-lane";
        case Kind::JOIN_MID_FIGHT: return "join-mid";
        case Kind::VIA_LANE: return "via-lane";
        case Kind::ESCORT_TAKER: return "escort-taker";
    }
    return "none";
}

struct Option
{
    Kind kind;
    int score;
};

// Everything one bot's decision reads. The caller (WsgPlan.cpp) fills it from the core in one place.
struct DecisionContext
{
    Role role = Role::MID;
    Tier tier = Tier::GOOD;
    bool hasFlag = false;
    std::uint8_t ourFlag = FLAG_ON_BASE;
    std::uint8_t theirFlag = FLAG_ON_BASE;
    Situation situation = Situation::NORMAL;
    std::int32_t leftMs = static_cast<std::int32_t>(MATCH_LIMIT_MS);
    std::uint32_t bothHeldMs = 0;
    std::uint32_t ownDebuff = 0;          // DebuffLevel of the carrier of the enemy flag on our team (0 when none)
    std::uint32_t enemyDebuff = 0;        // DebuffLevel of the enemy carrier of our flag
    bool efcPressed = false;              // EFC_PRESSURE_MIN of ours within EFC_PRESSURE_RADIUS of the enemy carrier
    std::uint32_t playMs = 0;             // since the gates opened
    std::uint32_t roleIndex = 0;          // among the bots of the same role, 0 first
    bool efcKnown = false;
    float efcToOurStand = 1000.0f;
    float botToEfc = 1000.0f;
    bool efcHunter = false;               // one of the two nearest to the enemy carrier
    bool ownGroundResponder = false;      // one of the two nearest to our dropped flag
    float botToOwnGround = 1000.0f;
    bool theirGroundResponder = false;    // one of the nearest strikers to their dropped flag
    bool strikeGathered = false;
    bool answerCall = false;              // the bot is the one answering a human's call
    bool powerupAvailable = false;
    bool healerFollower = false;          // one of the healers that follow our carrier (PickHealerFollowers)
    // Twin Peaks (BT-474), all false on Warsong Gulch:
    bool efcPastGy = false;               // the enemy carrier is past its own respawn point: the hunters stop (state C)
    bool wipeDefend = false;              // state G, the base is threatened: this bot runs home and holds a post
    bool wipeRegroup = false;             // state G, the base is quiet: this bot gathers at the own lane post
    bool wipeJoin = false;                // state H: this bot (the floater or a spare home guard) joins the strike group
    bool midJoin = false;                 // BT-694: the middle fight is locked, this idle home guard joins it
    bool viaLane = false;                 // BT-695: this striker walks to the other lane's post first
    bool midEscort = false;               // BT-695: this home guard walks with the taker
    bool stalemateAllIn = false;          // BT-706: both flags held long enough, the guards and the first escort hunt as well
};

inline bool Plays(DecisionContext const& c, Tier line)
{
    return BotSkillMath::Uses(c.tier, line);
}

// Scores: 1000 the carrier, 900s reactions to a flag event (905 the healers on the carrier), 700s to 800s the duty of
// the phase, 600s the post.
constexpr int HEAL_CARRIER_SCORE = 905;
constexpr int MID_JOIN_SCORE = 700;
// BT-695: the escort goes above the join and the post, below every flag event; the lane detour goes above the strike group's own
// walk to the enemy base (730 to 775) and below the hunt of an enemy carrier (780) and the carrier duties.
constexpr int ESCORT_TAKER_SCORE = 710;
constexpr int VIA_LANE_SCORE = 776;

inline std::vector<Option> Options(DecisionContext const& c)
{
    std::vector<Option> out;
    auto const add = [&out](Kind kind, int score) { out.push_back({ kind, score }); };

    bool const weCarry = c.theirFlag == FLAG_ON_PLAYER;
    bool const theyCarry = c.ourFlag == FLAG_ON_PLAYER;

    if (c.hasFlag)
    {
        // Core: run home. Good: hide near home while the enemy holds our flag, because the capture needs ours on its
        // stand. The stalemate rule and the last 2.5 minutes send an expert or good carrier home anyway (stalemate is
        // a good rule, the last minutes an expert one).
        bool const mustRun = (Plays(c, Tier::GOOD) && StalemateRuns(c.bothHeldMs, c.ownDebuff, c.efcPressed)) ||
                             (Plays(c, Tier::EXPERT) && c.leftMs <= RUN_ONLY_LEFT_MS);
        bool const hide = theyCarry && Plays(c, Tier::GOOD) && !mustRun;
        add(hide ? Kind::CARRY_HIDE : Kind::CARRY_HOME, 1000);
        add(hide ? Kind::CARRY_HOME : Kind::CARRY_HIDE, 600);
    }
    else
    {
        // A human's call: one bot answers it (good rule, the answer is cheap).
        if (c.answerCall && Plays(c, Tier::GOOD))
            add(Kind::ANSWER_CALL, 880);

        // State E: a dropped flag is returned at once by the two nearest (core), HOME also walks to it inside 60 yd.
        if (c.ourFlag == FLAG_ON_GROUND)
        {
            if (c.ownGroundResponder)
                add(Kind::RETURN_OWN_FLAG, 960);
            else if (c.role == Role::HOME && c.botToOwnGround <= 60.0f)
                add(Kind::RETURN_OWN_FLAG, 700);
        }
        if (c.theirFlag == FLAG_ON_GROUND && c.theirGroundResponder && IsStrikeRole(c.role))
            add(Kind::PICKUP_DROPPED_FLAG, 930);

        // Owner rule 2026-11-17: the healers follow our flag carrier (core rule: no tier gate, the tier only sets how
        // close a casual healer trails). Below the flag events above (a dropped flag is returned or picked up first)
        // and above every duty of a phase, because a carrier without a healer dies on the way home.
        if (c.healerFollower && weCarry)
            add(Kind::HEAL_CARRIER, HEAL_CARRIER_SCORE);

        // Twin Peaks state G (BT-474): the wiped group first guards the base when it is threatened, else it gathers again as one
        // group (above the pickup duties, below the flag events and the healers on the carrier).
        if (c.wipeDefend)
            add(Kind::HOLD_HOME_POST, 945);
        else if (c.wipeRegroup)
            add(Kind::REGROUP_LANE, 790);
        // State H: the floater and a spare home guard use the time to join the strike group.
        if (c.wipeJoin && !weCarry)
            add(c.strikeGathered ? Kind::TAKE_ENEMY_FLAG : Kind::GATHER_STRIKE, 775);

        // Past its own respawn point the carrier is out of reach of the hunters (Twin Peaks state C): it is not hunted. Not in
        // state D: a carrier that hides at its own base because both flags are out is hunted there (the stalemate).
        bool const efc = theyCarry && c.efcKnown && !(c.efcPastGy && !weCarry);
        bool const stalemateHunt = weCarry && theyCarry && Plays(c, Tier::GOOD) &&
                                   StalemateHunts(c.bothHeldMs, c.ownDebuff, c.enemyDebuff);

        switch (c.role)
        {
            case Role::HOME:
            {
                if (efc && c.efcToOurStand <= HOME_CHASE_LIMIT)
                {
                    // Sheet state C and D: the two nearest engage inside 40 yd of the stand, the others hold.
                    if (c.efcHunter)
                        add(Kind::HUNT_EFC, 900);
                    else if (weCarry && c.roleIndex >= 2 && Plays(c, Tier::GOOD))
                        add(Kind::HUNT_EFC, 780);
                }
                if (stalemateHunt && (c.roleIndex >= 2 || c.stalemateAllIn) && efc)
                    add(Kind::HUNT_EFC, 870);
                // BT-694: above the post, below every flag event and the hunt of a carrier at the door. The plan only sets
                // midJoin while no flag is out and nothing threatens the base; the carried-flag test here is the second lock.
                if (c.midJoin && !weCarry && !theyCarry)
                    add(Kind::JOIN_MID_FIGHT, MID_JOIN_SCORE);
                if (c.midEscort && !weCarry && !theyCarry)
                    add(Kind::ESCORT_TAKER, ESCORT_TAKER_SCORE);
                add(Plays(c, Tier::GOOD) ? Kind::HOLD_HOME_POST : Kind::HOLD_HOME_LOOSE, 650);
                break;
            }
            case Role::MID:
            {
                if (efc)
                {
                    if (c.efcHunter || weCarry)
                        add(Kind::HUNT_EFC, weCarry ? 860 : 880);
                    else if (Plays(c, Tier::GOOD))
                        add(Kind::CUT_EFC_LANE, 800);
                    else if (c.botToEfc <= 60.0f)
                        add(Kind::HUNT_EFC, 700);
                }
                else if (weCarry && Plays(c, Tier::GOOD))
                    add(Kind::COVER_CARRIER_REAR, 700);

                if (c.powerupAvailable && c.roleIndex == 0 && c.playMs < OPENING_MS && !weCarry && !theyCarry)
                    add(Kind::TAKE_POWERUP, 650);

                bool const holdsExit = (c.situation == Situation::LEAD2 || c.situation == Situation::LEAD1_LATE) &&
                                       c.roleIndex < LEAD_TUNNEL_HOLDERS;
                add(holdsExit ? Kind::HOLD_ENEMY_EXIT : Kind::HOLD_MID, holdsExit ? 640 : 600);
                break;
            }
            case Role::TAKER:
            case Role::BACKUP:
            case Role::ESCORT:
            {
                bool const taker = c.role == Role::TAKER;
                bool const theirFlagHome = c.theirFlag == FLAG_ON_BASE;
                bool const blocker = (c.role == Role::BACKUP || (c.role == Role::ESCORT && c.roleIndex == 0)) &&
                                     theyCarry && !weCarry && theirFlagHome && Plays(c, Tier::GOOD);

                if (blocker)
                {
                    // Taking their flag blocks their capture too (their flag is off its stand): at once.
                    add(Kind::BLOCK_STEAL, c.role == Role::BACKUP ? 920 : 915);
                }

                if (weCarry && theyCarry)
                {
                    // State D. The carrier hides and its escorts guard it; from 5 minutes on, good and expert bots
                    // hunt instead, all but one escort.
                    if (stalemateHunt && efc && (c.stalemateAllIn || !(c.role == Role::ESCORT && c.roleIndex == 0)))
                        add(Kind::HUNT_EFC, 870);
                    add(Kind::ESCORT_CARRIER, 850);
                }
                else if (weCarry)
                {
                    add(Kind::ESCORT_CARRIER, 800);
                }
                else if (efc)
                {
                    // State C: the rest of the strike group joins the hunt.
                    add(Kind::HUNT_EFC, 780);
                }
                else
                {
                    // Nothing of ours is out: the strike group walks to the enemy flag room together. BT-695: after a kill in the
                    // locked middle fight it goes round by the other lane first (the plan sets viaLane for the bots that detour).
                    if (c.viaLane)
                        add(Kind::VIA_LANE, VIA_LANE_SCORE);
                    int const base = taker ? 760 : c.role == Role::BACKUP ? 740 : 730;
                    bool const together = c.strikeGathered || !Plays(c, Tier::GOOD);
                    // Expert: time the pickup with the flag reset and go straight in after a capture.
                    bool const post = c.ourFlag == FLAG_WAIT_RESPAWN || c.theirFlag == FLAG_WAIT_RESPAWN;
                    if (together || (post && Plays(c, Tier::EXPERT)))
                        add(Kind::TAKE_ENEMY_FLAG, base);
                    else
                        add(Kind::GATHER_STRIKE, base);
                }
                break;
            }
            case Role::NONE:
                add(Kind::HOLD_MID, 300);
                break;
        }
    }

    // Highest score first; equal scores in the order of the Kind list, so the choice is deterministic.
    std::stable_sort(out.begin(), out.end(), [](Option const& a, Option const& b)
    {
        if (a.score != b.score)
            return a.score > b.score;
        return static_cast<std::uint8_t>(a.kind) < static_cast<std::uint8_t>(b.kind);
    });
    return out;
}

// ---------------------------------------------------------------------------------------------------------------------
// Graveyard rule.
// ---------------------------------------------------------------------------------------------------------------------

// Tracks how long a bot has stayed inside the enemy main graveyard zone.
struct DwellTracker
{
    std::uint32_t enteredMs = 0;
    bool inside = false;

    // Returns the time spent inside so far.
    std::uint32_t Update(bool nowInside, std::uint32_t nowMs)
    {
        if (!nowInside)
        {
            inside = false;
            enteredMs = 0;
            return 0;
        }
        if (!inside)
        {
            inside = true;
            enteredMs = nowMs ? nowMs : 1;
        }
        return nowMs - enteredMs;
    }
};

// The bot has to leave when it stayed past the limit, unless the enemy carries our flag and the carrier is on that lane.
inline bool GyMustLeave(std::uint32_t dwellMs, bool efcOnLane)
{
    return dwellMs > GY_DWELL_LIMIT_MS && !efcOnLane;
}

// ---------------------------------------------------------------------------------------------------------------------
// Own graveyard exit (BT-386).
// ---------------------------------------------------------------------------------------------------------------------

// Live probe 2026-11-17 (`.mmap path` from a bot standing at each main graveyard): from WorldSafeLocs 771 and 772 the
// 5.4.8 navmesh has a normal path to the side's tunnel exit and to the valley, and NO path (type 8) to the old
// graveyard waypoints on the shelf above it, to the flag room posts or to the flag stand. A respawned bot whose target
// lay in the base kept asking for the shelf and stood 100 to 190 s at its graveyard (9 of 10 Alliance bots in one
// match). The way out is always the tunnel exit, so the bot walks there first.
constexpr float GY_EXIT_HOP_RADIUS = 25.0f;     // a respawn lands within a few yards of the graveyard point
constexpr float GY_EXIT_HOP_MIN_TARGET = 40.0f; // a target this close is reached without the hop (a fight at the graveyard)
constexpr float GY_EXIT_HOP_AT_EXIT = 15.0f;    // a target already this close to the exit is the hop itself

inline bool NeedsGraveyardExitHop(float botToOwnGy, float botToTarget, float targetToOwnExit, bool carrying)
{
    if (carrying)
        return false;
    if (botToOwnGy > GY_EXIT_HOP_RADIUS)
        return false;
    return botToTarget > GY_EXIT_HOP_MIN_TARGET && targetToOwnExit > GY_EXIT_HOP_AT_EXIT;
}

// ---------------------------------------------------------------------------------------------------------------------
// The flag carrier runs home (owner rule 2026-11-17).
// ---------------------------------------------------------------------------------------------------------------------

// First live match (17:19 to 17:46, 10 pickups, no capture): a carrier fought the one attacker that reached it at the enemy
// stand, the guards joined, and every carrier died within 70 s. The rule now: the carrier runs home at once by the
// shortest route, uses its defensives and speed, and fights only when it cannot run (rooted, slowed) or has been caught
// alone. A human carrier does the same: run, never trade blows with the flag on the back.

// Enemy players this close to the carrier count against "caught alone" (the attackers are among them).
constexpr float CARRIER_ALONE_RADIUS = 35.0f;
// A run speed rate below this is a slow that matters (a 15 percent snare or more; the flag itself never lowers it).
constexpr float CARRIER_SLOWED_RATE = 0.85f;
// Below this health a lone attacker is no longer fought: the carrier runs and uses its defensives. Good and expert bots
// keep the higher floor; casual bots hold on longer.
constexpr float CARRIER_FIGHT_MIN_HP = 40.0f;
constexpr float CARRIER_FIGHT_MIN_HP_CASUAL = 25.0f;
// A fight that no hit has fed for this long ends: the carrier runs again (the old hold window).
constexpr std::uint32_t CARRIER_NO_HIT_MS = 8000;

// Targeting permission for a two-flag carrier, independent of its carry plan.
inline bool CarrierMayFight(std::uint32_t attackers, std::uint32_t enemiesNear, std::uint32_t fightersNear,
                            float healthPct, Tier tier)
{
    float const floor = BotSkillMath::Uses(tier, Tier::GOOD) ? CARRIER_FIGHT_MIN_HP : CARRIER_FIGHT_MIN_HP_CASUAL;
    return attackers == 1 && enemiesNear <= 1 && fightersNear == 0 && healthPct >= floor;
}

enum class CarrierChoice : std::uint8_t
{
    RUN,
    RUN_NEAR_CAPTURE,   // inside 25 yd of its own stand: never stops now
    FIGHT_ROOTED,
    FIGHT_SLOWED,
    FIGHT_ALONE
};

inline char const* CarrierChoiceName(CarrierChoice choice)
{
    switch (choice)
    {
        case CarrierChoice::RUN: return "run";
        case CarrierChoice::RUN_NEAR_CAPTURE: return "run-capture";
        case CarrierChoice::FIGHT_ROOTED: return "fight-rooted";
        case CarrierChoice::FIGHT_SLOWED: return "fight-slowed";
        case CarrierChoice::FIGHT_ALONE: return "fight-alone";
    }
    return "run";
}

inline bool CarrierFights(CarrierChoice choice)
{
    return choice == CarrierChoice::FIGHT_ROOTED || choice == CarrierChoice::FIGHT_SLOWED ||
           choice == CarrierChoice::FIGHT_ALONE;
}

// Everything the carrier's decision reads. The caller (WsgCarrier.cpp) fills it from the core.
struct CarrierFacts
{
    Tier tier = Tier::GOOD;
    bool nearOwnCapture = false;
    std::uint32_t attackers = 0;     // units that fight the carrier now
    std::uint32_t enemiesNear = 0;   // enemy players within CARRIER_ALONE_RADIUS
    std::uint32_t escortsNear = 0;   // our own bots within ESCORT_DISTANCE of the carrier
    bool rooted = false;             // cannot move: root, stun, fear
    bool slowed = false;             // run speed rate under CARRIER_SLOWED_RATE
    bool hitRecently = true;         // a hit landed within CARRIER_NO_HIT_MS
    float hpPercent = 100.0f;
};

struct CarrierOption
{
    CarrierChoice choice;
    int score;
    char const* why;
};

// Why the carrier keeps running, for the log; first match wins.
inline char const* RunWhy(CarrierFacts const& f)
{
    if (f.nearOwnCapture)
        return "near-capture";
    if (f.attackers == 0)
        return "no-attacker";
    if (!f.hitRecently)
        return "no-hit";
    if (f.attackers >= 2 || f.enemiesNear >= 2)
        return "outnumbered";
    if (f.escortsNear > 0)
        return "escorted";
    return "low-hp";
}

// Candidate choices with a deterministic score, highest first (the rules stay the fallback for a later decision layer).
// 960 rooted, 950 near the capture, 800 slowed, 600 alone, 100 run.
inline std::vector<CarrierOption> CarrierOptions(CarrierFacts const& f)
{
    std::vector<CarrierOption> out;
    out.push_back({ CarrierChoice::RUN, 100, RunWhy(f) });

    // Rooted, stunned or feared: running is not possible, so the carrier answers the attackers (core rule).
    if (f.rooted && f.attackers >= 1)
        out.push_back({ CarrierChoice::FIGHT_ROOTED, 960, "rooted" });

    // Inside 25 yd of its own stand it keeps running to the capture (a rooted carrier above still fights).
    if (f.nearOwnCapture)
        out.push_back({ CarrierChoice::RUN_NEAR_CAPTURE, 950, "near-capture" });

    // Slowed: the attackers cannot be outrun, so the carrier fights and the class rotation uses its defensives.
    if (f.slowed && f.attackers >= 1 && f.hitRecently && !f.nearOwnCapture)
        out.push_back({ CarrierChoice::FIGHT_SLOWED, 800, "slowed" });

    // Caught alone: exactly one attacker, nobody else of theirs within 35 yd, none of ours to peel it, and enough health
    // left. Every tier plays the same circumstances (owner 2026-11-17: casual carriers run too at the enemy stand); casual
    // bots only keep a lower health floor, so they fight on a little longer.
    if (f.attackers == 1 && f.hitRecently && !f.nearOwnCapture)
    {
        float const floor = BotSkillMath::Uses(f.tier, Tier::GOOD) ? CARRIER_FIGHT_MIN_HP : CARRIER_FIGHT_MIN_HP_CASUAL;
        if (f.enemiesNear <= 1 && f.escortsNear == 0 && f.hpPercent >= floor)
            out.push_back({ CarrierChoice::FIGHT_ALONE, 600, "alone" });
    }

    std::stable_sort(out.begin(), out.end(), [](CarrierOption const& a, CarrierOption const& b)
    {
        if (a.score != b.score)
            return a.score > b.score;
        return static_cast<std::uint8_t>(a.choice) < static_cast<std::uint8_t>(b.choice);
    });
    return out;
}

inline CarrierOption ChooseCarrier(CarrierFacts const& f)
{
    return CarrierOptions(f).front();
}

// Health at or below which a carrier under attack uses a big defensive (a class cooldown that keeps the flag: never an
// immunity, those drop it). Expert bots start earlier, casual bots late.
inline float CarrierDefensiveHp(Tier tier)
{
    switch (tier)
    {
        case Tier::EXPERT: return 65.0f;
        case Tier::GOOD: return 55.0f;
        case Tier::CASUAL: return 40.0f;
    }
    return 55.0f;
}

inline bool CarrierWantsDefensive(Tier tier, float hpPercent, std::uint32_t attackers)
{
    return attackers >= 1 && hpPercent <= CarrierDefensiveHp(tier);
}

// A run-speed spell (Sprint, Dash, Aspect of the Cheetah, ...) while chased, once, not already sped up, not rooted (a
// rooted carrier needs the freedom spell) and not inside the last yards of the capture.
inline bool CarrierWantsSpeed(std::uint32_t attackers, std::uint32_t enemiesNear, bool speedActive, bool rooted, bool nearOwnCapture)
{
    return !speedActive && !rooted && !nearOwnCapture && (attackers > 0 || enemiesNear > 0);
}

// A freedom spell (Hand of Freedom, Spirit Walk) when the carrier is rooted or slowed and somebody is on it.
inline bool CarrierWantsFreedom(bool rooted, bool slowed, std::uint32_t attackers)
{
    return (rooted || slowed) && attackers >= 1;
}

// ---------------------------------------------------------------------------------------------------------------------
// Healers follow the carrier (owner rule 2026-11-17).
// ---------------------------------------------------------------------------------------------------------------------

// At most two healers trail the carrier; a third stays with the lane (sheet section 7).
constexpr std::uint32_t HEALER_FOLLOWERS = 2;
// A healer tries to stay this close (yards): inside the heal reach, behind the melee escorts. Casual healers trail looser.
constexpr float HEALER_FOLLOW_YARDS = 15.0f;
constexpr float HEALER_FOLLOW_YARDS_CASUAL = 20.0f;
// A following healer closes up when the carrier is farther than this, also in combat (a healer in a fight otherwise
// stands where it is and lets the carrier run out of reach). Hysteresis: it closes to HEALER_CLOSE_TO.
constexpr float HEALER_LEASH_YARDS = 24.0f;
constexpr float HEALER_CLOSE_TO = 10.0f;
// A healer of the home guard joins only when the carrier is already this close to it (the carrier returns to its base).
constexpr float HOME_HEALER_JOIN_YARDS = 80.0f;

struct HealerCandidate
{
    std::uint32_t guidLow = 0;
    bool homeRole = false;
    float distToCarrier = 0.0f;
};

// The healers that follow: the nearest HEALER_FOLLOWERS to the carrier; a home guard healer only inside
// HOME_HEALER_JOIN_YARDS. Ties by GUID, so the pick is stable.
inline std::vector<std::uint32_t> PickHealerFollowers(std::vector<HealerCandidate> const& candidates)
{
    std::vector<HealerCandidate> eligible;
    for (HealerCandidate const& c : candidates)
        if (!c.homeRole || c.distToCarrier <= HOME_HEALER_JOIN_YARDS)
            eligible.push_back(c);

    std::sort(eligible.begin(), eligible.end(), [](HealerCandidate const& a, HealerCandidate const& b)
    {
        if (a.distToCarrier != b.distToCarrier)
            return a.distToCarrier < b.distToCarrier;
        return a.guidLow < b.guidLow;
    });

    std::vector<std::uint32_t> out;
    for (HealerCandidate const& c : eligible)
    {
        if (out.size() >= HEALER_FOLLOWERS)
            break;
        out.push_back(c.guidLow);
    }
    return out;
}

inline float HealerFollowYards(Tier tier)
{
    return BotSkillMath::Uses(tier, Tier::GOOD) ? HEALER_FOLLOW_YARDS : HEALER_FOLLOW_YARDS_CASUAL;
}

// A healer inside this reach of a hurt carrier stops to heal instead of running after it (an action that moves every tick
// would never let a heal start while the carrier keeps pace at the leash edge).
constexpr float HEALER_STOP_TO_HEAL_YARDS = 28.0f;
constexpr float HEALER_STOP_TO_HEAL_BELOW_HP = 90.0f;

inline bool HealerStopsToHeal(float distToCarrier, float carrierHpPercent)
{
    return distToCarrier <= HEALER_STOP_TO_HEAL_YARDS && carrierHpPercent < HEALER_STOP_TO_HEAL_BELOW_HP;
}

// A following healer moves when the carrier is out of its leash; `closing` is true while it is already on its way, and
// then it keeps going until it is close (no stop and go at the leash edge).
inline bool HealerShouldClose(float distToCarrier, bool closing)
{
    return closing ? distToCarrier > HEALER_CLOSE_TO : distToCarrier > HEALER_LEASH_YARDS;
}

// ---------------------------------------------------------------------------------------------------------------------
// The decision log line: at most one line per 10 s per bot and channel, and only when the decision changed.
// ---------------------------------------------------------------------------------------------------------------------

constexpr std::uint32_t DECISION_LOG_GAP_MS = 10000;

struct DecisionLogGate
{
    std::uint32_t lastMs = 0;
    std::uint32_t lastSignature = 0;
    bool logged = false;

    // True when a line should go out now; commits it. A change that falls inside the gap is not lost: the signature
    // stays different, so the next call after the gap logs it.
    bool Due(std::uint32_t signature, std::uint32_t nowMs)
    {
        if (logged && signature == lastSignature)
            return false;
        if (logged && nowMs - lastMs < DECISION_LOG_GAP_MS)
            return false;
        logged = true;
        lastSignature = signature;
        lastMs = nowMs ? nowMs : 1;
        return true;
    }
};

// ---------------------------------------------------------------------------------------------------------------------
// Call-outs.
// ---------------------------------------------------------------------------------------------------------------------

enum class Call : std::uint8_t
{
    EFC_TAKEN,
    EFC_MOVED,
    FLAG_GOT,
    FC_LOW,
    FLAG_DOWN,
    FLAG_BACK,
    INCOMING,
    CAPTURE,
    TIME_SCORE,
    ANSWER,
    STANCE,      // BT-481: the leader says what the team plays
    SCOUT,       // BT-483: an expert bot sees an enemy group on our half of the map
    EFC_PAST,    // BT-483: an expert bot sees the enemy carrier pass its own respawn point (Twin Peaks)
    MID_PUSH     // BT-694: the leader calls the idle home guards into a locked middle fight (Twin Peaks)
};

// One line per team every 6 s, at most 8 lines a minute per team, 20 s per bot between its own lines, and carrier position
// calls at most every 10 s (BgPlan::CallLimiter with the kind gap of this map's EFC_MOVED call).
struct CallLimiter : BgPlan::CallLimiter
{
    bool Allow(std::uint32_t guidLow, Call kind, std::uint32_t nowMs) const
    {
        BgPlan::KindGap gap;
        if (kind == Call::EFC_MOVED)
            gap = { true, static_cast<std::uint8_t>(kind), CALL_KIND_GAP_MOVED_MS };
        else if (kind == Call::STANCE)
            gap = { true, static_cast<std::uint8_t>(kind), CALL_KIND_GAP_STANCE_MS };
        return BgPlan::CallLimiter::Allow(guidLow, static_cast<std::uint8_t>(kind), nowMs, gap);
    }

    void Commit(std::uint32_t guidLow, Call kind, std::uint32_t nowMs)
    {
        BgPlan::CallLimiter::Commit(guidLow, static_cast<std::uint8_t>(kind), nowMs);
    }
};

// ---------------------------------------------------------------------------------------------------------------------
// Places and a human's calls.
// ---------------------------------------------------------------------------------------------------------------------

enum class Place : std::uint8_t
{
    NONE,
    FLAG_ROOM,
    TUNNEL,
    MID,
    RAMP,
    ROOF,
    GRAVEYARD,
    // Twin Peaks (BT-474): the lanes are named by the middle graveyard they hold, the buffs by their lane, the doors are the
    // flag room entrances.
    LANE_A,
    LANE_H,
    BERS_A,
    BERS_H,
    LEAF_A,
    LEAF_H,
    DOOR
};

inline char const* PlaceWord(Place place)
{
    switch (place)
    {
        case Place::FLAG_ROOM: return "fr";
        case Place::TUNNEL: return "tun";
        case Place::MID: return "mid";
        case Place::RAMP: return "ramp";
        case Place::ROOF: return "roof";
        case Place::GRAVEYARD: return "gy";
        case Place::LANE_A: return "lane a";
        case Place::LANE_H: return "lane h";
        case Place::BERS_A: return "bers a";
        case Place::BERS_H: return "bers h";
        case Place::LEAF_A: return "leaf a";
        case Place::LEAF_H: return "leaf h";
        case Place::DOOR: return "door";
        case Place::NONE: break;
    }
    return "mid";
}

// Which named place a point is in, for one team's view of the map. Anchors are the flag stands, the tunnel exits, the
// middle and the graveyards of the server's data (WsgPlan.cpp lists them); the nearest anchor within its radius wins.
struct PlaceAnchor
{
    Place place;
    float x;
    float y;
    float radius;
    // A zone of a higher priority names a point before a wider zone of a lower one (a graveyard inside its lane, BT-474); among
    // zones of the same priority the nearest one by its radius wins.
    int priority = 0;
};

inline Place ClassifyPlace(std::vector<PlaceAnchor> const& anchors, float x, float y)
{
    Place best = Place::NONE;
    float bestRatio = 2.0f;
    int bestPriority = 0;
    for (PlaceAnchor const& a : anchors)
    {
        float const dx = x - a.x;
        float const dy = y - a.y;
        float const ratio = std::sqrt(dx * dx + dy * dy) / a.radius;
        if (ratio > 1.0f)
            continue;
        if (best == Place::NONE || a.priority > bestPriority || (a.priority == bestPriority && ratio < bestRatio))
        {
            bestRatio = ratio;
            bestPriority = a.priority;
            best = a.place;
        }
    }
    return best;
}

}  // namespace WsgLogic

#endif
