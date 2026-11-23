#include "TestHarness.h"
#include "FollowGapPolicy.h"

TEST(follow_gap_clock_starts_keeps_and_resets)
{
    CHECK(FollowGapPolicy::NextFarSince(true, 0, 0, 500) == 500);
    CHECK(FollowGapPolicy::NextFarSince(true, 500, 528, 530) == 500);
    CHECK(FollowGapPolicy::NextFarSince(false, 500, 528, 530) == 0);
}
TEST(follow_gap_clock_restarts_after_a_reading_gap)
{
    CHECK(FollowGapPolicy::NextFarSince(true, 500, 510, 520) == 500);
    CHECK(FollowGapPolicy::NextFarSince(true, 500, 510, 521) == 521);
}
TEST(follow_gap_logs_after_twenty_seconds_then_every_twenty)
{
    CHECK(!FollowGapPolicy::ShouldLog(0, 1000, 0));
    CHECK(!FollowGapPolicy::ShouldLog(1000, 1019, 0));
    CHECK(FollowGapPolicy::ShouldLog(1000, 1020, 0));
    CHECK(!FollowGapPolicy::ShouldLog(1000, 1030, 1020));
    CHECK(FollowGapPolicy::ShouldLog(1000, 1040, 1020));
}
TEST(follow_gap_clock_keeps_across_a_drink)
{
    // Deadmines run 5: the drink holds the engine up to 18 s; the healer's far clock restarted at every reading.
    CHECK(FollowGapPolicy::NextFarSince(true, 500, 505, 523, true) == 500);
    CHECK(FollowGapPolicy::NextFarSince(true, 500, 505, 523, false) == 523);
    CHECK(FollowGapPolicy::NextFarSince(false, 500, 505, 523, true) == 0);
    CHECK(FollowGapPolicy::NextFarSince(true, 0, 505, 523, true) == 523);
}
TEST(follow_gap_state_is_shared_by_two_engines_of_one_bot)
{
    // Deadmines run 6: the combat and the non-combat engine each held a clock, so a reading by the other engine looked
    // like a gap. One state per bot keeps the clock across them.
    FollowGapPolicy::State state;
    CHECK(FollowGapPolicy::Due(state, 1000));
    CHECK(!FollowGapPolicy::Read(state, true, 1000, false));
    CHECK(state.farSince == 1000);
    CHECK(!FollowGapPolicy::Due(state, 1001));
    // the other engine reads 6 s later, the first one 9 s after that: one clock, the line comes at 20 s
    CHECK(FollowGapPolicy::Due(state, 1006));
    CHECK(!FollowGapPolicy::Read(state, true, 1006, false));
    CHECK(!FollowGapPolicy::Read(state, true, 1015, false));
    CHECK(FollowGapPolicy::Read(state, true, 1021, false));
    CHECK(state.farSince == 1000);
}
TEST(follow_gap_state_logs_then_waits_for_the_line_gap)
{
    FollowGapPolicy::State state;
    FollowGapPolicy::Read(state, true, 1000, false);
    FollowGapPolicy::Read(state, true, 1010, false);
    CHECK(FollowGapPolicy::Read(state, true, 1020, false));
    state.loggedAt = 1020;
    CHECK(!FollowGapPolicy::Read(state, true, 1030, false));
    CHECK(FollowGapPolicy::Read(state, true, 1040, false));
}
TEST(follow_gap_state_resets_when_the_bot_is_near_again)
{
    FollowGapPolicy::State state;
    FollowGapPolicy::Read(state, true, 1000, false);
    CHECK(!FollowGapPolicy::Read(state, false, 1004, false));
    CHECK(state.farSince == 0);
}
