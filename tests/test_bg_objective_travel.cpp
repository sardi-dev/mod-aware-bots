#include "TestHarness.h"
#include "BgObjectiveTravel.h"

TEST(bg_long_objectives_use_hops_only_on_maps_without_routes)
{
    CHECK(BgObjectiveTravel::UseHops(761, 395.0f));
    CHECK(BgObjectiveTravel::UseHops(761, 420.0f));
    CHECK(BgObjectiveTravel::UseHops(1105, 71.0f));
    CHECK(!BgObjectiveTravel::UseHops(761, 70.0f));
    CHECK(!BgObjectiveTravel::UseHops(1105, 10.0f));
    CHECK(!BgObjectiveTravel::UseHops(529, 395.0f));
}
