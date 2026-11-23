#include "TestHarness.h"
#include "AiPvpDecisionPolicy.h"
#include "BgLeaderAdapters.h"

#include <functional>

// BT-501: the battleground strategy asks: slot encoding, which points are asked, the pacing and the request cap, the judging of
// an answer (every refusal), the book of the points the leader policy does not hold, the lists of Arathi Basin, and every path
// on which the rule plan simply keeps playing.

using namespace AiPvp;
using BgLeader::DecisionPoint;
using BgLeader::Offer;

namespace
{
constexpr std::uint32_t T0 = 100000;

Offer List(DecisionPoint point, std::uint32_t version, std::vector<Option> options)
{
    Offer offer;
    offer.point = point;
    offer.stateVersion = version;
    offer.options = std::move(options);
    BgLeader::SortOptions(offer.options);
    return offer;
}

// A real choice: two options within the meaningful score.
Offer Choice(DecisionPoint point = DecisionPoint::D1_STANCE, std::uint32_t version = 3)
{
    return List(point, version, { { 1, 100 }, { 2, 60 }, { 3, 50 } });
}

// No real choice: the second option is far behind.
Offer NoChoice(DecisionPoint point = DecisionPoint::D1_STANCE, std::uint32_t version = 3)
{
    return List(point, version, { { 1, 100 }, { 2, 20 } });
}

GateInput Open(std::uint32_t nowMs)
{
    GateInput in;
    in.enabled = true;
    in.started = true;
    in.leaderPresent = true;
    in.nowMs = nowMs;
    return in;
}

PvpObs::MatchKey Key() { return { 7, 2 }; }

// A request in flight for the stance list, asked at T0.
PendingAsk Pending(Offer const& offer, std::uint32_t requestId = 1, std::uint32_t epoch = 4, std::uint32_t sentMs = T0)
{
    PendingAsk p;
    p.active = true;
    p.requestId = requestId;
    p.point = offer.point;
    p.stateVersion = offer.stateVersion;
    p.leaderEpoch = epoch;
    p.sentMs = sentMs;
    p.deadlineMs = 600;
    p.slots = BuildSlots(offer);
    return p;
}

RawAnswer Reply(Offer const& offer, std::string choice, std::uint32_t requestId = 1, std::uint32_t epoch = 4)
{
    RawAnswer a;
    a.key = Key();
    a.side = 1;
    a.leaderEpoch = epoch;
    a.requestId = requestId;
    a.stateVersion = offer.stateVersion;
    a.point = offer.point;
    a.transportOk = true;
    a.source = "cache";
    a.choice = std::move(choice);
    a.confidence = 0.8;
    a.decisionId = "d-1";
    return a;
}

JudgeContext Context(Offer const& current, std::uint32_t nowMs = T0 + 400, std::uint32_t epoch = 4)
{
    JudgeContext c;
    c.nowMs = nowMs;
    c.minConfidencePct = 60;
    c.key = Key();
    c.side = 1;
    c.leaderEpoch = epoch;
    c.current = &current;
    return c;
}
}  // namespace

// --- slots ------------------------------------------------------------------------------------------------------------

TEST(ai_pvp_slot_names_are_fixed_and_only_the_exact_names_parse)
{
    CHECK(SlotName(0) == "option_0");
    CHECK(SlotName(5) == "option_5");
    CHECK(SlotIndex("option_0") == std::optional<std::size_t>(0));
    CHECK(SlotIndex("option_5") == std::optional<std::size_t>(5));
    // The answer is data from outside: anything but a name that was sent is no slot.
    CHECK(!SlotIndex("option_6"));
    CHECK(!SlotIndex("option_"));
    CHECK(!SlotIndex("option_00"));
    CHECK(!SlotIndex("Option_0"));
    CHECK(!SlotIndex("option_0 "));
    CHECK(!SlotIndex("balanced"));
    CHECK(!SlotIndex(""));
}

TEST(ai_pvp_slot_table_keeps_the_rule_order_and_the_cap)
{
    Offer const offer = List(DecisionPoint::D1_STANCE, 1, { { 5, 100 }, { 0, 60 }, { 2, 50 }, { 3, 50 } });
    SlotTable const table = BuildSlots(offer);
    CHECK(table.slots.size() == 4);
    // Slot 0 is the rule's answer; the tie of 50 goes to the lower id (the same order as the list).
    CHECK(table.OptionOf("option_0") == std::optional<std::uint8_t>(5));
    CHECK(table.OptionOf("option_1") == std::optional<std::uint8_t>(0));
    CHECK(table.OptionOf("option_2") == std::optional<std::uint8_t>(2));
    CHECK(table.OptionOf("option_3") == std::optional<std::uint8_t>(3));
    CHECK(!table.OptionOf("option_4"));
    CHECK(!table.OptionOf("balanced"));

    std::vector<Option> many;
    for (std::uint8_t i = 0; i < 9; ++i)
        many.push_back({ i, 100 - i });
    CHECK(BuildSlots(List(DecisionPoint::D9_NODE_TARGET, 1, many)).slots.size() == MAX_SLOTS);
}

TEST(ai_pvp_interval_is_clamped_and_an_answer_lives_two_intervals)
{
    CHECK(ClampIntervalS(5) == 15);
    CHECK(ClampIntervalS(20) == 20);
    CHECK(ClampIntervalS(90) == 30);
    CHECK(AnswerLifeMs(20) == 40000);
    CHECK(AnswerLifeMs(1) == 30000);
}

// --- which point is asked ------------------------------------------------------------------------------------------------

TEST(ai_pvp_only_a_real_choice_is_asked)
{
    Offer const real = Choice();
    Offer const one = List(DecisionPoint::D1_STANCE, 1, { { 1, 100 } });
    Offer const lopsided = NoChoice();
    CHECK(ChoosePoint({ { DecisionPoint::D1_STANCE, &real, 0 } }));
    CHECK(ChoosePoint({ { DecisionPoint::D1_STANCE, &one, 0 } }) .has_value() == false);
    CHECK(ChoosePoint({ { DecisionPoint::D1_STANCE, &lopsided, 0 } }) .has_value() == false);
    CHECK(ChoosePoint({ { DecisionPoint::D1_STANCE, nullptr, 0 } }) .has_value() == false);
    CHECK(ChoosePoint({}) .has_value() == false);
}

namespace
{
// The point ChoosePoint picks from a list of candidates, or a point that no candidate has (D8) for none.
DecisionPoint Picked(std::vector<AskCandidate> const& candidates)
{
    std::optional<std::size_t> const index = ChoosePoint(candidates);
    return index ? candidates[*index].point : DecisionPoint::D8_STALEMATE;
}
}  // namespace

