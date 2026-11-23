#include "TestHarness.h"
#include "BgLeaderAdapters.h"

// BT-481: the battleground leader: election and handover, the stance and its call, the decision hook for a later decision
// layer, the adapters onto the two-flag and node plans, and the drift of casual bots.

using namespace BgLeader;
using BotSkillMath::Tier;

namespace
{
Candidate Bot(std::uint32_t guid, Tier tier, std::uint8_t level = 85, bool alive = true)
{
    Candidate c;
    c.guidLow = guid;
    c.tier = tier;
    c.level = level;
    c.bot = true;
    c.alive = alive;
    c.present = true;
    return c;
}

Candidate Human(std::uint32_t guid)
{
    Candidate c = Bot(guid, Tier::EXPERT, 90);
    c.bot = false;
    return c;
}

Offer D1(FlagPlanState const& state)
{
    return StanceOffer(state);
}

FlagPlanState Normal(BgPlan::Plan plan = BgPlan::Plan::BALANCED)
{
    return { plan, WsgLogic::Situation::NORMAL };
}
}  // namespace

// --- election ---------------------------------------------------------------------------------------------------------

TEST(leader_best_tier_wins_then_level_then_the_guid_hash)
{
    CHECK(Best({ Bot(1, Tier::CASUAL, 90), Bot(2, Tier::GOOD, 85), Bot(3, Tier::EXPERT, 80) }) == 3);
    CHECK(Best({ Bot(1, Tier::EXPERT, 80), Bot(2, Tier::EXPERT, 90), Bot(3, Tier::EXPERT, 85) }) == 2);

    // Same tier and level: the lower GUID hash, whatever the order of the list.
    std::vector<Candidate> same = { Bot(11, Tier::GOOD), Bot(12, Tier::GOOD), Bot(13, Tier::GOOD) };
    std::uint32_t const best = Best(same);
    std::vector<Candidate> reversed(same.rbegin(), same.rend());
    CHECK(Best(reversed) == best);
    for (Candidate const& c : same)
        CHECK(BotSkillMath::Mix(best) <= BotSkillMath::Mix(c.guidLow));
}

TEST(leader_a_human_is_never_the_leader)
{
    CHECK(Best({ Human(1), Bot(2, Tier::CASUAL, 20) }) == 2);
    CHECK(Best({ Human(1), Human(2) }) == 0);
    CHECK(Best({}) == 0);
}

TEST(leader_a_dead_or_absent_bot_is_not_eligible)
{
    Candidate gone = Bot(1, Tier::EXPERT);
    gone.present = false;
    CHECK(Best({ Bot(2, Tier::EXPERT, 85, false), gone, Bot(3, Tier::CASUAL) }) == 3);
}

TEST(leader_election_changes_are_named_and_counted)
{
    Election e;
    CHECK(Elect(e, { Bot(1, Tier::GOOD), Bot(2, Tier::EXPERT) }, 1000, true) == Change::ELECTED);
    CHECK(e.leader == 2);
    CHECK(e.epoch == 1);
    CHECK(Elect(e, { Bot(1, Tier::GOOD), Bot(2, Tier::EXPERT) }, 3000, true) == Change::NONE);
    CHECK(e.epoch == 1);

    // The leader dies: the next best takes over at the next refresh.
    CHECK(Elect(e, { Bot(1, Tier::GOOD), Bot(2, Tier::EXPERT, 85, false), Bot(3, Tier::CASUAL) }, 5000, true) == Change::HANDOVER);
    CHECK(e.leader == 1);
    CHECK(e.epoch == 2);
    CHECK(e.sinceMs == 5000);

    // Nobody can lead: no leader, counted once.
    CHECK(Elect(e, { Human(9), Bot(1, Tier::GOOD, 85, false) }, 7000, true) == Change::LOST);
    CHECK(e.leader == 0);
    CHECK(e.epoch == 3);
    CHECK(Elect(e, { Human(9) }, 9000, true) == Change::NONE);
    CHECK(e.epoch == 3);
    CHECK(Elect(e, { Human(9), Bot(4, Tier::CASUAL) }, 11000, true) == Change::ELECTED);
    CHECK(e.leader == 4);
}

