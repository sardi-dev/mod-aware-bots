#include "TestHarness.h"
#include "MistweaverHealingPolicy.h"

TEST(mistweaver_holds_melee_for_living_low_members)
{
    CHECK(MistweaverHealingPolicy::HoldMelee(true, 44.9f, 45.0f));
    CHECK(MistweaverHealingPolicy::HoldMelee(true, 1.0f, 45.0f));
    CHECK(!MistweaverHealingPolicy::HoldMelee(true, 45.0f, 45.0f));
    CHECK(!MistweaverHealingPolicy::HoldMelee(false, 0.0f, 45.0f));
}