TEST(ai_pvp_the_wipe_reply_comes_before_the_stance_and_is_asked_once_per_list)
{
    Offer const stance = Choice(DecisionPoint::D1_STANCE, 3);
    Offer const wipe = Choice(DecisionPoint::D7_WIPE_REPLY, 2);
    Offer const floater = Choice(DecisionPoint::D6_FLOATER, 5);

    CHECK(Picked({ { DecisionPoint::D1_STANCE, &stance, 0 }, { DecisionPoint::D6_FLOATER, &floater, 0 },
                   { DecisionPoint::D7_WIPE_REPLY, &wipe, 0 } }) == DecisionPoint::D7_WIPE_REPLY);

    // Asked for this version already: the next point is the floater, then the stance.
    CHECK(Picked({ { DecisionPoint::D1_STANCE, &stance, 0 }, { DecisionPoint::D6_FLOATER, &floater, 0 },
                   { DecisionPoint::D7_WIPE_REPLY, &wipe, 2 } }) == DecisionPoint::D6_FLOATER);
    CHECK(Picked({ { DecisionPoint::D1_STANCE, &stance, 0 }, { DecisionPoint::D6_FLOATER, &floater, 5 },
                   { DecisionPoint::D7_WIPE_REPLY, &wipe, 2 } }) == DecisionPoint::D1_STANCE);

    // A list of another version (the state moved on) is asked again.
    CHECK(Picked({ { DecisionPoint::D7_WIPE_REPLY, &wipe, 1 } }) == DecisionPoint::D7_WIPE_REPLY);

    // The stance and the node target repeat for the same version: they follow the clock and the ground.
    CHECK(Picked({ { DecisionPoint::D1_STANCE, &stance, 3 } }) == DecisionPoint::D1_STANCE);
    Offer const target = Choice(DecisionPoint::D9_NODE_TARGET, 1);
    CHECK(Picked({ { DecisionPoint::D9_NODE_TARGET, &target, 1 } }) == DecisionPoint::D9_NODE_TARGET);
}

TEST(ai_pvp_the_carrier_lane_is_not_asked_while_no_path_set_runs_it)
{
    CHECK(!IsWired(DecisionPoint::D4_CARRIER_LANE));
    CHECK(!IsWired(DecisionPoint::D2_PICKUP));
    CHECK(!IsWired(DecisionPoint::D8_STALEMATE));
    CHECK(IsWired(DecisionPoint::D1_STANCE));
    CHECK(IsWired(DecisionPoint::D6_FLOATER));
    CHECK(IsWired(DecisionPoint::D7_WIPE_REPLY));
    CHECK(IsWired(DecisionPoint::D9_NODE_TARGET));
    // The arena asks of the later step are a hook only: nothing is asked until the arena plan wires them.
    CHECK(!IsWired(DecisionPoint::D10_ARENA_KILL_TARGET));
    CHECK(!IsWired(DecisionPoint::D11_ARENA_CC_TARGET));
    CHECK(std::string(PointName(DecisionPoint::D10_ARENA_KILL_TARGET)) == "arena_kill_target");
    CHECK(std::string(PointName(DecisionPoint::D11_ARENA_CC_TARGET)) == "arena_cc_target");
    Offer const kill = Choice(DecisionPoint::D10_ARENA_KILL_TARGET, 1);
    CHECK(!ChoosePoint({ { DecisionPoint::D10_ARENA_KILL_TARGET, &kill, 0 } }).has_value());
    Offer const lane = Choice(DecisionPoint::D4_CARRIER_LANE, 1);
    CHECK(ChoosePoint({ { DecisionPoint::D4_CARRIER_LANE, &lane, 0 } }) .has_value() == false);
}

// --- pacing -----------------------------------------------------------------------------------------------------------

TEST(ai_pvp_gate_first_ask_at_once_then_every_interval)
{
    Pacing p;
    CHECK(AskGate(p, Open(T0)) == Gate::OPEN);
    NoteSent(p, T0);
    p.inFlight = false;   // the answer came back
    CHECK(AskGate(p, Open(T0 + 19999)) == Gate::NOT_DUE);
    CHECK(AskGate(p, Open(T0 + 20000)) == Gate::OPEN);
    GateInput quick = Open(T0 + 15000);
    quick.intervalMs = 15000;
    CHECK(AskGate(p, quick) == Gate::OPEN);
}

TEST(ai_pvp_gate_one_request_in_flight_per_side)
{
    Pacing p;
    NoteSent(p, T0);
    CHECK(AskGate(p, Open(T0 + 1000)) == Gate::IN_FLIGHT);
    // Not lost before the deadline, the slack and the grace have passed (600 + 4000 + 3000 ms).
    CHECK(AskGate(p, Open(T0 + 7599)) == Gate::IN_FLIGHT);
    CHECK(AskGate(p, Open(T0 + 20000)) == Gate::OPEN);
    CHECK(!p.inFlight);
}

TEST(ai_pvp_gate_an_event_asks_after_five_seconds_but_not_sooner)
{
    Pacing p;
    NoteSent(p, T0);
    p.inFlight = false;
    NoteEvent(p, T0 + 1000);
    CHECK(AskGate(p, Open(T0 + 4999)) == Gate::TOO_SOON);
    CHECK(AskGate(p, Open(T0 + 5000)) == Gate::OPEN);
    // The ask consumed the event: the next one waits for the interval.
    NoteSent(p, T0 + 5000);
    p.inFlight = false;
    CHECK(AskGate(p, Open(T0 + 11000)) == Gate::NOT_DUE);
    // A second event during the in-flight ask is kept for the next one.
    NoteEvent(p, T0 + 6000);
    CHECK(AskGate(p, Open(T0 + 10000)) == Gate::OPEN);
}

TEST(ai_pvp_gate_an_old_event_is_dropped_after_an_interval)
{
    Pacing p;
    NoteSent(p, T0);
    p.inFlight = false;
    NoteEvent(p, T0 + 100);
    // The interval passed with the event waiting: the routine ask takes it (it is OPEN, not an event ask) and is consumed.
    CHECK(AskGate(p, Open(T0 + 20100)) == Gate::OPEN);
    CHECK(!p.eventPending);
}

TEST(ai_pvp_gate_the_request_cap_and_the_switches)
{
    Pacing p;
    GateInput in = Open(T0);
    in.maxRequests = 2;
    NoteSent(p, T0);
    p.inFlight = false;
    NoteSent(p, T0 + 20000);
    p.inFlight = false;
    in.nowMs = T0 + 45000;
    CHECK(AskGate(p, in) == Gate::CAP);
    in.maxRequests = 0;
    Pacing fresh;
    CHECK(AskGate(fresh, in) == Gate::CAP);

    Pacing q;
    GateInput off = Open(T0);
    off.enabled = false;
    CHECK(AskGate(q, off) == Gate::OFF);
    GateInput waiting = Open(T0);
    waiting.started = false;
    CHECK(AskGate(q, waiting) == Gate::NOT_STARTED);
    GateInput nobody = Open(T0);
    nobody.leaderPresent = false;
    CHECK(AskGate(q, nobody) == Gate::NO_LEADER);
    GateInput over = Open(T0);
    over.matchOver = true;
    CHECK(AskGate(q, over) == Gate::MATCH_OVER);
}

