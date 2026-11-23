#include "TestHarness.h"
#include "DpsThreatPolicy.h"

TEST(dps_threat_policy_holds_damage_at_limits)
{
    CHECK(!DpsThreatPolicy::Block(false, false, false, 79, 0));
    CHECK(DpsThreatPolicy::Block(false, false, false, 80, 0));
    CHECK(DpsThreatPolicy::Block(false, false, true, 0, 50));
    CHECK(!DpsThreatPolicy::Block(true, false, true, 100, 100));
    CHECK(!DpsThreatPolicy::Block(false, true, true, 100, 100));
    CHECK(DpsThreatPolicy::IsInterruptOrControl("kick"));
    CHECK(!DpsThreatPolicy::IsInterruptOrControl("blackout kick"));
}
