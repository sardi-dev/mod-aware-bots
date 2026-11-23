#include "TestHarness.h"
#include "GroupRejoinPolicy.h"

namespace
{
constexpr float SIGHT = 100.0f;

// One reading of a bot flagged in combat at (-236, 2164) with the nearest member `member` yards away that last took
// damage at `damageAt` (0 = never).
StuckMemberPolicy::Sample Reading(float member, time_t damageAt = 0, float x = -236.0f)
{
    StuckMemberPolicy::Sample sample;
    sample.inCombat = true;
    sample.nearestMember = member;
    sample.x = x;
    sample.y = 2164.0f;
    sample.z = 96.0f;
    sample.lastDamageAt = damageAt;
    return sample;
}

// Reads every 2 s from `from` to `to` (inclusive) and returns the member state.
void ReadEvery2s(StuckMemberPolicy::State& state, time_t from, time_t to, StuckMemberPolicy::Sample const& sample)
{
    for (time_t t = from; t <= to; t += 2)
        StuckMemberPolicy::Read(state, sample, t);
}
}  // namespace

TEST(group_rejoin_waits_thirty_seconds_far_and_twenty_still)
{
    // Shadowfang Keep run 7: the healer stood still at 40 to 148 yd from every member.
    StuckMemberPolicy::State member;
    GroupRejoinPolicy::State rejoin;
    ReadEvery2s(member, 1000, 1028, Reading(60.0f));
    CHECK(!GroupRejoinPolicy::Wants(member, rejoin, SIGHT, 1028));
    ReadEvery2s(member, 1030, 1030, Reading(60.0f));
    CHECK(GroupRejoinPolicy::Wants(member, rejoin, SIGHT, 1030));
}
TEST(group_rejoin_a_walking_bot_inside_sight_is_left_to_the_follow_code)
{
    StuckMemberPolicy::State member;
    GroupRejoinPolicy::State rejoin;
    for (int i = 0; i <= 20; ++i)
        StuckMemberPolicy::Read(member, Reading(60.0f, 0, -236.0f + 4.0f * i), 1000 + 2 * i);
    CHECK(!GroupRejoinPolicy::Wants(member, rejoin, SIGHT, 1040));
}
TEST(group_rejoin_a_bot_beyond_sight_distance_walks_even_when_it_moves)
{
    StuckMemberPolicy::State member;
    GroupRejoinPolicy::State rejoin;
    for (int i = 0; i <= 15; ++i)
        StuckMemberPolicy::Read(member, Reading(120.0f, 0, -236.0f + 4.0f * i), 1000 + 2 * i);
    CHECK(GroupRejoinPolicy::Wants(member, rejoin, SIGHT, 1030));
}
TEST(group_rejoin_a_member_in_range_or_a_stale_reading_never_wants)
{
    StuckMemberPolicy::State member;
    GroupRejoinPolicy::State rejoin;
    ReadEvery2s(member, 1000, 1040, Reading(60.0f));
    CHECK(GroupRejoinPolicy::Wants(member, rejoin, SIGHT, 1040));
    // the engine stopped reading: the old numbers do not count
    CHECK(!GroupRejoinPolicy::Wants(member, rejoin, SIGHT, 1060));
    StuckMemberPolicy::Read(member, Reading(30.0f), 1042);
    CHECK(!GroupRejoinPolicy::Wants(member, rejoin, SIGHT, 1042));
}
TEST(group_rejoin_the_walk_is_judged_by_yards_closed_per_window)
{
    GroupRejoinPolicy::State rejoin;
    using GroupRejoinPolicy::Step;
    CHECK(GroupRejoinPolicy::NextStep(rejoin, 90.0f, 0, 0, 0, true, 1000) == Step::Walk);
    CHECK(GroupRejoinPolicy::NextStep(rejoin, 90.0f, 0, 0, 0, true, 1005) == Step::Walk);
    // a window that closed 6 yd is progress and clears the failures
    CHECK(GroupRejoinPolicy::NextStep(rejoin, 84.0f, 0, 0, 0, true, 1010) == Step::Walk);
    CHECK(rejoin.walkFails == 0);
    // two windows with no progress and no movement: the second one drops
    CHECK(GroupRejoinPolicy::NextStep(rejoin, 84.0f, 0, 0, 0, true, 1020) == Step::Walk);
    CHECK(rejoin.walkFails == 1);
    CHECK(GroupRejoinPolicy::NextStep(rejoin, 83.0f, 0, 0, 0, true, 1030) == Step::Drop);
    CHECK(rejoin.walkFails == 2);
}
TEST(group_rejoin_chasing_a_moving_tank_is_not_a_failed_window)
{
    // The tank walks on at the bot's speed: the distance never closes, but the bot ran 70 yd in 10 s.
    GroupRejoinPolicy::State rejoin;
    using GroupRejoinPolicy::Step;
    GroupRejoinPolicy::NextStep(rejoin, 90.0f, 0, 0, 0, true, 1000);
    CHECK(GroupRejoinPolicy::NextStep(rejoin, 90.0f, 70, 0, 0, true, 1010) == Step::Walk);
    CHECK(GroupRejoinPolicy::NextStep(rejoin, 92.0f, 140, 0, 0, true, 1020) == Step::Walk);
    CHECK(GroupRejoinPolicy::NextStep(rejoin, 95.0f, 210, 0, 0, true, 1030) == Step::Walk);
    CHECK(rejoin.walkFails == 0);
}
TEST(group_rejoin_a_moving_window_breaks_the_chain_of_failures)
{
    GroupRejoinPolicy::State rejoin;
    GroupRejoinPolicy::NextStep(rejoin, 90.0f, 0, 0, 0, true, 1000);
    GroupRejoinPolicy::NextStep(rejoin, 90.0f, 1, 0, 0, true, 1010);
    CHECK(rejoin.walkFails == 1);
    // moved 6 yd in the window, distance not closed: not a failure, the count starts over
    GroupRejoinPolicy::NextStep(rejoin, 91.0f, 7, 0, 0, true, 1020);
    CHECK(rejoin.walkFails == 0);
    // under 5 yd of own movement and no gain: a failure again
    GroupRejoinPolicy::NextStep(rejoin, 91.0f, 10, 0, 0, true, 1030);
    CHECK(rejoin.walkFails == 1);
}
TEST(group_rejoin_only_a_flagged_bot_drops)
{
    GroupRejoinPolicy::State rejoin;
    using GroupRejoinPolicy::Step;
    GroupRejoinPolicy::NextStep(rejoin, 90.0f, 0, 0, 0, false, 1000);
    CHECK(GroupRejoinPolicy::NextStep(rejoin, 90.0f, 0, 0, 0, false, 1010) == Step::Walk);
    CHECK(GroupRejoinPolicy::NextStep(rejoin, 90.0f, 0, 0, 0, false, 1020) == Step::Walk);
    CHECK(GroupRejoinPolicy::NextStep(rejoin, 90.0f, 0, 0, 0, false, 1030) == Step::Walk);
}
TEST(group_rejoin_idle_starts_the_walk_over)
{
    GroupRejoinPolicy::State rejoin;
    GroupRejoinPolicy::NextStep(rejoin, 90.0f, 0, 0, 0, true, 1000);
    GroupRejoinPolicy::NextStep(rejoin, 90.0f, 0, 0, 0, true, 1010);
    CHECK(rejoin.walkFails == 1);
    GroupRejoinPolicy::Idle(rejoin);
    CHECK(rejoin.walkSince == 0);
    CHECK(rejoin.walkFails == 0);
    CHECK(GroupRejoinPolicy::NextStep(rejoin, 90.0f, 0, 0, 0, true, 1011) == GroupRejoinPolicy::Step::Walk);
}
TEST(group_rejoin_the_flag_is_stale_only_when_no_damage_came_and_nothing_stands_at_it)
{
    // Run 7: three Officers 17 yd away, no damage for 20 minutes.
    StuckMemberPolicy::State member;
    ReadEvery2s(member, 1000, 1040, Reading(60.0f));
    CHECK(GroupRejoinPolicy::FlagIsStale(member, false, false, 1040));
    CHECK(!GroupRejoinPolicy::FlagIsStale(member, true, false, 1040));
    // a player or a player's pet attacks her: a fight whatever the distance
    CHECK(!GroupRejoinPolicy::FlagIsStale(member, false, true, 1040));
    // damage at 1042: not stale for 30 s after it
    StuckMemberPolicy::Read(member, Reading(60.0f, 1042), 1042);
    CHECK(!GroupRejoinPolicy::FlagIsStale(member, false, false, 1044));
    ReadEvery2s(member, 1044, 1070, Reading(60.0f, 1042));
    CHECK(!GroupRejoinPolicy::FlagIsStale(member, false, false, 1070));
    ReadEvery2s(member, 1072, 1072, Reading(60.0f, 1042));
    CHECK(GroupRejoinPolicy::FlagIsStale(member, false, false, 1072));
}
TEST(group_rejoin_damage_that_healed_back_still_counts)
{
    // The hook records the hit, so a reading that sees full health again cannot hide it (net health could).
    StuckMemberPolicy::State member;
    ReadEvery2s(member, 1000, 1040, Reading(60.0f, 1039));
    CHECK(member.hurtAt == 1039);
    CHECK(!GroupRejoinPolicy::FlagIsStale(member, false, false, 1040));
}
TEST(group_rejoin_far_for_less_than_thirty_seconds_is_never_stale)
{
    StuckMemberPolicy::State member;
    ReadEvery2s(member, 1000, 1020, Reading(60.0f));
    CHECK(!GroupRejoinPolicy::FlagIsStale(member, false, false, 1020));
}
TEST(group_rejoin_after_a_drop_the_bot_keeps_walking_until_a_member_is_near)
{
    StuckMemberPolicy::State member;
    GroupRejoinPolicy::State rejoin;
    rejoin.droppedAt = 1055;
    rejoin.followUntil = 1100;
    StuckMemberPolicy::Sample out = Reading(70.0f);
    out.inCombat = false;
    StuckMemberPolicy::Read(member, out, 1060);
    CHECK(GroupRejoinPolicy::Wants(member, rejoin, SIGHT, 1060));
    // near the group again, or the window over
    StuckMemberPolicy::Sample near = out;
    near.nearestMember = 30.0f;
    StuckMemberPolicy::Read(member, near, 1066);
    CHECK(!GroupRejoinPolicy::Wants(member, rejoin, SIGHT, 1066));
    StuckMemberPolicy::Read(member, out, 1101);
    CHECK(!GroupRejoinPolicy::Wants(member, rejoin, SIGHT, 1101));
}
TEST(group_rejoin_damage_after_a_drop_ends_the_post_drop_walk)
{
    StuckMemberPolicy::State member;
    GroupRejoinPolicy::State rejoin;
    rejoin.droppedAt = 1055;
    rejoin.followUntil = 1100;
    // damage from before the drop changes nothing
    GroupRejoinPolicy::EndFollowOnDamage(rejoin, 1040);
    CHECK(rejoin.followUntil == 1100);
    GroupRejoinPolicy::EndFollowOnDamage(rejoin, 1055);
    CHECK(rejoin.followUntil == 1100);
    // a creature that still holds her hits her at 1062: the walk is over for good
    GroupRejoinPolicy::EndFollowOnDamage(rejoin, 1062);
    CHECK(rejoin.followUntil == 0);
    StuckMemberPolicy::Sample out = Reading(70.0f, 1062);
    out.inCombat = false;
    StuckMemberPolicy::Read(member, out, 1064);
    CHECK(!GroupRejoinPolicy::Wants(member, rejoin, SIGHT, 1064));
    // and it stays over: the old stamp does not bring it back
    GroupRejoinPolicy::EndFollowOnDamage(rejoin, 1062);
    CHECK(rejoin.followUntil == 0);
}
TEST(group_rejoin_run_seven_healer_walks_twice_then_drops)
{
    // The whole story in numbers: flagged from 1000, far and still; reading every 2 s; the walk order every 2 s.
    StuckMemberPolicy::State member;
    GroupRejoinPolicy::State rejoin;
    time_t dropAt = 0;
    for (time_t t = 1000; t <= 1100 && !dropAt; t += 2)
    {
        StuckMemberPolicy::Read(member, Reading(72.0f), t);
        if (!GroupRejoinPolicy::Wants(member, rejoin, SIGHT, t))
        {
            GroupRejoinPolicy::Idle(rejoin);
            continue;
        }
        // the bot never moves: distance to the tank stays 72
        if (GroupRejoinPolicy::NextStep(rejoin, 72.0f, -236.0f, 2164.0f, 96.0f, true, t) == GroupRejoinPolicy::Step::Drop &&
            GroupRejoinPolicy::FlagIsStale(member, false, false, t))
            dropAt = t;
    }
    // 30 s far, then window 1 (10 s) and window 2 (10 s)
    CHECK(dropAt >= 1050 && dropAt <= 1054);
}

