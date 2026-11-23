#include "TestHarness.h"
#include "LfgLeavePolicy.h"

TEST(lfg_leave_waits_for_the_planned_time)
{
    CHECK(!LfgLeavePolicy::MayLeave(false, true, 0, false));
    CHECK(LfgLeavePolicy::MayLeave(true, true, 0, false));
}
TEST(lfg_leave_loot_and_fight_wait_at_most_45_seconds)
{
    CHECK(!LfgLeavePolicy::MayLeave(true, false, 44999, false));
    CHECK(LfgLeavePolicy::MayLeave(true, false, 45000, false));
}
TEST(lfg_leave_open_roll_holds_up_to_60_seconds)
{
    CHECK(!LfgLeavePolicy::MayLeave(true, true, 0, true));
    CHECK(!LfgLeavePolicy::MayLeave(true, true, 59999, true));
    CHECK(LfgLeavePolicy::MayLeave(true, true, 60000, true));
    CHECK(LfgLeavePolicy::MayLeave(true, false, 60000, true));
}

TEST(lfg_bot_deadline_never_removes_a_connected_humans_party)
{
    CHECK(!LfgLeavePolicy::MayTimeOut(false, true));
    CHECK(!LfgLeavePolicy::MayTimeOut(true, true));
    CHECK(!LfgLeavePolicy::MayTimeOut(false, false));
    CHECK(LfgLeavePolicy::MayTimeOut(true, false));
}

TEST(lfg_combat_leave_waits_for_the_fight_to_be_over)
{
    // BT-206: the Officers stood 17 yd from the healer and never hit her for 20 minutes: the fight is over but the flag.
    CHECK(LfgLeavePolicy::FightIsOver(false, false, LfgLeavePolicy::FIGHT_QUIET_SECONDS));
    CHECK(!LfgLeavePolicy::FightIsOver(false, false, LfgLeavePolicy::FIGHT_QUIET_SECONDS - 1));
    CHECK(!LfgLeavePolicy::FightIsOver(true, false, 600));
    CHECK(!LfgLeavePolicy::FightIsOver(false, true, 600));
}

TEST(lfg_combat_leave_only_from_a_group_without_a_player)
{
    CHECK(LfgLeavePolicy::MayLeaveFromCombat(false, true));
    CHECK(!LfgLeavePolicy::MayLeaveFromCombat(false, false));
    CHECK(!LfgLeavePolicy::MayLeaveFromCombat(true, true));
    CHECK(!LfgLeavePolicy::MayLeaveFromCombat(true, false));
}
