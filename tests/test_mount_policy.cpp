#include "BotMountPolicy.h"
#include "TestHarness.h"

TEST(mount_stale_aura_without_mount_is_healed)
{
    CHECK(BotMountPolicy::IsStaleMount(false, false, true));
}

TEST(mount_real_mount_flight_path_or_no_aura_is_left_alone)
{
    CHECK(!BotMountPolicy::IsStaleMount(true, false, true));
    CHECK(!BotMountPolicy::IsStaleMount(false, true, true));
    CHECK(!BotMountPolicy::IsStaleMount(false, false, false));
}

TEST(mount_first_mount_after_riding_is_trained)
{
    CHECK(!BotMountPolicy::ShouldBuyMount(0, false));
    CHECK(BotMountPolicy::ShouldBuyMount(75, false));
    CHECK(!BotMountPolicy::ShouldBuyMount(150, true));
}

TEST(mount_cheapest_affordable_above_the_reserve)
{
    std::vector<std::uint64_t> const prices = {50000, 10000, 20000};
    CHECK(BotMountPolicy::CheapestAffordable(prices, 100000, 0) == 1);
    CHECK(BotMountPolicy::CheapestAffordable(prices, 15000, 6000) == -1);
    CHECK(BotMountPolicy::CheapestAffordable(prices, 15000, 5000) == 1);
    CHECK(BotMountPolicy::CheapestAffordable({}, 100000, 0) == -1);
    CHECK(BotMountPolicy::CheapestAffordable(prices, 1000, 5000) == -1);
}
