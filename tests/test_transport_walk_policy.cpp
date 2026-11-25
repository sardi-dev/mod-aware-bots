#include "TestHarness.h"
#include "TransportWalkPolicy.h"

TEST(transport_walk_progress_uses_a_stable_point_and_a_three_yard_threshold)
{
    CHECK(TransportWalkPolicy::Improved(-1.0f, 50.0f));
    CHECK(!TransportWalkPolicy::Improved(50.0f, 47.0f));
    CHECK(TransportWalkPolicy::Improved(50.0f, 46.9f));
    CHECK(!TransportWalkPolicy::Improved(50.0f, 75.0f));
}

TEST(transport_walk_uses_the_node_only_after_reaching_the_land_point)
{
    CHECK(TransportWalkPolicy::UseDockNode(false, false, 0.0f, 4.0f));
    CHECK(!TransportWalkPolicy::UseDockNode(false, true, 4.0f, 4.0f));
    CHECK(!TransportWalkPolicy::UseDockNode(false, true, 20.0f, 4.0f));
    CHECK(TransportWalkPolicy::UseDockNode(false, true, 3.9f, 4.0f));
    CHECK(TransportWalkPolicy::UseDockNode(true, true, 20.0f, 4.0f));
}
