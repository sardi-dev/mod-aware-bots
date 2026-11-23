#include "TestHarness.h"
#include "DefendPolicy.h"

TEST(defend_holds_unpulled_mark_for_attacked_member)
{
    CHECK(DefendPolicy::HoldMark(false, true, true, true, false, true));
    CHECK(DefendPolicy::HoldMark(false, true, false, true, false, false));
    CHECK(!DefendPolicy::HoldMark(true, true, true, true, false, true));
}
TEST(defend_preserves_tank_pull_rule)
{
    CHECK(DefendPolicy::HoldMark(false, false, true, false, false, true));
    CHECK(!DefendPolicy::HoldMark(false, false, true, true, false, true));
    CHECK(!DefendPolicy::HoldMark(false, false, false, false, false, true));
}
