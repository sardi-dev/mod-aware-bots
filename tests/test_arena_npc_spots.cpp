/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "ArenaNpcSpots.h"
#include "TestHarness.h"

#include <cstring>
#include <set>
#include <string>

using namespace ArenaNpcSpots;

namespace
{
Facts Loaded()
{
    Facts facts;
    facts.mapLoaded = true;
    facts.gridLoaded = true;
    return facts;
}

bool HasLongDash(std::string const& text)
{
    return text.find("\xE2\x80\x93") != std::string::npos || text.find("\xE2\x80\x94") != std::string::npos;
}
}  // namespace

TEST(arena_npc_spots_are_the_four_owner_spots)
{
    CHECK(SPOT_COUNT == 4);
    CHECK(NPC_ENTRY == 9000100u);
    // Shrine of Seven Stars, Stormwind, Orgrimmar, Shrine of Two Moons.
    CHECK(SPOTS[0].map == 870u);
    CHECK_NEAR(SPOTS[0].x, 801.5, 0.01);
    CHECK_NEAR(SPOTS[0].y, 263.7, 0.01);
    CHECK_NEAR(SPOTS[0].z, 522.1, 0.01);
    CHECK_NEAR(SPOTS[0].orientation, 1.66, 0.01);
    CHECK(SPOTS[1].map == 0u);
    CHECK_NEAR(SPOTS[1].x, -8421.5, 0.01);
    CHECK_NEAR(SPOTS[1].y, 210.0, 0.01);
    CHECK(SPOTS[2].map == 1u);
    CHECK_NEAR(SPOTS[2].x, 1962.0, 0.01);
    CHECK_NEAR(SPOTS[2].y, -4806.0, 0.01);
    CHECK(SPOTS[3].map == 870u);
    CHECK_NEAR(SPOTS[3].x, 1683.6, 0.01);
    CHECK_NEAR(SPOTS[3].y, 952.5, 0.01);
}

TEST(arena_npc_spots_are_distinct_valid_and_labelled)
{
    std::set<std::string> labels;
    for (std::size_t i = 0; i < SPOT_COUNT; ++i)
    {
        Spot const& spot = SPOTS[i];
        CHECK(spot.map == 0u || spot.map == 1u || spot.map == 870u);  // continents only: instances are never served
        CHECK(std::fabs(spot.x) < 17066.0f && std::fabs(spot.y) < 17066.0f);
        CHECK(spot.orientation >= 0.0f && spot.orientation < 6.2832f);
        CHECK(spot.label != nullptr && std::strlen(spot.label) > 0);
        CHECK(!HasLongDash(spot.label));
        labels.insert(spot.label);
        for (std::size_t j = i + 1; j < SPOT_COUNT; ++j)
            CHECK(!(SPOTS[j].map == spot.map && ClaimsSpot(spot, SPOTS[j].x, SPOTS[j].y)));
    }
    CHECK(labels.size() == SPOT_COUNT);
}

TEST(arena_npc_nothing_without_a_loaded_grid)
{
    SpotState state;
    Facts facts;
    CHECK(Decide(facts, state, 0) == Step::NOTHING);  // no map yet
    facts.mapLoaded = true;
    CHECK(Decide(facts, state, 0) == Step::NOTHING);  // map, but the grid is not loaded
    facts.gridLoaded = true;
    facts.gridLeaving = true;
    CHECK(Decide(facts, state, 0) == Step::NOTHING);  // loaded but about to be unloaded
}

TEST(arena_npc_scans_first_then_spawns_on_an_empty_spot)
{
    SpotState state;
    Facts facts = Loaded();
    CHECK(Decide(facts, state, 0) == Step::SCAN);
    facts.nearby = NearbyCopy::NONE;
    CHECK(Decide(facts, state, 0) == Step::SPAWN);
}

TEST(arena_npc_adopts_a_copy_it_did_not_spawn)
{
    SpotState state;
    Facts facts = Loaded();
    facts.nearby = NearbyCopy::FOUND;
    CHECK(Decide(facts, state, 0) == Step::ADOPT);
    RecordSpawned(state, 4242);
    facts.trackedPresent = true;
    facts.nearby = NearbyCopy::UNKNOWN;
    CHECK(Decide(facts, state, 0) == Step::NOTHING);
}

TEST(arena_npc_never_two_copies_at_one_spot)
{
    SpotState state;
    Facts facts = Loaded();
    facts.nearby = NearbyCopy::NONE;
    CHECK(Decide(facts, state, 0) == Step::SPAWN);
    RecordSpawned(state, 7);

    // While the creature stands, every later pass leaves the spot alone, whatever else is known.
    facts.trackedPresent = true;
    for (NearbyCopy nearby : { NearbyCopy::UNKNOWN, NearbyCopy::NONE, NearbyCopy::FOUND })
    {
        facts.nearby = nearby;
        CHECK(Decide(facts, state, 100000) == Step::NOTHING);
    }
}

TEST(arena_npc_spawns_again_after_the_grid_was_unloaded)
{
    SpotState state;
    RecordSpawned(state, 7);
    // The grid unloads: the guid no longer resolves, the glue forgets it, the grid is not loaded.
    RecordGone(state);
    CHECK(state.guid == 0u);
    Facts facts;
    facts.mapLoaded = true;
    CHECK(Decide(facts, state, 5000) == Step::NOTHING);

    // The grid loads again: scan, then a new creature.
    facts.gridLoaded = true;
    CHECK(Decide(facts, state, 7000) == Step::SCAN);
    facts.nearby = NearbyCopy::NONE;
    CHECK(Decide(facts, state, 7000) == Step::SPAWN);
}

TEST(arena_npc_failed_spawn_rests_then_retries)
{
    SpotState state;
    Facts facts = Loaded();
    facts.nearby = NearbyCopy::NONE;
    CHECK(Decide(facts, state, 1000) == Step::SPAWN);
    RecordFailed(state, 1000);
    CHECK(state.guid == 0u);
    CHECK(Decide(facts, state, 1000 + SPAWN_RETRY_MS - 1) == Step::NOTHING);
    CHECK(Decide(facts, state, 1000 + SPAWN_RETRY_MS) == Step::SPAWN);
    RecordSpawned(state, 9);
    CHECK(state.retryAtMs == 0u);
}

TEST(arena_npc_pass_timer_fires_once_per_interval)
{
    std::uint32_t until = 0;
    CHECK(PassDue(until, 50));  // the first tick runs a pass at once
    CHECK(until == SCAN_INTERVAL_MS);

    int passes = 0;
    for (int tick = 0; tick < 100; ++tick)  // 100 ticks of 50 ms: 5 seconds
        if (PassDue(until, 50))
            ++passes;
    CHECK(passes == 2);

    until = SCAN_INTERVAL_MS;
    CHECK(PassDue(until, 60000));  // a long tick makes one pass, not thirty
    CHECK(until == SCAN_INTERVAL_MS);
}

TEST(arena_npc_claim_radius_is_flat_distance)
{
    Spot const& spot = SPOTS[1];
    CHECK(ClaimsSpot(spot, spot.x, spot.y));
    CHECK(ClaimsSpot(spot, spot.x + CLAIM_RADIUS - 0.1f, spot.y));
    CHECK(!ClaimsSpot(spot, spot.x + CLAIM_RADIUS + 0.1f, spot.y));
    CHECK(!ClaimsSpot(spot, spot.x + 6.0f, spot.y + 6.0f));  // 8.49 yards on the diagonal
    CHECK(ClaimsSpot(spot, spot.x + 4.0f, spot.y - 4.0f));
}