TEST(ai_pvp_plan_ask_publishes_nothing_to_wait_for_and_picks_the_point)
{
    SideAi ai;
    Offer const stance = Choice(DecisionPoint::D1_STANCE, 3);
    AskPlan plan = PlanAsk(ai, Open(T0), stance);
    CHECK(plan.ask);
    CHECK(plan.point == DecisionPoint::D1_STANCE);
    CHECK(plan.stateVersion == 3);
    CHECK(plan.slots.slots.size() == 3);

    // Planning does not send: nothing is counted until the request is handed to the client.
    CHECK(ai.pacing.requests == 0);
    std::uint32_t const id = BeginAsk(ai, plan, 4, 600, T0);
    CHECK(id == 1);
    CHECK(ai.pacing.requests == 1);
    CHECK(ai.pending.active && ai.pending.leaderEpoch == 4 && ai.pending.stateVersion == 3);
    CHECK(PlanAsk(ai, Open(T0 + 1000), stance).gate == Gate::IN_FLIGHT);

    Finish(ai, 99);   // another request's answer does not free the side
    CHECK(ai.pending.active);
    Finish(ai, id);
    CHECK(!ai.pending.active && !ai.pacing.inFlight);
    CHECK(PlanAsk(ai, Open(T0 + 1000), stance).gate == Gate::NOT_DUE);
}

TEST(ai_pvp_plan_ask_without_a_real_choice_asks_nothing)
{
    SideAi ai;
    Offer const stance = NoChoice();
    AskPlan const plan = PlanAsk(ai, Open(T0), stance);
    CHECK(!plan.ask);
    CHECK(plan.gate == Gate::NO_CHOICE);
    CHECK(ai.pacing.requests == 0);
}

TEST(ai_pvp_plan_ask_takes_a_wipe_reply_from_the_book_and_asks_it_once)
{
    SideAi ai;
    Offer const stance = Choice(DecisionPoint::D1_STANCE, 3);
    Publish(ai.book, Choice(DecisionPoint::D7_WIPE_REPLY), { "base_threatened:yes" }, true, T0);
    AskPlan plan = PlanAsk(ai, Open(T0), stance);
    CHECK(plan.ask && plan.point == DecisionPoint::D7_WIPE_REPLY);
    std::uint32_t const version = plan.stateVersion;
    BeginAsk(ai, plan, 4, 600, T0);
    Finish(ai, 1);

    // The next ask (the interval later) is the stance: the wipe list is the same version.
    Publish(ai.book, Choice(DecisionPoint::D7_WIPE_REPLY), { "base_threatened:yes" }, true, T0 + 20000);
    CHECK(ai.book.At(DecisionPoint::D7_WIPE_REPLY).version == version);
    plan = PlanAsk(ai, Open(T0 + 20000), stance);
    CHECK(plan.ask && plan.point == DecisionPoint::D1_STANCE);
}

TEST(ai_pvp_a_refused_ask_is_not_counted_and_waits_for_the_next_interval)
{
    SideAi ai;
    Offer const stance = Choice();
    CHECK(PlanAsk(ai, Open(T0), stance).ask);
    AskRefused(ai, T0);
    CHECK(ai.pacing.requests == 0);
    CHECK(ai.refused == 1);
    CHECK(PlanAsk(ai, Open(T0 + 2000), stance).gate == Gate::NOT_DUE);
    CHECK(PlanAsk(ai, Open(T0 + 20000), stance).ask);
}

TEST(ai_pvp_events_come_from_a_changed_signature_and_the_first_reading_is_the_baseline)
{
    SideAi ai;
    ObserveEvents(ai, 111, T0);
    CHECK(!ai.pacing.eventPending);
    ObserveEvents(ai, 111, T0 + 2000);
    CHECK(!ai.pacing.eventPending);
    ObserveEvents(ai, 222, T0 + 4000);
    CHECK(ai.pacing.eventPending);
    CHECK(ai.pacing.eventMs == T0 + 4000);
}

TEST(ai_pvp_a_handover_drops_the_awaited_request_and_a_match_end_drops_everything)
{
    SideAi ai;
    Offer const stance = Choice();
    SideAi other;
    BeginAsk(other, PlanAsk(other, Open(T0), stance), 4, 600, T0);
    other.lastLeaderEpoch = 4;
    OnLeaderEpoch(other, 4);
    CHECK(other.pending.active);
    OnLeaderEpoch(other, 5);
    CHECK(!other.pending.active && !other.pacing.inFlight);
    // The answer of the old leader's request is refused: nothing is awaited.
    Offer const now = Choice(DecisionPoint::D1_STANCE, 3);
    Judgement const j = Judge(other.pending, Reply(now, "option_1"), Context(now, T0 + 300, 5));
    CHECK(!j.accept && j.reason == Reject::STALE_REQUEST);

    Publish(ai.book, Choice(DecisionPoint::D6_FLOATER), {}, true, T0);
    SetChoice(ai.book, DecisionPoint::D6_FLOATER, 1, T0, 40000);
    CHECK(Override(ai.book, DecisionPoint::D6_FLOATER, T0 + 1000));
    BeginAsk(ai, PlanAsk(ai, Open(T0 + 30000), stance), 4, 600, T0 + 30000);
    OnMatchOver(ai);
    CHECK(!ai.pending.active && !ai.pacing.inFlight);
    CHECK(!Override(ai.book, DecisionPoint::D6_FLOATER, T0 + 1000));
}

// --- judging an answer: every refusal and the accept ----------------------------------------------------------------------

TEST(ai_pvp_an_answer_that_passes_every_check_is_accepted)
{
    Offer const offer = Choice();
    Judgement const j = Judge(Pending(offer), Reply(offer, "option_1"), Context(offer));
    CHECK(j.accept);
    CHECK(j.reason == Reject::NONE);
    // Slot 1 of the list { 1: 100, 2: 60, 3: 50 } is option id 2.
    CHECK(j.optionId == 2);
    // The rule's own slot is accepted too (the model agrees with the rule).
    CHECK(Judge(Pending(offer), Reply(offer, "option_0"), Context(offer)).optionId == 1);
}

