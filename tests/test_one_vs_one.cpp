#include "TestHarness.h"
#include "OneVsOnePolicy.h"

#include <set>
#include <string>

// 1v1 movement and crowd control policy (docs/duels/one-vs-one.md).

using namespace OneVsOne;

namespace
{
// A guid whose tier-window slip roll is clean at nowMs, found by search so the test does not depend on the hash.
std::uint32_t CleanGuid(Tier tier, std::uint32_t nowMs)
{
    for (std::uint32_t g = 1; g < 100000; ++g)
        if (!Slips(tier, g, nowMs))
            return g;
    return 0;
}

std::uint32_t SlippingGuid(Tier tier, std::uint32_t nowMs)
{
    for (std::uint32_t g = 1; g < 100000; ++g)
        if (Slips(tier, g, nowMs))
            return g;
    return 0;
}

Facts Ranged(Tier tier = Tier::EXPERT)
{
    Facts f;
    f.tier = tier;
    f.nowMs = 60000;
    f.guidLow = CleanGuid(tier, f.nowMs);
    f.self = Role::RANGED;
    f.enemy = Role::MELEE;
    f.threatForMs = 2000;
    f.contactForMs = 2000;
    return f;
}

Facts Melee(Tier tier = Tier::EXPERT)
{
    Facts f = Ranged(tier);
    f.self = Role::MELEE;
    f.enemy = Role::RANGED;
    f.apartForMs = 2000;
    return f;
}

bool Has(std::vector<Option> const& opts, Choice c)
{
    for (Option const& o : opts)
        if (o.choice == c)
            return true;
    return false;
}
}  // namespace

TEST(one_vs_one_hunter_in_melee_with_nothing_ready_holds)
{
    // The Goldshire picture: a hunter with a warrior on it and no spell ready fights where it stands (running at the same
    // speed only gives free hits), the slow comes first once it is ready.
    Facts f = Ranged();
    f.dist = 3.0f;
    CHECK(Choose(f).choice == Choice::HOLD);
}

TEST(one_vs_one_caught_ranged_escapes_first)
{
    Facts f = Ranged();
    f.dist = 3.0f;
    f.escapeReady = true;
    f.slowReady = true;
    f.slowRange = 40.0f;
    CHECK(Choose(f).choice == Choice::ESCAPE);
}

TEST(one_vs_one_approaching_melee_gets_slowed)
{
    Facts f = Ranged();
    f.dist = 18.0f;
    f.enemyApproaching = true;
    f.slowReady = true;
    f.slowRange = 40.0f;
    CHECK(Choose(f).choice == Choice::SLOW);
}

TEST(one_vs_one_slowed_melee_is_kited_to_keep_distance)
{
    Facts f = Ranged();
    f.dist = 9.0f;
    f.enemySlowed = true;
    f.enemyApproaching = true;
    Option const o = Choose(f);
    CHECK(o.choice == Choice::STEP_BACK);
    // expert keeps 25 yd: from 9 yd it steps back 16
    CHECK_NEAR(StepBackDistance(Tier::EXPERT, 9.0f), 16.0f, 0.01f);
    CHECK_NEAR(StepBackDistance(Tier::EXPERT, 24.0f), MIN_STEP_BACK, 0.01f);
    CHECK_NEAR(StepBackDistance(Tier::CASUAL, 1.0f), 14.0f, 0.01f);
}

TEST(one_vs_one_step_back_leaves_a_casting_band)
{
    // expert keeps 25 yd and starts a step only inside 18, so a slowed warrior at 20 yd gets shot at
    Facts f = Ranged();
    f.enemySlowed = true;
    f.enemyApproaching = true;
    f.dist = 20.0f;
    CHECK(Choose(f).choice == Choice::HOLD);
    f.dist = 17.5f;
    CHECK(Choose(f).choice == Choice::STEP_BACK);
    // casual keeps 15 and steps only inside 8
    Facts c = Ranged(Tier::CASUAL);
    c.enemySlowed = true;
    c.dist = 9.0f;
    CHECK(Choose(c).choice == Choice::HOLD);
    c.dist = 7.5f;
    CHECK(Choose(c).choice == Choice::STEP_BACK);
}

