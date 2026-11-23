#include "TestHarness.h"
#include "ReachPolicy.h"

TEST(reach_inside_and_in_sight_is_arrived)
{
    CHECK(ReachPolicy::Decide(7.9f, 8.0f, 3.0f, true) == ReachPolicy::Step::ARRIVED);
}
TEST(reach_inside_without_sight_walks_to_contact)
{
    // Deadmines run 5, the stairs Overseer: 5.0 yd between the bodies (8.0 centre to centre), no sight.
    CHECK(ReachPolicy::Decide(8.0f, 8.0f, 3.0f, false) == ReachPolicy::Step::WALK_TO_CONTACT);
    CHECK(ReachPolicy::Decide(3.8f, 8.0f, 3.0f, false) == ReachPolicy::Step::WALK_TO_CONTACT);
}
TEST(reach_touching_without_sight_stops)
{
    CHECK(ReachPolicy::Decide(3.0f, 8.0f, 3.0f, false) == ReachPolicy::Step::ARRIVED);
}
TEST(reach_beyond_walks_seen_or_not)
{
    CHECK(ReachPolicy::Decide(8.1f, 8.0f, 3.0f, true) == ReachPolicy::Step::WALK);
    CHECK(ReachPolicy::Decide(8.1f, 8.0f, 3.0f, false) == ReachPolicy::Step::WALK);
}
