#include "TestHarness.h"
#include "BgFlagMap.h"
#include "WsgPlanLogic.h"

// BT-470: the map adapter of the two-flag plan (docs/bg-sheets/twin-peaks.md gap G1). Warsong Gulch is the reference
// map: its data moved out of WsgPlan.cpp unchanged and its clock must read exactly as the plan always did.

using namespace BgFlagMap;

TEST(flag_map_warsong_clock_is_the_plans_old_limit)
{
    MapData const& wsg = Warsong();
    CHECK(wsg.mapId == 489);
    CHECK(wsg.clock.limitMs == WsgLogic::MATCH_LIMIT_MS);
    CHECK(wsg.clock.legacyLimitMs == wsg.clock.limitMs);

    // The same answers as WsgLogic::LeftMs at and past the limit (the Warsong plan must not change).
    for (std::uint32_t elapsed : { 0u, 1000u, 120000u, 26u * 60000u, 27u * 60000u, 28u * 60000u })
        CHECK(LeftMs(elapsed, wsg.clock) == WsgLogic::LeftMs(elapsed));
}

TEST(flag_map_longer_legacy_limit_is_read_live)
{
    // A core that still plays to 30 minutes while the fork's value is 27 (BT-461): before 27 minutes the shorter limit
    // counts, once the match runs past it the longer one does.
    Clock const clock = { 27u * 60000u, 30u * 60000u };
    CHECK(LeftMs(0, clock) == 27 * 60000);
    CHECK(LeftMs(26u * 60000u, clock) == 60000);
    CHECK(LeftMs(27u * 60000u, clock) == 3 * 60000);
    CHECK(LeftMs(29u * 60000u, clock) == 60000);
    CHECK(LeftMs(30u * 60000u, clock) == 0);

    // A core that ends at 27 minutes never reaches the longer limit: the clock reads the same as one without it.
    Clock const plain = { 27u * 60000u, 27u * 60000u };
    CHECK(LeftMs(26u * 60000u, plain) == LeftMs(26u * 60000u, clock));
}

TEST(flag_map_warsong_sides_keep_the_old_positions)
{
    MapData const& wsg = Warsong();
    // Alliance stand, Horde stand, the graveyards and the middle (BattlegroundWS.cpp:658-659, WorldSafeLocs 771 / 772).
    CHECK_NEAR(wsg.sides[0].standWalk.x, 1539.219, 0.001);
    CHECK_NEAR(wsg.sides[1].standWalk.x, 915.958, 0.001);
    CHECK_NEAR(wsg.sides[0].mainGraveyard.y, 1564.740, 0.001);
    CHECK_NEAR(wsg.sides[1].mainGraveyard.y, 1377.090, 0.001);
    CHECK_NEAR(wsg.sides[0].tunnelExit.x, 1348.020, 0.001);
    CHECK_NEAR(wsg.mid.x, 1227.446, 0.001);
    CHECK_NEAR(wsg.roofAboveStand, 12.0, 0.001);

    // The Horde roof post is the one left out until the live audit (sheet G11).
    CHECK(wsg.sides[1].hideUsable[0] && !wsg.sides[1].hideUsable[1] && wsg.sides[1].hideUsable[2]);
    for (bool usable : wsg.sides[0].hideUsable)
        CHECK(usable);
}

TEST(flag_map_view_starts_empty)
{
    FlagView view;
    for (int t = 0; t < 2; ++t)
    {
        CHECK(view.state[t] == WsgLogic::FLAG_ON_BASE);
        CHECK(view.picker[t] == 0 && view.dropped[t] == 0);
    }
}
