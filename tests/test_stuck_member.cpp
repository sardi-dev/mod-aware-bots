#include "TestHarness.h"
#include "StuckMemberPolicy.h"

namespace
{
// A bot flagged in combat at (x, 2164, 96) with the nearest member `member` yards away.
StuckMemberPolicy::Sample Flagged(float member, float x = -236.0f)
{
    StuckMemberPolicy::Sample sample;
    sample.inCombat = true;
    sample.nearestMember = member;
    sample.x = x;
    sample.y = 2164.0f;
    sample.z = 96.0f;
    return sample;
}
}  // namespace

TEST(stuck_member_far_needs_combat_and_a_member_beyond_forty_yards)
{
    CHECK(StuckMemberPolicy::IsFar(Flagged(40.1f)));
    CHECK(!StuckMemberPolicy::IsFar(Flagged(40.0f)));
    CHECK(!StuckMemberPolicy::IsFar(Flagged(-1.0f)));
    StuckMemberPolicy::Sample calm = Flagged(90.0f);
    calm.inCombat = false;
    CHECK(!StuckMemberPolicy::IsFar(calm));
}
TEST(stuck_member_logs_after_twenty_still_seconds_then_every_twenty)
{
    // Shadowfang Keep run 7: the healer stood at one spot 40 to 148 yd from every member for 20 minutes.
    StuckMemberPolicy::State state;
    CHECK(StuckMemberPolicy::Due(state, 1000));
    CHECK(!StuckMemberPolicy::Read(state, Flagged(45.0f), 1000));
    CHECK(!StuckMemberPolicy::Due(state, 1001));
    CHECK(!StuckMemberPolicy::Read(state, Flagged(45.0f), 1010));
    CHECK(!StuckMemberPolicy::Read(state, Flagged(60.0f), 1019));
    CHECK(StuckMemberPolicy::Read(state, Flagged(70.0f), 1020));
    state.loggedAt = 1020;
    CHECK(!StuckMemberPolicy::Read(state, Flagged(70.0f), 1030));
    CHECK(StuckMemberPolicy::Read(state, Flagged(70.0f), 1040));
}
TEST(stuck_member_a_walking_bot_is_not_stuck)
{
    StuckMemberPolicy::State state;
    StuckMemberPolicy::Read(state, Flagged(50.0f, -236.0f), 1000);
    StuckMemberPolicy::Read(state, Flagged(50.0f, -230.0f), 1010);
    CHECK(!StuckMemberPolicy::Read(state, Flagged(50.0f, -224.0f), 1020));
    CHECK(state.stillSince == 1020);
    CHECK(state.farSince == 1000);
    // standing again from 1020: the line comes 20 s later
    CHECK(!StuckMemberPolicy::Read(state, Flagged(50.0f, -224.0f), 1030));
    CHECK(StuckMemberPolicy::Read(state, Flagged(50.0f, -224.0f), 1040));
}
TEST(stuck_member_a_crawl_inside_the_still_radius_stays_still)
{
    StuckMemberPolicy::State state;
    StuckMemberPolicy::Read(state, Flagged(50.0f, -236.0f), 1000);
    CHECK(!StuckMemberPolicy::Read(state, Flagged(50.0f, -235.0f), 1010));
    CHECK(StuckMemberPolicy::Read(state, Flagged(50.0f, -234.5f), 1020));
}
TEST(stuck_member_a_member_in_range_or_the_end_of_combat_resets_the_clocks)
{
    StuckMemberPolicy::State state;
    StuckMemberPolicy::Read(state, Flagged(50.0f), 1000);
    StuckMemberPolicy::Read(state, Flagged(50.0f), 1010);
    CHECK(!StuckMemberPolicy::Read(state, Flagged(25.0f), 1012));
    CHECK(state.farSince == 0);
    CHECK(state.stillSince == 0);
    StuckMemberPolicy::Read(state, Flagged(50.0f), 1014);
    CHECK(state.stillSince == 1014);
    StuckMemberPolicy::Sample calm;
    CHECK(!StuckMemberPolicy::Read(state, calm, 1016));
    CHECK(state.farSince == 0);
}
TEST(stuck_member_a_reading_gap_starts_over)
{
    // The combat engine did not run for a while (a fight ended and another began): do not count the quiet time.
    StuckMemberPolicy::State state;
    StuckMemberPolicy::Read(state, Flagged(50.0f), 1000);
    CHECK(!StuckMemberPolicy::Read(state, Flagged(50.0f), 1100));
    CHECK(state.farSince == 1100);
    CHECK(state.stillSince == 1100);
}