TEST(one_vs_one_rooted_melee_in_contact_is_stepped_away_from)
{
    Facts f = Ranged();
    f.dist = 2.0f;
    f.enemyHeld = true;
    CHECK(Choose(f).choice == Choice::STEP_BACK);
}

TEST(one_vs_one_no_back_off_out_of_the_duel_area)
{
    Facts f = Ranged();
    f.dist = 6.0f;
    f.enemySlowed = true;
    f.roomToBackOff = false;
    CHECK(Choose(f).choice == Choice::HOLD);
}

TEST(one_vs_one_root_when_caught_beats_slow)
{
    Facts f = Ranged();
    f.dist = 4.0f;
    f.rootReady = true;
    f.rootRange = 10.0f;
    f.slowReady = true;
    f.slowRange = 10.0f;
    CHECK(Choose(f).choice == Choice::ROOT);
}

TEST(one_vs_one_ranged_vs_ranged_holds)
{
    Facts f = Ranged();
    f.enemy = Role::RANGED;
    f.dist = 5.0f;
    f.escapeReady = true;
    f.slowReady = true;
    f.slowRange = 40.0f;
    CHECK(Choose(f).choice == Choice::HOLD);
    CHECK(Options(f).size() == 1);
}

TEST(one_vs_one_reaction_delay_by_tier)
{
    for (std::uint32_t g = 1; g < 2000; ++g)
    {
        std::uint32_t const e = ReactionMs(Tier::EXPERT, g);
        std::uint32_t const gd = ReactionMs(Tier::GOOD, g);
        std::uint32_t const c = ReactionMs(Tier::CASUAL, g);
        CHECK(e >= 250 && e <= 350);
        CHECK(gd >= 350 && gd <= 500);
        CHECK(c >= 600 && c <= 900);
    }
    CHECK(ReactionMs(Tier::GOOD, 42) == ReactionMs(Tier::GOOD, 42));
}

TEST(one_vs_one_no_reaction_before_the_delay)
{
    Facts f = Ranged(Tier::CASUAL);
    f.dist = 3.0f;
    f.escapeReady = true;
    f.hpPct = 50.0f;
    f.contactForMs = 500;  // casual reacts in 600 to 900 ms
    f.threatForMs = 500;
    CHECK(Choose(f).choice == Choice::HOLD);
    f.contactForMs = 900;
    f.threatForMs = 900;
    CHECK(Choose(f).choice == Choice::ESCAPE);
}

TEST(one_vs_one_casual_keeps_escape_for_low_health)
{
    Facts f = Ranged(Tier::CASUAL);
    f.dist = 3.0f;
    f.escapeReady = true;
    f.hpPct = 80.0f;
    CHECK(Choose(f).choice == Choice::HOLD);
    f.hpPct = 60.0f;
    CHECK(Choose(f).choice == Choice::ESCAPE);
}

TEST(one_vs_one_casual_slows_only_close)
{
    Facts f = Ranged(Tier::CASUAL);
    f.dist = 15.0f;
    f.enemyApproaching = true;
    f.slowReady = true;
    f.slowRange = 40.0f;
    CHECK(Choose(f).choice == Choice::HOLD);
    f.dist = 9.0f;
    CHECK(Choose(f).choice == Choice::SLOW);
}

TEST(one_vs_one_expert_never_slips)
{
    for (std::uint32_t g = 1; g < 500; ++g)
        for (std::uint32_t t = 0; t < 60000; t += 3000)
            CHECK(!Slips(Tier::EXPERT, g, t));
}

