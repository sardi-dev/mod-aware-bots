#include "GearEarnPolicy.h"
#include "TestHarness.h"

TEST(gear_junk_materials_and_protections)
{
    using namespace GearEarnPolicy;
    CHECK(IsJunk(false, false, false, 1, 7, 8, 0));
    CHECK(!IsJunk(false, false, false, 0, 7, 8, Cooking));
    CHECK(!IsJunk(false, false, false, 1, 7, 9, Herbalism));
    CHECK(!IsJunk(false, false, false, 1, 7, 9, Inscription));
    CHECK(!IsJunk(false, false, false, 1, 7, 7, Mining));
    CHECK(!IsJunk(false, false, false, 1, 7, 7, Jewelcrafting));
    CHECK(!IsJunk(false, false, false, 1, 7, 6, Skinning));
    CHECK(!IsJunk(false, false, false, 1, 7, 5, FirstAid));
    CHECK(IsJunk(false, false, false, 1, 7, 5, Cooking));
    CHECK(!IsJunk(false, false, false, 1, 7, 12, Enchanting));
    CHECK(!IsJunk(false, false, false, 1, 7, 10, 0));
    CHECK(!IsJunk(false, false, false, 1, 7, 1, 0));
    CHECK(!IsJunk(true, false, false, 0, 7, 8, 0));
}

TEST(gear_junk_quest_and_future_gear)
{
    using namespace GearEarnPolicy;
    CHECK(!IsJunk(false, true, true, 0, 12, 0, 0));
    CHECK(IsJunk(false, true, false, 1, 12, 0, 0));
    CHECK(KeepFutureGear(15, 10, true, true));
    CHECK(!KeepFutureGear(16, 10, true, true));
    CHECK(!KeepFutureGear(15, 10, false, true));
    CHECK(!KeepFutureGear(15, 10, true, false));
}
