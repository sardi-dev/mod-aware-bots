/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef AWARE_BOTS_PLAYSTYLE_POLICY_H
#define AWARE_BOTS_PLAYSTYLE_POLICY_H

#include <algorithm>
#include <cstdint>

// Design decision 0033 point 15: each random bot has a lasting playstyle that leans the global activity weights
// (AiPlayerbot.RpgStatusProbWeight.*) its own way, so the bots differ from one another instead of all drawing from one
// table. The style comes from the bot's guid alone: the same bot keeps its style across restarts with no stored state.
// A style only reweights the draw; it never adds an activity the bot could not do, never turns a weight of 0 on, and the
// availability checks of the draw (gathering skill, quest work in the zone) still apply. The decision logic only.
// docs/social/social.md has the design and the config keys (AiPlayerbot.Social.Playstyle.*).
namespace PlaystylePolicy
{
enum class Style : uint8_t
{
    Quester,
    GathererCrafter,
    Trader,
    Socializer,
    PvPer,
    Count,
};

// The activities of the draw, one for each weighted NewRpgStatus (RPG_IDLE is never drawn). The order is the table's.
enum class Activity : uint8_t
{
    WanderRandom,
    WanderNpc,
    GoGrind,
    GoCamp,
    DoQuest,
    TravelFlight,
    Rest,
    OutdoorPvp,
    DuelSpot,
    DoGather,
    Count,
};

constexpr uint32_t STYLE_COUNT = uint32_t(Style::Count);
constexpr uint32_t ACTIVITY_COUNT = uint32_t(Activity::Count);

// Percent of the cohort that gets each style, in Style order. Only the proportions count: they are not required to add up
// to 100. All zero makes every bot a quester.
struct Shares
{
    uint32_t percent[STYLE_COUNT] = { 40, 15, 10, 20, 15 };
};

// What each style multiplies the base weight of each activity by, in percent (100 leaves it). Rows are Style, columns
// Activity (WanderRandom, WanderNpc, GoGrind, GoCamp, DoQuest, TravelFlight, Rest, OutdoorPvp, DuelSpot, DoGather).
//   Quester:         quests first, fewer idle walks and fights for fun.
//   GathererCrafter: nodes and the mobs that drop materials, trainers and vendors on the way, long trips along gathering routes.
//   Trader:          vendors, auction house and mailbox errands (WanderNpc), market trips, some gathering to sell.
//   Socializer:      people and places (camps, duel spots, the inn), many short walks, fewer solo tasks.
//   PvPer:           outdoor fights and duel spots, grinding, little else.
constexpr uint32_t TABLE[STYLE_COUNT][ACTIVITY_COUNT] = {
    //  wanderR wanderN grind camp  quest flight rest  pvp   duel  gather
    {   70,     70,     70,   70,   150,  100,   100,  60,   60,   80  },  // Quester
    {   100,    100,    130,  100,  80,   130,   100,  50,   50,   300 },  // GathererCrafter
    {   60,     200,    100,  100,  70,   150,   100,  50,   50,   120 },  // Trader
    {   120,    150,    70,   150,  70,   100,   200,  100,  200,  60  },  // Socializer
    {   80,     60,     120,  80,   60,   100,   60,   300,  250,  50  },  // PvPer
};

// A weight changed by a style never drops below this percent of the base, so no activity dies out for one style.
constexpr uint32_t MIN_PERCENT = 10;

// One round of a 64 bit mixer (splitmix64): the guids of random bots come in runs, and a plain modulo would hand a run of
// neighbours the same style. The salt keeps the style apart from other guid-derived leans such as the invite bias.
inline uint64_t Mix(uint64_t value)
{
    constexpr uint64_t SALT = 0x5851F42D4C957F2DULL;
    value += SALT + 0x9E3779B97F4A7C15ULL;
    value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ULL;
    value = (value ^ (value >> 27)) * 0x94D049BB133111EBULL;
    return value ^ (value >> 31);
}

// The bot's lasting style: the guid's hash picks a point on the shares' cumulative line.
inline Style StyleFor(uint32_t botGuid, Shares const& shares)
{
    uint64_t total = 0;
    for (uint32_t i = 0; i < STYLE_COUNT; ++i)
        total += shares.percent[i];
    if (total == 0)
        return Style::Quester;

    uint64_t const point = Mix(botGuid) % total;
    uint64_t edge = 0;
    for (uint32_t i = 0; i < STYLE_COUNT; ++i)
    {
        edge += shares.percent[i];
        if (point < edge)
            return Style(i);
    }
    return Style::Quester;
}

// The percent a style applies to an activity, with strengthPercent scaling how far it leans away from 100: 0 turns the
// styles off, 100 is the table, 200 doubles every lean. Never below MIN_PERCENT.
inline uint32_t Percent(Style style, Activity activity, uint32_t strengthPercent)
{
    if (style >= Style::Count || activity >= Activity::Count)
        return 100;

    int64_t const table = TABLE[uint32_t(style)][uint32_t(activity)];
    int64_t const scaled = 100 + (table - 100) * int64_t(strengthPercent) / 100;
    return uint32_t(std::max<int64_t>(MIN_PERCENT, scaled));
}

// The base weight with the style applied. A weight of 0 stays 0 (the owner's switch for an activity wins), any other
// weight stays at least 1.
inline uint32_t ScaledWeight(uint32_t baseWeight, Style style, Activity activity, uint32_t strengthPercent)
{
    if (baseWeight == 0)
        return 0;
    uint64_t const scaled = (uint64_t(baseWeight) * Percent(style, activity, strengthPercent) + 50) / 100;
    return uint32_t(std::max<uint64_t>(1, scaled));
}

// The activity a NewRpgStatus value belongs to (PlayerbotAIConfig.h: RPG_GO_GRIND = 1 ... RPG_DO_GATHER = 10). False for
// RPG_IDLE and for anything else, which the draw never weights; NewRpgBaseAction.cpp pins the numbers with static_asserts.
inline bool ActivityOfStatus(int status, Activity& activity)
{
    switch (status)
    {
        case 1: activity = Activity::GoGrind; return true;
        case 2: activity = Activity::GoCamp; return true;
        case 3: activity = Activity::WanderRandom; return true;
        case 4: activity = Activity::WanderNpc; return true;
        case 5: activity = Activity::DoQuest; return true;
        case 6: activity = Activity::TravelFlight; return true;
        case 7: activity = Activity::Rest; return true;
        case 8: activity = Activity::OutdoorPvp; return true;
        case 9: activity = Activity::DuelSpot; return true;
        case 10: activity = Activity::DoGather; return true;
        default: return false;
    }
}

inline char const* StyleName(Style style)
{
    switch (style)
    {
        case Style::Quester: return "quester";
        case Style::GathererCrafter: return "gatherer-crafter";
        case Style::Trader: return "trader";
        case Style::Socializer: return "socializer";
        case Style::PvPer: return "pvper";
        case Style::Count: break;
    }
    return "quester";
}
}  // namespace PlaystylePolicy

#endif
