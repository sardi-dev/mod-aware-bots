#include "TestHarness.h"
#include "DungeonReleasePolicy.h"

using namespace DungeonReleasePolicy;

TEST(dungeon_release_waits_only_for_plausible_resurrection)
{
    Facts f{0, 0, 60, 300, false, false, false, false};
    CHECK(Decide(f) == Decision::NoResurrecter);
    f.livingResurrecter = true;
    CHECK(Decide(f) == Decision::Wait);
    f.livingResurrecter = false;
    f.pendingRequest = true;
    CHECK(Decide(f) == Decision::Wait);
    f.pendingRequest = false;
    f.selfResurrection = true;
    CHECK(Decide(f) == Decision::Wait);
}

TEST(dungeon_release_obeys_combat_and_death_deadlines)
{
    Facts f{59, 59, 60, 300, false, false, false, true};
    CHECK(Decide(f) == Decision::Wait);
    f.secondsSinceCombatEnded = 60;
    CHECK(Decide(f) == Decision::CombatWaitExpired);
    f.pendingRequest = true;
    CHECK(Decide(f) == Decision::CombatWaitExpired);
    f.inCombat = true;
    f.secondsSinceDeath = 299;
    CHECK(Decide(f) == Decision::Wait);
    f.secondsSinceDeath = 300;
    CHECK(Decide(f) == Decision::DeathWaitExpired);
    f.selfResurrection = true;
    CHECK(Decide(f) == Decision::DeathWaitExpired);
}

TEST(dungeon_release_zero_limits_are_immediate)
{
    Facts f{0, 0, 0, 300, false, false, false, true};
    CHECK(Decide(f) == Decision::CombatWaitExpired);
    f.inCombat = true;
    CHECK(Decide(f) == Decision::Wait);
    f.maxWaitSeconds = 0;
    CHECK(Decide(f) == Decision::DeathWaitExpired);
}
