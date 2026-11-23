#ifndef AWARE_BOTS_QUEST_HAND_IN_POLICY_H
#define AWARE_BOTS_QUEST_HAND_IN_POLICY_H

#include <cstddef>
#include <cstdint>
#include <vector>

// A random bot's trip to the ender of a completed quest (BT-210). The quest picker only knows quest POIs in the bot's
// zone within 1,500 yd and the opportunistic turn-in looks 80 yd around, so breadcrumbs such as "Continue to
// Stormwind" stayed complete in the log for good. The bot now walks (or takes a route the travel graph offers) to the
// nearest spawn of the ender on its own continent and hands the quest in by talking to it. One trip at a time; a trip
// that runs out of time, gets stuck or finds nobody at the spawn fails and goes to the quest retry book (BT-208).
namespace QuestHandInPolicy
{
// Arrived at the ender's spawn point: the same distance as a GO_CAMP arrival.
constexpr float ARRIVE_DIST = 10.0f;
// At the spawn point this long without a turn-in: the ender is not there (phase, event, killed, wandering off).
constexpr uint32_t ARRIVAL_WAIT_MS = 60 * 1000;
// From this far the trip first asks the travel graph for a route with a flight path or transport leg.
constexpr float ROUTE_DIST = 1500.0f;
// The time budget assumes a slow pace, since fights, eating and detours to quest givers happen on the way.
constexpr float PLANNED_YARDS_PER_SECOND = 2.5f;
constexpr uint32_t MIN_BUDGET_MS = 5 * 60 * 1000;
constexpr uint32_t MAX_BUDGET_MS = 60 * 60 * 1000;

struct Spawn
{
    uint32_t spawnId = 0;
    uint32_t entry = 0;
    bool gameObject = false;
    uint32_t map = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

// The nearest usable ender spawn of one rewardable quest.
struct Candidate
{
    uint32_t questId = 0;
    Spawn spawn;
    float dist = 0.0f;
};

struct Trip
{
    bool active = false;
    uint32_t questId = 0;
    Spawn ender;
    uint32_t zone = 0;
    uint32_t startMs = 0;
    uint32_t budgetMs = 0;
    uint32_t arrivedMs = 0;  // first time at the spawn point (0 = not yet)
    uint32_t workedMs = 0;   // last tick the bot worked the trip (BT-374)
    bool crossMap = false;   // the ender is on another continent (BT-375)
};

// BT-374: a bot that leaves its trip for a while (a dungeon through the finder, a group, a long detour) keeps its time
// budget: the clock reads active time, which also runs in the dungeon, so on 2026-11-17 the bots of the night's proof
// runs came back to "out of time" give-ups with 980 s of 590 s used and the start distance still left. A gap of more
// than PAUSE_MS since the last worked tick moves the trip's clock marks forward by the gap.
constexpr uint32_t PAUSE_MS = 2 * 60 * 1000;

inline void Resume(Trip& trip, uint32_t now)
{
    if (!trip.active)
        return;
    if (trip.workedMs && uint32_t(now - trip.workedMs) > PAUSE_MS)
    {
        uint32_t const gap = now - trip.workedMs;
        trip.startMs += gap;
        if (trip.arrivedMs)
            trip.arrivedMs += gap;
    }
    trip.workedMs = now;
}

enum class Step
{
    Travel,
    Wait,
    Expired,
    NotThere
};

inline float DistSq2d(Spawn const& spawn, float x, float y)
{
    float const dx = spawn.x - x;
    float const dy = spawn.y - y;
    return dx * dx + dy * dy;
}

// Index of the nearest spawn on the bot's map, or -1. Spawns elsewhere are not on the bot's continent.
inline int NearestSpawn(std::vector<Spawn> const& spawns, uint32_t map, float x, float y)
{
    int best = -1;
    float bestSq = 0.0f;
    for (std::size_t i = 0; i < spawns.size(); ++i)
    {
        if (spawns[i].map != map)
            continue;
        float const sq = DistSq2d(spawns[i], x, y);
        if (best < 0 || sq < bestSq)
        {
            best = int(i);
            bestSq = sq;
        }
    }
    return best;
}

// The quest whose ender is nearest goes first; equal distances go to the lower quest id so the pick is stable.
inline int PickCandidate(std::vector<Candidate> const& candidates)
{
    int best = -1;
    for (std::size_t i = 0; i < candidates.size(); ++i)
    {
        Candidate const& c = candidates[i];
        if (best < 0 || c.dist < candidates[best].dist ||
            (c.dist == candidates[best].dist && c.questId < candidates[best].questId))
            best = int(i);
    }
    return best;
}

// BT-231: a trip from ROUTE_DIST on is made only for a quest still at the bot's level. A low-level one is handed in
// when the bot passes its ender (or its hand-in POI is in reach of the DO_QUEST pick).
inline bool WorthTheTrip(float dist, bool lowLevel)
{
    return !lowLevel || dist < ROUTE_DIST;
}

inline uint32_t BudgetMs(float dist)
{
    if (dist < 0.0f)
        dist = 0.0f;
    float const ms = float(MIN_BUDGET_MS) + dist / PLANNED_YARDS_PER_SECOND * 1000.0f;
    return ms >= float(MAX_BUDGET_MS) ? MAX_BUDGET_MS : uint32_t(ms);
}

inline Trip Start(Candidate const& candidate, uint32_t zone, uint32_t now)
{
    Trip trip;
    trip.active = true;
    trip.questId = candidate.questId;
    trip.ender = candidate.spawn;
    trip.zone = zone;
    trip.startMs = now;
    trip.budgetMs = BudgetMs(candidate.dist);
    trip.workedMs = now;
    return trip;
}

// BT-375: the ender of a completed quest stands on another continent ("Hero's Call: Outland!", "Report to General
// Kirika" in a bot that took a boat): 415 of the 1,384 completed entries in the bots' logs on 2026-11-17, never handed
// in, since a trip only looked at the bot's own map. With no ender on its map and the quest still at its level, the bot
// now travels there by boat, zeppelin or portal over the travel graph, as the roaming camp trips do. The distance over
// the sea is not known, so the trip gets the longest budget.
constexpr uint32_t CROSS_MAP_BUDGET_MS = MAX_BUDGET_MS;

// The cross-continent candidate: the lowest quest id first, so the pick is stable between tries.
inline int PickCrossMap(std::vector<Candidate> const& candidates)
{
    int best = -1;
    for (std::size_t i = 0; i < candidates.size(); ++i)
        if (best < 0 || candidates[i].questId < candidates[best].questId)
            best = int(i);
    return best;
}

// BT-380: the first ender spawn the route test accepts, or -1. A trip over the sea starts only where the travel graph
// can carry the bot: with the graph off every trip ended in seconds as "no route to the ender's continent".
template <typename RouteKnown>
inline int FirstReachable(std::vector<Spawn> const& spawns, RouteKnown routeKnown)
{
    for (std::size_t i = 0; i < spawns.size(); ++i)
        if (routeKnown(spawns[i]))
            return int(i);
    return -1;
}

inline Trip StartCrossMap(Candidate const& candidate, uint32_t zone, uint32_t now)
{
    Trip trip = Start(candidate, zone, now);
    trip.budgetMs = CROSS_MAP_BUDGET_MS;
    trip.crossMap = true;
    return trip;
}

inline bool Expired(Trip const& trip, uint32_t now)
{
    return trip.active && uint32_t(now - trip.startMs) >= trip.budgetMs;
}

// True when a move to (map, x, y) is this trip's move to the ender, which must never end in a stuck teleport.
inline bool SameDest(Trip const& trip, uint32_t map, float x, float y)
{
    return trip.active && trip.ender.map == map && DistSq2d(trip.ender, x, y) < 1.0f;
}

// One tick of a running trip, dist being the bot's distance to the ender's spawn point.
inline Step Judge(Trip& trip, uint32_t now, float dist)
{
    if (Expired(trip, now))
        return Step::Expired;
    if (dist > ARRIVE_DIST && !trip.arrivedMs)
        return Step::Travel;
    if (!trip.arrivedMs)
    {
        // 0 means "not yet", so an arrival at tick 0 is kept as tick 1.
        trip.arrivedMs = now ? now : 1;
        return Step::Wait;
    }
    return uint32_t(now - trip.arrivedMs) >= ARRIVAL_WAIT_MS ? Step::NotThere : Step::Wait;
}

inline char const* StepName(Step step)
{
    switch (step)
    {
        case Step::Travel: return "travel";
        case Step::Wait: return "wait";
        case Step::Expired: return "out of time";
        case Step::NotThere: return "ender not found at the spawn";
    }
    return "unknown";
}
}  // namespace QuestHandInPolicy

#endif
