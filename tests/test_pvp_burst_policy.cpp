#include "TestHarness.h"
#include "PvpBurstPolicy.h"

// BT-620: the burst window against an enemy player: tier gating, the closed cases, the reasons and their scores, the
// Metamorphosis entry and keep rules and the Presence of Mind route.

using namespace PvpBurst;

namespace
{
Inputs Player()
{
    Inputs in;
    in.targetIsEnemyPlayer = true;
    return in;
}
}  // namespace

TEST(burst_not_a_player_is_not_this_policys_business)
{
    Inputs in;
    in.lone = true;
    for (Tier tier : { Tier::EXPERT, Tier::GOOD, Tier::CASUAL })
    {
        Verdict v = Decide(in, tier);
        CHECK(!v.open);
        CHECK(v.reason == Reason::NONE);
    }
}

TEST(burst_duel_is_the_window_for_every_tier)
{
    Inputs in = Player();
    in.lone = true;
    for (Tier tier : { Tier::EXPERT, Tier::GOOD, Tier::CASUAL })
    {
        Verdict v = Decide(in, tier);
        CHECK(v.open);
        CHECK(v.reason == Reason::DUEL);
        CHECK(v.score == SCORE_DUEL);
    }
}

TEST(burst_expert_and_good_hold_a_fresh_fight_with_no_window)
{
    Inputs in = Player();
    in.fightMs = 3000;
    CHECK(!Decide(in, Tier::EXPERT).open);
    CHECK(!Decide(in, Tier::GOOD).open);
}

TEST(burst_casual_spends_at_once_on_any_player)
{
    Inputs in = Player();
    Verdict v = Decide(in, Tier::CASUAL);
    CHECK(v.open);
    CHECK(v.reason == Reason::OPPORTUNITY);
}

TEST(burst_long_fight_falls_back_after_the_tier_time)
{
    Inputs in = Player();
    in.fightMs = FALLBACK_GOOD_MS - 1;
    CHECK(!Decide(in, Tier::GOOD).open);
    in.fightMs = FALLBACK_GOOD_MS;
    Verdict good = Decide(in, Tier::GOOD);
    CHECK(good.open);
    CHECK(good.reason == Reason::LONG_FIGHT);

    // The expert waits longer than the good bot: it holds out for a real window.
    in.fightMs = FALLBACK_GOOD_MS;
    CHECK(!Decide(in, Tier::EXPERT).open);
    in.fightMs = FALLBACK_EXPERT_MS;
    CHECK(Decide(in, Tier::EXPERT).open);
    CHECK(FALLBACK_EXPERT_MS > FALLBACK_GOOD_MS);
}

TEST(burst_immune_target_closes_the_window_for_good_and_expert_but_not_casual)
{
    Inputs in = Player();
    in.lone = true;
    in.immune = true;
    Verdict expert = Decide(in, Tier::EXPERT);
    CHECK(!expert.open);
    CHECK(expert.reason == Reason::IMMUNE);
    CHECK(!Decide(in, Tier::GOOD).open);
    CHECK(Decide(in, Tier::CASUAL).open);  // a casual bot wastes it on an Ice Block
}

TEST(burst_breakable_cc_closes_the_window_so_the_sheep_is_not_woken_by_a_burst)
{
    Inputs in = Player();
    in.lone = true;
    in.breakableCc = true;
    Verdict v = Decide(in, Tier::EXPERT);
    CHECK(!v.open);
    CHECK(v.reason == Reason::BREAKABLE_CC);
    CHECK(!Decide(in, Tier::GOOD).open);
    CHECK(Decide(in, Tier::CASUAL).open);
}

TEST(burst_team_window_and_partner_burst_need_a_tier_that_reads_the_team)
{
    Inputs in = Player();
    in.teamWindow = true;
    Verdict expert = Decide(in, Tier::EXPERT);
    CHECK(expert.open && expert.reason == Reason::TEAM_WINDOW);
    CHECK(Decide(in, Tier::GOOD).open);

    in.teamWindow = false;
    in.partnerBurst = true;
    Verdict partner = Decide(in, Tier::GOOD);
    CHECK(partner.open && partner.reason == Reason::PARTNER_BURST);
}