TEST(leader_the_incumbent_stays_after_the_gates_but_not_before)
{
    // A good bot leads; an expert arrives. Before the gates the best bot present wins, after them the incumbent stays.
    Election settling;
    Elect(settling, { Bot(1, Tier::GOOD) }, 1000, false);
    CHECK(settling.leader == 1);
    CHECK(Elect(settling, { Bot(1, Tier::GOOD), Bot(2, Tier::EXPERT) }, 3000, false) == Change::HANDOVER);
    CHECK(settling.leader == 2);

    Election started;
    Elect(started, { Bot(1, Tier::GOOD) }, 1000, true);
    CHECK(Elect(started, { Bot(1, Tier::GOOD), Bot(2, Tier::EXPERT) }, 3000, true) == Change::NONE);
    CHECK(started.leader == 1);
}

TEST(leader_a_returning_leader_does_not_take_the_lead_back)
{
    Election e;
    Elect(e, { Bot(1, Tier::EXPERT), Bot(2, Tier::GOOD) }, 1000, true);
    Elect(e, { Bot(1, Tier::EXPERT, 85, false), Bot(2, Tier::GOOD) }, 3000, true);
    CHECK(e.leader == 2);
    CHECK(Elect(e, { Bot(1, Tier::EXPERT), Bot(2, Tier::GOOD) }, 40000, true) == Change::NONE);
    CHECK(e.leader == 2);
}

TEST(leader_a_one_bot_team_and_an_uneven_team_still_elect)
{
    Election e;
    CHECK(Elect(e, { Bot(5, Tier::CASUAL), Human(6), Human(7) }, 1000, true) == Change::ELECTED);
    CHECK(e.leader == 5);
}

TEST(leader_the_limiter_does_not_reset_at_a_handover)
{
    // The team limiter belongs to the team record, not to the leader: a new leader's line follows the same gap.
    BgPlan::CallLimiter limiter;
    limiter.Commit(1, 0, 10000);
    CHECK(!limiter.Allow(2, 0, 11000));
    CHECK(limiter.Allow(2, 0, 10000 + BgPlan::CALL_TEAM_GAP_MS));
}

// --- stance and its call ----------------------------------------------------------------------------------------------

TEST(leader_stance_follows_the_rule_and_is_called_at_the_gates)
{
    LeaderState s;
    std::vector<Candidate> const team = { Bot(1, Tier::EXPERT), Bot(2, Tier::GOOD) };
    FlagPlanState const rule = Normal();

    TickResult t = Tick(s, team, StanceOf(rule), D1(rule), 1000, false);
    CHECK(t.stance == Stance::BALANCED);
    CHECK(!t.stanceChanged);          // the first reading is not a change
    CHECK(!t.callDue);                // nothing is called before the gates
    CHECK(t.leader == 1);

    t = Tick(s, team, StanceOf(rule), D1(rule), 3000, true);
    CHECK(t.callDue);
    CHECK(s.callOwed);
    CallCommitted(s);
    t = Tick(s, team, StanceOf(rule), D1(rule), 5000, true);
    CHECK(!t.callDue);
}

TEST(leader_a_stance_change_is_called_once_and_counted)
{
    LeaderState s;
    std::vector<Candidate> const team = { Bot(1, Tier::EXPERT), Bot(2, Tier::GOOD) };
    FlagPlanState rule = Normal();
    Tick(s, team, StanceOf(rule), D1(rule), 1000, true);
    CallCommitted(s);

    rule.situation = WsgLogic::Situation::ALL_IN;
    TickResult t = Tick(s, team, StanceOf(rule), D1(rule), 3000, true);
    CHECK(t.stanceChanged);
    CHECK(t.stance == Stance::ALL_IN);
    CHECK(t.callDue);
    CHECK(s.stanceSeq == 1);
    CHECK(s.stanceVersion == 2);
    CallCommitted(s);
    CHECK(!Tick(s, team, StanceOf(rule), D1(rule), 5000, true).callDue);
}

