#include "TestHarness.h"
#include "WsgPlanLogic.h"

using namespace WsgLogic;

TEST(strike_run_roles_and_objective)
{
    for (Role role : {Role::TAKER, Role::BACKUP, Role::ESCORT})
        CHECK(StrikeRunOverridesCombat(role, true, true, 100.0f, 100.0f, false, 0, Tier::GOOD));
    for (Role role : {Role::HOME, Role::MID, Role::NONE})
        CHECK(!StrikeRunOverridesCombat(role, true, true, 100.0f, 100.0f, false, 0, Tier::GOOD));
    CHECK(!StrikeRunOverridesCombat(Role::TAKER, false, true, 100.0f, 100.0f, false, 0, Tier::GOOD));
    CHECK(!StrikeRunOverridesCombat(Role::TAKER, true, false, 100.0f, 100.0f, false, 0, Tier::GOOD));
}

TEST(strike_run_health_and_stand_boundaries)
{
    for (Tier tier : {Tier::GOOD, Tier::EXPERT})
    {
        CHECK(StrikeRunOverridesCombat(Role::TAKER, true, true, 40.0f, 30.1f, false, 0, tier));
        CHECK(!StrikeRunOverridesCombat(Role::TAKER, true, true, 39.9f, 30.1f, false, 0, tier));
    }
    CHECK(StrikeRunOverridesCombat(Role::BACKUP, true, true, 25.0f, 30.1f, false, 0, Tier::CASUAL));
    CHECK(!StrikeRunOverridesCombat(Role::BACKUP, true, true, 24.9f, 30.1f, false, 0, Tier::CASUAL));
    CHECK(!StrikeRunOverridesCombat(Role::TAKER, true, true, 100.0f, 30.0f, false, 0, Tier::GOOD));
    CHECK(!StrikeRunOverridesCombat(Role::TAKER, true, true, 100.0f, 10.0f, false, 0, Tier::GOOD));
}

TEST(strike_run_yields_when_held)
{
    CHECK(StrikeRunOverridesCombat(Role::ESCORT, true, true, 100.0f, 100.0f, false, 1, Tier::GOOD));
    CHECK(!StrikeRunOverridesCombat(Role::ESCORT, true, true, 100.0f, 100.0f, false, 2, Tier::GOOD));
    CHECK(!StrikeRunOverridesCombat(Role::ESCORT, true, true, 100.0f, 100.0f, true, 0, Tier::GOOD));
}