TEST(ai_pvp_every_refusal_keeps_the_rule_plan)
{
    Offer const offer = Choice();
    PendingAsk const pending = Pending(offer);
    JudgeContext const ctx = Context(offer);

    // Not the request that is awaited, or none at all.
    CHECK(Judge(pending, Reply(offer, "option_1", 2), ctx).reason == Reject::STALE_REQUEST);
    PendingAsk none = pending;
    none.active = false;
    CHECK(Judge(none, Reply(offer, "option_1"), ctx).reason == Reject::STALE_REQUEST);

    // The match ended.
    JudgeContext over = ctx;
    over.matchOver = true;
    CHECK(Judge(pending, Reply(offer, "option_1"), over).reason == Reject::MATCH_OVER);

    // No reply: the client's timeout, a busy client, a dead link.
    RawAnswer lost = Reply(offer, "option_1");
    lost.transportOk = false;
    lost.error = "timeout";
    CHECK(Judge(pending, lost, ctx).reason == Reject::TRANSPORT);

    // The sidecar says the rules should decide (budget, breaker, disabled, busy) or fails.
    for (char const* source : { "rules_hint", "error", "rules", "" })
    {
        RawAnswer hint = Reply(offer, "");
        hint.source = source;
        CHECK(Judge(pending, hint, ctx).reason == Reject::SOURCE);
    }

    CHECK(Judge(pending, Reply(offer, ""), ctx).reason == Reject::NO_CHOICE);
    CHECK(Judge(pending, Reply(offer, "balanced"), ctx).reason == Reject::UNKNOWN_SLOT);
    CHECK(Judge(pending, Reply(offer, "option_3"), ctx).reason == Reject::UNKNOWN_SLOT);   // only three slots were sent
    CHECK(Judge(pending, Reply(offer, "option_9"), ctx).reason == Reject::UNKNOWN_SLOT);

    // Too late: the deadline of 600 ms plus the slack of 4000 ms.
    CHECK(Judge(pending, Reply(offer, "option_1"), Context(offer, T0 + 4600)).accept);
    CHECK(Judge(pending, Reply(offer, "option_1"), Context(offer, T0 + 4601)).reason == Reject::LATE);

    // Low confidence, or none reported.
    RawAnswer unsure = Reply(offer, "option_1");
    unsure.confidence = 0.59;
    CHECK(Judge(pending, unsure, ctx).reason == Reject::LOW_CONFIDENCE);
    unsure.confidence = 0.6;
    CHECK(Judge(pending, unsure, ctx).accept);
    unsure.confidence = -1.0;
    CHECK(Judge(pending, unsure, ctx).reason == Reject::LOW_CONFIDENCE);
}

TEST(ai_pvp_stale_answers_are_refused_by_match_side_leader_state_and_point)
{
    Offer const offer = Choice();
    PendingAsk const pending = Pending(offer);

    RawAnswer otherGeneration = Reply(offer, "option_1");
    otherGeneration.key = { 7, 1 };
    CHECK(Judge(pending, otherGeneration, Context(offer)).reason == Reject::STALE_MATCH);

    RawAnswer otherSide = Reply(offer, "option_1");
    otherSide.side = 0;
    CHECK(Judge(pending, otherSide, Context(offer)).reason == Reject::WRONG_SIDE);

    // The leader changed since the request: the answer carries the old epoch.
    CHECK(Judge(pending, Reply(offer, "option_1", 1, 4), Context(offer, T0 + 400, 5)).reason == Reject::STALE_LEADER);

    // The state moved on: the list the plan publishes now is another version.
    Offer const newer = Choice(DecisionPoint::D1_STANCE, 4);
    CHECK(Judge(pending, Reply(offer, "option_1"), Context(newer)).reason == Reject::STALE_STATE);

    // Another decision point.
    Offer const wipe = Choice(DecisionPoint::D7_WIPE_REPLY, 3);
    CHECK(Judge(pending, Reply(offer, "option_1"), Context(wipe)).reason == Reject::WRONG_POINT);

    // No longer legal: the option is not in the list the plan publishes now (the same version, another set of legal options).
    Offer const narrowed = List(DecisionPoint::D1_STANCE, 3, { { 1, 100 }, { 3, 50 } });
    CHECK(Judge(pending, Reply(offer, "option_1"), Context(narrowed)).reason == Reject::NOT_OFFERED);
    CHECK(Judge(pending, Reply(offer, "option_2"), Context(narrowed)).accept);

    // The plan has no list now.
    JudgeContext nothing = Context(offer);
    nothing.current = nullptr;
    CHECK(Judge(pending, Reply(offer, "option_1"), nothing).reason == Reject::NOT_OFFERED);
}

TEST(ai_pvp_reject_names_are_the_ones_the_log_prints)
{
    CHECK(std::string(RejectName(Reject::NONE)) == "ok");
    CHECK(std::string(RejectName(Reject::LOW_CONFIDENCE)) == "low_confidence");
    CHECK(std::string(RejectName(Reject::STALE_LEADER)) == "stale_leader");
    CHECK(std::string(RejectName(Reject::NOT_OFFERED)) == "not_offered");
    CHECK(std::string(GateName(Gate::CAP)) == "cap");
    CHECK(std::string(PointName(DecisionPoint::D9_NODE_TARGET)) == "target_node");
    CHECK(SourceUsable("cache") && SourceUsable("fake"));
    CHECK(!SourceUsable("rules_hint"));
}

// --- the book ---------------------------------------------------------------------------------------------------------

TEST(ai_pvp_book_a_new_list_is_a_new_version_and_drops_the_old_answer)
{
    Book book;
    Publish(book, Choice(DecisionPoint::D6_FLOATER), { "home_alive:1" }, true, T0);
    BookEntry const& e = book.At(DecisionPoint::D6_FLOATER);
    CHECK(e.version == 1);
    CHECK(e.offer.stateVersion == 1);
    SetChoice(book, DecisionPoint::D6_FLOATER, 2, T0, 40000);
    CHECK(Override(book, DecisionPoint::D6_FLOATER, T0 + 1000) == std::optional<std::uint8_t>(2));

    // The same list again: same version, the answer stands.
    Publish(book, Choice(DecisionPoint::D6_FLOATER), { "home_alive:1" }, true, T0 + 2000);
    CHECK(book.At(DecisionPoint::D6_FLOATER).version == 1);
    CHECK(Override(book, DecisionPoint::D6_FLOATER, T0 + 3000));

    // Another first option (the rule changed its mind): a new version, the answer is gone.
    Publish(book, List(DecisionPoint::D6_FLOATER, 0, { { 2, 100 }, { 1, 60 }, { 3, 50 } }), {}, true, T0 + 4000);
    CHECK(book.At(DecisionPoint::D6_FLOATER).version == 2);
    CHECK(!Override(book, DecisionPoint::D6_FLOATER, T0 + 4500));
}

TEST(ai_pvp_book_the_node_target_ignores_the_order_of_the_list)
{
    Book book;
    Publish(book, List(DecisionPoint::D9_NODE_TARGET, 0, { { 1, 100 }, { 3, 70 } }), {}, false, T0);
    SetChoice(book, DecisionPoint::D9_NODE_TARGET, 3, T0, 40000);
    // The ground changed and node 3 is now the best: the same set of targets, the same version, the answer stands.
    Publish(book, List(DecisionPoint::D9_NODE_TARGET, 0, { { 3, 100 }, { 1, 80 } }), {}, false, T0 + 2000);
    CHECK(book.At(DecisionPoint::D9_NODE_TARGET).version == 1);
    CHECK(Override(book, DecisionPoint::D9_NODE_TARGET, T0 + 3000) == std::optional<std::uint8_t>(3));
    // A node left the list (it was taken): a new version.
    Publish(book, List(DecisionPoint::D9_NODE_TARGET, 0, { { 1, 100 } }), {}, false, T0 + 4000);
    CHECK(book.At(DecisionPoint::D9_NODE_TARGET).version == 2);
    CHECK(!Override(book, DecisionPoint::D9_NODE_TARGET, T0 + 4500));
}

