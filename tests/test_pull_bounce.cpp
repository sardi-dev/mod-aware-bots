#include "TestHarness.h"
#include "PullBouncePolicy.h"

TEST(pull_bounce_alone_keeps_the_shield)
{
    CHECK(!PullBouncePolicy::ShieldWouldBounce(0));
}
TEST(pull_bounce_one_neighbour_switches_to_a_single_target_pull)
{
    CHECK(PullBouncePolicy::ShieldWouldBounce(1));
    CHECK(PullBouncePolicy::ShieldWouldBounce(5));
}
TEST(pull_bounce_radius_edge)
{
    CHECK(PullBouncePolicy::InBounceRadius(12.0f));
    CHECK(!PullBouncePolicy::InBounceRadius(12.01f));
    CHECK(PullBouncePolicy::InBounceRadius(7.1f));
}
