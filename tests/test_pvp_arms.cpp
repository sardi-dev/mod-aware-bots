#include "TestHarness.h"
#include "PvpArmsPolicy.h"

// BT-673 to BT-676: the Arms package: the go (Recklessness, Skull Banner, Avatar) with pooled rage, the stun chain, Disarm after a
// stun, and the Touch of Karma id.

using namespace PvpArms;

namespace
{
GoInputs Window(std::uint32_t rage, std::uint32_t openMs = 0)
{
    GoInputs in;
    in.targetIsPlayer = true;
    in.windowOpen = true;
    in.rage = rage;
    in.windowOpenMs = openMs;
    return in;
}
}  // namespace

TEST(arms_go_expert_pools_eighty_rage_in_the_window)
{
    CHECK(Go(Window(80), Tier::EXPERT) == GoReason::GO);
    CHECK(Go(Window(100), Tier::EXPERT) == GoReason::GO);
    CHECK(Go(Window(79), Tier::EXPERT) == GoReason::POOLING);
    CHECK(!GoAllowed(Go(Window(40), Tier::EXPERT)));
}

TEST(arms_go_gives_up_the_pool_after_the_timeout_so_a_dumping_warrior_still_goes)
{
    CHECK(Go(Window(30, GO_POOL_TIMEOUT_MS - 1), Tier::EXPERT) == GoReason::POOLING);
    GoReason const late = Go(Window(30, GO_POOL_TIMEOUT_MS), Tier::EXPERT);
    CHECK(late == GoReason::POOL_TIMEOUT);
    CHECK(GoAllowed(late));
}

TEST(arms_go_good_bot_goes_with_the_window_and_no_pool)
{
    CHECK(Go(Window(0), Tier::GOOD) == GoReason::GO);
}

TEST(arms_go_waits_for_the_window_and_leaves_pve_and_casual_bots_alone)
{
    GoInputs in = Window(100);
    in.windowOpen = false;
    CHECK(Go(in, Tier::EXPERT) == GoReason::WINDOW_CLOSED);
    CHECK(!GoAllowed(Go(in, Tier::GOOD)));
    CHECK(Go(in, Tier::CASUAL) == GoReason::CASUAL_FREE);
    CHECK(GoAllowed(Go(in, Tier::CASUAL)));
    GoInputs pve;
    CHECK(Go(pve, Tier::EXPERT) == GoReason::NOT_A_PLAYER);
    CHECK(GoAllowed(Go(pve, Tier::EXPERT)));
}

namespace
{
StunInputs Close(bool closer = true)
{
    StunInputs in;
    in.targetIsPlayer = true;
    in.afterGapCloser = closer;
    in.stormBoltReady = true;
    in.shockwaveReady = true;
    in.targetDistance = 5.0f;
    in.targetInFront = true;
    in.enemiesInCone = 1;
    return in;
}
}  // namespace

TEST(arms_stun_storm_bolt_after_a_gap_closer)
{
    StunVerdict v = StunChain(Close(), Tier::EXPERT);
    CHECK(v.cast);
    CHECK(v.reason == StunReason::STORM_BOLT);
    CHECK(v.spellId == SPELL_STORM_BOLT);
    CHECK(v.score == SCORE_STUN_BASE + SCORE_STUN_AFTER_CLOSER);
    CHECK(StunChain(Close(), Tier::GOOD).cast);
}

TEST(arms_stun_shockwave_when_it_catches_a_second_player_or_storm_bolt_is_spent)
{
    StunInputs in = Close();
    in.enemiesInCone = 2;
    StunVerdict v = StunChain(in, Tier::EXPERT);
    CHECK(v.reason == StunReason::SHOCKWAVE);
    CHECK(v.spellId == SPELL_SHOCKWAVE);
    CHECK(v.score == SCORE_STUN_BASE + SCORE_STUN_AFTER_CLOSER + SCORE_SHOCKWAVE_PER_EXTRA);
    in = Close();
    in.stormBoltReady = false;
    CHECK(StunChain(in, Tier::EXPERT).reason == StunReason::SHOCKWAVE);
    in.targetInFront = false;
    CHECK(StunChain(in, Tier::EXPERT).reason == StunReason::NOT_READY);
}

