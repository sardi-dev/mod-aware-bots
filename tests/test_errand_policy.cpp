#include "TestHarness.h"
#include "ErrandPolicy.h"

using ErrandPolicy::Errand;

namespace
{
ErrandPolicy::Town BigCity()
{
    ErrandPolicy::Town town;
    town.repairer = town.vendor = town.trainer = town.mailbox = town.banker = town.auctioneer = town.innkeeper = true;
    return town;
}
}  // namespace

TEST(errand_policy_needs_come_first)
{
    ErrandPolicy::Town town = BigCity();
    town.needsRepair = town.needsVendor = town.needsTraining = town.hasMail = true;
    CHECK(ErrandPolicy::Choose(town, 50) == Errand::Repair);
    town.needsRepair = false;
    CHECK(ErrandPolicy::Choose(town, 50) == Errand::Vendor);
    town.needsVendor = false;
    CHECK(ErrandPolicy::Choose(town, 50) == Errand::Trainer);
    town.needsTraining = false;
    CHECK(ErrandPolicy::Choose(town, 50) == Errand::Mailbox);
}
TEST(errand_policy_a_need_without_its_npc_falls_to_idling)
{
    ErrandPolicy::Town town;
    town.needsRepair = true;
    town.innkeeper = true;
    CHECK(ErrandPolicy::Choose(town, 0) == Errand::Inn);
    CHECK(ErrandPolicy::Choose(ErrandPolicy::Town(), 10) == Errand::None);
}
TEST(errand_policy_idle_errands_follow_their_weights)
{
    ErrandPolicy::Town const town = BigCity();
    CHECK(ErrandPolicy::Choose(town, 0) == Errand::Bank);
    CHECK(ErrandPolicy::Choose(town, 29) == Errand::Bank);
    CHECK(ErrandPolicy::Choose(town, 30) == Errand::Auction);
    CHECK(ErrandPolicy::Choose(town, 60) == Errand::Inn);
    CHECK(ErrandPolicy::Choose(town, 85) == Errand::Mailbox);
    CHECK(ErrandPolicy::Choose(town, 99) == Errand::Mailbox);
}
TEST(errand_policy_stays_and_gestures_like_a_player)
{
    CHECK(ErrandPolicy::DwellMs(Errand::Inn, 0) == 60000);
    CHECK(ErrandPolicy::DwellMs(Errand::Inn, 99) == 180000);
    CHECK(ErrandPolicy::DwellMs(Errand::Vendor, 99) <= 20000);
    CHECK(ErrandPolicy::DwellMs(Errand::Auction, 50) > ErrandPolicy::DwellMs(Errand::Repair, 50));
    CHECK(ErrandPolicy::SitsThere(Errand::Inn));
    CHECK(!ErrandPolicy::SitsThere(Errand::Bank));
    CHECK(ErrandPolicy::NextGestureMs(0) == 20000);
    CHECK(ErrandPolicy::NextGestureMs(99) == 60000);
}
TEST(errand_policy_leaves_town_after_a_few_errands)
{
    CHECK(ErrandPolicy::AnotherErrand(1, 59));
    CHECK(!ErrandPolicy::AnotherErrand(1, 60));
    CHECK(ErrandPolicy::AnotherErrand(2, 29));
    CHECK(!ErrandPolicy::AnotherErrand(2, 30));
    CHECK(!ErrandPolicy::AnotherErrand(3, 0));
}

TEST(errand_a_soak_bot_whose_visit_is_due_goes_to_the_auction_house_before_idling)
{
    ErrandPolicy::Town town;
    town.auctioneer = true;
    town.banker = true;
    town.innkeeper = true;
    town.mailbox = true;
    town.auctionDue = true;
    for (uint32_t roll = 0; roll < 100; ++roll)
        CHECK(ErrandPolicy::Choose(town, roll) == ErrandPolicy::Errand::Auction);

    // Needs come first, as before.
    town.needsRepair = true;
    town.repairer = true;
    CHECK(ErrandPolicy::Choose(town, 50) == ErrandPolicy::Errand::Repair);
}

TEST(errand_a_due_visit_needs_an_auctioneer_in_reach)
{
    ErrandPolicy::Town town;
    town.banker = true;
    town.auctionDue = true;
    CHECK(ErrandPolicy::Choose(town, 0) == ErrandPolicy::Errand::Bank);
}
