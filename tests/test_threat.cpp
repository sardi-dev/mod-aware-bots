/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "TestHarness.h"
#include "ThreatMath.h"

TEST(threat_tank_without_threat_and_bot_with_some_is_zero)
{
    // This case used to divide by zero (BT-112).
    CHECK(ThreatMath::Ratio(500.0f, 0.0f) == 0);
}

TEST(threat_nobody_has_threat_is_zero)
{
    CHECK(ThreatMath::Ratio(0.0f, 0.0f) == 0);
    CHECK(ThreatMath::Ratio(0.0f, -1.0f) == 0);
}

TEST(threat_normal_ratio)
{
    CHECK(ThreatMath::Ratio(50.0f, 100.0f) == 50);
    CHECK(ThreatMath::Ratio(100.0f, 100.0f) == 100);
    CHECK(ThreatMath::Ratio(130.0f, 100.0f) == 130);
}

TEST(threat_far_above_the_tank_is_capped)
{
    CHECK(ThreatMath::Ratio(100000.0f, 1.0f) == 255);
    CHECK(ThreatMath::Ratio(255.0f, 100.0f) == 255);
}