TEST(one_vs_one_slip_rates_by_tier)
{
    std::uint32_t good = 0, casual = 0, n = 0;
    for (std::uint32_t g = 1; g < 400; ++g)
        for (std::uint32_t t = 0; t < 300000; t += SLIP_WINDOW_MS)
        {
            ++n;
            good += Slips(Tier::GOOD, g, t) ? 1 : 0;
            casual += Slips(Tier::CASUAL, g, t) ? 1 : 0;
        }
    CHECK(good * 100 / n >= 15 && good * 100 / n <= 25);
    CHECK(casual * 100 / n >= 45 && casual * 100 / n <= 55);
    // the same window replays the same way
    CHECK(Slips(Tier::CASUAL, 77, 4000) == Slips(Tier::CASUAL, 77, 5999));
}

TEST(one_vs_one_good_slip_skips_the_step_back_not_the_slow)
{
    Facts f = Ranged(Tier::GOOD);
    f.guidLow = SlippingGuid(Tier::GOOD, f.nowMs);
    f.dist = 9.0f;
    f.enemySlowed = true;
    CHECK(Choose(f).choice == Choice::HOLD);
    f.enemySlowed = false;
    f.enemyApproaching = true;
    f.slowReady = true;
    f.slowRange = 40.0f;
    CHECK(Choose(f).choice == Choice::SLOW);
}

TEST(one_vs_one_melee_uses_gap_closer_in_band)
{
    Facts f = Melee();
    f.dist = 15.0f;
    f.closerReady = true;
    f.closerMin = 8.0f;
    f.closerMax = 25.0f;
    CHECK(Choose(f).choice == Choice::GAP_CLOSER);
    f.dist = 6.5f;  // inside Charge's minimum: walk in
    CHECK(Choose(f).choice == Choice::CLOSE);
    f.dist = 30.0f;
    CHECK(Choose(f).choice == Choice::CLOSE);
}

TEST(one_vs_one_melee_waits_its_reaction_after_a_separation)
{
    Facts f = Melee(Tier::GOOD);
    f.dist = 15.0f;
    f.closerReady = true;
    f.closerMin = 8.0f;
    f.closerMax = 25.0f;
    f.apartForMs = 200;
    CHECK(Choose(f).choice == Choice::CLOSE);
    f.apartForMs = 600;
    CHECK(Choose(f).choice == Choice::GAP_CLOSER);
}

TEST(one_vs_one_melee_slows_a_ranged_enemy_in_reach)
{
    Facts f = Melee();
    f.dist = 3.0f;
    f.slowReady = true;
    f.slowRange = 5.0f;
    CHECK(Choose(f).choice == Choice::SLOW);
    f.enemySlowed = true;
    CHECK(Choose(f).choice == Choice::HOLD);
}

TEST(one_vs_one_melee_vs_melee_slows_only_a_runner)
{
    Facts f = Melee();
    f.enemy = Role::MELEE;
    f.dist = 3.0f;
    f.slowReady = true;
    f.slowRange = 5.0f;
    CHECK(Choose(f).choice == Choice::HOLD);
    f.enemyMovingAway = true;
    CHECK(Choose(f).choice == Choice::SLOW);
}

TEST(one_vs_one_melee_speed_after_a_long_gap_and_freedom_when_slowed)
{
    Facts f = Melee();
    f.dist = 20.0f;
    f.speedReady = true;
    f.apartForMs = 1000;
    CHECK(Choose(f).choice == Choice::CLOSE);
    f.apartForMs = 4000;
    CHECK(Choose(f).choice == Choice::SPEED);
    f.selfSlowed = true;
    f.freedomReady = true;
    CHECK(Choose(f).choice == Choice::FREEDOM);
    CHECK(Has(Options(f), Choice::SPEED));  // the speed spell stays the next option
}

TEST(one_vs_one_casual_melee_fumbles_in_a_slipping_window)
{
    Facts f = Melee(Tier::CASUAL);
    f.guidLow = SlippingGuid(Tier::CASUAL, f.nowMs);
    f.dist = 15.0f;
    f.closerReady = true;
    f.closerMin = 8.0f;
    f.closerMax = 25.0f;
    CHECK(Choose(f).choice == Choice::CLOSE);
}

