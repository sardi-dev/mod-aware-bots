#include "TestHarness.h"
#include "PvpSubtletyPolicy.h"

// BT-691 to BT-693: the Subtlety Rogue package: Sap on a second enemy in the opener, the go held for the window, and Tricks of the Trade
// on a damage partner at the team go.

using namespace PvpSubtlety;

namespace
{
SapInputs Unaware()
{
    SapInputs in;
    in.ready = true;
    in.candidateDistance = 6.0f;
    return in;
}
}  // namespace

TEST(sub_sap_takes_the_healer_first_then_a_caster)
{
    SapInputs healer = Unaware();
    healer.candidateIsHealer = true;
    SapVerdict h = Sap(healer, Tier::EXPERT);
    CHECK(h.cast);
    CHECK(h.reason == SapReason::HEALER);
    SapInputs caster = Unaware();
    caster.candidateIsCaster = true;
    SapVerdict c = Sap(caster, Tier::EXPERT);
    CHECK(c.reason == SapReason::CASTER);
    CHECK(h.score > c.score);
}

TEST(sub_sap_expert_keeps_it_for_a_healer_or_caster_and_a_good_bot_saps_any_second_enemy)
{
    CHECK(!Sap(Unaware(), Tier::EXPERT).cast);
    SapVerdict g = Sap(Unaware(), Tier::GOOD);
    CHECK(g.cast);
    CHECK(g.reason == SapReason::OTHER);
    CHECK(g.score < SCORE_SAP_CASTER);
}

TEST(sub_sap_needs_stealth_an_unaware_target_and_reach)
{
    SapInputs in = Unaware();
    in.candidateIsHealer = true;
    in.ready = false;
    CHECK(Sap(in, Tier::EXPERT).reason == SapReason::NOT_READY);
    in = Unaware();
    in.candidateIsHealer = true;
    in.botInCombat = true;
    CHECK(Sap(in, Tier::EXPERT).reason == SapReason::IN_COMBAT);
    in = Unaware();
    in.candidateIsHealer = true;
    in.candidateInCombat = true;
    CHECK(Sap(in, Tier::EXPERT).reason == SapReason::IN_COMBAT);
    in = Unaware();
    in.candidateIsHealer = true;
    in.candidateDistance = 12.0f;
    CHECK(Sap(in, Tier::EXPERT).reason == SapReason::OUT_OF_REACH);
}

TEST(sub_sap_never_the_kill_target_a_held_one_or_a_spent_category_for_an_expert)
{
    SapInputs in = Unaware();
    in.candidateIsHealer = true;
    in.candidateIsKillTarget = true;
    CHECK(Sap(in, Tier::EXPERT).reason == SapReason::KILL_TARGET);
    in = Unaware();
    in.candidateIsHealer = true;
    in.candidateHeld = true;
    CHECK(Sap(in, Tier::GOOD).reason == SapReason::ALREADY_HELD);
    in = Unaware();
    in.candidateIsHealer = true;
    in.drStep = 3;
    CHECK(Sap(in, Tier::EXPERT).reason == SapReason::DR_SPENT);
    CHECK(Sap(in, Tier::GOOD).cast);   // a good bot does not read the diminishing returns
    CHECK(Sap(in, Tier::CASUAL).reason == SapReason::CASUAL_SKIPS);
}

TEST(sub_go_is_held_for_the_window_against_a_player)
{
    CHECK(Go(true, false, Tier::EXPERT) == GoReason::HOLD);
    CHECK(!GoAllowed(Go(true, false, Tier::GOOD)));
    CHECK(Go(true, true, Tier::EXPERT) == GoReason::WINDOW);
    CHECK(GoAllowed(Go(true, true, Tier::GOOD)));
}

TEST(sub_go_pve_and_casual_bots_keep_the_old_rotation)
{
    CHECK(Go(false, false, Tier::EXPERT) == GoReason::NOT_A_PLAYER);
    CHECK(GoAllowed(Go(false, false, Tier::EXPERT)));
    CHECK(Go(true, false, Tier::CASUAL) == GoReason::CASUAL_FREE);
    CHECK(GoAllowed(Go(true, false, Tier::CASUAL)));
}

TEST(sub_tricks_goes_to_a_damage_partner_when_the_team_go_opens_for_an_expert)
{
    TricksInputs in;
    in.ready = true;
    in.teamGoOpen = true;
    TricksCandidate p;
    p.damageDealer = true;
    CHECK(Tricks(in, true, p, Tier::EXPERT) == TricksReason::PARTNER_DAMAGE);
    p.bursting = true;
    CHECK(Tricks(in, true, p, Tier::EXPERT) == TricksReason::PARTNER_BURSTING);
    CHECK(TricksDue(TricksReason::PARTNER_BURSTING));
    CHECK(Tricks(in, true, p, Tier::GOOD) == TricksReason::TIER_SKIPS);
}

TEST(sub_tricks_needs_the_go_a_ready_spell_and_a_damage_dealer_not_a_healer_or_a_tank)
{
    TricksInputs in;
    in.ready = true;
    TricksCandidate p;
    p.damageDealer = true;
    CHECK(Tricks(in, true, p, Tier::EXPERT) == TricksReason::NO_GO);
    in.teamGoOpen = true;
    p.damageDealer = false;
    CHECK(Tricks(in, true, p, Tier::EXPERT) == TricksReason::NO_PARTNER);
    CHECK(Tricks(in, false, p, Tier::EXPERT) == TricksReason::NO_PARTNER);
    in.ready = false;
    p.damageDealer = true;
    CHECK(Tricks(in, true, p, Tier::EXPERT) == TricksReason::NOT_READY);
}

TEST(sub_tricks_score_prefers_the_bursting_and_the_healthier_partner)
{
    TricksCandidate a;
    a.damageDealer = true;
    TricksCandidate b = a;
    b.bursting = true;
    CHECK(TricksScore(b) > TricksScore(a));
    TricksCandidate c = a;
    c.healthPercent = 50.0f;
    CHECK(TricksScore(a) > TricksScore(c));
}
