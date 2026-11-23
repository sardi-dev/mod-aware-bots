#include "TestHarness.h"
#include "EyFlagLogic.h"

// BT-583: the flag in the middle of Eye of the Storm (docs/bg-sheets/eye-of-the-storm.md sections 6 and 16): D15 the pickup, D16
// the carrier's base, D17 the answer to an enemy carrier, and the duties of the other bots.

using namespace EyFlag;

namespace
{
BaseFact Owned(float distance, unsigned guards = 1, unsigned enemies = 0, int margin = 31, bool nearOwn = false)
{
    BaseFact f;
    f.owned = true;
    f.distance = distance;
    f.guards = guards;
    f.enemies = enemies;
    f.margin = margin;
    f.nearOwnStart = nearOwn;
    return f;
}
}  // namespace

TEST(ey_flag_the_stand_is_taken_when_it_is_clear_and_a_base_is_held)
{
    ActionInput in;
    in.ourBases = 2;
    in.theirBases = 1;
    CHECK(RuleAction(in) == Action::TAKE_NOW);
    std::vector<ActionOption> const list = Actions(in);
    CHECK(list.front().score == SCORE_TAKE_NOW && list.back().action == Action::IGNORE);
}

TEST(ey_flag_waits_for_a_clear_stand_when_the_odds_are_even_or_better)
{
    ActionInput in;
    in.ourBases = 2;
    in.enemiesNearStand = 2;
    in.oddsAtStand = 1;
    CHECK(RuleAction(in) == Action::WAIT_CLEAR);
    // Behind in the fight: waiting is still above taking, a cast under attack is interrupted.
    in.oddsAtStand = -3;
    CHECK(RuleAction(in) != Action::TAKE_NOW || Actions(in).front().score < SCORE_TAKE_NOW);
}

TEST(ey_flag_with_no_base_is_taken_to_deny_by_good_and_expert_bots_only)
{
    ActionInput in;
    in.ourBases = 0;
    in.theirBases = 2;
    in.tier = Tier::GOOD;
    CHECK(RuleAction(in) == Action::DENY_ONLY);
    in.tier = Tier::EXPERT;
    CHECK(RuleAction(in) == Action::DENY_ONLY);
    in.tier = Tier::CASUAL;
    CHECK(RuleAction(in) == Action::IGNORE);
    in.tier = Tier::GOOD;
    in.theirBases = 0;
    CHECK(RuleAction(in) == Action::IGNORE);        // nobody holds a base: nothing to deny
}

TEST(ey_flag_carrier_picks_the_nearest_guarded_base)
{
    BaseFact facts[BASE_COUNT] = { Owned(0), BaseFact(), Owned(0), Owned(0) };
    facts[0] = Owned(207.0f);
    facts[2] = Owned(201.0f);
    facts[3] = Owned(196.0f, 1, 0, 31, true);
    std::vector<DestOption> const list = Destinations(facts, false, 0);
    CHECK(list.size() == 3 && list.front().base == 3);
    CHECK(RuleDestination(list) == 3);
}

TEST(ey_flag_carrier_never_runs_to_a_base_that_is_falling_while_another_is_not)
{
    BaseFact facts[BASE_COUNT] = { BaseFact(), BaseFact(), Owned(201.0f), Owned(196.0f, 1, 3) };   // the nearer base is swamped
    std::vector<DestOption> const list = Destinations(facts, false, 0);
    CHECK(list.size() == 1 && list.front().base == 2);
    // A base nobody guards is not offered either.
    BaseFact bare[BASE_COUNT] = { BaseFact(), BaseFact(), Owned(201.0f, 0), Owned(220.0f, 2) };
    CHECK(RuleDestination(Destinations(bare, false, 0)) == 3);
}

TEST(ey_flag_carrier_with_only_bad_bases_still_gets_the_best_of_them)
{
    BaseFact facts[BASE_COUNT] = { BaseFact(), BaseFact(), Owned(201.0f, 0), Owned(196.0f, 0, 2) };
    std::vector<DestOption> const list = Destinations(facts, false, 0);
    CHECK(list.size() == 2);                         // nothing eligible: all owned bases, scored
    CHECK(RuleDestination(list) == 2);               // the swamped one scores lowest
    BaseFact none[BASE_COUNT] = {};
    CHECK(Destinations(none, false, 0).empty() && RuleDestination(Destinations(none, false, 0)) == BASE_NONE);
}

