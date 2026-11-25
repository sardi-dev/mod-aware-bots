#include "TestHarness.h"
#include "ReachPolicy.h"
#include "ResurrectPolicy.h"

TEST(hidden_healing_and_resurrection_approach_does_not_back_away)
{
    CHECK(ReachPolicy::HiddenApproachDistance() == 0.0f);
    CHECK(ResurrectPolicy::ApproachDistance(false) == 0.0f);
    CHECK(ResurrectPolicy::ApproachDistance(true) == 15.0f);
}

TEST(healer_paths_to_fighting_tank_even_before_entering_combat)
{
    CHECK(ReachPolicy::UseHealingPath(false, false, true));
    CHECK(ReachPolicy::UseHealingPath(false, true, false));
    CHECK(!ReachPolicy::UseHealingPath(true, true, true));
    CHECK(!ReachPolicy::UseHealingPath(false, false, false));
}
