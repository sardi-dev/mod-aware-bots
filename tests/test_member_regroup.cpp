#include "TestHarness.h"
#include "MemberRegroupPolicy.h"

TEST(member_regroup_clock_starts_keeps_and_resets)
{
    CHECK(MemberRegroupPolicy::NextSince(true, 0, 100) == 100);
    CHECK(MemberRegroupPolicy::NextSince(true, 100, 130) == 100);
    CHECK(MemberRegroupPolicy::NextSince(false, 100, 130) == 0);
}
TEST(member_regroup_walks_after_fifteen_seconds_for_thirty)
{
    // Deadmines run 5: the damage dealers stood by the leader 4 min while the watcher was out of their sight.
    CHECK(!MemberRegroupPolicy::ShouldWalk(0, 1000, 20.0f, 30.0f, 10.0f));
    CHECK(!MemberRegroupPolicy::ShouldWalk(1000, 1014, 20.0f, 30.0f, 10.0f));
    CHECK(MemberRegroupPolicy::ShouldWalk(1000, 1015, 20.0f, 30.0f, 10.0f));
    CHECK(MemberRegroupPolicy::ShouldWalk(1000, 1044, 20.0f, 30.0f, 10.0f));
    CHECK(!MemberRegroupPolicy::ShouldWalk(1000, 1045, 20.0f, 30.0f, 10.0f));
}
TEST(member_regroup_stops_at_the_tank_side)
{
    CHECK(!MemberRegroupPolicy::ShouldWalk(1000, 1020, 6.0f, 30.0f, 10.0f));
    CHECK(MemberRegroupPolicy::ShouldWalk(1000, 1020, 6.1f, 30.0f, 10.0f));
}
TEST(member_regroup_never_ahead_of_the_tank)
{
    CHECK(!MemberRegroupPolicy::ShouldWalk(1000, 1020, 20.0f, 12.0f, 10.0f));
    CHECK(MemberRegroupPolicy::ShouldWalk(1000, 1020, 20.0f, 12.1f, 10.0f));
    CHECK(!MemberRegroupPolicy::ShouldWalk(1000, 1020, 20.0f, 5.0f, 10.0f));
}