TEST(leader_a_new_leader_says_the_stance_again)
{
    LeaderState s;
    FlagPlanState const rule = Normal();
    Tick(s, { Bot(1, Tier::EXPERT), Bot(2, Tier::GOOD) }, StanceOf(rule), D1(rule), 1000, true);
    CallCommitted(s);
    TickResult const t = Tick(s, { Bot(1, Tier::EXPERT, 85, false), Bot(2, Tier::GOOD) }, StanceOf(rule), D1(rule), 3000, true);
    CHECK(t.change == Change::HANDOVER);
    CHECK(t.leader == 2);
    CHECK(t.callDue);
}

TEST(leader_a_call_that_never_got_its_turn_is_dropped_after_a_while)
{
    LeaderState s;
    std::vector<Candidate> const team = { Bot(1, Tier::EXPERT) };
    FlagPlanState rule = Normal();
    Tick(s, team, StanceOf(rule), D1(rule), 1000, true);
    CallCommitted(s);

    rule.situation = WsgLogic::Situation::LEAD2;
    CHECK(Tick(s, team, StanceOf(rule), D1(rule), 3000, true).callDue);
    CHECK(Tick(s, team, StanceOf(rule), D1(rule), 3000 + STANCE_CALL_STALE_MS - 1, true).callDue);
    CHECK(!Tick(s, team, StanceOf(rule), D1(rule), 3000 + STANCE_CALL_STALE_MS, true).callDue);
}

TEST(leader_with_no_bot_to_lead_owes_the_call_but_nobody_says_it)
{
    LeaderState s;
    FlagPlanState const rule = Normal();
    TickResult const t = Tick(s, { Human(1) }, StanceOf(rule), D1(rule), 1000, true);
    CHECK(t.leader == 0);
    CHECK(!t.callDue);
    CHECK(s.callOwed);
}

TEST(leader_close_out_is_not_said_by_the_leader)
{
    LeaderState s;
    std::vector<Candidate> const team = { Bot(1, Tier::EXPERT) };
    Tick(s, team, Stance::BALANCED, StanceOffer(BgPlan::Plan::BALANCED, AbLogic::Regime::EVEN, {}), 1000, true);
    CallCommitted(s);
    TickResult const t = Tick(s, team, Stance::CLOSE_OUT, StanceOffer(BgPlan::Plan::BALANCED, AbLogic::Regime::CLOSE_OUT, {}), 3000, true);
    CHECK(t.stanceChanged);
    CHECK(!t.callDue);
    CHECK(s.called == Stance::CLOSE_OUT);
}

// --- the hook for a decision layer ------------------------------------------------------------------------------------

TEST(leader_d1_offer_starts_with_the_rule_for_every_plan_and_situation)
{
    for (BgPlan::Plan plan : { BgPlan::Plan::BALANCED, BgPlan::Plan::OFFENSE, BgPlan::Plan::DEFENSE })
    {
        for (WsgLogic::Situation sit : { WsgLogic::Situation::NORMAL, WsgLogic::Situation::LEAD2, WsgLogic::Situation::ALL_IN,
                                         WsgLogic::Situation::LEVEL_LATE, WsgLogic::Situation::LEAD1_LATE })
        {
            FlagPlanState const state{ plan, sit };
            Offer const offer = StanceOffer(state);
            CHECK(!offer.options.empty());
            CHECK(offer.Fallback() == static_cast<std::uint8_t>(StanceOf(state)));
            CHECK(offer.options.front().score == 100);
            for (std::size_t i = 1; i < offer.options.size(); ++i)
            {
                CHECK(offer.options[i].score < offer.options[i - 1].score ||
                      (offer.options[i].score == offer.options[i - 1].score && offer.options[i].id > offer.options[i - 1].id));
                CHECK(offer.options[i].id != offer.options[0].id);
            }
        }
    }
}

