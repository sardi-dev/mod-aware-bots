#include "TestHarness.h"
#include "BgIdleTracker.h"

TEST(bg_idle_waits_over_thirty_seconds_and_limits_logs)
{
    BgIdleTracker t;
    CHECK(!t.Update(0, 1, 0, 0, 0, true));
    CHECK(!t.Update(30000, 1, 10, 0, 0, true));
    CHECK(t.Update(30001, 1, 10, 0, 0, true));
    CHECK(!t.Update(60000, 1, 0, 0, 0, true));
    CHECK(t.Update(60001, 1, 0, 0, 0, true));
}

TEST(bg_idle_resets_anchor_and_eligibility_but_preserves_rate_limit)
{
    BgIdleTracker t;
    CHECK(!t.Update(0, 1, 0, 0, 0, true));
    CHECK(t.Update(30001, 1, 0, 0, 0, true));
    CHECK(!t.Update(30002, 1, 11, 0, 0, true));
    CHECK(!t.Update(30003, 1, 11, 0, 0, false));
    CHECK(!t.Update(30004, 1, 11, 0, 0, true));
    CHECK(t.Update(60005, 1, 11, 0, 0, true));
    CHECK(!t.Update(60006, 2, 11, 0, 0, true));
    CHECK(t.Update(90007, 2, 11, 0, 0, true));
}

TEST(bg_idle_handles_clock_wrap)
{
    BgIdleTracker t;
    CHECK(!t.Update(0xfffffff0u, 1, 0, 0, 0, true));
    CHECK(t.Update(30000, 1, 0, 0, 0, true));
}

TEST(bg_idle_rate_limit_survives_match_change)
{
    BgIdleTracker t;
    CHECK(!t.Update(0, 1, 0, 0, 0, true));
    CHECK(t.Update(30001, 1, 0, 0, 0, true));
    CHECK(!t.Update(30002, 2, 0, 0, 0, true));
    CHECK(!t.Update(60001, 2, 0, 0, 0, true));
    CHECK(t.Update(60003, 2, 0, 0, 0, true));
}
