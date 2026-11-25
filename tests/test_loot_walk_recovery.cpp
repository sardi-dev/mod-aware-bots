#include "TestHarness.h"
#include "LootWalkPolicy.h"

TEST(loot_walk_progress_clears_previous_failures)
{
    LootWalkPolicy::Walk walk;
    walk.Observe(false, true, 12.0f, 1000);
    walk.Observe(false, true, 12.0f, 3000);
    CHECK(walk.failedTries == 2);
    walk.Observe(true, false, 10.9f, 5000);
    CHECK(walk.failedTries == 0);
    walk.Observe(false, true, 10.9f, 7000);
    walk.Observe(false, true, 10.9f, 9000);
    walk.Observe(false, true, 10.9f, 11000);
    CHECK(walk.GaveUp());
}

TEST(loot_walk_alternate_is_limited_to_nearby_visible_targets)
{
    CHECK(LootWalkPolicy::UseAlternateApproach(2, 6.7f, true));
    CHECK(LootWalkPolicy::UseAlternateApproach(2, 15.0f, true));
    CHECK(!LootWalkPolicy::UseAlternateApproach(1, 6.7f, true));
    CHECK(!LootWalkPolicy::UseAlternateApproach(3, 6.7f, true));
    CHECK(!LootWalkPolicy::UseAlternateApproach(2, 15.1f, true));
    CHECK(!LootWalkPolicy::UseAlternateApproach(2, 6.7f, false));
    CHECK(!LootWalkPolicy::UseAlternateApproach(2, 0.0f, true));
}

TEST(loot_walk_small_distance_changes_do_not_reset_failures)
{
    LootWalkPolicy::Walk walk;
    walk.Observe(false, true, 12.0f, 1000);
    walk.Observe(false, true, 11.5f, 3000);
    CHECK(walk.failedTries == 2);
    walk.Observe(false, true, 11.5f, 5000);
    CHECK(walk.GaveUp());
}
