#include "TestHarness.h"
#include "DpsOffhealPolicy.h"

TEST(dps_offheal_hysteresis_and_health_limits)
{
    using namespace DpsOffhealPolicy;
    CHECK(Enabled(false, false, 0, false));
    CHECK(Enabled(false, true, 14.9f, true));
    CHECK(!Enabled(false, true, 15, false));
    CHECK(Enabled(true, true, 30, false));
    CHECK(!Enabled(true, true, 30.1f, true));
    CHECK(!Enabled(false, true, 20, false));
    CHECK(Heal(true, 34.9f));
    CHECK(!Heal(true, 35));
    CHECK(!Heal(false, 1));
}
