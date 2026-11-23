#include "TestHarness.h"
#include "PvpRestoDruidPolicy.h"

// BT-668 to BT-672: the Restoration Druid package: Cenarion Ward on the focused ally and in a silence, Ironbark early on a burst,
// Nature's Swiftness then Cyclone on a bursting damage dealer, and Displacer Beast or Typhoon against a melee player that connected.

using namespace PvpRestoDruid;

namespace
{
WardInputs Focused(float pct, std::int32_t attackers = 1)
{
    WardInputs in;
    in.ready = true;
    in.allyHealthPercent = pct;
    in.playerAttackers = attackers;
    return in;
}
}  // namespace

TEST(rd_ward_goes_on_the_focused_ally_by_tier_line)
{
    CHECK(CenarionWard(Focused(95.0f), Tier::EXPERT).cast);
    CHECK(CenarionWard(Focused(95.0f), Tier::EXPERT).reason == WardReason::ON_FOCUS);
    CHECK(CenarionWard(Focused(96.0f), Tier::EXPERT).reason == WardReason::TOO_HEALTHY);
    CHECK(!CenarionWard(Focused(85.0f), Tier::GOOD).cast);   // the good line is 80
    CHECK(CenarionWard(Focused(80.0f), Tier::GOOD).cast);
}

TEST(rd_ward_needs_a_player_on_the_ally_or_a_silence)
{
    CHECK(CenarionWard(Focused(50.0f, 0), Tier::EXPERT).reason == WardReason::NOT_FOCUSED);
    WardInputs in = Focused(60.0f, 0);
    in.druidSilenced = true;
    WardVerdict v = CenarionWard(in, Tier::EXPERT);
    CHECK(v.cast);
    CHECK(v.reason == WardReason::WHILE_SILENCED);
    in.allyHealthPercent = 95.0f;   // nothing to ward yet
    CHECK(!CenarionWard(in, Tier::EXPERT).cast);
}

TEST(rd_ward_is_not_cast_twice_on_one_ally_or_by_a_casual_bot)
{
    WardInputs in = Focused(50.0f);
    in.allyHasWard = true;
    CHECK(CenarionWard(in, Tier::EXPERT).reason == WardReason::NOT_READY);
    in = Focused(50.0f);
    in.ready = false;
    CHECK(!CenarionWard(in, Tier::EXPERT).cast);
    CHECK(CenarionWard(Focused(50.0f), Tier::CASUAL).reason == WardReason::CASUAL_SKIPS);
}

TEST(rd_ward_score_picks_the_most_pressed_ally_and_counts_attackers_to_three)
{
    std::int32_t const one = CenarionWard(Focused(80.0f, 1), Tier::EXPERT).score;
    CHECK(CenarionWard(Focused(60.0f, 1), Tier::EXPERT).score > one);
    CHECK(CenarionWard(Focused(80.0f, 2), Tier::EXPERT).score == one + SCORE_WARD_PER_ATTACKER);
    CHECK(CenarionWard(Focused(80.0f, 9), Tier::EXPERT).score == CenarionWard(Focused(80.0f, ATTACKERS_COUNTED), Tier::EXPERT).score);
}

namespace
{
IronbarkInputs Hit(float pct, bool burst = false)
{
    IronbarkInputs in;
    in.ready = true;
    in.allyHealthPercent = pct;
    in.playerAttackers = 1;
    in.enemyBurstSeen = burst;
    return in;
}
}  // namespace

TEST(rd_ironbark_expert_answers_a_burst_high_on_health)
{
    IronbarkVerdict v = Ironbark(Hit(80.0f, true), Tier::EXPERT);
    CHECK(v.cast);
    CHECK(v.reason == IronbarkReason::BURST_SEEN);
    CHECK(!Ironbark(Hit(81.0f, true), Tier::EXPERT).cast);
    CHECK(Ironbark(Hit(80.0f, false), Tier::EXPERT).reason == IronbarkReason::TOO_HEALTHY);
}