TEST(ey_flag_carrier_waits_for_escorts_only_when_blocked_and_never_as_the_rule)
{
    BaseFact facts[BASE_COUNT] = { BaseFact(), BaseFact(), Owned(201.0f), BaseFact() };
    std::vector<DestOption> free = Destinations(facts, true, 1);
    CHECK(free.size() == 1);
    std::vector<DestOption> blocked = Destinations(facts, true, HOLD_BACK_BLOCKED_ENEMIES);
    CHECK(blocked.size() == 2 && blocked.back().base == HOLD_BACK);
    CHECK(RuleDestination(blocked) == 2);
    CHECK(Destinations(facts, false, 9).size() == 1);   // no escort near: no waiting
}

TEST(ey_flag_destination_score_is_the_sheets_formula)
{
    BaseFact f = Owned(300.0f, 2, 1, 31, true);
    CHECK(DestScore(f) == 200 - 100 + 80 - 50 + 60);
    f.enemies = 3;
    CHECK(Falling(f) && DestScore(f) == 200 - 100 + 80 - 150 - 120 + 60);
    CHECK(Falling(Owned(100.0f, 2, 1, 1)));       // one enemy at a base captured at the threshold: it falls in 2 s
    CHECK(!Falling(Owned(100.0f, 2, 1, 31)));
}

TEST(ey_flag_enemy_carrier_with_four_bases_is_hunted_by_all)
{
    EfcInput in;
    in.theirBases = 4;
    in.distanceToEfc = 250.0f;
    CHECK(EfcActions(in).front().action == EfcAction::HUNT_ALL);
    in.theirBases = 2;
    CHECK(EfcActions(in).front().action == EfcAction::HUNT_TWO_CUT);
    in.distanceToEfc = 60.0f;
    CHECK(EfcActions(in).front().action == EfcAction::HUNT_ALL);       // within 100 yd everybody in reach goes
    in.distanceToEfc = 250.0f;
    in.theirBases = 4;
    CHECK(EfcActions(in).front().action == EfcAction::HUNT_ALL);
}

TEST(ey_flag_an_expert_neutralises_a_fresh_or_thinly_guarded_target_base)
{
    EfcInput in;
    in.theirBases = 3;
    in.distanceToEfc = 200.0f;
    in.expertAvailable = true;
    in.haveTarget = true;
    in.targetFresh = true;
    CHECK(EfcActions(in).front().action == EfcAction::NEUTRALIZE_TARGET_BASE);
    in.targetFresh = false;
    in.targetGuards = 3;
    CHECK(EfcActions(in).front().action != EfcAction::NEUTRALIZE_TARGET_BASE);
    in.targetGuards = 1;
    CHECK(EfcActions(in).front().action == EfcAction::NEUTRALIZE_TARGET_BASE);
    in.expertAvailable = false;
    CHECK(EfcActions(in).front().action == EfcAction::HUNT_TWO_CUT);
}

TEST(ey_flag_enemy_carrier_with_no_base_cannot_score_and_is_ignored)
{
    EfcInput in;
    in.theirBases = 0;
    CHECK(EfcActions(in).front().action == EfcAction::IGNORE);
}

TEST(ey_flag_duty_on_the_stand_is_one_taker_and_three_cover)
{
    DutyInput in;
    in.flagState = FLAG_ON_BASE;
    in.action = Action::TAKE_NOW;
    in.rankStand = 0;
    CHECK(DutyFor(in) == Duty::TAKE_FLAG);
    in.rankStand = 3;
    CHECK(DutyFor(in) == Duty::STAGE);
    in.rankStand = 4;
    CHECK(DutyFor(in) == Duty::NONE);
    in.rankStand = 0;
    in.action = Action::WAIT_CLEAR;
    CHECK(DutyFor(in) == Duty::STAGE);           // nobody casts into an enemy group
    in.action = Action::DENY_ONLY;
    CHECK(DutyFor(in) == Duty::TAKE_FLAG);
    in.action = Action::IGNORE;
    CHECK(DutyFor(in) == Duty::NONE);
    // After a capture the flag waits 8 s: the group stands ready, nobody casts yet.
    in.action = Action::TAKE_NOW;
    in.flagState = FLAG_WAIT_RESPAWN;
    CHECK(DutyFor(in) == Duty::STAGE);
}

