#include "TestHarness.h"
#include "WsgPlanLogic.h"

using namespace WsgLogic;

TEST(wsg_carrier_target_fight_alone)
{
    CHECK(CarrierMayFight(1, 1, 0, 40.0f, Tier::GOOD));
    CHECK(CarrierMayFight(1, 0, 0, 40.0f, Tier::EXPERT));
    CHECK(CarrierMayFight(1, 1, 0, 25.0f, Tier::CASUAL));
    CHECK(!CarrierMayFight(0, 0, 0, 100.0f, Tier::EXPERT));
    CHECK(!CarrierMayFight(2, 1, 0, 100.0f, Tier::GOOD));
    CHECK(!CarrierMayFight(1, 2, 0, 100.0f, Tier::GOOD));
    CHECK(!CarrierMayFight(1, 1, 1, 100.0f, Tier::GOOD));
    CHECK(!CarrierMayFight(1, 1, 0, 39.9f, Tier::GOOD));
    CHECK(!CarrierMayFight(1, 1, 0, 24.9f, Tier::CASUAL));
}