TEST(one_vs_one_acts_only_on_its_own_moves)
{
    CHECK(!Acts(Choice::HOLD));
    CHECK(!Acts(Choice::CLOSE));
    CHECK(Acts(Choice::STEP_BACK));
    CHECK(Acts(Choice::GAP_CLOSER));
}

TEST(one_vs_one_kit_rows_are_sane)
{
    std::set<std::uint32_t> seen;
    for (KitSpell const& k : Kit())
    {
        CHECK(k.spellId != 0);
        CHECK((k.cls >= CLS_WARRIOR && k.cls <= CLS_DRUID) || k.cls == CLS_ANY);
        CHECK(k.minRange >= 0.0f && k.maxRange > k.minRange);
        CHECK(seen.insert(k.spellId).second);  // one row per spell
    }
    // every class has at least one row; the bench classes have their key spells
    for (std::uint8_t c = CLS_WARRIOR; c <= CLS_DRUID; ++c)
        CHECK(!(KitFor(c, Kind::SLOW).empty() && KitFor(c, Kind::ROOT).empty() && KitFor(c, Kind::GAP_CLOSER).empty() &&
                KitFor(c, Kind::SPEED).empty() && KitFor(c, Kind::FREEDOM).empty() && KitFor(c, Kind::ESCAPE).empty()));
    CHECK(KitFor(CLS_HUNTER, Kind::SLOW).front().spellId == 5116);
    CHECK(KitFor(CLS_HUNTER, Kind::ESCAPE).front().spellId == 781);
    CHECK(KitFor(CLS_WARRIOR, Kind::GAP_CLOSER).front().spellId == 100);
    CHECK(KitFor(CLS_MAGE, Kind::ROOT).front().spellId == 122);
    CHECK(KitFor(CLS_ROGUE, Kind::GAP_CLOSER).front().spellId == 36554);
}

TEST(one_vs_one_good_hunter_steps_back_from_a_slowed_melee_at_four_yards)
{
    // Live 2026-11-18 (build 12): "Hesteria vs Flarany choice=hold dist=4.0 tier=1 slowed=1" after its Concussive Shot.
    // The policy wants the step back; the hold came from the glue counting Auto Shot as a cast (selfCasting).
    Facts f = Ranged(Tier::GOOD);
    f.dist = 4.0f;
    f.enemySlowed = true;
    CHECK(Choose(f).choice == Choice::STEP_BACK);
    CHECK_NEAR(StepBackDistance(Tier::GOOD, 4.0f), 16.0f, 0.01f);
    // Round 2: a good or expert bot walks away from a caught melee enemy in the middle of a real cast (the move cancels it);
    // a cast with the enemy still coming (not caught yet) is finished first; casual bots always finish it.
    f.selfCasting = true;
    CHECK(Choose(f).choice == Choice::STEP_BACK);
    f.dist = 12.0f;
    CHECK(Choose(f).choice == Choice::HOLD);
    Facts c = Ranged(Tier::CASUAL);
    c.dist = 3.0f;
    c.enemySlowed = true;
    c.selfCasting = true;
    CHECK(Choose(c).choice != Choice::STEP_BACK);
}

TEST(one_vs_one_melee_hunter_side_plays_melee)
{
    // A hunter without a ranged weapon is given the melee role by the glue: it closes and never kites.
    Facts f = Melee();
    f.enemy = Role::MELEE;
    f.dist = 3.0f;
    f.slowReady = true;
    f.slowRange = 40.0f;
    CHECK(Choose(f).choice == Choice::HOLD);
}

