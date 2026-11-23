/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_ARENANPCSPOTS_H
#define _PLAYERBOT_ARENANPCSPOTS_H

// BT-154: where the ladder NPC "Pit Chronicler" stands, and the rules of when the module puts it there. The NPC belongs to
// the bots module, not to the core (owner, 2026-11-12): the template row is install data (data/sql/db-world/2026_11_12_00_arena_ladder_npc.sql)
// but no creature row exists. The module spawns the NPC at run time, as a normal grid creature that is never saved, when the
// grid around one of these spots is loaded; the core deletes it with its grid. This header is free of game types so that
// tests/ can run it; ArenaNpcSpawn.cpp does the map work.

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace ArenaNpcSpots
{
// creature_template entry (docs/client-interface.md, reserved custom range 9000100 to 9000199).
constexpr std::uint32_t NPC_ENTRY = 9000100;

struct Spot
{
    std::uint32_t map;
    float x;
    float y;
    float z;
    float orientation;
    char const* label;
};

// Next to the arena battlemasters and the vendors of the capitals (positions read from the world data, owner 2026-11-12:
// Pandaria first). More capitals are one row each: Ironforge (-5038.2, -1270.2, 510.4), Darnassus (9975.6, 2324.5, 1330.9),
// Undercity (1329.9, 341.6, -63.6), Thunder Bluff (-1384.3, -98.6, 159.0). Only continent maps may be listed: the spawn code
// looks at the one base map of an id, never at instances.
constexpr std::size_t SPOT_COUNT = 4;
constexpr std::array<Spot, SPOT_COUNT> SPOTS = { {
    { 870, 801.5f, 263.7f, 522.1f, 1.66f, "Shrine of Seven Stars, upper floor by the raid vendors" },
    { 0, -8421.5f, 210.0f, 155.4f, 0.07f, "Stormwind, beside the battlemasters" },
    { 1, 1962.0f, -4806.0f, 84.4f, 2.48f, "Orgrimmar, beside the battlemasters" },
    { 870, 1683.6f, 952.5f, 489.6f, 2.62f, "Shrine of Two Moons, upper floor by the inn" },
} };

// A creature of the NPC's entry this close (yards, flat distance) to a spot counts as the spot's NPC. It catches a leftover
// database spawn (the removal SQL clears those) so the spot never gets two copies.
constexpr float CLAIM_RADIUS = 8.0f;

// Time base of the spawn pass. Short enough that the NPC is there a moment after a player arrives in a capital (the grid
// loads inside the map update, the pass runs after it), long enough to cost nothing: four map lookups per pass.
constexpr std::uint32_t SCAN_INTERVAL_MS = 2000;

// After a failed spawn (the core refused the creature) a spot rests this long, so a broken template cannot fill the log.
constexpr std::uint32_t SPAWN_RETRY_MS = 60000;

// What the glue code knows about one spot right now. NearbyCopy is filled lazily: UNKNOWN until Decide asked for the scan.
enum class NearbyCopy : std::uint8_t
{
    UNKNOWN,
    NONE,
    FOUND
};

struct Facts
{
    bool mapLoaded = false;      // the base map of the spot exists (a player or bot has been there since start-up)
    bool gridLoaded = false;     // the grid of the spot has its object data loaded
    bool gridLeaving = false;    // the grid is in the removal state, so a new creature would only be deleted again
    bool trackedPresent = false; // the creature remembered for this spot still resolves on its map
    NearbyCopy nearby = NearbyCopy::UNKNOWN;
};

struct SpotState
{
    std::uint64_t guid = 0;           // the creature remembered for the spot (spawned by the module, or adopted)
    std::uint64_t retryAtMs = 0;      // module clock: no spawn before this time
};

enum class Step : std::uint8_t
{
    NOTHING,  // leave the spot alone
    SCAN,     // look for a copy near the spot, then ask again with the answer
    ADOPT,    // a copy exists that the module did not spawn: remember it, spawn nothing
    SPAWN     // create the NPC
};

// The whole decision, in the order the facts are cheap to get. Pure and deterministic.
inline Step Decide(Facts const& facts, SpotState const& state, std::uint64_t nowMs)
{
    if (!facts.mapLoaded || !facts.gridLoaded || facts.gridLeaving)
        return Step::NOTHING;
    if (facts.trackedPresent)
        return Step::NOTHING;
    if (nowMs < state.retryAtMs)
        return Step::NOTHING;
    if (facts.nearby == NearbyCopy::UNKNOWN)
        return Step::SCAN;
    return facts.nearby == NearbyCopy::FOUND ? Step::ADOPT : Step::SPAWN;
}

// Booking after the glue code acted on a step. A spawned or adopted guid is remembered; a failure starts the rest.
inline void RecordSpawned(SpotState& state, std::uint64_t guid)
{
    state.guid = guid;
    state.retryAtMs = 0;
}

inline void RecordFailed(SpotState& state, std::uint64_t nowMs)
{
    state.guid = 0;
    state.retryAtMs = nowMs + SPAWN_RETRY_MS;
}

// A guid that no longer resolves (the grid was unloaded) is forgotten, so the next load spawns again.
inline void RecordGone(SpotState& state)
{
    state.guid = 0;
}

// The pass timer: true once per SCAN_INTERVAL_MS of world time. The remainder carries over, so a long tick does not queue
// several passes.
inline bool PassDue(std::uint32_t& untilMs, std::uint32_t diffMs)
{
    if (untilMs > diffMs)
    {
        untilMs -= diffMs;
        return false;
    }
    untilMs = SCAN_INTERVAL_MS;
    return true;
}

// Flat distance test of the claim radius.
inline bool ClaimsSpot(Spot const& spot, float x, float y)
{
    float const dx = x - spot.x;
    float const dy = y - spot.y;
    return std::sqrt(dx * dx + dy * dy) <= CLAIM_RADIUS;
}
}  // namespace ArenaNpcSpots

#endif
