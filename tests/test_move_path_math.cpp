#include "TestHarness.h"
#include "MovePathMath.h"

#include <vector>

namespace
{
struct Pt
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};
}  // namespace

TEST(move_path_math_cuts_a_long_route_at_the_leg_length)
{
    // A road with a turn: 100 yd east, then 300 yd north. The leg ends 80 yd up the northern stretch.
    std::vector<Pt> const route = {{0, 0, 10}, {100, 0, 10}, {100, 300, 40}};
    Pt end;
    bool cut = false;
    CHECK(MovePathMath::PointAlong(route, MovePathMath::LEG_YARDS, end, cut));
    CHECK(cut);
    CHECK_NEAR(end.x, 100.0, 0.01);
    CHECK_NEAR(end.y, 300.0 * (80.0 / MovePathMath::Dist(route[1], route[2])), 0.5);
    CHECK(end.z > 10.0f && end.z < 40.0f);
}
TEST(move_path_math_keeps_a_short_route_whole)
{
    std::vector<Pt> const route = {{0, 0, 0}, {30, 40, 0}, {60, 80, 0}};
    Pt end;
    bool cut = true;
    CHECK(MovePathMath::PointAlong(route, MovePathMath::LEG_YARDS, end, cut));
    CHECK(!cut);
    CHECK_NEAR(end.x, 60.0, 0.01);
    CHECK_NEAR(end.y, 80.0, 0.01);
}
TEST(move_path_math_refuses_a_path_without_a_step)
{
    std::vector<Pt> const single = {{1, 2, 3}};
    Pt end;
    bool cut = false;
    CHECK(!MovePathMath::PointAlong(single, 50.0f, end, cut));
    CHECK(!MovePathMath::PointAlong(std::vector<Pt>{}, 50.0f, end, cut));
}
TEST(move_path_math_wants_real_progress_from_a_leg)
{
    CHECK(MovePathMath::LegMakesProgress(300.0f, 480.0f));
    // A dead-end corridor that ends beside the bot is no leg.
    CHECK(!MovePathMath::LegMakesProgress(478.0f, 480.0f));
    CHECK(!MovePathMath::LegMakesProgress(475.0f, 480.0f));
}
TEST(move_path_math_leg_fits_the_core_smooth_path_cap)
{
    // PathGenerator: 74 points at most, 4 yd apart. A leg must leave room for the corners smoothing adds.
    CHECK(MovePathMath::LEG_YARDS < 74.0f * 4.0f * 0.75f);
    CHECK(MovePathMath::LONG_TRIP_YARDS > MovePathMath::LEG_YARDS);
}

TEST(first_point_where_skips_the_start_and_stops_at_the_first_match)
{
    std::vector<Pt> const route = {{0, 0, 0}, {10, 0, 0}, {20, 0, 0}, {30, 0, 0}};
    CHECK(MovePathMath::FirstPointWhere(route, [](Pt const& p) { return p.x >= 15; }) == 2u);
    CHECK(MovePathMath::FirstPointWhere(route, [](Pt const& p) { return p.x >= 0; }) == 1u);
    CHECK(MovePathMath::FirstPointWhere(route, [](Pt const& p) { return p.x > 100; }) == route.size());
}
