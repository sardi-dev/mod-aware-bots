#include "TestHarness.h"
#include "RaidGeometry.h"

using RaidGeometry::Point;

TEST(raid_geometry_signed_angle_wraps_to_half_turns)
{
    CHECK_NEAR(RaidGeometry::SignedAngle(0.0f), 0.0, 1e-5);
    CHECK_NEAR(RaidGeometry::SignedAngle(1.5f * RaidGeometry::PI), -0.5 * RaidGeometry::PI, 1e-4);
    CHECK_NEAR(RaidGeometry::SignedAngle(-1.5f * RaidGeometry::PI), 0.5 * RaidGeometry::PI, 1e-4);
    CHECK_NEAR(RaidGeometry::SignedAngle(4.0f * RaidGeometry::PI + 0.25f), 0.25, 1e-4);
}

TEST(raid_geometry_front_and_rear_cones_follow_the_facing)
{
    Point const boss{ 0.0f, 0.0f };
    float const facingEast = 0.0f;
    // 90 degree cones: 45 degrees to each side.
    float const cone = RaidGeometry::PI / 2.0f;
    CHECK(RaidGeometry::InFrontCone(boss, facingEast, { 10.0f, 1.0f }, cone));
    CHECK(!RaidGeometry::InFrontCone(boss, facingEast, { 1.0f, 10.0f }, cone));
    CHECK(RaidGeometry::InRearCone(boss, facingEast, { -10.0f, -2.0f }, cone));
    CHECK(!RaidGeometry::InRearCone(boss, facingEast, { 0.0f, 10.0f }, cone));
    // Facing north turns the cones with him.
    float const facingNorth = RaidGeometry::PI / 2.0f;
    CHECK(RaidGeometry::InFrontCone(boss, facingNorth, { 1.0f, 10.0f }, cone));
    CHECK(RaidGeometry::InRearCone(boss, facingNorth, { 0.5f, -10.0f }, cone));
}

TEST(raid_geometry_flank_point_picks_the_nearer_side)
{
    Point const boss{ 0.0f, 0.0f };
    // Boss faces east, the bot stands north-east in front: the left (north) flank is nearer.
    Point const side = RaidGeometry::FlankPoint(boss, 0.0f, RaidGeometry::PI / 2.0f, 8.0f, { 6.0f, 3.0f });
    CHECK_NEAR(side.x, 0.0, 1e-4);
    CHECK_NEAR(side.y, 8.0, 1e-4);
    Point const other = RaidGeometry::FlankPoint(boss, 0.0f, RaidGeometry::PI / 2.0f, 8.0f, { 6.0f, -3.0f });
    CHECK_NEAR(other.y, -8.0, 1e-4);
    // A flank point is outside a 90 degree frontal cone.
    CHECK(!RaidGeometry::InFrontCone(boss, 0.0f, side, RaidGeometry::PI / 2.0f));
}

TEST(raid_geometry_away_from_keeps_the_direction)
{
    Point const step = RaidGeometry::AwayFrom({ 10.0f, 10.0f }, { 13.0f, 14.0f }, 10.0f);
    CHECK_NEAR(step.x, 16.0, 1e-4);
    CHECK_NEAR(step.y, 18.0, 1e-4);
    // Standing on the hazard: still a step of the full distance.
    Point const onTop = RaidGeometry::AwayFrom({ 5.0f, 5.0f }, { 5.0f, 5.0f }, 4.0f);
    CHECK_NEAR(std::hypot(onTop.x - 5.0f, onTop.y - 5.0f), 4.0, 1e-4);
}

TEST(raid_geometry_nearest_point_outside_leaves_every_circle)
{
    std::vector<RaidGeometry::Circle> const circles = { { { 0.0f, 0.0f }, 5.0f }, { { 6.0f, 0.0f }, 4.0f } };
    CHECK(RaidGeometry::InAnyCircle({ 1.0f, 1.0f }, circles));
    CHECK(!RaidGeometry::InAnyCircle({ 0.0f, 6.0f }, circles));

    Point out{ 0.0f, 0.0f };
    CHECK(RaidGeometry::NearestPointOutside({ 1.0f, 0.0f }, circles, 1.0f, 20.0f, out));
    CHECK(!RaidGeometry::InAnyCircle(out, circles));
    // The first ring that clears both circles is used, never a farther one.
    CHECK(std::hypot(out.x - 1.0f, out.y) <= 6.001f);

    // Boxed in: no candidate within reach.
    std::vector<RaidGeometry::Circle> const wall = { { { 0.0f, 0.0f }, 50.0f } };
    CHECK(!RaidGeometry::NearestPointOutside({ 0.0f, 0.0f }, wall, 2.0f, 10.0f, out));
}