TEST(ai_pvp_book_an_answer_expires_and_a_withdrawn_point_has_none)
{
    Book book;
    Publish(book, Choice(DecisionPoint::D7_WIPE_REPLY), {}, true, T0);
    SetChoice(book, DecisionPoint::D7_WIPE_REPLY, 2, T0, 40000);
    CHECK(Override(book, DecisionPoint::D7_WIPE_REPLY, T0 + 39999));
    CHECK(!Override(book, DecisionPoint::D7_WIPE_REPLY, T0 + 40000));

    SetChoice(book, DecisionPoint::D7_WIPE_REPLY, 2, T0, 40000);
    Withdraw(book, DecisionPoint::D7_WIPE_REPLY);
    CHECK(!Override(book, DecisionPoint::D7_WIPE_REPLY, T0 + 1000));
    CHECK(!book.At(DecisionPoint::D7_WIPE_REPLY).published);

    // An answer for an id the list no longer holds stands for nothing.
    Publish(book, Choice(DecisionPoint::D7_WIPE_REPLY), {}, false, T0 + 5000);
    SetChoice(book, DecisionPoint::D7_WIPE_REPLY, 9, T0 + 5000, 40000);
    CHECK(!Override(book, DecisionPoint::D7_WIPE_REPLY, T0 + 6000));
}

// --- the request cap over a whole match ---------------------------------------------------------------------------------

TEST(ai_pvp_a_side_never_asks_more_than_the_cap_in_a_match)
{
    SideAi ai;
    Offer const stance = Choice();
    GateInput in = Open(T0);
    in.maxRequests = 60;
    std::uint32_t asks = 0;
    // A 30 minute match, a refresh every 2 seconds, every answer back after 500 ms, events every 7 seconds: the cap holds.
    for (std::uint32_t t = 0; t < 30u * 60u * 1000u; t += 2000)
    {
        in.nowMs = T0 + t;
        if (t % 7000 == 0)
            NoteEvent(ai.pacing, in.nowMs);
        if (ai.pacing.inFlight && in.nowMs - ai.pending.sentMs >= 500)
            Finish(ai, ai.pending.requestId);
        AskPlan const plan = PlanAsk(ai, in, stance);
        if (plan.ask)
        {
            BeginAsk(ai, plan, 1, 600, in.nowMs);
            ++asks;
        }
    }
    CHECK(asks == 60);
    CHECK(ai.pacing.requests == 60);
    CHECK(PlanAsk(ai, in, stance).gate == Gate::CAP);
}

TEST(ai_pvp_the_default_cadence_makes_about_ninety_asks_in_thirty_minutes_without_events)
{
    SideAi ai;
    Offer const stance = Choice();
    GateInput in = Open(T0);
    in.maxRequests = 600;
    std::uint32_t asks = 0;
    for (std::uint32_t t = 0; t < 30u * 60u * 1000u; t += 2000)
    {
        in.nowMs = T0 + t;
        if (ai.pacing.inFlight)
            Finish(ai, ai.pending.requestId);
        AskPlan const plan = PlanAsk(ai, in, stance);
        if (plan.ask)
        {
            BeginAsk(ai, plan, 1, 600, in.nowMs);
            ++asks;
        }
    }
    CHECK(asks == 90);
}

// --- Arathi Basin lists ---------------------------------------------------------------------------------------------------

TEST(ai_pvp_ab_even_play_offers_the_plans_hold_and_push_when_they_are_legal)
{
    using BgLeader::Stance;
    Offer const none = BgLeader::StanceOffer(BgPlan::Plan::BALANCED, AbLogic::Regime::EVEN, { 0, 0 });
    CHECK(none.Fallback() == static_cast<std::uint8_t>(Stance::BALANCED));
    CHECK(none.options.size() == 3);   // the two other plans only: nothing to hold or flip

    Offer const three = BgLeader::StanceOffer(BgPlan::Plan::BALANCED, AbLogic::Regime::EVEN, { 3, 2 });
    CHECK(three.options.size() == 5);
    CHECK(three.options[0].id == static_cast<std::uint8_t>(Stance::BALANCED));
    CHECK(three.options[1].id == static_cast<std::uint8_t>(Stance::HOLD));    // 55
    CHECK(three.options[2].id == static_cast<std::uint8_t>(Stance::PUSH));    // 52
    CHECK(BgLeader::IsMeaningful(three));
}

TEST(ai_pvp_ab_regime_lists_keep_the_rule_first_and_close_out_has_no_choice)
{
    using BgLeader::Stance;
    using BgLeader::StanceOffer;
    for (AbLogic::Regime regime : { AbLogic::Regime::EVEN, AbLogic::Regime::HOLD, AbLogic::Regime::PUSH, AbLogic::Regime::LAST_NODE,
                                    AbLogic::Regime::PANIC, AbLogic::Regime::CLOSE_OUT })
    {
        for (BgLeader::AbStanceFacts const facts : { BgLeader::AbStanceFacts{ 0, 0 }, BgLeader::AbStanceFacts{ 1, 4 },
                                                     BgLeader::AbStanceFacts{ 3, 2 }, BgLeader::AbStanceFacts{ 4, 1 } })
        {
            Offer const offer = StanceOffer(BgPlan::Plan::OFFENSE, regime, facts);
            CHECK(offer.options.front().score == 100);
            CHECK(offer.Fallback() == static_cast<std::uint8_t>(BgLeader::StanceOf(BgPlan::Plan::OFFENSE, regime)));
            for (std::size_t i = 1; i < offer.options.size(); ++i)
                CHECK(offer.options[i].score < 100);
            // Close out is never an answer: its list is the rule alone.
            for (BgLeader::Option const& o : offer.options)
                CHECK(o.id != static_cast<std::uint8_t>(Stance::CLOSE_OUT) || regime == AbLogic::Regime::CLOSE_OUT);
        }
    }
    CHECK(!BgLeader::IsMeaningful(StanceOffer(BgPlan::Plan::BALANCED, AbLogic::Regime::CLOSE_OUT, { 4, 1 })));

    // The last node: push or defense; panic: push, and the last node only when one node is ours.
    Offer const last = StanceOffer(BgPlan::Plan::BALANCED, AbLogic::Regime::LAST_NODE, { 1, 4 });
    CHECK(last.options.size() == 3 && last.options[1].id == static_cast<std::uint8_t>(Stance::PUSH));
    Offer const panic = StanceOffer(BgPlan::Plan::BALANCED, AbLogic::Regime::PANIC, { 1, 4 });
    CHECK(panic.options.size() == 3 && panic.options[2].id == static_cast<std::uint8_t>(Stance::LAST_NODE));
    CHECK(StanceOffer(BgPlan::Plan::BALANCED, AbLogic::Regime::PANIC, { 2, 4 }).options.size() == 2);
}

