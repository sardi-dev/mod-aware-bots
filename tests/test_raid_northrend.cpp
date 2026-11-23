#include "TestHarness.h"
#include "HeiganDanceLogic.h"
#include "OsTsunamiLogic.h"

#include <vector>

// The 5.4.8 Sartharion script's lanes: three waves towards +X, then two towards -X (boss_sartharion.cpp)
static std::vector<float> const RIGHT_LANES = {573.211f, 532.211f, 491.211f};
static std::vector<float> const LEFT_LANES = {552.711f, 511.711f};

TEST(os_tsunami_wave_passed_depends_on_its_direction)
{
    CHECK(OsTsunami::WavePassed(3250.0f, 3240.0f, true));
    CHECK(!OsTsunami::WavePassed(3250.0f, 3260.0f, true));
    CHECK(OsTsunami::WavePassed(3250.0f, 3260.0f, false));
    CHECK(!OsTsunami::WavePassed(3250.0f, 3240.0f, false));
}

TEST(os_tsunami_main_tank_spot_lies_in_a_right_lane)
{
    float safeY = 0.0f;
    CHECK(OsTsunami::DodgeY(532.5f, RIGHT_LANES, safeY));
    CHECK(std::fabs(safeY - (532.211f + OsTsunami::SIDE_STEP)) < 0.01f);
    CHECK(!OsTsunami::InLane(safeY, RIGHT_LANES));
}

TEST(os_tsunami_steps_to_the_nearer_side)
{
    float safeY = 0.0f;
    CHECK(OsTsunami::DodgeY(528.0f, RIGHT_LANES, safeY));
    CHECK(safeY < 532.211f - OsTsunami::LANE_HALF_WIDTH);
    CHECK(OsTsunami::DodgeY(515.0f, LEFT_LANES, safeY));
    CHECK(safeY > 511.711f + OsTsunami::LANE_HALF_WIDTH);
}

TEST(os_tsunami_no_move_outside_the_lanes)
{
    float safeY = 0.0f;
    CHECK(!OsTsunami::DodgeY(511.7f, RIGHT_LANES, safeY));
    CHECK(!OsTsunami::DodgeY(532.2f, LEFT_LANES, safeY));
    CHECK(!OsTsunami::DodgeY(532.2f, {}, safeY));
}

TEST(os_tsunami_side_step_clears_the_damage_radius)
{
    CHECK(OsTsunami::SIDE_STEP > OsTsunami::LANE_HALF_WIDTH);
    CHECK(OsTsunami::LANE_HALF_WIDTH > OsTsunami::DAMAGE_RADIUS);
}

TEST(heigan_slow_dance_first_spot_holds_until_the_first_eruption)
{
    CHECK(HeiganDance::SafeWaypoint(false, 0) == 0);
    CHECK(HeiganDance::SafeWaypoint(false, 15000) == 0);  // inside the safety margin
    CHECK(HeiganDance::SafeWaypoint(false, 15400) == 1);
    CHECK(HeiganDance::MsUntilNextEruption(false, 10000) == 5000);
}

TEST(heigan_spots_walk_out_and_back)
{
    // eruptions at 15, 25, 35, 45, 55, 65, 75 s: spots 1, 2, 3, 2, 1, 0, 1
    uint32_t const expected[] = {1, 2, 3, 2, 1, 0, 1};
    for (uint32_t i = 0; i < 7; ++i)
        CHECK(HeiganDance::SafeWaypoint(false, 15000 + i * 10000 + 500) == expected[i]);
}

TEST(heigan_fast_dance_uses_the_core_timings)
{
    // 5.4.8 boss_heigan.cpp: first eruption at 8 s, then every 3 s, 13 in the 45 s phase
    CHECK(HeiganDance::EruptionIndex(true, 7999) == 0);
    CHECK(HeiganDance::EruptionIndex(true, 8400) == 1);
    CHECK(HeiganDance::EruptionIndex(true, 11400) == 2);
    CHECK(HeiganDance::EruptionIndex(true, 44400) == 13);
    CHECK(HeiganDance::SafeWaypoint(true, 44400) == 0);  // after the last one: the next phase's first spot
    CHECK(HeiganDance::MsUntilNextEruption(true, 9000) == 2000);
}

TEST(heigan_after_the_last_eruption_waits_for_the_next_phase)
{
    // slow dance: 8th eruption at 85 s, the fast dance starts at 90 s and erupts 8 s into it
    CHECK(HeiganDance::MsUntilNextEruption(false, 86000) == 90000 + 8000 - 86000);
    // fast dance: 13th eruption at 44 s, the slow dance starts at 45 s and erupts 15 s into it
    CHECK(HeiganDance::MsUntilNextEruption(true, 44500) == 45000 + 15000 - 44500);
}
