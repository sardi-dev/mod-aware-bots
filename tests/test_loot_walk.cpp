#include "TestHarness.h"
#include "LootWalkPolicy.h"

TEST(loot_walk_refused_counts_only_when_standing)
{
    CHECK(LootWalkPolicy::TryFailed(false, true, 20.0f, 0.0f));
    CHECK(!LootWalkPolicy::TryFailed(false, false, 20.0f, 0.0f));
}
TEST(loot_walk_launch_needs_progress)
{
    CHECK(!LootWalkPolicy::TryFailed(true, true, 20.0f, 0.0f));
    CHECK(!LootWalkPolicy::TryFailed(true, true, 12.0f, 20.0f));
    CHECK(LootWalkPolicy::TryFailed(true, true, 19.5f, 20.0f));
    CHECK(LootWalkPolicy::TryFailed(true, true, 21.0f, 20.0f));
}
TEST(loot_walk_gives_up_after_three)
{
    CHECK(!LootWalkPolicy::GiveUp(2));
    CHECK(LootWalkPolicy::GiveUp(3));
}
TEST(loot_walk_skip_expires)
{
    CHECK(!LootWalkPolicy::StillSkipped(0, 100));
    CHECK(LootWalkPolicy::StillSkipped(100, 699));
    CHECK(!LootWalkPolicy::StillSkipped(100, 700));
}
TEST(loot_walk_los_error_every_thirty_seconds)
{
    CHECK(LootWalkPolicy::LosErrorDue(0, 100));
    CHECK(!LootWalkPolicy::LosErrorDue(100, 129));
    CHECK(LootWalkPolicy::LosErrorDue(100, 130));
}
TEST(loot_walk_quick_ticks_count_once_per_window)
{
    // Deadmines run 5: a walk in progress was given up within 3 s at 7 to 14 yd with the corpse in sight.
    LootWalkPolicy::Walk walk;
    walk.Observe(true, true, 12.0f, 1000);
    for (unsigned ms = 1100; ms < 3000; ms += 100)
        walk.Observe(true, true, 12.0f, ms);
    CHECK(walk.failedTries == 0);
    CHECK(!walk.GaveUp());
}
TEST(loot_walk_progress_measured_across_the_window)
{
    LootWalkPolicy::Walk walk;
    walk.Observe(true, false, 14.0f, 1000);
    walk.Observe(true, false, 13.5f, 1500);
    walk.Observe(true, false, 9.0f, 3000);
    CHECK(walk.failedTries == 0);
    walk.Observe(true, true, 8.9f, 5000);
    CHECK(walk.failedTries == 1);
}
TEST(loot_walk_a_running_walk_is_never_a_failed_try)
{
    // BT-195: a bot still moving toward the corpse makes no failed try, even without a yard of progress in the window
    CHECK(!LootWalkPolicy::TryFailed(true, false, 21.0f, 20.0f));
    CHECK(!LootWalkPolicy::TryFailed(false, false, 6.7f, 6.7f));
    LootWalkPolicy::Walk walk;
    walk.Observe(true, false, 9.0f, 1000);
    for (unsigned ms = 3000; ms <= 9000; ms += 2000)
        walk.Observe(true, false, 9.0f - float(ms) / 4000.0f, ms);
    CHECK(walk.failedTries == 0);
    CHECK(!walk.GaveUp());
}
TEST(loot_walk_a_relaunched_walk_without_progress_still_gives_up)
{
    // BT-195: always "moving" but never a yard closer: one failed try per 10 s, given up after three
    LootWalkPolicy::Walk walk;
    walk.Observe(true, false, 6.7f, 1000);
    for (unsigned ms = 3000; ms <= 9000; ms += 2000)
        walk.Observe(true, false, 6.7f, ms);
    CHECK(walk.failedTries == 0);
    walk.Observe(true, false, 6.7f, 11000);
    CHECK(walk.failedTries == 1);
    for (unsigned ms = 13000; ms <= 31000; ms += 2000)
        walk.Observe(true, false, 6.7f, ms);
    CHECK(walk.GaveUp());
}
TEST(loot_walk_stuck_walk_gives_up_after_three_windows)
{
    LootWalkPolicy::Walk walk;
    walk.Observe(false, true, 10.0f, 1000);
    walk.Observe(false, true, 10.0f, 3000);
    CHECK(!walk.GaveUp());
    walk.Observe(false, true, 10.0f, 4999);
    CHECK(!walk.GaveUp());
    walk.Observe(false, true, 10.0f, 5000);
    CHECK(walk.GaveUp());
}
TEST(loot_walk_window_survives_the_clock_wrap)
{
    LootWalkPolicy::Walk walk;
    walk.Observe(false, true, 10.0f, 0xFFFFFF00u);
    walk.Observe(false, true, 10.0f, 0x00000100u);
    CHECK(walk.failedTries == 1);
    walk.Observe(false, true, 10.0f, 0x00000900u);
    CHECK(walk.failedTries == 2);
}