TEST(ey_flag_duty_yields_to_the_fourth_base_and_to_a_guard_post)
{
    DutyInput in;
    in.rankStand = 0;
    in.yields = true;
    CHECK(DutyFor(in) == Duty::NONE);
    in.yields = false;
    in.free = false;     // a guard, a raider or a retreat
    CHECK(DutyFor(in) == Duty::NONE);
}

TEST(ey_flag_our_carrier_gets_two_escorts_and_two_healers)
{
    DutyInput in;
    in.flagState = FLAG_ON_PLAYER;
    in.weCarry = true;
    in.rankCarrier = 1;
    CHECK(DutyFor(in) == Duty::ESCORT);
    in.rankCarrier = 2;
    CHECK(DutyFor(in) == Duty::NONE);
    in.isHealer = true;
    in.rankHealer = 1;
    CHECK(DutyFor(in) == Duty::HEAL_CARRIER);
    in.rankHealer = 2;
    CHECK(DutyFor(in) == Duty::NONE);
}

TEST(ey_flag_enemy_carrier_two_hunt_two_cut_or_all_hunt)
{
    DutyInput in;
    in.flagState = FLAG_ON_PLAYER;
    in.theyCarry = true;
    in.efc = EfcAction::HUNT_TWO_CUT;
    in.rankEfc = 0;
    CHECK(DutyFor(in) == Duty::HUNT);
    in.rankEfc = 2;
    CHECK(DutyFor(in) == Duty::CUT);
    in.rankEfc = 4;
    CHECK(DutyFor(in) == Duty::NONE);
    in.efc = EfcAction::HUNT_ALL;
    in.rankEfc = 4;
    CHECK(DutyFor(in) == Duty::HUNT);
    in.rankEfc = 5;
    CHECK(DutyFor(in) == Duty::NONE);
    in.efc = EfcAction::NEUTRALIZE_TARGET_BASE;
    in.neutralizer = true;
    CHECK(DutyFor(in) == Duty::NEUTRALIZE);
    in.neutralizer = false;
    in.rankEfc = 0;
    CHECK(DutyFor(in) == Duty::HUNT);
    in.efc = EfcAction::IGNORE;
    CHECK(DutyFor(in) == Duty::NONE);
    in.efc = EfcAction::HUNT_ALL;
    in.yields = true;
    CHECK(DutyFor(in) == Duty::NONE);
}

TEST(ey_flag_dropped_flag_is_clicked_by_the_two_nearest_whatever_the_plan_does)
{
    DutyInput in;
    in.flagState = FLAG_ON_GROUND;
    in.yields = true;
    in.rankDropped = 0;
    CHECK(DutyFor(in) == Duty::CLICK_DROPPED);
    in.rankDropped = 1;
    CHECK(DutyFor(in) == Duty::CLICK_DROPPED);
    in.rankDropped = 2;
    CHECK(DutyFor(in) == Duty::NONE);
    in.free = false;
    in.rankDropped = 0;
    CHECK(DutyFor(in) == Duty::NONE);
}

TEST(ey_flag_dynamic_duties_follow_a_moving_target)
{
    CHECK(IsDynamic(Duty::ESCORT) && IsDynamic(Duty::HEAL_CARRIER) && IsDynamic(Duty::HUNT) && IsDynamic(Duty::CLICK_DROPPED));
    CHECK(!IsDynamic(Duty::TAKE_FLAG) && !IsDynamic(Duty::STAGE) && !IsDynamic(Duty::CUT) && !IsDynamic(Duty::NEUTRALIZE));
    CHECK(std::string(DutyName(Duty::CUT)) == "cut" && std::string(ActionName(Action::DENY_ONLY)) == "deny_only");
    CHECK(std::string(EfcActionName(EfcAction::NEUTRALIZE_TARGET_BASE)) == "neutralize_target_base");
}
