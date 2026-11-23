#include "TestHarness.h"
#include "RoamPolicy.h"

#include <vector>

TEST(roam_policy_steps_stay_inside_the_leash)
{
    CHECK_NEAR(RoamPolicy::AnchorRadius(40.0f, 0.0f), 14.0, 0.01);
    CHECK(RoamPolicy::AnchorRadius(40.0f, 0.999f) <= 40.0f);
    CHECK(RoamPolicy::OutsideLeash(51.0f, 40.0f));
    CHECK(!RoamPolicy::OutsideLeash(49.0f, 40.0f));
}
TEST(roam_policy_counts_every_climb_of_a_path)
{
    // Up 10, down 10, up 10: a walk over a hill and up the next climbs 20 yd although it ends only 10 yd higher.
    std::vector<float> const hills = {100, 110, 100, 110};
    CHECK_NEAR(RoamPolicy::TotalClimb(hills), 20.0, 0.001);
    std::vector<float> const downhill = {100, 95, 90};
    CHECK_NEAR(RoamPolicy::TotalClimb(downhill), 0.0, 0.001);
}
TEST(roam_policy_refuses_mountain_steps_and_detours)
{
    // 30 yd across a meadow with a 5 yd rise: fine.
    CHECK(RoamPolicy::StepTerrainOk(30.0f, 5.0f, 32.0f));
    // 30 yd across but 20 yd up a mountainside: refused.
    CHECK(!RoamPolicy::StepTerrainOk(30.0f, 20.0f, 36.0f));
    // A step whose walk goes 70 yd around a ridge for 30 yd of distance: refused.
    CHECK(!RoamPolicy::StepTerrainOk(30.0f, 2.0f, 70.0f));
}
TEST(roam_policy_ends_a_wander_without_work)
{
    CHECK(!RoamPolicy::WanderHasNoWork(60000, false));
    CHECK(RoamPolicy::WanderHasNoWork(RoamPolicy::NO_WORK_MS, false));
    CHECK(!RoamPolicy::WanderHasNoWork(RoamPolicy::NO_WORK_MS * 2, true));
}
TEST(roam_policy_still_watch_fires_after_three_still_minutes)
{
    RoamPolicy::StillWatch watch;
    CHECK(!RoamPolicy::StillTooLong(watch, true, 100, 100, 50, 1000));
    // Turning on the spot and shuffling 3 yd is not moving.
    CHECK(!RoamPolicy::StillTooLong(watch, true, 103, 100, 50, 60000));
    CHECK(!RoamPolicy::StillTooLong(watch, true, 101, 101, 50, 1000 + RoamPolicy::STILL_MS - 1));
    CHECK(RoamPolicy::StillTooLong(watch, true, 101, 101, 50, 1000 + RoamPolicy::STILL_MS));
    // The watch starts over after it fired.
    CHECK(!RoamPolicy::StillTooLong(watch, true, 101, 101, 50, 2000 + RoamPolicy::STILL_MS));
}
TEST(roam_policy_still_watch_resets_on_movement_and_excuses)
{
    RoamPolicy::StillWatch watch;
    CHECK(!RoamPolicy::StillTooLong(watch, true, 0, 0, 0, 1000));
    CHECK(!RoamPolicy::StillTooLong(watch, true, 10, 0, 0, 100000));
    CHECK(!RoamPolicy::StillTooLong(watch, true, 10, 0, 0, 100000 + RoamPolicy::STILL_MS - 1));
    // A fight (or a break, a ride) excuses standing and clears the watch.
    CHECK(!RoamPolicy::StillTooLong(watch, false, 10, 0, 0, 400000));
    CHECK(watch.sinceMs == 0);
    CHECK(!RoamPolicy::StillTooLong(watch, true, 10, 0, 0, 400000 + RoamPolicy::STILL_MS - 1));
}
