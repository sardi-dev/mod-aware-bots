#include "TestHarness.h"
#include "WarlockPetPolicy.h"
TEST(warlock_keeps_a_living_correct_demon_when_a_summon_is_queued)
{
    CHECK(!WarlockPetPolicy::NeedsSummon(false, true, 1860, 1860, 58960));
    CHECK(!WarlockPetPolicy::NeedsSummon(false, true, 58960, 1860, 58960));
    CHECK(WarlockPetPolicy::NeedsSummon(false, true, 416, 1860, 58960));
}
TEST(warlock_recovers_a_lost_demon_but_honors_sacrifice)
{
    CHECK(WarlockPetPolicy::NeedsSummon(false, false, 1860, 1860, 58960));
    CHECK(WarlockPetPolicy::NeedsSummon(false, false, 0, 1860, 58960));
    CHECK(!WarlockPetPolicy::NeedsSummon(true, false, 0, 1860, 58960));
}