TEST(group_rejoin_walk_order_backs_off_from_two_seconds_to_thirty_while_no_ground_is_gained)
{
    // BT-698: Jihma stood 5890 yd from Sonwyn and was ordered to walk on every trigger tick for two hours.
    GroupRejoinPolicy::State state;
    time_t t = 1000;
    CHECK(GroupRejoinPolicy::MoveDue(state, 300.0f, t));  // the first order goes out at once
    CHECK(state.moveGap == 2);
    t += 1;
    CHECK(!GroupRejoinPolicy::MoveDue(state, 300.0f, t));  // inside the gap
    t += 1;
    CHECK(GroupRejoinPolicy::MoveDue(state, 300.0f, t));  // 2 s after the first
    CHECK(state.moveGap == 4);
    t += 3;
    CHECK(!GroupRejoinPolicy::MoveDue(state, 300.0f, t));
    t += 1;
    CHECK(GroupRejoinPolicy::MoveDue(state, 300.0f, t));
    CHECK(state.moveGap == 8);
    t += 8;
    CHECK(GroupRejoinPolicy::MoveDue(state, 300.0f, t));
    CHECK(state.moveGap == 16);
    t += 16;
    CHECK(GroupRejoinPolicy::MoveDue(state, 300.0f, t));
    CHECK(state.moveGap == 30);
    t += 30;
    CHECK(GroupRejoinPolicy::MoveDue(state, 300.0f, t));
    CHECK(state.moveGap == 30);  // capped
}

