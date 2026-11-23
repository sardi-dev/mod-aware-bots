#include "TestHarness.h"
#include "MarkPullPolicy.h"

TEST(mark_pull_health_thresholds)
{
    CHECK(!MarkPullPolicy::HealthReady(79.99f, true));
    CHECK(MarkPullPolicy::HealthReady(80.0f, true));
    CHECK(!MarkPullPolicy::HealthReady(59.99f, false));
    CHECK(MarkPullPolicy::HealthReady(60.0f, false));
}
TEST(mark_pull_mana_thresholds)
{
    CHECK(!MarkPullPolicy::ManaReady(69.99f, true, true));
    CHECK(MarkPullPolicy::ManaReady(70.0f, true, true));
    CHECK(!MarkPullPolicy::ManaReady(49.99f, true, false));
    CHECK(MarkPullPolicy::ManaReady(50.0f, true, false));
    CHECK(MarkPullPolicy::ManaReady(50.0f, false, true));
    CHECK(!MarkPullPolicy::ManaReady(49.99f, false, false));
}
TEST(mark_pull_defence_ends_active_pull)
{
    CHECK(MarkPullPolicy::EndPullForDefence(true, true));
    CHECK(!MarkPullPolicy::EndPullForDefence(true, false));
    CHECK(!MarkPullPolicy::EndPullForDefence(false, true));
    CHECK(!MarkPullPolicy::EndPullForDefence(false, false));
}

TEST(mark_pull_damage_mana_hold_is_capped)
{
    CHECK(MarkPullPolicy::ManaWaits(10.0f, false, false, true, 19));
    CHECK(!MarkPullPolicy::ManaWaits(10.0f, false, false, true, 20));
    CHECK(!MarkPullPolicy::ManaWaits(50.0f, false, false, true, 0));
}
TEST(mark_pull_healer_and_tank_mana_stay_strict)
{
    CHECK(MarkPullPolicy::ManaWaits(40.0f, true, false, false, 600));
    CHECK(MarkPullPolicy::ManaWaits(60.0f, true, true, false, 600));
    CHECK(!MarkPullPolicy::ManaWaits(70.0f, true, true, false, 0));
    CHECK(MarkPullPolicy::ManaWaits(30.0f, false, false, false, 600));
}
TEST(mark_pull_mark_reach)
{
    CHECK(MarkPullPolicy::MarkReachable(40.0f, 40.0f, false));
    CHECK(!MarkPullPolicy::MarkReachable(41.4f, 40.0f, false));
    CHECK(MarkPullPolicy::MarkReachable(41.4f, 40.0f, true));
    CHECK(MarkPullPolicy::MarkReachable(60.0f, 40.0f, true));
    CHECK(!MarkPullPolicy::MarkReachable(60.1f, 40.0f, true));
}
TEST(mark_pull_stuck_after_ten_ready_seconds_standing)
{
    CHECK(!MarkPullPolicy::PullStuck(0, 1000, false));
    CHECK(!MarkPullPolicy::PullStuck(1000, 1009, false));
    CHECK(MarkPullPolicy::PullStuck(1000, 1010, false));
    CHECK(!MarkPullPolicy::PullStuck(1000, 1030, true));
}
TEST(mark_pull_walk_ends_inside_the_range)
{
    // Deadmines run 5: Provoke 40 yd minus contact is 39.5, the two bodies 3.0 yd; the walk aimed at 36.5 and stopped
    // a fraction outside the range test.
    float const range = 39.5f;
    float const bodies = 3.0f;
    float const reach = MarkPullPolicy::PullReachDistance(range, bodies);
    CHECK_NEAR(reach, 34.5f, 0.001f);
    CHECK(reach + bodies <= range - MarkPullPolicy::PULL_RANGE_MARGIN_YARDS + 0.001f);
    CHECK_NEAR(MarkPullPolicy::PullReachDistance(4.0f, 3.0f), 0.0f, 0.001f);
}
TEST(mark_pull_body_pull_only_an_immune_dungeon_boss)
{
    // Deadmines run 5, Foe Reaper 5000: flags 393538 while stunned, 320 after the stun (0x100 kept until he aggroes).
    CHECK(MarkPullPolicy::BodyPull(true, true, false, 320u));
    CHECK(!MarkPullPolicy::BodyPull(true, true, false, 393538u));
    CHECK(!MarkPullPolicy::BodyPull(true, true, false, 0x40u));
}
TEST(mark_pull_body_pull_never_outside_a_dungeon_boss)
{
    CHECK(!MarkPullPolicy::BodyPull(false, true, false, 320u));
    CHECK(!MarkPullPolicy::BodyPull(true, false, false, 320u));
    CHECK(!MarkPullPolicy::BodyPull(true, true, true, 320u));
}
TEST(mark_pull_body_pull_blocked_by_any_other_flag)
{
    unsigned const blockers[] = { 0x2u, 0x80u, 0x10000u, 0x20000u, 0x40000u, 0x2000000u };
    for (unsigned flag : blockers)
        CHECK(!MarkPullPolicy::BodyPull(true, true, false, 0x100u | flag));
}