TEST(one_vs_one_role_by_class)
{
    // The bench pair that read "enemy=ranged" for a holy paladin: paladins and monks fight in melee in every spec.
    CHECK(RoleOfClass(CLS_PALADIN, true, false, false) == Role::MELEE);
    CHECK(RoleOfClass(CLS_MONK, true, false, false) == Role::MELEE);
    CHECK(RoleOfClass(CLS_WARRIOR, false, false, false) == Role::MELEE);
    CHECK(RoleOfClass(CLS_ROGUE, false, false, false) == Role::MELEE);
    CHECK(RoleOfClass(CLS_DEATH_KNIGHT, false, false, false) == Role::MELEE);
    CHECK(RoleOfClass(CLS_HUNTER, false, false, true) == Role::RANGED);
    CHECK(RoleOfClass(CLS_HUNTER, true, false, false) == Role::MELEE);
    CHECK(RoleOfClass(CLS_MAGE, true, false, false) == Role::RANGED);
    CHECK(RoleOfClass(CLS_WARLOCK, true, false, false) == Role::RANGED);
    CHECK(RoleOfClass(CLS_DRUID, false, false, false) == Role::MELEE);   // feral
    CHECK(RoleOfClass(CLS_DRUID, true, false, false) == Role::RANGED);   // balance, restoration
    CHECK(RoleOfClass(CLS_DRUID, true, true, false) == Role::MELEE);     // a tank
    CHECK(RoleOfClass(CLS_SHAMAN, false, false, false) == Role::MELEE);  // enhancement
}

TEST(one_vs_one_defensive_follows_the_tier_health_line)
{
    // expert 55, good 40, casual 25 percent
    Facts e = Ranged(Tier::EXPERT);
    e.dist = 3.0f;
    e.defensiveReady = true;
    e.lowHpForMs = 2000;
    e.hpPct = 60.0f;
    CHECK(!Has(Options(e), Choice::DEFENSIVE));
    e.hpPct = 54.0f;
    CHECK(Choose(e).choice == Choice::DEFENSIVE);

    Facts g = Ranged(Tier::GOOD);
    g.dist = 3.0f;
    g.defensiveReady = true;
    g.lowHpForMs = 2000;
    g.hpPct = 45.0f;
    CHECK(!Has(Options(g), Choice::DEFENSIVE));
    g.hpPct = 39.0f;
    CHECK(Choose(g).choice == Choice::DEFENSIVE);

    Facts c = Ranged(Tier::CASUAL);
    c.dist = 3.0f;
    c.defensiveReady = true;
    c.lowHpForMs = 2000;
    c.hpPct = 30.0f;
    CHECK(!Has(Options(c), Choice::DEFENSIVE));
    c.hpPct = 24.0f;
    c.guidLow = CleanGuid(Tier::CASUAL, c.nowMs);
    CHECK(Choose(c).choice == Choice::DEFENSIVE);
}

TEST(one_vs_one_defensive_waits_for_the_reaction_and_needs_a_reason)
{
    Facts f = Ranged(Tier::EXPERT);
    f.dist = 3.0f;
    f.defensiveReady = true;
    f.hpPct = 40.0f;
    f.lowHpForMs = ReactionMs(Tier::EXPERT, f.guidLow) - 1;
    CHECK(!Has(Options(f), Choice::DEFENSIVE));
    f.lowHpForMs = ReactionMs(Tier::EXPERT, f.guidLow);
    CHECK(Has(Options(f), Choice::DEFENSIVE));
    f.enemyHeld = true;  // nothing hits it: no defensive
    CHECK(!Has(Options(f), Choice::DEFENSIVE));
    f.enemyHeld = false;
    f.dist = 30.0f;      // a melee enemy that far away is no threat yet
    CHECK(!Has(Options(f), Choice::DEFENSIVE));
    f.enemy = Role::RANGED;  // a caster does reach it
    CHECK(Has(Options(f), Choice::DEFENSIVE));
    f.defensiveReady = false;
    CHECK(!Has(Options(f), Choice::DEFENSIVE));
}

