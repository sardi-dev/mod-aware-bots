#include "TestHarness.h"
#include "StaleGroupPolicy.h"

using namespace StaleGroupPolicy;

TEST(stale_group_leader_offline_or_on_another_map_is_apart)
{
    CHECK(Apart(false, true, 5.0f, APART_YARDS));
    CHECK(Apart(true, false, 5.0f, APART_YARDS));
    CHECK(!Apart(true, true, 5.0f, APART_YARDS));
}

TEST(stale_group_a_far_leader_on_the_same_map_is_apart)
{
    // Members at 3000 to 4700 yd from the leader in one continent.
    CHECK(Apart(true, true, 4668.0f, APART_YARDS));
    CHECK(Apart(true, true, APART_YARDS + 1.0f, APART_YARDS));
    CHECK(!Apart(true, true, APART_YARDS, APART_YARDS));
}

TEST(stale_group_a_member_leaves_only_after_the_hold_time)
{
    CHECK(!MayLeave(0, 100000, APART_SECONDS));
    CHECK(!MayLeave(1000, 1000 + APART_SECONDS - 1, APART_SECONDS));
    CHECK(MayLeave(1000, 1000 + APART_SECONDS, APART_SECONDS));
}

TEST(stale_group_only_groups_of_random_bots_with_nothing_special_are_swept)
{
    CHECK(SweepsGroup(true, false, false, false));
    CHECK(!SweepsGroup(false, false, false, false));
    CHECK(!SweepsGroup(true, true, false, false));
    CHECK(!SweepsGroup(true, false, true, false));
    CHECK(!SweepsGroup(true, false, false, true));
}
