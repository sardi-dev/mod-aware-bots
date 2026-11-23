#include "TestHarness.h"
#include "WsgPlanLogic.h"

// BT-472: the debuff-aware stalemate rule (docs/bg-sheets/twin-peaks.md gap G4, decision point D8). The module reads the
// live stacks of Focused Assault (46392) and Brutal Assault (46393) on the two carriers, so the rule is right on the core of
// today (one Focused Assault at 10 minutes, one Brutal Assault at 15) and on the core of the fork (a stack a minute).

using namespace WsgLogic;

namespace
{
DecisionContext Carrier(Tier tier)
{
    DecisionContext c;
    c.tier = tier;
    c.role = Role::TAKER;
    c.hasFlag = true;
    c.ourFlag = FLAG_ON_PLAYER;      // they carry ours
    c.theirFlag = FLAG_ON_PLAYER;    // we carry theirs
    return c;
}

DecisionContext Striker(Tier tier, Role role = Role::ESCORT)
{
    DecisionContext c;
    c.tier = tier;
    c.role = role;
    c.roleIndex = 1;
    c.ourFlag = FLAG_ON_PLAYER;
    c.theirFlag = FLAG_ON_PLAYER;
    c.efcKnown = true;
    return c;
}

bool Has(std::vector<Option> const& options, Kind kind)
{
    for (Option const& o : options)
        if (o.kind == kind)
            return true;
    return false;
}
}  // namespace

TEST(stalemate_debuff_level_counts_brutal_as_at_least_the_fifth_stack)
{
    CHECK(DebuffLevel(0, 0) == 0);
    CHECK(DebuffLevel(1, 0) == 1);
    CHECK(DebuffLevel(4, 0) == 4);
    // The core of today casts Brutal Assault once at 15 minutes; the reference core stacks it from the fifth.
    CHECK(DebuffLevel(0, 1) == BRUTAL_MIN_LEVEL);
    CHECK(DebuffLevel(0, 5) == 5);
    CHECK(DebuffLevel(0, 10) == 10);
    // Both auras at once (a transition tick): the heavier reading counts.
    CHECK(DebuffLevel(3, 6) == 6);
}

TEST(stalemate_hunt_starts_at_the_clock_or_at_the_first_stack_on_either_carrier)
{
    CHECK(!StalemateHunts(0, 0, 0));
    CHECK(!StalemateHunts(STALEMATE_HUNT_MS - 1, 0, 0));
    CHECK(StalemateHunts(STALEMATE_HUNT_MS, 0, 0));
    // A core with stacking debuffs from the first minute: a stack on either carrier ends the waiting.
    CHECK(StalemateHunts(60000, 1, 0));
    CHECK(StalemateHunts(60000, 0, 1));
    CHECK(StalemateHunts(60000, 0, 4));
}

TEST(stalemate_run_starts_at_the_clock_or_at_two_stacks_when_nobody_is_on_the_enemy_carrier)
{
    CHECK(!StalemateRuns(0, 0, false));
    CHECK(!StalemateRuns(STALEMATE_RUN_MS - 1, 0, false));
    CHECK(StalemateRuns(STALEMATE_RUN_MS, 0, false));
    CHECK(StalemateRuns(STALEMATE_RUN_MS, 0, true));       // the clock does not care about the pressure
    CHECK(!StalemateRuns(120000, 1, false));               // one stack is not enough
    CHECK(StalemateRuns(120000, 2, false));
    CHECK(!StalemateRuns(120000, 2, true));                // a working hunt keeps the carrier where it is
    CHECK(StalemateRuns(120000, 5, false));
}

TEST(stalemate_carrier_hides_until_two_stacks_and_then_runs_home)
{
    DecisionContext c = Carrier(Tier::GOOD);
    c.bothHeldMs = 60000;
    c.ownDebuff = 1;
    CHECK(Options(c).front().kind == Kind::CARRY_HIDE);

    c.ownDebuff = 2;
    CHECK(Options(c).front().kind == Kind::CARRY_HOME);

    // The hunt is on the enemy carrier: the carrier stays hidden.
    c.efcPressed = true;
    CHECK(Options(c).front().kind == Kind::CARRY_HIDE);
}

TEST(stalemate_casual_carrier_plays_the_core_rule_whatever_the_stacks)
{
    DecisionContext c = Carrier(Tier::CASUAL);
    c.bothHeldMs = 60000;
    c.ownDebuff = 8;
    // Hiding while the own flag is out is a good rule, so a casual carrier runs home with or without stacks.
    CHECK(Options(c).front().kind == Kind::CARRY_HOME);
}

TEST(stalemate_good_escort_hunts_at_the_first_stack_and_a_casual_one_does_not)
{
    DecisionContext good = Striker(Tier::GOOD);
    good.bothHeldMs = 70000;
    CHECK(Options(good).front().kind == Kind::ESCORT_CARRIER);
    good.enemyDebuff = 1;
    CHECK(Options(good).front().kind == Kind::HUNT_EFC);

    DecisionContext casual = Striker(Tier::CASUAL);
    casual.bothHeldMs = 70000;
    casual.enemyDebuff = 3;
    CHECK(Options(casual).front().kind == Kind::ESCORT_CARRIER);
}

TEST(stalemate_the_first_escort_stays_with_the_carrier_while_the_rest_hunt)
{
    DecisionContext second = Striker(Tier::EXPERT);
    second.bothHeldMs = 30000;
    second.ownDebuff = 1;
    CHECK(Options(second).front().kind == Kind::HUNT_EFC);

    DecisionContext first = second;
    first.roleIndex = 0;
    CHECK(Options(first).front().kind == Kind::ESCORT_CARRIER);
    CHECK(Has(Options(first), Kind::ESCORT_CARRIER));
}

TEST(stalemate_the_clock_numbers_stay_the_fallback_without_any_stack)
{
    DecisionContext c = Striker(Tier::GOOD);
    c.bothHeldMs = STALEMATE_HUNT_MS;
    CHECK(Options(c).front().kind == Kind::HUNT_EFC);

    DecisionContext carrier = Carrier(Tier::GOOD);
    carrier.bothHeldMs = STALEMATE_RUN_MS;
    CHECK(Options(carrier).front().kind == Kind::CARRY_HOME);
}
