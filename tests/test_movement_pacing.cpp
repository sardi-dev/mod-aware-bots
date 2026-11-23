#include "TestHarness.h"
#include "MovementPacing.h"

TEST(movement_pacing_keeps_the_delay_when_not_walking)
{
    CHECK(MovementPacing::CapToWalkEnd(12000, 0) == 12000);
    CHECK(MovementPacing::CapToWalkEnd(12000, -50) == 12000);
}
TEST(movement_pacing_wakes_a_city_bot_just_before_its_walk_ends)
{
    // A resting bot drew a 12 s pause while its walk to the bank had 5 s left: it decides again 0.7 s before arrival.
    CHECK(MovementPacing::CapToWalkEnd(12000, 5000) == 4300);
    // An open-world bot drew 2 s with a 20 s walk ahead: the shorter delay stays, it checks on its walk as before.
    CHECK(MovementPacing::CapToWalkEnd(2000, 20000) == 2000);
}
TEST(movement_pacing_never_goes_below_the_floor)
{
    CHECK(MovementPacing::CapToWalkEnd(3000, 600) == MovementPacing::FLOOR_MS);
    CHECK(MovementPacing::CapToWalkEnd(3000, 1) == MovementPacing::FLOOR_MS);
    // ...but never longer than the delay chosen.
    CHECK(MovementPacing::CapToWalkEnd(100, 600) == 100);
}
