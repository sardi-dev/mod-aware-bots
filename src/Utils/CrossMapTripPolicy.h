/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef AWARE_BOTS_CROSS_MAP_TRIP_POLICY_H
#define AWARE_BOTS_CROSS_MAP_TRIP_POLICY_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

// The pure parts of a roam trip to another continent (NewRpgRoamTransport.cpp): when the hearthstone may stand for a
// leg, how the hearth leg is cast, which travel nodes a route may start from, what the pre-check of a quest trip
// remembers, and how a trip's outcome is named in the log.
namespace CrossMapTripPolicy
{
constexpr uint32_t HEARTHSTONE_ITEM = 6948;
constexpr uint32_t HEARTHSTONE_SPELL = 8690;

// ---- BT-440: the hearth leg ----------------------------------------------------------------------------------------
// A route may use the hearthstone only when the bot can cast it now (alive, the stone in its bags, no cooldown) and it
// is bound on the continent the trip goes to: a hearth to a third continent leaves the trip on the wrong map with the
// stone on cooldown, and a hearth on the bot's own map is never a cross-continent leg.
inline bool HearthLegUsable(bool alive, bool hasStone, bool onCooldown, uint32_t homeMap, uint32_t startMap,
                            uint32_t goalMap)
{
    return alive && hasStone && !onCooldown && homeMap == goalMap && homeMap != startMap;
}

// The legs that make a route worth a trip. A hearth leg counts only for a destination on another map.
inline uint32_t QualifyingLegs(uint32_t transport, uint32_t flight, uint32_t portal, uint32_t hearth, bool crossMap)
{
    return transport + flight + portal + (crossMap ? hearth : 0u);
}

// After the cast was sent, the bot is either still casting, already on the home map (the spell teleported it), or
// neither: the cast was refused or interrupted (moved, attacked). A refused cast is tried again a few times.
constexpr uint32_t HEARTH_SETTLE_MS = 2000;  // a finished cast hands the bot to the teleport within this time
constexpr uint32_t HEARTH_MAX_TRIES = 3;
constexpr float HEARTH_ARRIVE_DIST = 100.0f;  // from the home bind: the spell lands the bot on it

enum class HearthStep
{
    Cast,     // send the cast now
    Wait,     // casting, or the teleport is about to happen
    Arrived,  // on the home map near the bind
    GiveUp    // refused or interrupted too often
};

inline HearthStep NextHearthStep(bool castSent, bool arrived, bool casting, uint32_t sinceCastMs, uint32_t tries)
{
    if (arrived)
        return HearthStep::Arrived;
    if (!castSent)
        return tries >= HEARTH_MAX_TRIES ? HearthStep::GiveUp : HearthStep::Cast;
    if (casting || sinceCastMs < HEARTH_SETTLE_MS)
        return HearthStep::Wait;
    return tries >= HEARTH_MAX_TRIES ? HearthStep::GiveUp : HearthStep::Cast;
}

// ---- BT-441: the route's start node --------------------------------------------------------------------------------
// Candidates are the nearest travel nodes on the bot's map, nearest first. One within freeReach is walked as before (no
// path test); farther ones count only when the bot can walk to them. When none can be walked to, the nearest is kept
// alone: a hearth route casts where the bot stands, and any other route from it is refused later (start-unreachable).
struct StartCandidate
{
    float dist = 0.0f;
    bool reachable = false;
};

inline std::vector<size_t> PickStartCandidates(std::vector<StartCandidate> const& candidates, float freeReach,
                                               size_t keep)
{
    std::vector<size_t> picked;
    for (size_t i = 0; i < candidates.size() && picked.size() < keep; ++i)
        if (candidates[i].dist <= freeReach || candidates[i].reachable)
            picked.push_back(i);
    if (picked.empty() && !candidates.empty() && keep)
        picked.push_back(0);
    return picked;
}

// ---- BT-442: the pre-check of a cross-continent quest trip ---------------------------------------------------------
// The pre-check builds and validates the same route the trip would start with, and remembers the answer per
// destination continent: a yes for a minute while the bot stays where it was (the trip reuses that route), a no for
// the retry gate's ten minutes while the bot stays in the area.
constexpr uint32_t PLAN_OK_TTL_MS = 60 * 1000;
constexpr float PLAN_OK_RADIUS = 10.0f;
constexpr uint32_t PLAN_FAIL_TTL_MS = 10 * 60 * 1000;
constexpr float PLAN_FAIL_RADIUS = 200.0f;

struct PlanMemo
{
    bool valid = false;
    bool ok = false;
    uint32_t atMs = 0;
    uint32_t fromMap = 0;
    float x = 0.0f, y = 0.0f, z = 0.0f;
    uint32_t destMap = 0;
};

enum class MemoAnswer
{
    Unknown,
    Yes,
    No
};

inline MemoAnswer AskMemo(PlanMemo const& memo, uint32_t now, uint32_t fromMap, float x, float y, float z,
                          uint32_t destMap)
{
    if (!memo.valid || memo.fromMap != fromMap || memo.destMap != destMap)
        return MemoAnswer::Unknown;
    float const dx = x - memo.x, dy = y - memo.y, dz = z - memo.z;
    float const distSq = dx * dx + dy * dy + dz * dz;
    uint32_t const age = now - memo.atMs;
    if (memo.ok)
        return (age < PLAN_OK_TTL_MS && distSq < PLAN_OK_RADIUS * PLAN_OK_RADIUS) ? MemoAnswer::Yes
                                                                                : MemoAnswer::Unknown;
    return (age < PLAN_FAIL_TTL_MS && distSq < PLAN_FAIL_RADIUS * PLAN_FAIL_RADIUS) ? MemoAnswer::No
                                                                                    : MemoAnswer::Unknown;
}

// A failed trip toward a continent closes the pre-check for that continent as long as the trip's own retry gate would.
inline bool RetryGateClosed(uint32_t failAtMs, uint32_t now, uint32_t gateMs, uint32_t failedDestMap, uint32_t destMap)
{
    return failAtMs && uint32_t(now - failAtMs) < gateMs && failedDestMap == destMap;
}

// ---- BT-443: one info line per cross-continent trip outcome -------------------------------------------------------
enum class Outcome
{
    Arrived,  // the bot stands on the destination's continent
    GaveUp,   // the trip started and was abandoned before that
    NoRoute   // no route the trip could follow was found
};

inline char const* OutcomeName(Outcome o)
{
    switch (o)
    {
        case Outcome::Arrived: return "arrived";
        case Outcome::GaveUp: return "gave-up";
        case Outcome::NoRoute: return "noroute";
    }
    return "?";
}

// The end of a started trip: arrived when the bot is on the destination's map, whatever ended the trip.
inline Outcome EndOutcome(uint32_t botMap, uint32_t destMap)
{
    return botMap == destMap ? Outcome::Arrived : Outcome::GaveUp;
}

// Noroute reasons that only repeat an outcome already logged (the retry gate) or a passing state (on a ride) are no
// outcome of their own.
inline bool NoRouteIsOutcome(char const* why)
{
    return why && std::strcmp(why, "retry-gate") != 0 && std::strcmp(why, "busy") != 0;
}
// ---- BT-444: where the route search of a roam trip ends ------------------------------------------------------------
// AC's A* ends at the goal node or at the first walkable node on any map other than the start's. A roam trip to another
// map ends only on the goal's own map, so its route never stops in a transit map such as the Deeprun Tram. True when a
// walkable node on curMap ends the search (the goal node always does).
inline bool SearchMayEndOn(uint32_t curMap, uint32_t startMap, uint32_t goalMap, bool roamTrip)
{
    return roamTrip ? (curMap == goalMap && goalMap != startMap) : curMap != startMap;
}

}  // namespace CrossMapTripPolicy

#endif