TEST(leader_stances_map_back_to_the_plan_state_they_name)
{
    FlagPlanState const rule = Normal(BgPlan::Plan::OFFENSE);
    for (Stance stance : { Stance::BALANCED, Stance::OFFENSE, Stance::DEFENSE, Stance::ALL_IN, Stance::TURTLE })
        CHECK(StanceOf(Apply(stance, rule)) == stance);
    // The drawn plan survives all in and turtle.
    CHECK(Apply(Stance::ALL_IN, rule).plan == BgPlan::Plan::OFFENSE);
    CHECK(Apply(Stance::TURTLE, rule).plan == BgPlan::Plan::OFFENSE);
    // A stance of the node plan changes nothing on the flag plan.
    FlagPlanState const same = Apply(Stance::HOLD, rule);
    CHECK(same.plan == rule.plan && same.situation == rule.situation);
}

TEST(leader_only_a_normal_game_offers_a_meaningful_alternative)
{
    CHECK(IsMeaningful(StanceOffer(Normal())));
    CHECK(IsMeaningful(StanceOffer({ BgPlan::Plan::BALANCED, WsgLogic::Situation::LEAD1_LATE })));
    CHECK(!IsMeaningful(StanceOffer({ BgPlan::Plan::BALANCED, WsgLogic::Situation::ALL_IN })));
    CHECK(!IsMeaningful(StanceOffer({ BgPlan::Plan::BALANCED, WsgLogic::Situation::LEAD2 })));
    // Arathi Basin: the even play and the regimes with a real alternative are asked; the close out (the fifth node ends it) is not.
    CHECK(IsMeaningful(StanceOffer(BgPlan::Plan::BALANCED, AbLogic::Regime::PUSH, {})));
    CHECK(!IsMeaningful(StanceOffer(BgPlan::Plan::BALANCED, AbLogic::Regime::CLOSE_OUT, { 4, 1 })));
}

TEST(leader_the_node_plan_names_its_regimes)
{
    CHECK(StanceOf(BgPlan::Plan::OFFENSE, AbLogic::Regime::EVEN) == Stance::OFFENSE);
    CHECK(StanceOf(BgPlan::Plan::OFFENSE, AbLogic::Regime::HOLD) == Stance::HOLD);
    CHECK(StanceOf(BgPlan::Plan::DEFENSE, AbLogic::Regime::PUSH) == Stance::PUSH);
    CHECK(StanceOf(BgPlan::Plan::BALANCED, AbLogic::Regime::CLOSE_OUT) == Stance::CLOSE_OUT);
    CHECK(StanceOf(BgPlan::Plan::BALANCED, AbLogic::Regime::LAST_NODE) == Stance::LAST_NODE);
    CHECK(StanceOf(BgPlan::Plan::BALANCED, AbLogic::Regime::PANIC) == Stance::PANIC);
}

namespace
{
struct AnswerSetup
{
    LeaderState s;
    PvpObs::MatchKey key{ 9, 4 };
    std::vector<Candidate> team = { Bot(1, Tier::EXPERT), Bot(2, Tier::GOOD) };
    FlagPlanState rule = Normal();

    AnswerSetup() { Tick(s, team, StanceOf(rule), D1(rule), 1000, true); }

    Answer Good(Stance stance) const
    {
        Answer a;
        a.key = key;
        a.side = 0;
        a.leaderEpoch = s.election.epoch;
        a.stateVersion = s.stanceVersion;
        a.point = DecisionPoint::D1_STANCE;
        a.optionId = static_cast<std::uint8_t>(stance);
        return a;
    }
};
}  // namespace