TEST(one_vs_one_good_and_casual_defensives_slip_in_a_bad_window)
{
    for (Tier t : { Tier::GOOD, Tier::CASUAL })
    {
        Facts f = Ranged(t);
        f.dist = 3.0f;
        f.defensiveReady = true;
        f.hpPct = 10.0f;
        f.lowHpForMs = 5000;
        f.guidLow = SlippingGuid(t, f.nowMs);
        CHECK(!Has(Options(f), Choice::DEFENSIVE));
        f.guidLow = CleanGuid(t, f.nowMs);
        CHECK(Has(Options(f), Choice::DEFENSIVE));
    }
}

TEST(one_vs_one_cc_break_comes_first_after_the_reaction)
{
    Facts f = Melee(Tier::GOOD);
    f.enemy = Role::MELEE;
    f.dist = 3.0f;
    f.selfControlLost = true;
    f.selfHeld = true;
    f.ccBreakReady = true;
    f.escapeReady = true;
    f.controlLostForMs = ReactionMs(Tier::GOOD, f.guidLow) - 1;
    CHECK(!Has(Options(f), Choice::CC_BREAK));
    f.controlLostForMs += 1;
    CHECK(Choose(f).choice == Choice::CC_BREAK);
    f.ccBreakReady = false;
    CHECK(!Has(Options(f), Choice::CC_BREAK));
}

TEST(one_vs_one_cc_break_applies_to_a_ranged_bot_too)
{
    Facts f = Ranged(Tier::EXPERT);
    f.dist = 3.0f;
    f.selfControlLost = true;
    f.selfHeld = true;
    f.ccBreakReady = true;
    f.escapeReady = true;
    f.controlLostForMs = 1000;
    CHECK(Choose(f).choice == Choice::CC_BREAK);
}

TEST(one_vs_one_hurt_ranged_bot_takes_cover_from_a_caster)
{
    Facts f = Ranged(Tier::EXPERT);
    f.enemy = Role::RANGED;
    f.dist = 25.0f;
    f.hpPct = 60.0f;
    f.hurtForMs = 2000;
    f.coverReady = true;
    CHECK(Choose(f).choice == Choice::COVER);
    f.hpPct = 70.0f;  // above the expert line (65)
    CHECK(Choose(f).choice == Choice::HOLD);
    f.hpPct = 60.0f;
    f.selfCasting = true;
    CHECK(Choose(f).choice == Choice::HOLD);  // finishes the cast first
    f.selfCasting = false;
    f.coverReady = false;
    CHECK(Choose(f).choice == Choice::HOLD);
}

TEST(one_vs_one_cover_is_held_for_a_few_seconds_then_the_fight_resumes)
{
    Facts f = Ranged(Tier::GOOD);
    f.enemy = Role::RANGED;
    f.dist = 25.0f;
    f.hpPct = 40.0f;
    f.hurtForMs = 3000;
    f.inCover = true;
    f.coverForMs = 2000;
    CHECK(Choose(f).choice == Choice::COVER_HOLD);
    f.coverForMs = COVER_MAX_MS;
    CHECK(Choose(f).choice == Choice::HOLD);
    f.coverForMs = 2000;
    f.inCover = false;  // the enemy walked into view: back to the rotation
    CHECK(Choose(f).choice == Choice::HOLD);
    f.coverForMs = 0;
    f.coverReady = false;
    CHECK(Choose(f).choice == Choice::HOLD);
}

TEST(one_vs_one_casual_never_takes_cover_and_never_against_melee)
{
    Facts c = Ranged(Tier::CASUAL);
    c.enemy = Role::RANGED;
    c.dist = 25.0f;
    c.hpPct = 10.0f;
    c.hurtForMs = 9000;
    c.coverReady = true;
    CHECK(!Has(Options(c), Choice::COVER));
    Facts m = Ranged(Tier::EXPERT);  // a melee enemy is kited, not hidden from
    m.dist = 10.0f;
    m.hpPct = 10.0f;
    m.hurtForMs = 9000;
    m.coverReady = true;
    CHECK(!Has(Options(m), Choice::COVER));
}