TEST(mark_pull_tank_walks_to_the_leader_after_ten_seconds_out_of_reach)
{
    using namespace MarkPullPolicy;
    // Not out of reach: no clock, no walk.
    CHECK(!WalksToLeader(0, 1000, 80.0f));
    // The first nine seconds the tank waits as before; at ten it walks.
    CHECK(!WalksToLeader(1000, 1000 + LEADER_WALK_AFTER_SECONDS - 1, 80.0f));
    CHECK(WalksToLeader(1000, 1000 + LEADER_WALK_AFTER_SECONDS, 80.0f));
    // Near the leader (the follow already holds the tank there) it stays.
    CHECK(!WalksToLeader(1000, 1100, LEADER_NEAR_YARDS));
    CHECK(WalksToLeader(1000, 1100, LEADER_NEAR_YARDS + 0.1f));
    // The walk ends nearer than the walk starts, or the tank would start and stop at the edge.
    CHECK(LEADER_SIDE_YARDS < LEADER_NEAR_YARDS);
}

TEST(mark_pull_healer_sight_recovery_boundaries)
{
    using namespace MarkPullPolicy;
    CHECK(!HealerWalksIntoSight(0, 1000, 40.0f, false));
    CHECK(!HealerWalksIntoSight(1000, 1009, 40.0f, false));
    CHECK(HealerWalksIntoSight(1000, 1010, 40.0f, false));
    CHECK(!HealerWalksIntoSight(1000, 1010, 40.01f, false));
    CHECK(!HealerWalksIntoSight(1000, 1010, 10.0f, true));
    CHECK(!HealerSightWalkTimedOut(0, 1025, false));
    CHECK(!HealerSightWalkTimedOut(1010, 1024, false));
    CHECK(HealerSightWalkTimedOut(1010, 1025, false));
    CHECK(!HealerSightWalkTimedOut(1010, 1025, true));
}

TEST(mark_pull_tank_steps_into_a_bot_healers_sight_after_her_own_walk)
{
    using namespace MarkPullPolicy;
    // Shadowfang Keep run 8: one mark held 2 min 33 s on "healer is out of sight". A bot healer gets her wait and her
    // walk (10 + 15 s) first; then the tank steps.
    CHECK(!TankStepsIntoHealerSight(0, 1100, true, false));
    CHECK(!TankStepsIntoHealerSight(1000, 1000 + HEALER_SIGHT_WAIT_SECONDS + HEALER_SIGHT_WALK_SECONDS - 1, true, false));
    CHECK(TankStepsIntoHealerSight(1000, 1000 + HEALER_SIGHT_WAIT_SECONDS + HEALER_SIGHT_WALK_SECONDS, true, false));
    // In sight: nothing to step for.
    CHECK(!TankStepsIntoHealerSight(1000, 1100, true, true));
    // A real player healer is walked by nobody, so the tank steps after the plain wait.
    CHECK(!TankStepsIntoHealerSight(1000, 1000 + HEALER_SIGHT_WAIT_SECONDS - 1, false, false));
    CHECK(TankStepsIntoHealerSight(1000, 1000 + HEALER_SIGHT_WAIT_SECONDS, false, false));
    // The step ends inside the healer range the ready test reads, so it cannot undo the "caught up" test.
    CHECK(TANK_SIGHT_STEP_YARDS < HEALER_RANGE);
}