TEST(leader_a_valid_answer_replaces_the_stance_until_the_rule_moves)
{
    AnswerSetup t;
    CHECK(AcceptStanceAnswer(t.s, t.Good(Stance::OFFENSE), t.key, 0) == Verdict::ACCEPT);
    CHECK(Effective(t.s) == Stance::OFFENSE);
    TickResult tick = Tick(t.s, t.team, StanceOf(t.rule), D1(t.rule), 3000, true);
    CHECK(tick.stance == Stance::OFFENSE);
    CHECK(tick.callDue);

    // The rule's stance moves: the answer was for the old state and is gone.
    t.rule.situation = WsgLogic::Situation::ALL_IN;
    tick = Tick(t.s, t.team, StanceOf(t.rule), D1(t.rule), 5000, true);
    CHECK(tick.stance == Stance::ALL_IN);
    CHECK(!t.s.layerChosen);
}

TEST(leader_an_answer_is_dropped_for_every_kind_of_staleness)
{
    AnswerSetup t;
    Answer a = t.Good(Stance::OFFENSE);
    a.key.generation = 5;
    CHECK(AcceptStanceAnswer(t.s, a, t.key, 0) == Verdict::STALE_MATCH);
    a = t.Good(Stance::OFFENSE);
    a.key.instanceId = 10;
    CHECK(AcceptStanceAnswer(t.s, a, t.key, 0) == Verdict::STALE_MATCH);
    a = t.Good(Stance::OFFENSE);
    a.side = 1;
    CHECK(AcceptStanceAnswer(t.s, a, t.key, 0) == Verdict::WRONG_SIDE);
    a = t.Good(Stance::OFFENSE);
    a.leaderEpoch += 1;
    CHECK(AcceptStanceAnswer(t.s, a, t.key, 0) == Verdict::STALE_LEADER);
    a = t.Good(Stance::OFFENSE);
    a.stateVersion += 1;
    CHECK(AcceptStanceAnswer(t.s, a, t.key, 0) == Verdict::STALE_STATE);
    a = t.Good(Stance::OFFENSE);
    a.point = DecisionPoint::D6_FLOATER;
    CHECK(AcceptStanceAnswer(t.s, a, t.key, 0) == Verdict::WRONG_POINT);
    CHECK(AcceptStanceAnswer(t.s, t.Good(Stance::PANIC), t.key, 0) == Verdict::UNKNOWN_OPTION);
    CHECK(!t.s.layerChosen);
}

TEST(leader_an_answer_dies_with_a_handover)
{
    AnswerSetup t;
    Answer const a = t.Good(Stance::OFFENSE);
    Tick(t.s, { Bot(1, Tier::EXPERT, 85, false), Bot(2, Tier::GOOD) }, StanceOf(t.rule), D1(t.rule), 3000, true);
    CHECK(AcceptStanceAnswer(t.s, a, t.key, 0) == Verdict::STALE_LEADER);
}

TEST(leader_the_lists_of_the_twin_peaks_plan_are_offers_of_the_same_shape)
{
    // D7 wipe reply, D6 floater and D4 carrier lane keep their order and their scores; the first option is the rule's.
    TpLogic::WipeFacts wipe;
    wipe.baseThreatened = true;
    Offer const d7 = OfferFrom(DecisionPoint::D7_WIPE_REPLY, 3, TpLogic::WipeReplies(wipe));
    CHECK(d7.point == DecisionPoint::D7_WIPE_REPLY);
    CHECK(d7.stateVersion == 3);
    CHECK(d7.options.size() == 3);
    CHECK(d7.Fallback() == static_cast<std::uint8_t>(TpLogic::WipeReply::DEFEND_FIRST));
    CHECK(IsMeaningful(d7));

    TpLogic::FloaterFacts floater;
    floater.theirAttackWiped = true;
    Offer const d6 = OfferFrom(DecisionPoint::D6_FLOATER, 1, TpLogic::FloaterJobs(floater));
    CHECK(d6.Fallback() == static_cast<std::uint8_t>(TpLogic::FloaterJob::TO_STRIKE));

    Offer const d4 = OfferFrom(DecisionPoint::D4_CARRIER_LANE, 1, TpLogic::CarrierLanes(true, 3));
    CHECK(d4.Fallback() == static_cast<std::uint8_t>(TpLogic::CarrierLane::OTHER_LANE));
    CHECK(!IsMeaningful(OfferFrom(DecisionPoint::D4_CARRIER_LANE, 1, TpLogic::CarrierLanes(false, 0))));

    // An answer for one of these lists is checked the same way as a stance.
    Answer answer;
    answer.key = { 1, 1 };
    answer.side = 1;
    answer.leaderEpoch = 2;
    answer.stateVersion = 3;
    answer.point = DecisionPoint::D7_WIPE_REPLY;
    answer.optionId = static_cast<std::uint8_t>(TpLogic::WipeReply::REGROUP_LANE);
    CHECK(CheckAnswer(d7, answer, { 1, 1 }, 1, 2) == Verdict::ACCEPT);
}

