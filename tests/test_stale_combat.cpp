#include "TestHarness.h"
#include "StaleCombatPolicy.h"

TEST(stale_combat_needs_the_flag_and_a_quiet_stretch)
{
    CHECK(!StaleCombatPolicy::IsStale(true, 0, 1000));
    CHECK(!StaleCombatPolicy::IsStale(true, 1000, 1014));
    CHECK(StaleCombatPolicy::IsStale(true, 1000, 1015));
    CHECK(!StaleCombatPolicy::IsStale(false, 1000, 1100));
}
TEST(stale_combat_quiet_clock_starts_keeps_and_resets)
{
    CHECK(StaleCombatPolicy::NextQuietSince(true, false, 0, 500) == 500);
    CHECK(StaleCombatPolicy::NextQuietSince(true, false, 500, 520) == 500);
    CHECK(StaleCombatPolicy::NextQuietSince(true, true, 500, 520) == 0);
    CHECK(StaleCombatPolicy::NextQuietSince(false, false, 500, 520) == 0);
}
TEST(stale_target_kept_without_sight_for_twenty_seconds)
{
    CHECK(!StaleCombatPolicy::KeepWithoutSight(0, 1000));
    CHECK(StaleCombatPolicy::KeepWithoutSight(1000, 1000));
    CHECK(StaleCombatPolicy::KeepWithoutSight(1000, 1019));
    CHECK(!StaleCombatPolicy::KeepWithoutSight(1000, 1020));
}
TEST(stale_combat_ignores_creatures_a_player_cannot_select)
{
    CHECK(StaleCombatPolicy::HoldsInFight(0));
    CHECK(StaleCombatPolicy::HoldsInFight(0x00008000));          // an ordinary mob's flags
    CHECK(!StaleCombatPolicy::HoldsInFight(0x02000000));         // Springvale Spawn Stalker: not selectable
    CHECK(!StaleCombatPolicy::HoldsInFight(0x00000002));         // not attackable
    CHECK(!StaleCombatPolicy::HoldsInFight(0x02008000));
}