TEST(mark_pull_tank_sight_clock_runs_per_mark_and_healer)
{
    using namespace MarkPullPolicy;
    TankSightState state;
    NoteTankSight(state, 7, 0, 1000);
    CHECK(state.markGuid == 7ull && state.since == 0);
    NoteTankSight(state, 7, 42, 1000);
    CHECK(state.since == 1000 && state.healerGuid == 42ull);
    NoteTankSight(state, 7, 42, 1030);
    CHECK(state.since == 1000);
    // Sight back for a moment does not restart the clock, so a follow move round the corner cannot hold the pull anew.
    NoteTankSight(state, 7, 0, 1031);
    CHECK(state.since == 1000);
    NoteTankSight(state, 7, 42, 1040);
    CHECK(state.since == 1000);
    // Another healer out of sight starts her own clock.
    NoteTankSight(state, 7, 43, 1040);
    CHECK(state.since == 1040 && state.healerGuid == 43ull);
    // A new mark starts everything again, including the log flags.
    state.stepLogged = true;
    NoteTankSight(state, 8, 42, 1041);
    CHECK(state.markGuid == 8ull && state.since == 1041 && !state.stepLogged);
}

TEST(mark_pull_healer_sight_waived_after_a_minute_for_that_mark_only)
{
    using namespace MarkPullPolicy;
    TankSightState state;
    NoteTankSight(state, 7, 42, 1000);
    NoteTankSight(state, 7, 42, 1000 + HEALER_SIGHT_WAIVE_SECONDS - 1);
    CHECK(!SightWaivedFor(state, 7));
    NoteTankSight(state, 7, 42, 1000 + HEALER_SIGHT_WAIVE_SECONDS);
    CHECK(SightWaivedFor(state, 7));
    // The waive lasts for the mark even when the next reading has no hidden healer (the pull is now possible), or the
    // clock would restart and hold the same pull another minute.
    NoteTankSight(state, 7, 0, 1000 + HEALER_SIGHT_WAIVE_SECONDS + 2);
    CHECK(SightWaivedFor(state, 7));
    CHECK(!SightWaivedFor(state, 8));
    NoteTankSight(state, 8, 42, 2000);
    CHECK(!SightWaivedFor(state, 8));
    // Both walks come before the waive.
    CHECK(TANK_SIGHT_STEP_AFTER_BOT_HEALER_SECONDS < HEALER_SIGHT_WAIVE_SECONDS);
}

TEST(mark_pull_blind_mark_in_range_walks_to_the_leader_after_15_seconds)
{
    using namespace MarkPullPolicy;
    CHECK(!WalksToLeaderForSight(0, 1000, 80.0f));
    CHECK(!WalksToLeaderForSight(1000, 1000 + BLIND_MARK_WALK_AFTER_SECONDS - 1, 30.0f));
    CHECK(WalksToLeaderForSight(1000, 1000 + BLIND_MARK_WALK_AFTER_SECONDS, 30.0f));
    // nearer than the blind rule's own range the tank already stands beside the leader
    CHECK(!WalksToLeaderForSight(1000, 1100, BLIND_LEADER_NEAR_YARDS));
    CHECK(WalksToLeaderForSight(1000, 1100, BLIND_LEADER_NEAR_YARDS + 0.1f));
    // the out-of-reach walk is quicker and starts further out
    CHECK(BLIND_MARK_WALK_AFTER_SECONDS > LEADER_WALK_AFTER_SECONDS);
    CHECK(BLIND_LEADER_NEAR_YARDS < LEADER_NEAR_YARDS);
}