TEST(burst_held_target_is_a_window_for_good_and_expert)
{
    Inputs in = Player();
    in.controlled = true;
    Verdict v = Decide(in, Tier::GOOD);
    CHECK(v.open && v.reason == Reason::TARGET_CONTROLLED);
    CHECK(Decide(in, Tier::EXPERT).open);
}

TEST(burst_trinket_seen_down_is_read_by_experts_only)
{
    Inputs in = Player();
    in.trinketKnownDown = true;
    Verdict expert = Decide(in, Tier::EXPERT);
    CHECK(expert.open && expert.reason == Reason::TRINKET_DOWN);
    CHECK(!Decide(in, Tier::GOOD).open);
}

TEST(burst_hurt_target_is_finished)
{
    Inputs in = Player();
    in.targetHpPercent = TARGET_LOW_HP_PERCENT;
    Verdict v = Decide(in, Tier::GOOD);
    CHECK(v.open && v.reason == Reason::TARGET_LOW);
    in.targetHpPercent = TARGET_LOW_HP_PERCENT + 1;
    CHECK(!Decide(in, Tier::GOOD).open);
}

TEST(burst_best_reason_wins_by_score)
{
    Inputs in = Player();
    in.lone = true;
    in.teamWindow = true;
    in.controlled = true;
    in.targetHpPercent = 10;
    in.fightMs = 60000;
    CHECK(Decide(in, Tier::EXPERT).reason == Reason::DUEL);
    in.lone = false;
    CHECK(Decide(in, Tier::EXPERT).reason == Reason::TEAM_WINDOW);
    in.teamWindow = false;
    CHECK(Decide(in, Tier::EXPERT).reason == Reason::TARGET_CONTROLLED);
    in.controlled = false;
    CHECK(Decide(in, Tier::EXPERT).reason == Reason::TARGET_LOW);
    in.targetHpPercent = 100;
    CHECK(Decide(in, Tier::EXPERT).reason == Reason::LONG_FIGHT);
}

TEST(burst_decision_is_deterministic)
{
    Inputs in = Player();
    in.controlled = true;
    in.fightMs = 5000;
    Verdict a = Decide(in, Tier::GOOD);
    Verdict b = Decide(in, Tier::GOOD);
    CHECK(a.open == b.open && a.reason == b.reason && a.score == b.score);
}

TEST(burst_metamorphosis_enters_from_300_fury_in_a_window_and_not_twice)
{
    Verdict open;
    open.open = true;
    open.reason = Reason::DUEL;
    CHECK(MetamorphosisEnterVsPlayer(open, META_PVP_ENTER_FURY, false));
    CHECK(!MetamorphosisEnterVsPlayer(open, META_PVP_ENTER_FURY - 1, false));
    CHECK(!MetamorphosisEnterVsPlayer(open, 900, true));
    Verdict closed;
    CHECK(!MetamorphosisEnterVsPlayer(closed, 900, false));
}

TEST(burst_metamorphosis_keep_floor_is_below_the_entry_floor_against_a_player)
{
    // The PvE trigger drops the form at 300; if the PvP entry at 300 used the same drop it would be undone at once.
    CHECK(META_PVP_LEAVE_FURY < META_PVP_ENTER_FURY);
    CHECK(!MetamorphosisLeave(META_PVP_ENTER_FURY, true, 300));
    CHECK(MetamorphosisLeave(META_PVP_LEAVE_FURY, true, 300));
    CHECK(MetamorphosisLeave(300, false, 300));   // PvE: unchanged
    CHECK(!MetamorphosisLeave(301, false, 300));
}

TEST(burst_presence_of_mind_goes_to_control_never_to_a_pyroblast)
{
    PomInputs in;
    CHECK(RoutePresenceOfMind(in) == PomRoute::HOLD);

    in.windowOpen = true;
    in.frostjawReady = true;
    CHECK(RoutePresenceOfMind(in) == PomRoute::FROSTJAW);

    in.targetAlreadyHeld = true;
    CHECK(RoutePresenceOfMind(in) == PomRoute::HOLD);

    in.polymorphDue = true;  // the control plan's link wins over the freeze setup
    CHECK(RoutePresenceOfMind(in) == PomRoute::POLYMORPH);

    PomInputs noWindow;
    noWindow.frostjawReady = true;
    CHECK(RoutePresenceOfMind(noWindow) == PomRoute::HOLD);
}