TEST(ai_pvp_ab_a_stance_answer_becomes_a_plan_and_a_regime)
{
    using BgLeader::Stance;
    BgLeader::AbPlanState const rule{ BgPlan::Plan::DEFENSE, AbLogic::Regime::EVEN };
    auto const hold = BgLeader::Apply(Stance::HOLD, rule);
    CHECK(hold.plan == BgPlan::Plan::DEFENSE && hold.regime == AbLogic::Regime::HOLD);
    CHECK(BgLeader::Apply(Stance::PUSH, rule).regime == AbLogic::Regime::PUSH);
    CHECK(BgLeader::Apply(Stance::LAST_NODE, rule).regime == AbLogic::Regime::LAST_NODE);
    CHECK(BgLeader::Apply(Stance::PANIC, rule).regime == AbLogic::Regime::PANIC);
    auto const offense = BgLeader::Apply(Stance::OFFENSE, { BgPlan::Plan::BALANCED, AbLogic::Regime::PUSH });
    CHECK(offense.plan == BgPlan::Plan::OFFENSE && offense.regime == AbLogic::Regime::EVEN);
    // Close out is the rule's own: an answer of it changes nothing.
    auto const same = BgLeader::Apply(Stance::CLOSE_OUT, rule);
    CHECK(same.plan == rule.plan && same.regime == rule.regime);

    // The names an answered stance has, read back through StanceOf.
    for (Stance stance : { Stance::HOLD, Stance::PUSH, Stance::LAST_NODE, Stance::PANIC, Stance::BALANCED, Stance::OFFENSE, Stance::DEFENSE })
    {
        auto const applied = BgLeader::Apply(stance, rule);
        CHECK(BgLeader::StanceOf(applied.plan, applied.regime) == stance);
    }
}

TEST(ai_pvp_ab_node_targets_are_the_best_three_by_share_of_the_best)
{
    std::vector<AbLogic::TargetScore> const sorted = { { 4, 240 }, { 1, 180 }, { 3, 60 }, { 0, 10 } };
    Offer const offer = BgLeader::NodeTargetOffer(sorted);
    CHECK(offer.point == DecisionPoint::D9_NODE_TARGET);
    CHECK(offer.options.size() == 3);
    CHECK(offer.options[0].id == 4 && offer.options[0].score == 100);
    CHECK(offer.options[1].id == 1 && offer.options[1].score == 75);
    CHECK(offer.options[2].id == 3 && offer.options[2].score == 25);
    CHECK(BgLeader::IsMeaningful(offer));

    // A far second target is no real choice; one target is none; a negative best leaves the rest at zero.
    CHECK(!BgLeader::IsMeaningful(BgLeader::NodeTargetOffer({ { 4, 240 }, { 1, 30 } })));
    CHECK(!BgLeader::IsMeaningful(BgLeader::NodeTargetOffer({ { 4, 240 } })));
    CHECK(BgLeader::NodeTargetOffer({}).options.empty());
    Offer const negative = BgLeader::NodeTargetOffer({ { 2, -10 }, { 3, -40 } });
    CHECK(negative.options[0].score == 100 && negative.options[1].score == 0);
}

TEST(ai_pvp_candidate_names_are_a_fixed_vocabulary)
{
    using BgLeader::CandidateName;
    CHECK(std::string(CandidateName(DecisionPoint::D1_STANCE, static_cast<std::uint8_t>(BgLeader::Stance::ALL_IN))) == "all_in");
    CHECK(std::string(CandidateName(DecisionPoint::D1_STANCE, static_cast<std::uint8_t>(BgLeader::Stance::LAST_NODE))) == "last_node");
    CHECK(std::string(CandidateName(DecisionPoint::D6_FLOATER, static_cast<std::uint8_t>(TpLogic::FloaterJob::TO_STRIKE))) == "to_strike");
    CHECK(std::string(CandidateName(DecisionPoint::D6_FLOATER, static_cast<std::uint8_t>(TpLogic::FloaterJob::STAY_COVER))) == "stay_cover");
    CHECK(std::string(CandidateName(DecisionPoint::D7_WIPE_REPLY, static_cast<std::uint8_t>(TpLogic::WipeReply::REGROUP_LANE))) == "regroup_lane");
    CHECK(std::string(CandidateName(DecisionPoint::D4_CARRIER_LANE, static_cast<std::uint8_t>(TpLogic::CarrierLane::OTHER_LANE))) == "other_lane");
    CHECK(std::string(CandidateName(DecisionPoint::D9_NODE_TARGET, AbLogic::NODE_LUMBER_MILL)) == "lumber_mill");
    CHECK(std::string(CandidateName(DecisionPoint::D9_NODE_TARGET, 99)) == "unknown");
    CHECK(std::string(BgLeader::NodeName(AbLogic::NODE_STABLES)) == "stables");
}

// --- the leader policy's side of the hook -------------------------------------------------------------------------------------

namespace
{
BgLeader::Candidate Lead(std::uint32_t guid)
{
    BgLeader::Candidate c;
    c.guidLow = guid;
    c.tier = BotSkillMath::Tier::EXPERT;
    c.level = 85;
    c.bot = true;
    c.alive = true;
    c.present = true;
    return c;
}

BgLeader::FlagPlanState Normal() { return { BgPlan::Plan::BALANCED, WsgLogic::Situation::NORMAL }; }
}  // namespace

TEST(ai_pvp_an_accepted_stance_holds_for_its_life_and_then_the_rule_plays_again)
{
    using namespace BgLeader;
    LeaderState s;
    std::vector<Candidate> const team = { Lead(1) };
    FlagPlanState const rule = Normal();
    Tick(s, team, StanceOf(rule), StanceOffer(rule), T0, true);
    CallCommitted(s);

    Answer answer;
    answer.key = Key();
    answer.side = 0;
    answer.leaderEpoch = s.election.epoch;
    answer.stateVersion = s.stanceVersion;
    answer.point = DecisionPoint::D1_STANCE;
    answer.optionId = static_cast<std::uint8_t>(Stance::OFFENSE);
    CHECK(AcceptStanceAnswer(s, answer, Key(), 0, T0 + 1000, 40000) == Verdict::ACCEPT);
    CHECK(Effective(s) == Stance::OFFENSE);

    // The next tick: the layer's stance is a stance change of the team (the casual bots drift), and the leader owes the call.
    TickResult t = Tick(s, team, StanceOf(rule), StanceOffer(rule), T0 + 3000, true);
    CHECK(t.stance == Stance::OFFENSE);
    CHECK(t.stanceChanged);
    CHECK(t.callDue);
    CallCommitted(s);
    CHECK(!Tick(s, team, StanceOf(rule), StanceOffer(rule), T0 + 5000, true).stanceChanged);

    // It lives until T0 + 41000, then the rule plays and that is a change too.
    t = Tick(s, team, StanceOf(rule), StanceOffer(rule), T0 + 40999, true);
    CHECK(t.stance == Stance::OFFENSE && !t.stanceChanged);
    t = Tick(s, team, StanceOf(rule), StanceOffer(rule), T0 + 41000, true);
    CHECK(t.stance == Stance::BALANCED);
    CHECK(t.stanceChanged);
}