TEST(rd_ironbark_without_a_burst_waits_for_the_tier_line_and_a_good_bot_reads_no_burst)
{
    CHECK(Ironbark(Hit(55.0f), Tier::EXPERT).reason == IronbarkReason::UNDER_FIRE);
    CHECK(!Ironbark(Hit(56.0f), Tier::EXPERT).cast);
    CHECK(!Ironbark(Hit(50.0f), Tier::GOOD).cast);
    CHECK(Ironbark(Hit(45.0f), Tier::GOOD).cast);
    CHECK(!Ironbark(Hit(70.0f, true), Tier::GOOD).cast);
}

TEST(rd_ironbark_needs_a_player_on_the_ally_and_is_not_cast_twice)
{
    IronbarkInputs in = Hit(30.0f);
    in.playerAttackers = 0;
    CHECK(Ironbark(in, Tier::EXPERT).reason == IronbarkReason::NOT_FOCUSED);
    in = Hit(30.0f);
    in.allyHasIronbark = true;
    CHECK(Ironbark(in, Tier::EXPERT).reason == IronbarkReason::NOT_READY);
    CHECK(Ironbark(Hit(30.0f), Tier::CASUAL).reason == IronbarkReason::CASUAL_SKIPS);
}

namespace
{
CycloneInputs Burster()
{
    CycloneInputs in;
    in.known = true;
    in.targetIsPlayer = true;
    in.targetBursting = true;
    in.allyHitByTarget = 1;
    in.targetDistance = 25.0f;
    return in;
}
}  // namespace

TEST(rd_cyclone_presses_swiftness_first_then_casts_instant)
{
    CycloneInputs in = Burster();
    in.swiftnessReady = true;
    CycloneVerdict v = BurstCyclone(in, Tier::EXPERT);
    CHECK(!v.cast);
    CHECK(v.pressSwiftness);
    CHECK(v.reason == CycloneReason::SWIFTNESS_FIRST);
    in.swiftnessUp = true;
    v = BurstCyclone(in, Tier::EXPERT);
    CHECK(v.cast);
    CHECK(!v.pressSwiftness);
    CHECK(v.reason == CycloneReason::INSTANT);
}

TEST(rd_cyclone_hard_cast_only_when_no_kick_is_to_be_feared)
{
    CycloneInputs in = Burster();
    in.enemyKickNotSpent = true;
    CHECK(BurstCyclone(in, Tier::EXPERT).reason == CycloneReason::HARD_CAST_KICKED);
    in.enemyKickNotSpent = false;
    CycloneVerdict v = BurstCyclone(in, Tier::EXPERT);
    CHECK(v.cast);
    CHECK(v.reason == CycloneReason::HARD_CAST);
    CHECK(v.score < BurstCyclone([&] { CycloneInputs i = Burster(); i.swiftnessUp = true; return i; }(), Tier::EXPERT).score);
}

TEST(rd_cyclone_leaves_the_healer_to_the_chain_and_needs_a_burst_on_an_ally)
{
    CycloneInputs in = Burster();
    in.targetIsHealer = true;
    CHECK(BurstCyclone(in, Tier::EXPERT).reason == CycloneReason::IS_HEALER);
    in = Burster();
    in.targetBursting = false;
    CHECK(BurstCyclone(in, Tier::EXPERT).reason == CycloneReason::NOT_BURSTING);
    in = Burster();
    in.allyHitByTarget = 0;
    CHECK(BurstCyclone(in, Tier::EXPERT).reason == CycloneReason::NO_THREAT);
}

