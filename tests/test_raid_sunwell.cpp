#include "TestHarness.h"
#include "SunwellLogic.h"

#include <cmath>
#include <vector>

using RaidGeometry::Point;
using namespace SunwellLogic;

TEST(sunwell_soak_rotation_starts_with_the_first_team_and_keeps_it_until_four_stacks)
{
    CHECK(ChooseSoaker(0, 0, SOAK_SWAP_STACKS, true, true) == 0);
    CHECK(ChooseSoaker(1, 0, SOAK_SWAP_STACKS, true, true) == 0);
    CHECK(ChooseSoaker(3, 0, SOAK_SWAP_STACKS, true, true) == 0);
    // At four stacks the rested team takes over and keeps soaking while the first team still carries its stacks.
    CHECK(ChooseSoaker(4, 0, SOAK_SWAP_STACKS, true, true) == 1);
    CHECK(ChooseSoaker(4, 1, SOAK_SWAP_STACKS, true, true) == 1);
    CHECK(ChooseSoaker(4, 3, SOAK_SWAP_STACKS, true, true) == 1);
    // The first team's stacks fell off (40 s): the second team keeps going until its own four.
    CHECK(ChooseSoaker(0, 3, SOAK_SWAP_STACKS, true, true) == 1);
    CHECK(ChooseSoaker(0, 4, SOAK_SWAP_STACKS, true, true) == 0);
}

TEST(sunwell_soak_rotation_simulated_over_a_long_fight_keeps_stacks_low)
{
    // Hits every 11 s, debuff 40 s refreshed by each hit; both teams soak in turn.
    int stacks[2] = { 0, 0 };
    int lastHit[2] = { -1000, -1000 };
    int worst = 0;
    for (int time = 0; time < 600; time += 11)
    {
        for (int team = 0; team < 2; ++team)
            if (time - lastHit[team] >= 40)
                stacks[team] = 0;

        int const soaker = ChooseSoaker(stacks[0], stacks[1], SOAK_SWAP_STACKS, true, true);
        CHECK(soaker == 0 || soaker == 1);
        ++stacks[soaker];
        lastHit[soaker] = time;
        worst = std::max(worst, stacks[soaker]);
    }
    CHECK(worst <= SOAK_SWAP_STACKS + 1);
}

TEST(sunwell_soak_rotation_uses_the_only_team_left)
{
    CHECK(ChooseSoaker(9, 0, SOAK_SWAP_STACKS, true, false) == 0);
    CHECK(ChooseSoaker(0, 9, SOAK_SWAP_STACKS, false, true) == 1);
    CHECK(ChooseSoaker(0, 0, SOAK_SWAP_STACKS, false, false) == -1);
}

TEST(sunwell_tank_swap_waits_for_the_other_tank_to_lose_its_stacks)
{
    // Side 0 is the tank Brutallus hits now.
    CHECK(ChooseSoaker(3, 0, TANK_SWAP_STACKS, true, true) == 0);
    CHECK(ChooseSoaker(4, 0, TANK_SWAP_STACKS, true, true) == 1);
    // Just after the taunt the new tank has fewer stacks than the old one: it stays.
    CHECK(ChooseSoaker(1, 4, TANK_SWAP_STACKS, true, true) == 0);
}

TEST(sunwell_soak_spots_fan_out_inside_the_meteor_cone)
{
    Point const boss{ 0.0f, 0.0f };
    float const facing = 0.0f;
    for (int slot = 0; slot < 5; ++slot)
    {
        Point const spot = SoakSpot(boss, facing, slot, 5, 6.0f);
        CHECK_NEAR(std::hypot(spot.x, spot.y), 6.0, 1e-4);
        CHECK(InMeteorCone(boss, facing, spot, 0.0f));
    }
    // One soaker stands straight ahead.
    Point const single = SoakSpot(boss, facing, 0, 1, 10.0f);
    CHECK_NEAR(single.x, 10.0, 1e-4);
    CHECK_NEAR(single.y, 0.0, 1e-4);
    // Neighbouring spots differ.
    Point const a = SoakSpot(boss, facing, 0, 2, 15.0f);
    Point const b = SoakSpot(boss, facing, 1, 2, 15.0f);
    CHECK(std::hypot(a.x - b.x, a.y - b.y) > 3.0f);
}

TEST(sunwell_meteor_cone_margin_and_range)
{
    Point const boss{ 0.0f, 0.0f };
    // 50 degrees off his facing: outside the 45 degree edge, inside the margin.
    Point const nearEdge{ std::cos(50.0f * RaidGeometry::PI / 180.0f) * 10.0f,
        std::sin(50.0f * RaidGeometry::PI / 180.0f) * 10.0f };
    CHECK(!InMeteorCone(boss, 0.0f, nearEdge, 0.0f));
    CHECK(InMeteorCone(boss, 0.0f, nearEdge, METEOR_CONE_MARGIN));
    CHECK(!InMeteorCone(boss, 0.0f, { 70.0f, 0.0f }, 0.0f));
    CHECK(!InMeteorCone(boss, 0.0f, { -5.0f, 0.0f }, METEOR_CONE_MARGIN));
}