TEST(ai_pvp_the_hook_judges_against_the_list_just_published)
{
    using namespace BgLeader;
    LeaderState s;
    std::vector<Candidate> const team = { Lead(1) };
    FlagPlanState const rule = Normal();
    Tick(s, team, StanceOf(rule), StanceOffer(rule), T0, true);
    std::uint32_t const version = s.stanceVersion;
    std::uint32_t const epoch = s.election.epoch;

    // The rule's stance changes between the ask and the answer (ahead by two): the version moved on, the old answer is stale.
    FlagPlanState const lead2 = { BgPlan::Plan::BALANCED, WsgLogic::Situation::LEAD2 };
    Verdict verdict = Verdict::ACCEPT;
    TickWith(s, team, StanceOf(lead2), StanceOffer(lead2), T0 + 2000, true, [&](LeaderState& state)
    {
        Answer answer;
        answer.key = Key();
        answer.side = 0;
        answer.leaderEpoch = epoch;
        answer.stateVersion = version;
        answer.point = DecisionPoint::D1_STANCE;
        answer.optionId = static_cast<std::uint8_t>(Stance::OFFENSE);
        verdict = AcceptStanceAnswer(state, answer, Key(), 0, T0 + 2000, 40000);
    });
    CHECK(verdict == Verdict::STALE_STATE);
    CHECK(!s.layerChosen);
}

TEST(ai_pvp_an_answer_asked_of_a_leader_that_was_replaced_is_stale)
{
    using namespace BgLeader;
    LeaderState s;
    FlagPlanState const rule = Normal();
    Tick(s, { Lead(1), Lead(2) }, StanceOf(rule), StanceOffer(rule), T0, true);
    std::uint32_t const oldEpoch = s.election.epoch;
    std::uint32_t const oldLeader = s.election.leader;
    // The leader dies: the other bot follows at the next refresh, another epoch.
    std::vector<Candidate> next = { Lead(1), Lead(2) };
    for (Candidate& c : next)
        if (c.guidLow == oldLeader)
            c.alive = false;
    Tick(s, next, StanceOf(rule), StanceOffer(rule), T0 + 2000, true);
    CHECK(s.election.epoch != oldEpoch);

    Answer answer;
    answer.key = Key();
    answer.side = 0;
    answer.leaderEpoch = oldEpoch;
    answer.stateVersion = s.stanceVersion;
    answer.point = DecisionPoint::D1_STANCE;
    answer.optionId = static_cast<std::uint8_t>(Stance::OFFENSE);
    CHECK(AcceptStanceAnswer(s, answer, Key(), 0, T0 + 2500, 40000) == Verdict::STALE_LEADER);
    CHECK(!s.layerChosen);
}

// --- the whole round: ask, answer, judge, apply -------------------------------------------------------------------------------

namespace
{
struct Round
{
    BgLeader::LeaderState leader;
    SideAi ai;
    std::vector<std::string> log;    // one entry per answer: "yes" or the refusal

    Round()
    {
        BgLeader::FlagPlanState const rule = Normal();
        BgLeader::Tick(leader, { Lead(1) }, BgLeader::StanceOf(rule), BgLeader::StanceOffer(rule), T0, true);
    }

    // The side asks for the stance now; returns the request id (0 for none).
    std::uint32_t Ask(std::uint32_t nowMs)
    {
        GateInput gate = Open(nowMs);
        AskPlan const plan = PlanAsk(ai, gate, leader.offer);
        if (!plan.ask)
            return 0;
        return BeginAsk(ai, plan, leader.election.epoch, 600, nowMs);
    }

    RawAnswer Say(std::string choice, std::uint32_t requestId, double confidence = 0.8) const
    {
        RawAnswer a;
        a.key = Key();
        a.side = 0;
        a.leaderEpoch = ai.pending.leaderEpoch;
        a.requestId = requestId;
        a.stateVersion = ai.pending.stateVersion;
        a.point = ai.pending.point;
        a.transportOk = true;
        a.source = "cache";
        a.choice = std::move(choice);
        a.confidence = confidence;
        a.decisionId = "d-" + std::to_string(requestId);
        a.costUsd = 0.00002;
        return a;
    }

    void Process(std::vector<RawAnswer> const& answers, std::uint32_t nowMs, bool over = false)
    {
        ReceiveInput in;
        in.key = Key();
        in.side = 0;
        in.nowMs = nowMs;
        in.lifeMs = AnswerLifeMs(20);
        in.matchOver = over;
        ProcessAnswers(ai, leader, in, answers, [this](PendingAsk const&, RawAnswer const&, Judgement const& j)
        {
            log.push_back(j.accept ? "yes" : RejectName(j.reason));
        });
    }
};
}  // namespace

TEST(ai_pvp_round_an_accepted_stance_answer_replaces_the_rule_for_its_life)
{
    Round round;
    std::uint32_t const id = round.Ask(T0 + 2000);
    CHECK(id == 1);
    // The normal-play list: balanced (rule) first, then offense, defense. Slot 1 is the second candidate.
    CHECK(round.ai.pending.slots.slots.size() == 3);
    std::uint8_t const second = round.ai.pending.slots.slots[1].optionId;

    round.Process({ round.Say("option_1", id) }, T0 + 2400);
    CHECK(round.log == std::vector<std::string>{ "yes" });
    CHECK(round.leader.layerChosen);
    CHECK(static_cast<std::uint8_t>(round.leader.layer) == second);
    CHECK(BgLeader::Effective(round.leader) == static_cast<BgLeader::Stance>(second));
    CHECK(round.ai.accepted == 1 && round.ai.rejected == 0);
    CHECK(round.ai.acceptedIds == std::vector<std::string>{ "d-1" });
    CHECK(!round.ai.pending.active && !round.ai.pacing.inFlight);
    CHECK_NEAR(round.ai.costUsd, 0.00002, 1e-12);
}