// --- drift ------------------------------------------------------------------------------------------------------------

TEST(leader_only_casual_bots_drift)
{
    for (std::uint32_t guid = 1; guid < 50; ++guid)
    {
        CHECK(RoleKeepRefreshes(Tier::EXPERT, guid, 1) == 0);
        CHECK(RoleKeepRefreshes(Tier::GOOD, guid, 1) == 0);
    }
    CHECK(StanceExtraLagMs(Tier::EXPERT) == 0);
    CHECK(StanceExtraLagMs(Tier::GOOD) == 0);
    CHECK(StanceExtraLagMs(Tier::CASUAL) == BgPlan::REFRESH_MS);
}

TEST(leader_about_a_quarter_of_casual_bots_keep_their_role_and_the_roll_is_stable)
{
    std::uint32_t kept = 0;
    std::uint32_t const total = 4000;
    for (std::uint32_t guid = 1; guid <= total; ++guid)
    {
        std::uint32_t const keep = RoleKeepRefreshes(Tier::CASUAL, guid, 3);
        CHECK(keep <= 1);
        CHECK(keep == RoleKeepRefreshes(Tier::CASUAL, guid, 3));
        kept += keep;
    }
    CHECK(kept > total * 20 / 100);
    CHECK(kept < total * 30 / 100);

    // A later stance change rolls again: not the same bots every time.
    std::uint32_t differs = 0;
    for (std::uint32_t guid = 1; guid <= 200; ++guid)
        if (RoleKeepRefreshes(Tier::CASUAL, guid, 3) != RoleKeepRefreshes(Tier::CASUAL, guid, 4))
            ++differs;
    CHECK(differs > 0);
}

TEST(leader_a_critical_duty_never_drifts)
{
    CHECK(MayKeepRole(1, false));
    CHECK(!MayKeepRole(1, true));
    CHECK(!MayKeepRole(0, false));
}

TEST(leader_stance_call_has_the_same_gap_in_both_plans_and_both_limiters)
{
    CHECK(WsgLogic::CALL_KIND_GAP_STANCE_MS == STANCE_CALL_GAP_MS);
    CHECK(AbLogic::CALL_STANCE_GAP_MS == STANCE_CALL_GAP_MS);

    // Two stance lines of one team: the second waits, whoever says it, even past the team's 6 s gap.
    WsgLogic::CallLimiter wsg;
    wsg.Commit(1, WsgLogic::Call::STANCE, 10000);
    CHECK(!wsg.Allow(2, WsgLogic::Call::STANCE, 10000 + BgPlan::CALL_TEAM_GAP_MS));
    CHECK(wsg.Allow(2, WsgLogic::Call::STANCE, 10000 + STANCE_CALL_GAP_MS));
    CHECK(wsg.Allow(2, WsgLogic::Call::EFC_TAKEN, 10000 + BgPlan::CALL_TEAM_GAP_MS));

    AbLogic::CallLimiter ab;
    ab.Commit(1, AbLogic::Call::STANCE, 10000);
    CHECK(!ab.Allow(2, AbLogic::Call::STANCE, 10000 + BgPlan::CALL_TEAM_GAP_MS));
    CHECK(ab.Allow(2, AbLogic::Call::STANCE, 10000 + STANCE_CALL_GAP_MS));
}
