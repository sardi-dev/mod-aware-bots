#include "TestHarness.h"
#include "AuctionVisitPolicy.h"
#include "SocialSoakPolicy.h"

TEST(auction_mailbox_does_not_start_auction_cooldown)
{
    CHECK(!AuctionVisitPolicy::StartsAuctionCooldown(false));
    CHECK(AuctionVisitPolicy::StartsAuctionCooldown(true));
}

TEST(auction_snapshot_detects_truncation_with_one_extra_listing)
{
    using namespace AuctionVisitPolicy;
    CHECK(SnapshotOmittedLowerBound(SNAPSHOT_LIMIT - 1) == 0);
    CHECK(SnapshotOmittedLowerBound(SNAPSHOT_LIMIT) == 0);
    CHECK(SnapshotOmittedLowerBound(SNAPSHOT_LIMIT + 1) == 1);
}

TEST(social_summary_boundaries_do_not_drift_after_a_late_update)
{
    CHECK(SocialSoak::NextSummaryBoundary(0, 0) == 0);
    CHECK(SocialSoak::NextSummaryBoundary(901, 900) == 1800);
    CHECK(SocialSoak::NextSummaryBoundary(1800, 900) == 2700);
    CHECK(SocialSoak::NextSummaryBoundary(1819, 900) == 2700);
}