TEST(ai_pvp_round_every_failure_leaves_the_rule_playing)
{
    struct Case
    {
        char const* name;
        std::function<void(RawAnswer&)> spoil;
        char const* reason;
    };
    std::vector<Case> const cases = {
        { "timeout", [](RawAnswer& a) { a.transportOk = false; a.error = "timeout"; }, "transport" },
        { "client busy", [](RawAnswer& a) { a.transportOk = false; a.error = "client_busy"; }, "transport" },
        { "budget", [](RawAnswer& a) { a.source = "rules_hint"; a.reason = "budget_exhausted"; a.choice.clear(); }, "source" },
        { "breaker", [](RawAnswer& a) { a.source = "rules_hint"; a.reason = "breaker_open"; a.choice.clear(); }, "source" },
        { "provider error", [](RawAnswer& a) { a.source = "error"; a.reason = "timeout"; a.choice.clear(); }, "source" },
        { "no choice", [](RawAnswer& a) { a.choice.clear(); }, "no_choice" },
        { "unknown slot", [](RawAnswer& a) { a.choice = "option_5"; }, "unknown_slot" },
        { "words", [](RawAnswer& a) { a.choice = "offense"; }, "unknown_slot" },
        { "low confidence", [](RawAnswer& a) { a.confidence = 0.3; }, "low_confidence" },
        { "no confidence", [](RawAnswer& a) { a.confidence = -1.0; }, "low_confidence" },
        { "other match", [](RawAnswer& a) { a.key = { 7, 9 }; }, "stale_match" },
        { "other side", [](RawAnswer& a) { a.side = 1; }, "wrong_side" },
        { "other leader", [](RawAnswer& a) { a.leaderEpoch += 1; }, "stale_leader" },
        { "old state", [](RawAnswer& a) { a.stateVersion += 1; }, "stale_state" },
        { "other point", [](RawAnswer& a) { a.point = DecisionPoint::D7_WIPE_REPLY; }, "wrong_point" },
    };
    for (Case const& c : cases)
    {
        Round round;
        std::uint32_t const id = round.Ask(T0 + 2000);
        RawAnswer answer = round.Say("option_1", id);
        c.spoil(answer);
        // An answer of the other side would not even be taken from the queue; the judging still refuses it.
        round.Process({ answer }, T0 + 2400);
        CHECK(round.log.size() == 1);
        CHECK(!round.log.empty() && round.log[0] == c.reason);
        CHECK(!round.leader.layerChosen);
        CHECK(BgLeader::Effective(round.leader) == BgLeader::Stance::BALANCED);
        CHECK(round.ai.accepted == 0 && round.ai.rejected == 1);
        // The request is over whatever the answer was: the side may ask again at its next interval.
        CHECK(!round.ai.pacing.inFlight);
    }
}

TEST(ai_pvp_round_a_late_answer_and_a_handover_are_discarded)
{
    Round round;
    std::uint32_t id = round.Ask(T0 + 2000);
    round.Process({ round.Say("option_1", id) }, T0 + 2000 + 4601);
    CHECK(round.log.back() == "late");
    CHECK(!round.leader.layerChosen);

    // A new leader: the request of the old one is dropped, its answer is another request's.
    Round other;
    id = other.Ask(T0 + 2000);
    RawAnswer const answer = other.Say("option_1", id);
    other.ai.lastLeaderEpoch = other.leader.election.epoch;
    other.leader.election.epoch += 1;
    other.Process({ answer }, T0 + 2400);
    CHECK(other.log.back() == "stale_request");
    CHECK(!other.leader.layerChosen);
    CHECK(!other.ai.pacing.inFlight);
}

TEST(ai_pvp_round_the_answer_of_a_lost_request_does_not_free_the_one_in_flight)
{
    Round round;
    std::uint32_t const first = round.Ask(T0 + 2000);
    // The first request was never answered and was let go; the next ask went out.
    round.ai.pacing.inFlight = false;
    round.ai.pending.active = false;
    std::uint32_t const second = round.Ask(T0 + 30000);
    CHECK(second == first + 1);

    RawAnswer lateOne = round.Say("option_1", first);
    round.Process({ lateOne }, T0 + 30300);
    CHECK(round.log.back() == "stale_request");
    CHECK(round.ai.pending.active && round.ai.pacing.inFlight);   // the second is still awaited

    round.Process({ round.Say("option_1", second) }, T0 + 30500);
    CHECK(round.log.back() == "yes");
    CHECK(!round.ai.pending.active);
}

TEST(ai_pvp_round_a_match_over_discards_every_answer)
{
    Round round;
    std::uint32_t const id = round.Ask(T0 + 2000);
    round.Process({ round.Say("option_1", id) }, T0 + 2400, true);
    CHECK(round.log == std::vector<std::string>{ "match_over" });
    CHECK(!round.leader.layerChosen);
}

TEST(ai_pvp_round_the_book_takes_a_wipe_reply_and_keeps_it_for_its_life)
{
    Round round;
    Publish(round.ai.book, List(DecisionPoint::D7_WIPE_REPLY, 0, { { 2, 100 }, { 0, 70 }, { 1, 30 } }), { "our_attack_wiped:yes" }, true, T0 + 1000);
    std::uint32_t const id = round.Ask(T0 + 2000);
    CHECK(round.ai.pending.point == DecisionPoint::D7_WIPE_REPLY);

    round.Process({ round.Say("option_1", id) }, T0 + 2400);
    CHECK(round.log.back() == "yes");
    CHECK(Override(round.ai.book, DecisionPoint::D7_WIPE_REPLY, T0 + 2500) == std::optional<std::uint8_t>(0));
    CHECK(!Override(round.ai.book, DecisionPoint::D7_WIPE_REPLY, T0 + 2400 + AnswerLifeMs(20)));
    // The stance was not touched.
    CHECK(!round.leader.layerChosen);
}

TEST(ai_pvp_round_the_book_refuses_an_answer_for_a_list_that_changed)
{
    Round round;
    Publish(round.ai.book, List(DecisionPoint::D6_FLOATER, 0, { { 1, 95 }, { 2, 60 }, { 0, 20 } }), {}, true, T0 + 1000);
    std::uint32_t const id = round.Ask(T0 + 2000);
    CHECK(round.ai.pending.point == DecisionPoint::D6_FLOATER);

    // The wipe is over before the answer comes: the rule's first candidate changed, a new version.
    Publish(round.ai.book, List(DecisionPoint::D6_FLOATER, 0, { { 2, 60 }, { 1, 10 }, { 0, 20 } }), {}, true, T0 + 2200);
    round.Process({ round.Say("option_1", id) }, T0 + 2400);
    CHECK(round.log.back() == "stale_state");
    CHECK(!Override(round.ai.book, DecisionPoint::D6_FLOATER, T0 + 2500));

    // The list is withdrawn: nothing to apply the answer to.
    Round gone;
    Publish(gone.ai.book, List(DecisionPoint::D6_FLOATER, 0, { { 1, 95 }, { 2, 60 } }), {}, true, T0 + 1000);
    std::uint32_t const id2 = gone.Ask(T0 + 2000);
    Withdraw(gone.ai.book, DecisionPoint::D6_FLOATER);
    gone.Process({ gone.Say("option_1", id2) }, T0 + 2400);
    CHECK(gone.log.back() == "not_offered");
}
