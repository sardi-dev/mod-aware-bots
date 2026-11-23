#include "TestHarness.h"
#include "DpsTargetPolicy.h"
TEST(dps_group_target_order_and_stickiness)
{
    using namespace DpsTargetPolicy;
    CHECK(Rank(true, true, true, true, true) == HealerAttacker);
    CHECK(Rank(false, true, true, true, true) == MemberAttacker);
    CHECK(Rank(false, false, true, true, true) == Skull);
    CHECK(Rank(false, false, false, true, true) == Cross);
    CHECK(Rank(false, false, false, false, true) == TankTarget);
    CHECK(Rank(false, false, false, false, false) == Existing);
    CHECK(Keep(true, 3999, 1000));
    CHECK(!Keep(true, 4000, 1000));
    CHECK(!Keep(false, 1001, 1000));
    CHECK(Keep(true, 5, 0xfffffff0u));
}