TEST(rd_cyclone_skips_a_held_target_a_spent_category_and_a_far_one_and_is_expert_only)
{
    CycloneInputs in = Burster();
    in.targetHeld = true;
    CHECK(BurstCyclone(in, Tier::EXPERT).reason == CycloneReason::ALREADY_HELD);
    in = Burster();
    in.drStep = DR_IMMUNE_STEP;
    CHECK(BurstCyclone(in, Tier::EXPERT).reason == CycloneReason::DR_SPENT);
    in = Burster();
    in.drStep = DR_IMMUNE_STEP - 1;   // half length still counts
    CHECK(BurstCyclone(in, Tier::EXPERT).cast);
    in.targetDistance = CYCLONE_RANGE_YD + 1.0f;
    CHECK(BurstCyclone(in, Tier::EXPERT).reason == CycloneReason::NOT_A_PLAYER);
    CHECK(BurstCyclone(Burster(), Tier::GOOD).reason == CycloneReason::TIER_SKIPS);
    CHECK(BurstCyclone(Burster(), Tier::CASUAL).reason == CycloneReason::TIER_SKIPS);
    in = Burster();
    in.known = false;
    CHECK(BurstCyclone(in, Tier::EXPERT).reason == CycloneReason::NOT_KNOWN);
}

namespace
{
EscapeInputs Mauled(float hp)
{
    EscapeInputs in;
    in.inPvpFight = true;
    in.meleeAttackers = 1;
    in.healthPercent = hp;
    in.displacerReady = true;
    in.typhoonReady = true;
    return in;
}
}  // namespace

TEST(rd_displacer_is_traded_after_the_melee_connected_and_the_health_says_so)
{
    CHECK(MeleeEscape(Mauled(60.0f), Tier::EXPERT).reason == EscapeReason::DISPLACER);
    CHECK(MeleeEscape(Mauled(70.0f), Tier::EXPERT).reason == EscapeReason::TYPHOON);   // not low enough for the escape: the peel
    CHECK(MeleeEscape(Mauled(45.0f), Tier::GOOD).reason == EscapeReason::DISPLACER);
    CHECK(MeleeEscape(Mauled(50.0f), Tier::GOOD).reason == EscapeReason::NO);
    CHECK(MeleeEscape(Mauled(30.0f), Tier::CASUAL).reason == EscapeReason::DISPLACER);
    CHECK(MeleeEscape(Mauled(40.0f), Tier::CASUAL).reason == EscapeReason::NO);
}

TEST(rd_typhoon_peels_only_for_an_expert_and_never_into_a_team_sheep)
{
    EscapeInputs in = Mauled(70.0f);
    in.displacerReady = false;
    EscapeVerdict v = MeleeEscape(in, Tier::EXPERT);
    CHECK(v.reason == EscapeReason::TYPHOON);
    CHECK(MeleeEscape(in, Tier::GOOD).reason == EscapeReason::NO);
    in.breakableHeldNear = 1;
    CHECK(MeleeEscape(in, Tier::EXPERT).reason == EscapeReason::TEAM_CONTROL);
    in = Mauled(80.0f);
    in.displacerReady = false;
    CHECK(MeleeEscape(in, Tier::EXPERT).reason == EscapeReason::NO);   // above the peel line
}

TEST(rd_escape_needs_a_pvp_fight_and_a_melee_player_on_the_druid)
{
    EscapeInputs in = Mauled(20.0f);
    in.inPvpFight = false;
    CHECK(MeleeEscape(in, Tier::EXPERT).reason == EscapeReason::NOT_PVP);
    in = Mauled(20.0f);
    in.meleeAttackers = 0;
    CHECK(MeleeEscape(in, Tier::EXPERT).reason == EscapeReason::NO_MELEE);
    in = Mauled(20.0f);
    in.displacerReady = false;
    in.typhoonReady = false;
    CHECK(MeleeEscape(in, Tier::EXPERT).reason == EscapeReason::NO);
}

TEST(rd_escape_score_grows_with_damage_and_melee)
{
    std::int32_t const a = MeleeEscape(Mauled(60.0f), Tier::EXPERT).score;
    CHECK(MeleeEscape(Mauled(40.0f), Tier::EXPERT).score > a);
    EscapeInputs two = Mauled(60.0f);
    two.meleeAttackers = 2;
    CHECK(MeleeEscape(two, Tier::EXPERT).score == a + 5);
}