TEST(one_vs_one_new_choices_act_and_are_named)
{
    CHECK(Acts(Choice::DEFENSIVE) && Acts(Choice::CC_BREAK) && Acts(Choice::COVER) && Acts(Choice::COVER_HOLD));
    CHECK(CastsKitSpell(Choice::DEFENSIVE) && CastsKitSpell(Choice::CC_BREAK));
    CHECK(!CastsKitSpell(Choice::COVER) && !CastsKitSpell(Choice::COVER_HOLD) && !CastsKitSpell(Choice::STEP_BACK));
    CHECK(std::string(ChoiceName(Choice::COVER_HOLD)) == "cover-hold");
}

TEST(one_vs_one_kit_has_a_defensive_for_every_class_and_racials_for_all)
{
    for (std::uint8_t c = CLS_WARRIOR; c <= CLS_DRUID; ++c)
    {
        CHECK(!KitFor(c, Kind::DEFENSIVE).empty());
        CHECK(KitFor(c, Kind::CC_BREAK).size() >= 3);  // the three racial rows reach every class
    }
    CHECK(KitFor(CLS_MAGE, Kind::DEFENSIVE).back().spellId == 45438);  // Ice Block last: the big one
    CHECK(KitFor(CLS_WARRIOR, Kind::CC_BREAK).front().spellId == 18499);
}

TEST(one_vs_one_cover_hold_ends_when_the_enemy_is_on_the_bot)
{
    Facts f = Ranged(Tier::EXPERT);
    f.enemy = Role::RANGED;
    f.inCover = true;
    f.coverForMs = 2000;
    f.dist = 20.0f;
    CHECK(Choose(f).choice == Choice::COVER_HOLD);
    f.dist = 6.0f;
    CHECK(Choose(f).choice == Choice::HOLD);
}

TEST(one_vs_one_defensives_that_fit_one_kind_of_enemy)
{
    CHECK(DefensiveFits(5277, Role::MELEE) && !DefensiveFits(5277, Role::RANGED));    // Evasion
    CHECK(DefensiveFits(31224, Role::RANGED) && !DefensiveFits(31224, Role::MELEE));  // Cloak of Shadows
    CHECK(DefensiveFits(48707, Role::RANGED) && !DefensiveFits(48707, Role::MELEE));  // Anti-Magic Shell
    CHECK(DefensiveFits(45438, Role::MELEE) && DefensiveFits(45438, Role::RANGED));   // Ice Block
}

TEST(one_vs_one_no_speed_spell_while_one_is_running)
{
    // Live 2026-11-18, mage v rogue at 60: Burst of Speed (no cooldown) was cast 92 times in 131 s; the engine's own action
    // ended every tick, so the rogue never took a step toward the mage.
    Facts f = Melee(Tier::GOOD);
    f.dist = 15.0f;
    f.apartForMs = 8000;
    f.speedReady = true;
    CHECK(Has(Options(f), Choice::SPEED));
    f.selfFast = true;
    CHECK(!Has(Options(f), Choice::SPEED));
    CHECK(Choose(f).choice == Choice::CLOSE);
}

TEST(one_vs_one_no_second_slow_on_a_snared_enemy)
{
    // Bench 2026-11-18, warlock v paladin at 60: 8 Curse of Exhaustion in one duel, because the paladin's Pursuit of Justice
    // kept it above 85 percent of the warlock's speed even with the curse on it; every recast cost a spell the rotation
    // would have cast.
    Facts f = Ranged(Tier::GOOD);
    f.dist = 6.0f;
    f.enemyApproaching = true;
    f.slowReady = true;
    f.slowRange = 40.0f;
    CHECK(Choose(f).choice == Choice::SLOW);
    f.enemySnared = true;
    CHECK(Choose(f).choice == Choice::HOLD);
    Facts m = Melee(Tier::GOOD);
    m.dist = 4.0f;
    m.slowReady = true;
    m.slowRange = 5.0f;
    CHECK(Has(Options(m), Choice::SLOW));
    m.enemySnared = true;
    CHECK(!Has(Options(m), Choice::SLOW));
}
