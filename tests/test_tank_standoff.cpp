#include "TestHarness.h"
#include "TankStandoffPolicy.h"

using namespace TankStandoffPolicy;

TEST(tank_standoff_a_new_target_starts_the_clock)
{
    State state;
    CHECK(ObserveDue(state, 1000));
    CHECK(Observe(state, 7, 5000, 1000) == 0);
    CHECK(!ObserveDue(state, 1000));
    CHECK(ObserveDue(state, 1001));
    CHECK(Observe(state, 7, 5000, 1004) == 4);
}
TEST(tank_standoff_a_health_loss_restarts_the_clock)
{
    State state;
    Observe(state, 7, 5000, 1000);
    CHECK(Observe(state, 7, 5000, 1006) == 6);
    CHECK(Observe(state, 7, 5000, 1012) == 12);
    CHECK(Observe(state, 7, 4900, 1013) == 0);
    CHECK(Observe(state, 7, 4900, 1020) == 7);
}
TEST(tank_standoff_regeneration_does_not_restart_the_clock)
{
    State state;
    Observe(state, 7, 5000, 1000);
    CHECK(Observe(state, 7, 5100, 1010) == 10);
    // the raised health is the new mark: a loss from it counts
    CHECK(Observe(state, 7, 5050, 1011) == 0);
}
TEST(tank_standoff_a_target_change_or_no_target_resets)
{
    State state;
    Observe(state, 7, 5000, 1000);
    CHECK(Observe(state, 8, 3000, 1010) == 0);
    CHECK(Observe(state, 0, 0, 1011) == 0);
    CHECK(state.targetGuid == 0);
    CHECK(Observe(state, 8, 3000, 1012) == 0);
}
TEST(tank_standoff_a_reading_gap_starts_over)
{
    // the combat engine did not run for a while: the old target at the same health is not a 30 s stall
    State state;
    Observe(state, 7, 5000, 1000);
    CHECK(Observe(state, 7, 5000, 1010) == 10);
    CHECK(Observe(state, 7, 5000, 1021) == 0);
}
TEST(tank_standoff_logs_after_fifteen_seconds_then_every_fifteen)
{
    CHECK(!ShouldLog(14, 1014, 0));
    CHECK(ShouldLog(15, 1015, 0));
    CHECK(!ShouldLog(25, 1025, 1015));
    CHECK(ShouldLog(30, 1030, 1015));
}