TEST(group_rejoin_walk_order_starts_over_as_soon_as_the_distance_shrinks)
{
    GroupRejoinPolicy::State state;
    time_t t = 1000;
    GroupRejoinPolicy::MoveDue(state, 300.0f, t);
    for (int i = 0; i < 5; ++i)
    {
        t += state.moveGap;
        GroupRejoinPolicy::MoveDue(state, 300.0f, t);
    }
    CHECK(state.moveGap == 30);
    // The bot got moving: 14 yd gained in 2 s is ordered again at once, with the short gap.
    t += 2;
    CHECK(GroupRejoinPolicy::MoveDue(state, 286.0f, t));
    CHECK(state.moveGap == GroupRejoinPolicy::MOVE_GAP_MIN_SECONDS);
    // A bot that walks is ordered every 2 s, as before the backoff.
    for (float d = 272.0f; d > 200.0f; d -= 14.0f)
    {
        t += 2;
        CHECK(GroupRejoinPolicy::MoveDue(state, d, t));
        CHECK(state.moveGap == GroupRejoinPolicy::MOVE_GAP_MIN_SECONDS);
    }
}

TEST(group_rejoin_walk_order_beyond_five_hundred_yards_waits_thirty_seconds_after_the_second_order)
{
    GroupRejoinPolicy::State state;
    time_t t = 2000;
    CHECK(GroupRejoinPolicy::MoveDue(state, 5890.0f, t));
    t += 2;
    CHECK(GroupRejoinPolicy::MoveDue(state, 5890.0f, t));
    CHECK(state.moveGap == GroupRejoinPolicy::MOVE_GAP_MAX_SECONDS);
    // Two hours of 2 s ticks send one order every 30 s: 240 orders instead of 3600.
    uint32_t orders = 2;
    for (time_t end = t + 7200; t < end; t += 2)
        if (GroupRejoinPolicy::MoveDue(state, 5890.0f, t + 2))
            ++orders;
    CHECK(orders >= 230 && orders <= 245);
}

TEST(group_rejoin_walk_order_of_an_earlier_episode_does_not_hold_back_the_next)
{
    GroupRejoinPolicy::State state;
    time_t t = 1000;
    GroupRejoinPolicy::MoveDue(state, 300.0f, t);
    for (int i = 0; i < 6; ++i)
    {
        t += state.moveGap;
        GroupRejoinPolicy::MoveDue(state, 300.0f, t);
    }
    CHECK(state.moveGap == 30);
    // The walk was not wanted for several minutes; a new one begins with the short gap.
    t += GroupRejoinPolicy::MOVE_EPISODE_SECONDS + 1;
    CHECK(GroupRejoinPolicy::MoveDue(state, 300.0f, t));
    CHECK(state.moveGap == GroupRejoinPolicy::MOVE_GAP_MIN_SECONDS);
}