TEST(sunwell_meteor_avoid_spots_leave_the_cone_with_margin)
{
    Point const boss{ 0.0f, 0.0f };
    Point const fromLeft{ 8.0f, 2.0f };
    Point const melee = MeteorAvoidSpot(boss, 0.0f, fromLeft, true, 4.0f);
    CHECK(!InMeteorCone(boss, 0.0f, melee, METEOR_CONE_MARGIN));
    CHECK(melee.y > 0.0f);  // the side nearer to the bot
    CHECK_NEAR(std::hypot(melee.x, melee.y), 4.0, 1e-4);

    Point const ranged = MeteorAvoidSpot(boss, 0.0f, { 25.0f, -3.0f }, false, 4.0f);
    CHECK(!InMeteorCone(boss, 0.0f, ranged, METEOR_CONE_MARGIN));
    CHECK(ranged.y < 0.0f);
    float const rangedDistance = std::hypot(ranged.x, ranged.y);
    CHECK(rangedDistance >= 10.0f - 1e-3f && rangedDistance <= 30.0f + 1e-3f);
}

TEST(sunwell_felmyst_phase_from_what_the_raid_sees)
{
    CHECK(ClassifyFelmyst(1.0f, false, false, false) == FelmystPhase::Ground);
    CHECK(ClassifyFelmyst(1.0f, true, true, true) == FelmystPhase::Ground);
    CHECK(ClassifyFelmyst(10.0f, true, false, false) == FelmystPhase::AirVapor);
    CHECK(ClassifyFelmyst(10.0f, false, false, false) == FelmystPhase::AirBreath);
    CHECK(ClassifyFelmyst(10.0f, false, true, true) == FelmystPhase::AirBreath);
    CHECK(ClassifyFelmyst(10.0f, false, false, true) == FelmystPhase::AirMoving);
}

TEST(sunwell_breath_lane_covers_the_line_ahead_and_not_the_sides)
{
    Point const start{ 0.0f, 0.0f };
    std::vector<RaidGeometry::Circle> const lane =
        BreathLaneCircles(start, RaidGeometry::PI / 2.0f, BREATH_LANE_LENGTH, 22.0f, BREATH_LANE_SPACING);
    CHECK(!lane.empty());
    // Straight ahead (north) through the aimed-at player and past him.
    CHECK(RaidGeometry::InAnyCircle({ 0.0f, 70.0f }, lane));
    CHECK(RaidGeometry::InAnyCircle({ 15.0f, 120.0f }, lane));
    // Beside the lane and behind her.
    CHECK(!RaidGeometry::InAnyCircle({ 25.0f, 70.0f }, lane));
    CHECK(!RaidGeometry::InAnyCircle({ 0.0f, -30.0f }, lane));
    CHECK(!RaidGeometry::InAnyCircle({ 0.0f, 175.0f }, lane));

    // The nearest safe point for a bot on the line is beside it, not along it.
    Point safe{ 0.0f, 0.0f };
    CHECK(RaidGeometry::NearestPointOutside({ 3.0f, 70.0f }, lane, 2.0f, 30.0f, safe));
    CHECK(std::fabs(safe.x) >= 22.0f);
    CHECK(std::fabs(safe.y - 70.0f) < 12.0f);

    CHECK(BreathLaneCircles(start, 0.0f, 10.0f, 5.0f, 0.0f).empty());
}

TEST(sunwell_regroup_distance_by_role)
{
    CHECK(!NeedsRegroup(8.0f, false));
    CHECK(NeedsRegroup(12.0f, false));
    CHECK(!NeedsRegroup(20.0f, true));
    CHECK(NeedsRegroup(30.0f, true));
}

TEST(sunwell_kite_point_runs_from_the_chaser_and_the_raid)
{
    Point const bot{ 0.0f, 0.0f };
    Point const chaser{ -5.0f, 0.0f };
    Point const raid{ 0.0f, -10.0f };
    Point const step = KitePoint(bot, chaser, raid, 8.0f);
    CHECK_NEAR(std::hypot(step.x, step.y), 8.0, 1e-4);
    CHECK(step.x > 0.0f);  // away from the chaser
    CHECK(step.y > 0.0f);  // and away from the raid
    // On top of the chaser and the raid: still a full step.
    Point const stacked = KitePoint(bot, bot, bot, 8.0f);
    CHECK_NEAR(std::hypot(stacked.x, stacked.y), 8.0, 1e-4);
}

TEST(sunwell_reflections_on_healers_die_first)
{
    CHECK(ReflectionRank(true, true) == 0);
    CHECK(ReflectionRank(false, true) == 1);
    CHECK(ReflectionRank(false, false) == 2);
    CHECK(ReflectionBefore(0, 90.0f, 1, 10.0f));
    CHECK(!ReflectionBefore(2, 5.0f, 1, 90.0f));
    CHECK(ReflectionBefore(1, 20.0f, 1, 40.0f));
}