TEST(arms_stun_needs_a_setup_and_a_free_target_with_a_full_stun_for_an_expert)
{
    StunInputs in = Close(false);
    CHECK(StunChain(in, Tier::EXPERT).reason == StunReason::NO_SETUP);
    in.windowOpen = true;
    CHECK(StunChain(in, Tier::EXPERT).cast);   // the window is a setup too
    in.targetHeld = true;
    CHECK(StunChain(in, Tier::EXPERT).reason == StunReason::TARGET_HELD);
    in.targetHeld = false;
    in.stunDrStep = 1;
    CHECK(StunChain(in, Tier::EXPERT).reason == StunReason::DR_SPENT);
    CHECK(StunChain(in, Tier::GOOD).cast);   // a good bot does not read the diminishing returns
}

TEST(arms_stun_range_player_and_casual_gates)
{
    StunInputs in = Close();
    in.targetDistance = 31.0f;
    CHECK(StunChain(in, Tier::EXPERT).reason == StunReason::NOT_READY);
    in.targetDistance = 25.0f;
    CHECK(StunChain(in, Tier::EXPERT).reason == StunReason::STORM_BOLT);   // Storm Bolt reaches, Shockwave does not
    in = Close();
    in.targetIsPlayer = false;
    CHECK(StunChain(in, Tier::EXPERT).reason == StunReason::NOT_A_PLAYER);
    CHECK(StunChain(Close(), Tier::CASUAL).reason == StunReason::CASUAL_SKIPS);
}

TEST(arms_stun_score_is_higher_after_a_closer_and_inside_the_window)
{
    StunInputs a = Close(false);
    a.windowOpen = true;
    StunInputs b = Close(true);
    CHECK(StunChain(b, Tier::EXPERT).score > StunChain(a, Tier::EXPERT).score);
    b.windowOpen = true;
    CHECK(StunChain(b, Tier::EXPERT).score == SCORE_STUN_BASE + SCORE_STUN_AFTER_CLOSER + SCORE_STUN_WINDOW);
}

namespace
{
DisarmInputs Brawler()
{
    DisarmInputs in;
    in.ready = true;
    in.targetIsMeleePlayer = true;
    in.targetDistance = 3.0f;
    return in;
}
}  // namespace

TEST(arms_disarm_after_a_stun_for_good_and_expert_bots)
{
    DisarmInputs in = Brawler();
    in.targetStunned = true;
    CHECK(Disarm(in, Tier::EXPERT) == DisarmReason::AFTER_STUN);
    CHECK(DisarmDue(Disarm(in, Tier::GOOD)));
    CHECK(Disarm(in, Tier::CASUAL) == DisarmReason::CASUAL_SKIPS);
}

TEST(arms_disarm_against_a_physical_burst_is_an_expert_read_and_a_good_bot_uses_the_window)
{
    DisarmInputs in = Brawler();
    in.targetBursting = true;
    CHECK(Disarm(in, Tier::EXPERT) == DisarmReason::PHYSICAL_BURST);
    CHECK(Disarm(in, Tier::GOOD) == DisarmReason::NO);
    in.windowOpen = true;
    CHECK(Disarm(in, Tier::GOOD) == DisarmReason::WINDOW);
    in.targetBursting = false;
    CHECK(Disarm(in, Tier::EXPERT) == DisarmReason::NO);   // a window alone is not enough for an expert
}

TEST(arms_disarm_refuses_a_ranged_target_a_disarmed_one_a_far_one_and_an_unready_spell)
{
    DisarmInputs in = Brawler();
    in.targetStunned = true;
    in.targetIsMeleePlayer = false;
    CHECK(Disarm(in, Tier::EXPERT) == DisarmReason::NOT_A_MELEE_PLAYER);
    in = Brawler();
    in.targetStunned = true;
    in.alreadyDisarmed = true;
    CHECK(Disarm(in, Tier::EXPERT) == DisarmReason::ALREADY_DISARMED);
    in = Brawler();
    in.targetStunned = true;
    in.targetDistance = 6.0f;
    CHECK(Disarm(in, Tier::EXPERT) == DisarmReason::OUT_OF_REACH);
    in = Brawler();
    in.targetStunned = true;
    in.ready = false;
    CHECK(Disarm(in, Tier::EXPERT) == DisarmReason::NOT_READY);
}

TEST(arms_spell_ids_match_the_5_4_8_data)
{
    CHECK(SPELL_RECKLESSNESS == 1719);
    CHECK(SPELL_SKULL_BANNER == 114207);
    CHECK(SPELL_STORM_BOLT == 107570);
    CHECK(SPELL_SHOCKWAVE == 46968);
    CHECK(AURA_TOUCH_OF_KARMA == 122470);
}
