/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_ARENAMATH_H
#define _PLAYERBOT_ARENAMATH_H

// BT-154: the pure logic of the bot arena ladder, free of game types so that tests/ can run it: the team rating,
// the owner's clock and the activity schedule, the choice of the two teams of a match, the streak and the draw rule.
// The game side (ArenaLadder.cpp) feeds it plain numbers and applies what it returns.

#include "BotSkillMath.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

namespace ArenaMath
{
// ---------------------------------------------------------------------------------------------------------------
// Rating. The core's own team rating formula (its arena team code is never reached on this core, because no arena team
// object is ever created), so the numbers read like the real arena: the expected score is 1 / (1 + 10^((opp - own) /
// 650)) and the change is K * (score - expected). K is higher for a team's first games so a new team settles quickly.
// ---------------------------------------------------------------------------------------------------------------
constexpr double RATING_SCALE = 650.0;
constexpr std::uint32_t K_NEW_TEAM = 32;
constexpr std::uint32_t K_SETTLED_TEAM = 24;
constexpr std::uint32_t SETTLE_GAMES = 10;

// Seed rating by skill tier, plus a fixed jitter per team so that one tier is not one number.
constexpr std::int32_t SEED_EXPERT = 1750;
constexpr std::int32_t SEED_GOOD = 1500;
constexpr std::int32_t SEED_CASUAL = 1300;
constexpr std::int32_t SEED_JITTER = 40;  // -40 to +40

enum class Outcome : std::uint8_t
{
    LOSS,
    DRAW,
    WIN
};

// Why a match was recorded the way it was: 0 draw, 1 team A, 2 team B, 3 voided (matches the match table column).
enum class Winner : std::uint8_t
{
    DRAW = 0,
    TEAM_A = 1,
    TEAM_B = 2,
    VOIDED = 3
};

inline std::int32_t TierSeed(BotSkillMath::Tier tier)
{
    switch (tier)
    {
        case BotSkillMath::Tier::EXPERT:
            return SEED_EXPERT;
        case BotSkillMath::Tier::GOOD:
            return SEED_GOOD;
        case BotSkillMath::Tier::CASUAL:
            return SEED_CASUAL;
    }
    return SEED_GOOD;
}

// The jitter comes from the team id alone, so a restart does not change it.
inline std::int32_t SeedRating(BotSkillMath::Tier tier, std::uint32_t teamId)
{
    std::int32_t const span = 2 * SEED_JITTER + 1;
    std::int32_t const jitter = std::int32_t(BotSkillMath::Mix(teamId) % std::uint32_t(span)) - SEED_JITTER;
    return TierSeed(tier) + jitter;
}

inline double ExpectedScore(std::int32_t own, std::int32_t opponent)
{
    return 1.0 / (1.0 + std::pow(10.0, double(opponent - own) / RATING_SCALE));
}

inline std::uint32_t KFactor(std::uint32_t gamesPlayed)
{
    return gamesPlayed < SETTLE_GAMES ? K_NEW_TEAM : K_SETTLED_TEAM;
}

// Round half away from zero, so that +12.0 and -12.0 are symmetric.
inline std::int32_t RoundAway(double value)
{
    return std::int32_t(value < 0.0 ? std::ceil(value - 0.5) : std::floor(value + 0.5));
}

inline double ScoreOf(Outcome outcome)
{
    return outcome == Outcome::WIN ? 1.0 : outcome == Outcome::DRAW ? 0.5 : 0.0;
}

inline std::int32_t RatingChange(std::int32_t own, std::int32_t opponent, Outcome outcome, std::uint32_t gamesPlayed)
{
    return RoundAway(double(KFactor(gamesPlayed)) * (ScoreOf(outcome) - ExpectedScore(own, opponent)));
}

// The rating after a change: never below 0, as in the core.
inline std::uint32_t Apply(std::uint32_t rating, std::int32_t change)
{
    std::int64_t const next = std::int64_t(rating) + change;
    return next < 0 ? 0u : std::uint32_t(next);
}

inline Outcome Flip(Outcome outcome)
{
    return outcome == Outcome::WIN ? Outcome::LOSS : outcome == Outcome::LOSS ? Outcome::WIN : Outcome::DRAW;
}

// A streak is +n for n wins in a row, -n for n losses in a row and 0 after a draw.
inline std::int32_t NextStreak(std::int32_t streak, Outcome outcome)
{
    if (outcome == Outcome::WIN)
        return streak > 0 ? streak + 1 : 1;
    if (outcome == Outcome::LOSS)
        return streak < 0 ? streak - 1 : -1;
    return 0;
}

// A replaced partner pulls the rating a quarter of the way back to the tier base (the team keeps name and history).
constexpr double PARTNER_REPLACE_PULL = 0.25;
inline std::uint32_t RatingAfterPartnerReplaced(std::uint32_t rating, std::int32_t tierBase)
{
    double const pulled = double(rating) - PARTNER_REPLACE_PULL * (double(rating) - double(tierBase));
    return std::uint32_t(std::max(0, RoundAway(pulled)));
}

// ---------------------------------------------------------------------------------------------------------------
// The draw rule: a match of two bot teams that nobody has won after this long is cut and counts as a draw. A match
// with a human in it is never cut.
// ---------------------------------------------------------------------------------------------------------------
constexpr std::uint32_t BOT_MATCH_CUT_SECONDS = 12 * 60;

inline bool IsCutAsDraw(std::uint32_t elapsedSeconds, bool hasHuman, std::uint32_t cutSeconds = BOT_MATCH_CUT_SECONDS)
{
    return !hasHuman && cutSeconds != 0 && elapsedSeconds >= cutSeconds;
}

// ---------------------------------------------------------------------------------------------------------------
// The owner's clock (US Eastern with daylight saving) from UTC alone, so that no time zone database is needed in the
// container and tests can fix the time. clockOffsetMinutes is the test hook that moves the clock to 20:00 or 03:00.
// ---------------------------------------------------------------------------------------------------------------
constexpr std::int32_t OWNER_UTC_OFFSET_HOURS = -5;  // standard time
constexpr std::int64_t SECONDS_PER_HOUR = 3600;
constexpr std::int64_t SECONDS_PER_DAY = 86400;

struct LocalTime
{
    std::int32_t year;
    std::uint32_t month;    // 1 to 12
    std::uint32_t day;      // 1 to 31
    std::uint32_t hour;     // 0 to 23
    std::uint32_t minute;   // 0 to 59
    std::uint32_t weekday;  // 0 Sunday to 6 Saturday
    bool daylight;          // daylight saving time was applied
};

// Days since 1970-01-01 of a civil date (proleptic Gregorian); the well known days_from_civil algorithm.
inline std::int64_t DaysFromCivil(std::int32_t year, std::uint32_t month, std::uint32_t day)
{
    year -= month <= 2 ? 1 : 0;
    std::int64_t const era = (year >= 0 ? year : year - 399) / 400;
    std::uint32_t const yoe = std::uint32_t(year - era * 400);
    std::uint32_t const shifted = month > 2 ? month - 3 : month + 9;  // March is month 0 of the shifted year
    std::uint32_t const doy = (153 * shifted + 2) / 5 + day - 1;
    std::uint32_t const doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + std::int64_t(doe) - 719468;
}

inline void CivilFromDays(std::int64_t days, std::int32_t& year, std::uint32_t& month, std::uint32_t& day)
{
    days += 719468;
    std::int64_t const era = (days >= 0 ? days : days - 146096) / 146097;
    std::uint32_t const doe = std::uint32_t(days - era * 146097);
    std::uint32_t const yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    std::int64_t const y = std::int64_t(yoe) + era * 400;
    std::uint32_t const doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    std::uint32_t const mp = (5 * doy + 2) / 153;
    day = doy - (153 * mp + 2) / 5 + 1;
    month = mp < 10 ? mp + 3 : mp - 9;
    year = std::int32_t(y + (month <= 2 ? 1 : 0));
}

// 0 Sunday to 6 Saturday; 1970-01-01 was a Thursday.
inline std::uint32_t WeekdayOfDays(std::int64_t days)
{
    std::int64_t const w = (days + 4) % 7;
    return std::uint32_t(w < 0 ? w + 7 : w);
}

// The n-th Sunday (1 based) of a month, as days since the epoch.
inline std::int64_t NthSunday(std::int32_t year, std::uint32_t month, std::uint32_t n)
{
    std::int64_t const first = DaysFromCivil(year, month, 1);
    std::uint32_t const toSunday = (7 - WeekdayOfDays(first)) % 7;
    return first + toSunday + std::int64_t(n - 1) * 7;
}

// US rule: daylight saving time runs from 02:00 standard time on the second Sunday of March to 02:00 daylight time on
// the first Sunday of November. Both instants are computed in UTC for the given year of the standard clock.
inline bool IsUsDaylight(std::int64_t utcEpoch, std::int32_t standardOffsetHours)
{
    std::int64_t const standardSeconds = utcEpoch + standardOffsetHours * SECONDS_PER_HOUR;
    std::int32_t year;
    std::uint32_t month;
    std::uint32_t day;
    CivilFromDays((standardSeconds >= 0 ? standardSeconds : standardSeconds - (SECONDS_PER_DAY - 1)) / SECONDS_PER_DAY,
        year, month, day);

    std::int64_t const startUtc = NthSunday(year, 3, 2) * SECONDS_PER_DAY + 2 * SECONDS_PER_HOUR - standardOffsetHours * SECONDS_PER_HOUR;
    std::int64_t const endUtc = NthSunday(year, 11, 1) * SECONDS_PER_DAY + 2 * SECONDS_PER_HOUR - (standardOffsetHours + 1) * SECONDS_PER_HOUR;
    return utcEpoch >= startUtc && utcEpoch < endUtc;
}

inline LocalTime OwnerLocalTime(std::int64_t utcEpoch, std::int32_t standardOffsetHours, bool usDaylight,
    std::int32_t clockOffsetMinutes = 0)
{
    bool const daylight = usDaylight && IsUsDaylight(utcEpoch, standardOffsetHours);
    std::int64_t const local = utcEpoch + (standardOffsetHours + (daylight ? 1 : 0)) * SECONDS_PER_HOUR +
        std::int64_t(clockOffsetMinutes) * 60;
    std::int64_t const days = (local >= 0 ? local : local - (SECONDS_PER_DAY - 1)) / SECONDS_PER_DAY;
    std::int64_t const secondOfDay = local - days * SECONDS_PER_DAY;

    LocalTime result;
    CivilFromDays(days, result.year, result.month, result.day);
    result.hour = std::uint32_t(secondOfDay / SECONDS_PER_HOUR);
    result.minute = std::uint32_t((secondOfDay % SECONDS_PER_HOUR) / 60);
    result.weekday = WeekdayOfDays(days);
    result.daylight = daylight;
    return result;
}

// ---------------------------------------------------------------------------------------------------------------
// The activity weight, in percent. Quiet from 23:00 to 11:00 every day; then 11 to 17, 17 to 19 and 19 to 23, with
// busier daytime and early evening on Saturday and Sunday (owner time).
// ---------------------------------------------------------------------------------------------------------------
constexpr std::uint32_t QUIET_FROM_HOUR = 23;
constexpr std::uint32_t QUIET_UNTIL_HOUR = 11;
constexpr std::uint32_t AFTERNOON_UNTIL_HOUR = 17;
constexpr std::uint32_t EARLY_EVENING_UNTIL_HOUR = 19;

constexpr std::uint32_t WEIGHT_WEEKDAY_AFTERNOON = 25;
constexpr std::uint32_t WEIGHT_WEEKDAY_EARLY_EVENING = 60;
constexpr std::uint32_t WEIGHT_WEEKEND_AFTERNOON = 60;
constexpr std::uint32_t WEIGHT_WEEKEND_EARLY_EVENING = 80;
constexpr std::uint32_t WEIGHT_PRIME_TIME = 100;

inline bool IsQuietHour(std::uint32_t hour)
{
    return hour >= QUIET_FROM_HOUR || hour < QUIET_UNTIL_HOUR;
}

inline bool IsWeekend(std::uint32_t weekday)
{
    return weekday == 0 || weekday == 6;
}

inline std::uint32_t ActivityWeightPercent(LocalTime const& time)
{
    if (IsQuietHour(time.hour))
        return 0;

    bool const weekend = IsWeekend(time.weekday);
    if (time.hour < AFTERNOON_UNTIL_HOUR)
        return weekend ? WEIGHT_WEEKEND_AFTERNOON : WEIGHT_WEEKDAY_AFTERNOON;
    if (time.hour < EARLY_EVENING_UNTIL_HOUR)
        return weekend ? WEIGHT_WEEKEND_EARLY_EVENING : WEIGHT_WEEKDAY_EARLY_EVENING;
    return WEIGHT_PRIME_TIME;
}

// The scheduler runs once a minute. The chance that a pass starts a bot match is weight * peakMatchesPerHour / 60;
// `roll` is a number in [0, 1) the caller draws (a parameter, so that tests stay deterministic).
constexpr std::uint32_t SCHEDULER_PASSES_PER_HOUR = 60;

inline double StartChance(std::uint32_t weightPercent, std::uint32_t peakMatchesPerHour)
{
    double const chance = double(weightPercent) / 100.0 * double(peakMatchesPerHour) / double(SCHEDULER_PASSES_PER_HOUR);
    return std::min(1.0, chance);
}

inline bool ShouldStart(std::uint32_t weightPercent, std::uint32_t peakMatchesPerHour, double roll)
{
    return roll < StartChance(weightPercent, peakMatchesPerHour);
}

// The scheduler does nothing while the world update is slow (the average update time in milliseconds).
inline bool WorldLoadAllows(std::uint32_t updateMs, std::uint32_t maxUpdateMs)
{
    return updateMs <= maxUpdateMs;
}

// ---------------------------------------------------------------------------------------------------------------
// The choice of the two teams of a match.
//   A is the available team that has waited longest (ties: the lower id).
//   B is the available team of the same bracket and level band, not A, with the lowest cost
//     cost = |R_A - R_B| + REPEAT_PENALTY * (times B was A's opponent in A's last matches) - OPPOSING_FACTION_BONUS
//   and the match is made when cost is within the window, which widens by WINDOW_STEP for every pass in which A found
//   nobody, up to WINDOW_MAX, so that a lone strong team still gets a match after a few minutes.
// ---------------------------------------------------------------------------------------------------------------
constexpr std::int32_t WINDOW_BASE = 150;
constexpr std::int32_t WINDOW_STEP = 50;
constexpr std::int32_t WINDOW_MAX = 400;
constexpr std::int32_t REPEAT_PENALTY = 40;
constexpr std::int32_t OPPOSING_FACTION_BONUS = 20;

struct TeamView
{
    std::uint32_t id;
    std::int32_t rating;
    std::uint8_t bracket;
    std::uint8_t band;
    std::uint8_t faction;
    std::uint32_t idleSeconds;
    std::uint32_t failedPasses;
    std::vector<std::uint32_t> recentOpponents;  // ids of A's last matches, newest last
};

struct PairChoice
{
    bool found = false;
    std::uint32_t teamA = 0;
    std::uint32_t teamB = 0;
    std::int32_t cost = 0;
    std::vector<std::uint32_t> unmatched;  // teams that were tried as A and found nobody; the caller counts a failed pass
};

inline std::int32_t WindowFor(std::uint32_t failedPasses)
{
    std::int64_t const window = std::int64_t(WINDOW_BASE) + std::int64_t(WINDOW_STEP) * failedPasses;
    return std::int32_t(std::min<std::int64_t>(window, WINDOW_MAX));
}

inline std::int32_t PairCost(TeamView const& a, TeamView const& b)
{
    std::int32_t cost = std::abs(a.rating - b.rating);
    for (std::uint32_t const opponent : a.recentOpponents)
        if (opponent == b.id)
            cost += REPEAT_PENALTY;
    if (a.faction != b.faction)
        cost -= OPPOSING_FACTION_BONUS;
    return cost;
}

inline PairChoice PickPair(std::vector<TeamView> const& available)
{
    std::vector<TeamView const*> order;
    order.reserve(available.size());
    for (TeamView const& team : available)
        order.push_back(&team);
    std::sort(order.begin(), order.end(), [](TeamView const* x, TeamView const* y)
    {
        return x->idleSeconds != y->idleSeconds ? x->idleSeconds > y->idleSeconds : x->id < y->id;
    });

    PairChoice choice;
    for (TeamView const* a : order)
    {
        TeamView const* best = nullptr;
        std::int32_t bestCost = 0;
        for (TeamView const* b : order)
        {
            if (b == a || b->bracket != a->bracket || b->band != a->band)
                continue;

            std::int32_t const cost = PairCost(*a, *b);
            if (!best || cost < bestCost || (cost == bestCost && b->id < best->id))
            {
                best = b;
                bestCost = cost;
            }
        }

        if (best && bestCost <= WindowFor(a->failedPasses))
        {
            choice.found = true;
            choice.teamA = a->id;
            choice.teamB = best->id;
            choice.cost = bestCost;
            return choice;
        }

        choice.unmatched.push_back(a->id);
    }

    return choice;
}

// The level band of a level: the core's PvP bracket (5 levels each from 10; 90 alone). Teams stay in the band of
// their members and move up with them.
constexpr std::uint32_t BAND_FIRST_LEVEL = 10;
constexpr std::uint32_t BAND_WIDTH = 5;
constexpr std::uint32_t BAND_TOP_LEVEL = 90;
constexpr std::uint32_t BAND_TOP = 16;

inline std::uint32_t BandOfLevel(std::uint32_t level)
{
    if (level >= BAND_TOP_LEVEL)
        return BAND_TOP;
    return level < BAND_FIRST_LEVEL ? 0 : (level - BAND_FIRST_LEVEL) / BAND_WIDTH;
}

inline std::uint32_t BandMinLevel(std::uint32_t band)
{
    return band >= BAND_TOP ? BAND_TOP_LEVEL : BAND_FIRST_LEVEL + band * BAND_WIDTH;
}

inline std::uint32_t BandMaxLevel(std::uint32_t band)
{
    return band >= BAND_TOP ? BAND_TOP_LEVEL : BandMinLevel(band) + BAND_WIDTH - 1;
}

// A team is worth forming for a band only when the bots can reach it: the band must lie at or above the minimum level.
inline bool BandAllowed(std::uint32_t band, std::uint32_t minLevel)
{
    return BandMaxLevel(band) >= minLevel;
}

// What the periodic check of a team finds. The members stay together; a team moves up a band when both members have
// reached it (a team follows its members), and it is retired when the members keep being out of step or fall below the
// minimum level for `retireAfter` checks in a row (a re-created bot, a levelled-away partner).
enum class Validation : std::uint8_t
{
    OK,
    MOVE_BAND,
    OUT_OF_STEP,   // the members are in different bands
    BELOW_MINIMUM, // both are in a band under the minimum level
    RETIRE
};

struct ValidationResult
{
    Validation action = Validation::OK;
    std::uint32_t band = 0;           // the band to keep or move to
    std::uint32_t failedChecks = 0;   // the new count of failed checks in a row
};

inline ValidationResult ValidateTeam(std::uint32_t currentBand, std::uint32_t levelFirst, std::uint32_t levelSecond,
    std::uint32_t minLevel, std::uint32_t failedChecks, std::uint32_t retireAfter)
{
    ValidationResult result;
    result.band = currentBand;
    std::uint32_t const bandFirst = BandOfLevel(levelFirst);
    std::uint32_t const bandSecond = BandOfLevel(levelSecond);

    if (bandFirst == bandSecond && BandAllowed(bandFirst, minLevel))
    {
        result.action = bandFirst == currentBand ? Validation::OK : Validation::MOVE_BAND;
        result.band = bandFirst;
        result.failedChecks = 0;
        return result;
    }

    result.action = bandFirst == bandSecond ? Validation::BELOW_MINIMUM : Validation::OUT_OF_STEP;
    result.failedChecks = failedChecks + 1;
    if (result.failedChecks >= retireAfter)
        result.action = Validation::RETIRE;
    return result;
}

// A team plays at the tier of its weakest member (owner, 2026-11-12). Tier is ordered expert < good < casual.
inline BotSkillMath::Tier WeakestTier(BotSkillMath::Tier a, BotSkillMath::Tier b)
{
    return std::uint8_t(a) >= std::uint8_t(b) ? a : b;
}
}  // namespace ArenaMath

#endif
