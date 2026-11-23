/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "TestHarness.h"

#include "ActivityWakePolicy.h"

using namespace ActivityWakePolicy;

TEST(activity_wake_scans_once_a_second_with_a_radius)
{
    CHECK(ScanDue(0, 5000, true));
    CHECK(!ScanDue(4500, 5000, true));
    CHECK(ScanDue(4000, 5000, true));
    CHECK(!ScanDue(0, 5000, false));
}

TEST(activity_wake_only_a_sleeping_bot_that_may_wake)
{
    CHECK(Wake(false, true, false));
    CHECK(!Wake(true, true, false));
    CHECK(!Wake(false, false, false));
    CHECK(!Wake(false, true, true));
}
