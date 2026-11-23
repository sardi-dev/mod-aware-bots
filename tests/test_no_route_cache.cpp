#include "TestHarness.h"
#include "NoRouteCache.h"

using namespace NoRouteCache;

// BT-697: the shared "no route" memory of the cross-continent route builders.

TEST(no_route_cache_answers_for_ten_minutes_then_forgets)
{
    Cache cache;
    Key const key = MakeKey(1, 1500.0f, -4400.0f, 0, false);
    CHECK(!cache.Known(1000, key));
    cache.Remember(1000, key);
    CHECK(cache.Known(1000, key));
    CHECK(cache.Known(1000 + TTL_MS - 1, key));
    CHECK(!cache.Known(1000 + TTL_MS, key));
    // Remembered again: a fresh ten minutes.
    cache.Remember(1000 + TTL_MS, key);
    CHECK(cache.Known(1000 + TTL_MS + 5000, key));
}

TEST(no_route_cache_expiry_survives_the_millisecond_clock_wrapping)
{
    Cache cache;
    Key const key = MakeKey(1, 0.0f, 0.0f, 530, false);
    uint32_t const before = 0xFFFFFFFFu - 1000u;
    cache.Remember(before, key);
    CHECK(cache.Known(before + 5000u, key));  // wrapped past zero, 5 s later
    CHECK(!cache.Known(before + TTL_MS + 1u, key));
}

TEST(no_route_cache_key_is_map_cell_destination_and_hearth)
{
    Cache cache;
    cache.Remember(10, MakeKey(1, 100.0f, 100.0f, 0, false));
    // Same 256 yd cell, same maps: known.
    CHECK(cache.Known(20, MakeKey(1, 200.0f, 10.0f, 0, false)));
    // Another cell, another destination map, another start map, or a hearth leg open: not known.
    CHECK(!cache.Known(20, MakeKey(1, 300.0f, 100.0f, 0, false)));
    CHECK(!cache.Known(20, MakeKey(1, 100.0f, 100.0f, 530, false)));
    CHECK(!cache.Known(20, MakeKey(0, 100.0f, 100.0f, 0, false)));
    CHECK(!cache.Known(20, MakeKey(1, 100.0f, 100.0f, 0, true)));
    // Negative coordinates fall in their own cells (floor, not truncation).
    CHECK(!cache.Known(20, MakeKey(1, -100.0f, 100.0f, 0, false)));
}

TEST(no_route_cache_is_capped_and_drops_expired_entries_first)
{
    Cache cache;
    for (uint32_t i = 0; i < MAX_ENTRIES; ++i)
        cache.Remember(1000 + i, MakeKey(1, float(i) * CELL_YD, 0.0f, 0, false));
    CHECK(cache.Size() == MAX_ENTRIES);
    // Every entry is live: the oldest goes for the new one.
    cache.Remember(2000, MakeKey(1, 0.0f, 5000.0f, 0, false));
    CHECK(cache.Size() == MAX_ENTRIES);
    CHECK(!cache.Known(2000, MakeKey(1, 0.0f, 0.0f, 0, false)));
    CHECK(cache.Known(2000, MakeKey(1, 1.0f * CELL_YD, 0.0f, 0, false)));
    // Long after: all expired, the sweep empties the table before the insert.
    cache.Remember(2000 + 2 * TTL_MS, MakeKey(1, 0.0f, 9000.0f, 0, false));
    CHECK(cache.Size() == 1);
}

TEST(no_route_precheck_gap_is_one_per_minute_per_bot)
{
    CHECK(PrecheckDue(0, 5));
    CHECK(!PrecheckDue(1000, 1000 + PRECHECK_GAP_MS - 1));
    CHECK(PrecheckDue(1000, 1000 + PRECHECK_GAP_MS));
}
