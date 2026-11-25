#include "GearEarnPolicy.h"
#include "TestHarness.h"

TEST(gear_reward_ties_are_deterministic)
{
    using namespace GearEarnPolicy;
    Reward a{0, true, 10, 5}, b{0, false, 100, 50};
    CHECK(BetterReward(a, b));
    b.equip = true;
    CHECK(BetterReward(b, a));
    b.level = 10;
    CHECK(BetterReward(b, a));
    CHECK(!BetterReward(a, a));
    a.score = 1;
    CHECK(BetterReward(a, b));
}

TEST(gear_zero_score_fallback_requires_usable_improvement)
{
    using namespace GearEarnPolicy;
    CHECK(BetterZeroScore(0, 0, true, 5, 4));
    CHECK(!BetterZeroScore(0, 0, false, 5, 4));
    CHECK(!BetterZeroScore(1, 0, true, 5, 4));
    CHECK(!BetterZeroScore(0, 0, true, 4, 4));
}

TEST(gear_bag_choice_respects_budget_and_size)
{
    using namespace GearEarnPolicy;
    CHECK(BetterBag(8, 50, 50, 6, 10));
    CHECK(!BetterBag(8, 51, 50, 6, 10));
    CHECK(BetterBag(8, 20, 50, 8, 30));
    CHECK(!BetterBag(6, 1, 50, 8, 30));
}

TEST(gear_junk_respects_protections_and_professions)
{
    using namespace GearEarnPolicy;
    CHECK(Junk(false, true, false, false, false));
    CHECK(Junk(false, false, true, false, false));
    CHECK(!Junk(false, false, true, true, false));
    CHECK(!Junk(true, true, true, false, true));
    CHECK(Junk(false, false, false, false, true));
}
