/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_AIPVPDECISIONPOLICY_H
#define _PLAYERBOT_AIPVPDECISIONPOLICY_H

// BT-501: the pure part of the battleground strategy asks (design decision 0032 point 9, pvp-system ARCHITECTURE section 3). The
// leader of a side may ask the decision layer which of a few legal candidates the team should play; the deterministic plan
// is published first and keeps playing, an answer only replaces it when it is timely, offered, still legal and sure enough.
//
//  - Slots: the candidates of a decision point travel as FIXED slots (option_0 to option_5, best rule score first). The map
//    thread keeps the slot table of the request; the answer names a slot, never an id, a GUID or a coordinate.
//  - Pacing: one request in flight per side, a routine ask every interval (15 to 30 s) and an ask after a flag or node event,
//    never more than once in 5 s, at most N asks per side and match, and only when two materially different legal options exist.
//  - Judging an answer: transport, source, slot, age, then the leader policy's own check (match, side, leader epoch, state
//    version, decision point, offered id), then the confidence. Every refusal has a reason that the log prints.
//  - The book: the decision points the leader policy does not hold itself (carrier lane, floater, wipe reply, node target)
//    with their versions and the answer that holds for a while.
//
// Free of game types so that tests/ can run it (tests/test_ai_pvp_decision.cpp). The sidecar link, the log and the plans'
// wiring are src/AwareAi/AiPvpDecision.{h,cpp}.

#include "BgLeaderPolicy.h"
#include "PvpObservationMath.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace AiPvp
{
using BgLeader::DecisionPoint;
using BgLeader::Offer;
using BgLeader::Option;

// ---------------------------------------------------------------------------------------------------------------------
// Constants (docs: AwareAi.Pvp.* in conf/playerbots.conf.dist).
// ---------------------------------------------------------------------------------------------------------------------
constexpr std::uint32_t INTERVAL_MIN_S = 15;
constexpr std::uint32_t INTERVAL_MAX_S = 30;
constexpr std::uint32_t INTERVAL_DEFAULT_S = 20;
// After a flag or node event the leader may ask again this soon after its last ask.
constexpr std::uint32_t EVENT_MIN_GAP_MS = 5000;
constexpr std::uint32_t MIN_CONFIDENCE_DEFAULT_PCT = 60;
constexpr std::uint32_t MAX_REQUESTS_DEFAULT = 60;
// The sidecar's template knows this many slots, option_0 to option_5.
constexpr std::size_t MAX_SLOTS = 6;
// An answer that arrives later than the deadline plus this is old news: the sidecar's deadline is its own, the rest is the
// wait in the queue until the map thread's next refresh and the HTTP margin.
constexpr std::uint32_t ANSWER_LATE_SLACK_MS = 4000;
// A request that has not come back this long after its deadline is counted lost and the side may ask again; its answer, if it
// ever arrives, is refused as another request's.
constexpr std::uint32_t LOST_SLACK_MS = 3000;
// How many accepted decisions a side remembers for the outcome at the match end.
constexpr std::size_t MAX_REMEMBERED = 16;
// The answers queued for the map thread per match (the IO thread drops the rest); a side asks one at a time, so a few suffice.
constexpr std::size_t MAX_QUEUED_ANSWERS = 16;

inline std::uint32_t ClampIntervalS(std::uint32_t seconds)
{
    return std::clamp(seconds, INTERVAL_MIN_S, INTERVAL_MAX_S);
}

// An accepted answer holds for two intervals: long enough to bridge one lost or refused ask, short enough that a dead sidecar
// leaves the rules playing within a minute.
inline std::uint32_t AnswerLifeMs(std::uint32_t intervalS)
{
    return 2u * ClampIntervalS(intervalS) * 1000u;
}

// ---------------------------------------------------------------------------------------------------------------------
// Slots.
// ---------------------------------------------------------------------------------------------------------------------
inline std::string SlotName(std::size_t index)
{
    return "option_" + std::to_string(index);
}

// The index of a slot name, or nothing for any other text (the sidecar answers with one of the names it was sent, but the
// answer is data from outside: only the exact names count).
inline std::optional<std::size_t> SlotIndex(std::string const& name)
{
    static constexpr char PREFIX[] = "option_";
    constexpr std::size_t PREFIX_LEN = sizeof(PREFIX) - 1;
    if (name.size() != PREFIX_LEN + 1 || name.compare(0, PREFIX_LEN, PREFIX) != 0)
        return std::nullopt;
    char const digit = name[PREFIX_LEN];
    if (digit < '0' || digit >= static_cast<char>('0' + MAX_SLOTS))
        return std::nullopt;
    return static_cast<std::size_t>(digit - '0');
}

struct Slot
{
    std::uint8_t optionId = 0;
    int score = 0;
};

// The slot table of one request: slot i holds the i-th option of the offer (best rule score first). Map thread only.
struct SlotTable
{
    std::vector<Slot> slots;

    std::optional<std::uint8_t> OptionOf(std::string const& name) const
    {
        std::optional<std::size_t> const index = SlotIndex(name);
        if (!index || *index >= slots.size())
            return std::nullopt;
        return slots[*index].optionId;
    }
};

inline SlotTable BuildSlots(Offer const& offer)
{
    SlotTable table;
    for (Option const& o : offer.options)
    {
        if (table.slots.size() >= MAX_SLOTS)
            break;
        table.slots.push_back({ o.id, o.score });
    }
    return table;
}

// ---------------------------------------------------------------------------------------------------------------------
// Decision points: which are asked, in which order, and how often.
// ---------------------------------------------------------------------------------------------------------------------
// Only the points whose answer something executes are asked. The carrier lane (D4) has its list but no carrier path set to
// run the other lane yet (twin-peaks.md section 13), so an answer would change nothing: it is not asked. The arena points (D10,
// D11) are the hook of the arena step: they ask nothing until the arena plan offers and applies them.
inline bool IsWired(DecisionPoint point)
{
    switch (point)
    {
        case DecisionPoint::D1_STANCE:
        case DecisionPoint::D6_FLOATER:
        case DecisionPoint::D7_WIPE_REPLY:
        case DecisionPoint::D9_NODE_TARGET:
        case DecisionPoint::D13_BFG_FIGHT:
        case DecisionPoint::D14_BFG_STAGE:
        case DecisionPoint::D15_EYE_FLAG:
        case DecisionPoint::D16_EYE_CARRIER:
        case DecisionPoint::D17_EYE_EFC:
        case DecisionPoint::D18_SM_CART:
        case DecisionPoint::D19_SM_SWITCH:
            return true;
        default:
            break;
    }
    return false;
}

// Higher is asked first when several points have a real choice at the same time: a wiped attack needs its reply before the
// stance matters again.
inline int PointPriority(DecisionPoint point)
{
    switch (point)
    {
        case DecisionPoint::D7_WIPE_REPLY: return 50;
        case DecisionPoint::D19_SM_SWITCH: return 49;
        case DecisionPoint::D16_EYE_CARRIER: return 48;
        case DecisionPoint::D17_EYE_EFC: return 47;
        case DecisionPoint::D13_BFG_FIGHT: return 45;
        case DecisionPoint::D18_SM_CART: return 44;
        case DecisionPoint::D15_EYE_FLAG: return 35;
        case DecisionPoint::D6_FLOATER: return 40;
        case DecisionPoint::D14_BFG_STAGE: return 30;
        case DecisionPoint::D9_NODE_TARGET: return 20;
        case DecisionPoint::D1_STANCE: return 10;
        default: break;
    }
    return 0;
}

// The stance and the target node follow the clock (the ground and the scores change under them): they are asked again every
// interval. The wipe reply and the floater job are asked once per state of their list.
inline bool IsRepeatable(DecisionPoint point)
{
    return point == DecisionPoint::D1_STANCE || point == DecisionPoint::D9_NODE_TARGET || point == DecisionPoint::D13_BFG_FIGHT ||
           point == DecisionPoint::D16_EYE_CARRIER || point == DecisionPoint::D18_SM_CART;
}

inline char const* PointName(DecisionPoint point)
{
    switch (point)
    {
        case DecisionPoint::D1_STANCE: return "stance";
        case DecisionPoint::D2_PICKUP: return "pickup";
        case DecisionPoint::D3_ROUTE: return "route";
        case DecisionPoint::D4_CARRIER_LANE: return "carrier_lane";
        case DecisionPoint::D5_HUNT_SIZE: return "hunt_size";
        case DecisionPoint::D6_FLOATER: return "floater";
        case DecisionPoint::D7_WIPE_REPLY: return "wipe_reply";
        case DecisionPoint::D8_STALEMATE: return "stalemate";
        case DecisionPoint::D9_NODE_TARGET: return "target_node";
        case DecisionPoint::D10_ARENA_KILL_TARGET: return "arena_kill_target";
        case DecisionPoint::D11_ARENA_CC_TARGET: return "arena_cc_target";
        case DecisionPoint::D12_BFG_ROUTE: return "bfg_route";
        case DecisionPoint::D13_BFG_FIGHT: return "bfg_fight";
        case DecisionPoint::D14_BFG_STAGE: return "bfg_stage";
        case DecisionPoint::D15_EYE_FLAG: return "eye_flag";
        case DecisionPoint::D16_EYE_CARRIER: return "eye_carrier";
        case DecisionPoint::D17_EYE_EFC: return "eye_efc";
        case DecisionPoint::D18_SM_CART: return "sm_cart";
        case DecisionPoint::D19_SM_SWITCH: return "sm_switch";
    }
    return "unknown";
}

// The question the model reads for a point, in one line.
inline char const* PointQuestion(DecisionPoint point)
{
    switch (point)
    {
        case DecisionPoint::D1_STANCE: return "Which stance should our team play for the next half minute?";
        case DecisionPoint::D4_CARRIER_LANE: return "Which lane should our flag carrier run home by?";
        case DecisionPoint::D6_FLOATER: return "Where should our spare healer go now?";
        case DecisionPoint::D7_WIPE_REPLY: return "How should our wiped attack group act now that it is back?";
        case DecisionPoint::D9_NODE_TARGET: return "Which node should our assault group take next?";
        case DecisionPoint::D12_BFG_ROUTE: return "Which way should our main group take to the central node?";
        case DecisionPoint::D13_BFG_FIGHT: return "Our group is in a fight at a node: fight on, fall back, send two stealth players to ink their base, or reinforce?";
        case DecisionPoint::D14_BFG_STAGE: return "At which node should our reserve wait now?";
        case DecisionPoint::D15_EYE_FLAG: return "The flag in the middle is free: take it now, wait for a clear stand, take it only to deny, or ignore it?";
        case DecisionPoint::D16_EYE_CARRIER: return "Which of our bases should our flag carrier run to?";
        case DecisionPoint::D17_EYE_EFC: return "The enemy carries the flag: hunt it with everybody, two hunt and two cut it off, neutralise the base it runs to, or ignore it?";
        case DecisionPoint::D18_SM_CART: return "Which mine cart should our main group contest now?";
        case DecisionPoint::D19_SM_SWITCH: return "Our switch holder stands at a track switch that is set the wrong way for us: click now, wait for the last moment before the cart passes, or leave?";
        default: break;
    }
    return "Which option is best for our team now?";
}

// ---------------------------------------------------------------------------------------------------------------------
// The book: the points the leader policy does not hold itself. Each entry keeps the list the plan published this refresh, its
// version and the answer that holds for a while. Map thread only.
// ---------------------------------------------------------------------------------------------------------------------
constexpr std::size_t BOOK_SIZE = 21;   // indexed by the point's number (1 to 19)

struct BookEntry
{
    bool published = false;
    Offer offer;
    std::uint32_t version = 0;
    std::uint64_t signature = 0;
    std::vector<std::string> facts;   // short "key:value" lines the request carries for this point
    bool chosen = false;
    std::uint8_t chosenId = 0;
    std::uint32_t untilMs = 0;
    std::uint32_t askedVersion = 0;   // the version last asked about (0 = never)
};

struct Book
{
    std::array<BookEntry, BOOK_SIZE> entries;

    BookEntry& At(DecisionPoint point) { return entries[static_cast<std::size_t>(point) % BOOK_SIZE]; }
    BookEntry const& At(DecisionPoint point) const { return entries[static_cast<std::size_t>(point) % BOOK_SIZE]; }
};

// What identifies a list: its option ids and, when the rule's own answer matters, which one comes first. A list that keeps
// its ids and its first option is the same question; another one is a new version and the old answers are stale.
inline std::uint64_t ListSignature(Offer const& offer, bool ruleFirstMatters)
{
    std::vector<std::uint8_t> ids;
    for (Option const& o : offer.options)
        ids.push_back(o.id);
    std::sort(ids.begin(), ids.end());
    std::uint64_t h = 1469598103934665603ull;
    auto const mix = [&h](std::uint64_t v) { h = (h ^ v) * 1099511628211ull; };
    for (std::uint8_t id : ids)
        mix(id);
    mix(ruleFirstMatters ? 0x100u + offer.Fallback() : 0u);
    return h;
}

// The plan publishes the list of a point at every refresh. A new list is a new version and drops the answer of the old one.
inline void Publish(Book& book, Offer offer, std::vector<std::string> facts, bool ruleFirstMatters, std::uint32_t nowMs)
{
    BookEntry& e = book.At(offer.point);
    std::uint64_t const signature = ListSignature(offer, ruleFirstMatters);
    if (!e.published || signature != e.signature)
    {
        ++e.version;
        e.signature = signature;
        e.chosen = false;
    }
    if (e.chosen && PvpObs::Reached(nowMs, e.untilMs))
        e.chosen = false;
    offer.stateVersion = e.version;
    e.offer = std::move(offer);
    e.facts = std::move(facts);
    e.published = true;
}

// The point has no list now (the plan is not in the state that asks it): nothing to ask, nothing to apply.
inline void Withdraw(Book& book, DecisionPoint point)
{
    BookEntry& e = book.At(point);
    e.published = false;
    e.chosen = false;
}

// The answer that stands for the point now: chosen, not expired and still one of the options of the published list.
inline std::optional<std::uint8_t> Override(Book const& book, DecisionPoint point, std::uint32_t nowMs)
{
    BookEntry const& e = book.At(point);
    if (!e.published || !e.chosen || PvpObs::Reached(nowMs, e.untilMs))
        return std::nullopt;
    for (Option const& o : e.offer.options)
        if (o.id == e.chosenId)
            return e.chosenId;
    return std::nullopt;
}

inline void SetChoice(Book& book, DecisionPoint point, std::uint8_t optionId, std::uint32_t nowMs, std::uint32_t lifeMs)
{
    BookEntry& e = book.At(point);
    e.chosen = true;
    e.chosenId = optionId;
    e.untilMs = PvpObs::Stamp(nowMs + lifeMs);
}

// ---------------------------------------------------------------------------------------------------------------------
// Pacing.
// ---------------------------------------------------------------------------------------------------------------------
struct Pacing
{
    bool inFlight = false;
    std::uint32_t requestId = 0;      // the id of the last request sent (counts up from 1)
    std::uint32_t sentMs = 0;
    std::uint32_t lastAskMs = 0;      // when the last request went out (0 = never)
    std::uint32_t requests = 0;       // sent in this match by this side
    bool eventPending = false;
    std::uint32_t eventMs = 0;
};

enum class Gate : std::uint8_t
{
    OPEN,
    OFF,              // the layer, the feature or the link is off
    NOT_STARTED,      // the gates are not open
    MATCH_OVER,
    NO_LEADER,
    IN_FLIGHT,
    CAP,              // the per-match request cap is reached
    NOT_DUE,          // the interval has not passed and no event waits
    TOO_SOON,         // an event waits but the last ask was less than 5 s ago
    NO_CHOICE         // due, but no point has two materially different legal options
};

inline char const* GateName(Gate gate)
{
    switch (gate)
    {
        case Gate::OPEN: return "open";
        case Gate::OFF: return "off";
        case Gate::NOT_STARTED: return "not_started";
        case Gate::MATCH_OVER: return "match_over";
        case Gate::NO_LEADER: return "no_leader";
        case Gate::IN_FLIGHT: return "in_flight";
        case Gate::CAP: return "cap";
        case Gate::NOT_DUE: return "not_due";
        case Gate::TOO_SOON: return "too_soon";
        case Gate::NO_CHOICE: return "no_choice";
    }
    return "off";
}

struct GateInput
{
    bool enabled = false;
    bool started = false;
    bool matchOver = false;
    bool leaderPresent = false;
    std::uint32_t nowMs = 0;
    std::uint32_t intervalMs = INTERVAL_DEFAULT_S * 1000u;
    std::uint32_t deadlineMs = 600;
    std::uint32_t maxRequests = MAX_REQUESTS_DEFAULT;
};

inline bool RequestLost(Pacing const& p, GateInput const& in)
{
    return p.inFlight && PvpObs::Reached(in.nowMs, p.sentMs + in.deadlineMs + ANSWER_LATE_SLACK_MS + LOST_SLACK_MS);
}

// Whether a request may go out now, before the question of what to ask. A request that never came back is let go of here.
inline Gate AskGate(Pacing& p, GateInput const& in)
{
    if (!in.enabled)
        return Gate::OFF;
    if (in.matchOver)
        return Gate::MATCH_OVER;
    if (!in.started)
        return Gate::NOT_STARTED;
    if (!in.leaderPresent)
        return Gate::NO_LEADER;
    if (RequestLost(p, in))
        p.inFlight = false;
    // An event that waited a whole interval is old news: the routine ask covers it.
    if (p.eventPending && PvpObs::Reached(in.nowMs, p.eventMs + in.intervalMs))
        p.eventPending = false;
    if (p.inFlight)
        return Gate::IN_FLIGHT;
    if (p.requests >= in.maxRequests)
        return Gate::CAP;

    bool const never = p.lastAskMs == 0;
    bool const routineDue = never || PvpObs::Reached(in.nowMs, p.lastAskMs + in.intervalMs);
    if (routineDue)
        return Gate::OPEN;
    if (!p.eventPending)
        return Gate::NOT_DUE;
    return PvpObs::Reached(in.nowMs, p.lastAskMs + EVENT_MIN_GAP_MS) ? Gate::OPEN : Gate::TOO_SOON;
}

// A flag or node event happened (the plan saw its signature change).
inline void NoteEvent(Pacing& p, std::uint32_t nowMs)
{
    if (!p.eventPending)
    {
        p.eventPending = true;
        p.eventMs = PvpObs::Stamp(nowMs);
    }
}

inline std::uint32_t NoteSent(Pacing& p, std::uint32_t nowMs)
{
    p.inFlight = true;
    p.sentMs = PvpObs::Stamp(nowMs);
    p.lastAskMs = PvpObs::Stamp(nowMs);
    p.eventPending = false;
    ++p.requests;
    return ++p.requestId;
}

// ---------------------------------------------------------------------------------------------------------------------
// What to ask: the point with a real choice and the highest priority.
// ---------------------------------------------------------------------------------------------------------------------
struct AskCandidate
{
    DecisionPoint point = DecisionPoint::D1_STANCE;
    Offer const* offer = nullptr;
    std::uint32_t askedVersion = 0;
};

// The index of the wired point that has two materially different legal options (IsMeaningful), was not asked for its current
// version unless it is a repeating point, and has the highest priority; nothing for none.
inline std::optional<std::size_t> ChoosePoint(std::vector<AskCandidate> const& candidates)
{
    std::optional<std::size_t> best;
    for (std::size_t i = 0; i < candidates.size(); ++i)
    {
        AskCandidate const& c = candidates[i];
        if (!c.offer || !IsWired(c.point) || !BgLeader::IsMeaningful(*c.offer))
            continue;
        if (!IsRepeatable(c.point) && c.askedVersion == c.offer->stateVersion)
            continue;
        if (!best || PointPriority(c.point) > PointPriority(candidates[*best].point))
            best = i;
    }
    return best;
}

// ---------------------------------------------------------------------------------------------------------------------
// The request in flight and the answer that comes back.
// ---------------------------------------------------------------------------------------------------------------------
struct PendingAsk
{
    bool active = false;
    std::uint32_t requestId = 0;
    DecisionPoint point = DecisionPoint::D1_STANCE;
    std::uint32_t stateVersion = 0;
    std::uint32_t leaderEpoch = 0;
    std::uint32_t sentMs = 0;
    std::uint32_t deadlineMs = 0;
    SlotTable slots;
};

// What the IO thread queues for the map thread: the sidecar's reply (or why there is none) tagged with what was asked. The
// IO callback does no judging and touches no game object.
struct RawAnswer
{
    PvpObs::MatchKey key;
    std::uint8_t side = 0;
    std::uint32_t leaderEpoch = 0;
    std::uint32_t requestId = 0;
    std::uint32_t stateVersion = 0;
    DecisionPoint point = DecisionPoint::D1_STANCE;
    bool transportOk = false;
    std::string error;           // the client's reason when there is no reply (timeout, client_busy, http_500, ...)
    std::string source;
    std::string reason;          // the sidecar's reason of a rules hint
    std::string choice;
    std::string decisionId;
    double confidence = -1.0;
    std::uint32_t latencyMs = 0;
    double costUsd = 0.0;
};

// The answers the sidecar gives for a decision are the decision model's, a cached one or (development) a fake one. A rules
// hint, an error and the sidecar's own rules carry no choice to follow. The model's source name is not spelled here: any
// source that is not one of these refusals counts.
inline bool SourceUsable(std::string const& source)
{
    return !(source.empty() || source == "rules_hint" || source == "error" || source == "rules");
}

enum class Reject : std::uint8_t
{
    NONE,
    STALE_REQUEST,     // not the request that is in flight (a lost one that came back late, a request of a handed over leader)
    MATCH_OVER,
    TRANSPORT,         // no reply: timeout, link down, client busy, http error
    SOURCE,            // a rules hint or an error from the sidecar
    NO_CHOICE,
    UNKNOWN_SLOT,
    LATE,
    STALE_MATCH,
    WRONG_SIDE,
    STALE_LEADER,
    STALE_STATE,
    WRONG_POINT,
    NOT_OFFERED,       // the answer's option is no longer in the list the plan publishes (not legal now)
    LOW_CONFIDENCE
};

inline char const* RejectName(Reject reason)
{
    switch (reason)
    {
        case Reject::NONE: return "ok";
        case Reject::STALE_REQUEST: return "stale_request";
        case Reject::MATCH_OVER: return "match_over";
        case Reject::TRANSPORT: return "transport";
        case Reject::SOURCE: return "source";
        case Reject::NO_CHOICE: return "no_choice";
        case Reject::UNKNOWN_SLOT: return "unknown_slot";
        case Reject::LATE: return "late";
        case Reject::STALE_MATCH: return "stale_match";
        case Reject::WRONG_SIDE: return "wrong_side";
        case Reject::STALE_LEADER: return "stale_leader";
        case Reject::STALE_STATE: return "stale_state";
        case Reject::WRONG_POINT: return "wrong_point";
        case Reject::NOT_OFFERED: return "not_offered";
        case Reject::LOW_CONFIDENCE: return "low_confidence";
    }
    return "unknown";
}

struct Judgement
{
    bool accept = false;
    Reject reason = Reject::NONE;
    std::uint8_t optionId = 0;
};

struct JudgeContext
{
    std::uint32_t nowMs = 0;
    std::uint32_t minConfidencePct = MIN_CONFIDENCE_DEFAULT_PCT;
    bool matchOver = false;
    PvpObs::MatchKey key;
    std::uint8_t side = 0;
    std::uint32_t leaderEpoch = 0;
    Offer const* current = nullptr;   // the list of the answer's point as the plan publishes it now
};

inline Reject RejectOf(BgLeader::Verdict verdict)
{
    switch (verdict)
    {
        case BgLeader::Verdict::ACCEPT: return Reject::NONE;
        case BgLeader::Verdict::STALE_MATCH: return Reject::STALE_MATCH;
        case BgLeader::Verdict::WRONG_SIDE: return Reject::WRONG_SIDE;
        case BgLeader::Verdict::STALE_LEADER: return Reject::STALE_LEADER;
        case BgLeader::Verdict::STALE_STATE: return Reject::STALE_STATE;
        case BgLeader::Verdict::WRONG_POINT: return Reject::WRONG_POINT;
        case BgLeader::Verdict::UNKNOWN_OPTION: return Reject::NOT_OFFERED;
    }
    return Reject::NOT_OFFERED;
}

// Judges one queued answer against the request in flight and the plan's list as it stands now. Accepts only an answer that is
// for this request, from a usable source, names an offered slot, is timely, passes the leader policy's stale checks (match,
// side, leader epoch, state version, point, option still offered) and has the confidence.
inline Judgement Judge(PendingAsk const& pending, RawAnswer const& answer, JudgeContext const& ctx)
{
    auto const no = [](Reject reason) { return Judgement{ false, reason, 0 }; };

    if (ctx.matchOver)
        return no(Reject::MATCH_OVER);
    if (!pending.active || pending.requestId != answer.requestId)
        return no(Reject::STALE_REQUEST);
    if (!answer.transportOk)
        return no(Reject::TRANSPORT);
    if (!SourceUsable(answer.source))
        return no(Reject::SOURCE);
    if (answer.choice.empty())
        return no(Reject::NO_CHOICE);
    std::optional<std::uint8_t> const optionId = pending.slots.OptionOf(answer.choice);
    if (!optionId)
        return no(Reject::UNKNOWN_SLOT);
    if (static_cast<std::int32_t>(ctx.nowMs - pending.sentMs) > static_cast<std::int32_t>(pending.deadlineMs + ANSWER_LATE_SLACK_MS))
        return no(Reject::LATE);

    if (!ctx.current)
        return no(Reject::NOT_OFFERED);
    BgLeader::Answer leaderAnswer;
    leaderAnswer.key = answer.key;
    leaderAnswer.side = answer.side;
    leaderAnswer.leaderEpoch = answer.leaderEpoch;
    leaderAnswer.stateVersion = answer.stateVersion;
    leaderAnswer.point = answer.point;
    leaderAnswer.optionId = *optionId;
    Reject const stale = RejectOf(BgLeader::CheckAnswer(*ctx.current, leaderAnswer, ctx.key, ctx.side, ctx.leaderEpoch));
    if (stale != Reject::NONE)
        return no(stale);

    // A missing confidence (the sidecar reports none) is as good as none: the rule keeps playing.
    if (answer.confidence < 0.0 || answer.confidence * 100.0 + 1e-9 < static_cast<double>(ctx.minConfidencePct))
        return no(Reject::LOW_CONFIDENCE);
    return { true, Reject::NONE, *optionId };
}

// ---------------------------------------------------------------------------------------------------------------------
// One side's state: pacing, the request in flight, the book and the accounts. Map thread only.
// ---------------------------------------------------------------------------------------------------------------------
struct SideAi
{
    Pacing pacing;
    PendingAsk pending;
    Book book;
    bool eventKnown = false;
    std::uint64_t lastEventSignature = 0;
    std::uint32_t lastLeaderEpoch = 0;
    bool capLogged = false;

    std::uint32_t accepted = 0;
    std::uint32_t rejected = 0;
    std::uint32_t refused = 0;        // asks the client could not take (busy, link down)
    double costUsd = 0.0;
    std::vector<std::string> acceptedIds;   // decision ids of accepted answers, for the outcome at the match end
    bool summarised = false;
};

// The plan's signature of flags (or nodes) and scores changed: a flag or node event. The first reading is the baseline.
inline void ObserveEvents(SideAi& ai, std::uint64_t signature, std::uint32_t nowMs)
{
    if (ai.eventKnown && signature != ai.lastEventSignature)
        NoteEvent(ai.pacing, nowMs);
    ai.lastEventSignature = signature;
    ai.eventKnown = true;
}

// A new leader: the request of the old one is no longer awaited (its answer is refused as another request's). The answers
// already accepted stay until they expire, so a handover does not flip the plan back at once.
inline void OnLeaderEpoch(SideAi& ai, std::uint32_t epoch)
{
    if (epoch == ai.lastLeaderEpoch)
        return;
    ai.lastLeaderEpoch = epoch;
    ai.pending.active = false;
    ai.pacing.inFlight = false;
}

// The match is over: nothing is awaited and nothing stands.
inline void OnMatchOver(SideAi& ai)
{
    ai.pending.active = false;
    ai.pacing.inFlight = false;
    for (BookEntry& e : ai.book.entries)
        e.chosen = false;
}

inline void Remember(SideAi& ai, std::string const& decisionId)
{
    if (decisionId.empty())
        return;
    if (ai.acceptedIds.size() >= MAX_REMEMBERED)
        ai.acceptedIds.erase(ai.acceptedIds.begin());
    ai.acceptedIds.push_back(decisionId);
}

// What to ask now, if anything.
struct AskPlan
{
    Gate gate = Gate::OFF;
    bool ask = false;
    DecisionPoint point = DecisionPoint::D1_STANCE;
    std::uint32_t stateVersion = 0;
    SlotTable slots;
};

// The pacing gate and then the point. `stance` is the leader policy's D1 list (the book holds the others).
inline AskPlan PlanAsk(SideAi& ai, GateInput const& in, Offer const& stance)
{
    AskPlan plan;
    plan.gate = AskGate(ai.pacing, in);
    if (plan.gate != Gate::OPEN)
        return plan;

    std::vector<AskCandidate> candidates;
    candidates.push_back({ DecisionPoint::D1_STANCE, &stance, 0 });
    for (std::size_t i = 0; i < BOOK_SIZE; ++i)
    {
        BookEntry const& e = ai.book.entries[i];
        if (e.published)
            candidates.push_back({ e.offer.point, &e.offer, e.askedVersion });
    }

    std::optional<std::size_t> const index = ChoosePoint(candidates);
    if (!index)
    {
        plan.gate = Gate::NO_CHOICE;
        return plan;
    }
    AskCandidate const& chosen = candidates[*index];
    plan.ask = true;
    plan.point = chosen.point;
    plan.stateVersion = chosen.offer->stateVersion;
    plan.slots = BuildSlots(*chosen.offer);
    return plan;
}

// The request was handed to the client: it is now awaited. The point's version is marked as asked, so a once-per-version point
// is not asked again for the same list.
inline std::uint32_t BeginAsk(SideAi& ai, AskPlan const& plan, std::uint32_t leaderEpoch, std::uint32_t deadlineMs,
                              std::uint32_t nowMs)
{
    std::uint32_t const id = NoteSent(ai.pacing, nowMs);
    ai.pending.active = true;
    ai.pending.requestId = id;
    ai.pending.point = plan.point;
    ai.pending.stateVersion = plan.stateVersion;
    ai.pending.leaderEpoch = leaderEpoch;
    ai.lastLeaderEpoch = leaderEpoch;   // the epoch this request was made under is the one a later change is measured from
    ai.pending.sentMs = PvpObs::Stamp(nowMs);
    ai.pending.deadlineMs = deadlineMs;
    ai.pending.slots = plan.slots;
    if (plan.point != DecisionPoint::D1_STANCE)
        ai.book.At(plan.point).askedVersion = plan.stateVersion;
    return id;
}

// The client could not take the request (busy, link down): nothing is awaited and the ask is not counted. The interval is
// restarted so the refusal is not retried every refresh.
inline void AskRefused(SideAi& ai, std::uint32_t nowMs)
{
    ai.pacing.lastAskMs = PvpObs::Stamp(nowMs);
    ++ai.refused;
}

// The answer of the request in flight came in (whatever it says): it is no longer awaited.
inline void Finish(SideAi& ai, std::uint32_t requestId)
{
    if (ai.pending.active && ai.pending.requestId == requestId)
    {
        ai.pending.active = false;
        ai.pacing.inFlight = false;
    }
}

// The list of the answer's point as the plan publishes it now, nullptr when it publishes none.
inline Offer const* CurrentOffer(BgLeader::LeaderState const& leader, SideAi const& ai, DecisionPoint point)
{
    if (point == DecisionPoint::D1_STANCE)
        return &leader.offer;

    BookEntry const& entry = ai.book.At(point);
    return entry.published ? &entry.offer : nullptr;
}

struct ReceiveInput
{
    PvpObs::MatchKey key;
    std::uint8_t side = 0;
    std::uint32_t nowMs = 0;
    std::uint32_t minConfidencePct = MIN_CONFIDENCE_DEFAULT_PCT;
    std::uint32_t lifeMs = 0;
    bool matchOver = false;
};

// Judges the queued answers of one side, in order, and applies the accepted ones: the leader's stance for D1, the book for the
// other points. Every answer ends the request it answers (a request is not awaited twice) and its cost is counted whatever the
// verdict. `report` is called once per answer with the request as it was awaited (the copy taken before it ended), the answer
// and the verdict: the caller logs and reports outcomes there. A handover (a new leader epoch) is noted first, so the answer of
// the old leader's request is refused as another request's.
template <class Report>
void ProcessAnswers(SideAi& ai, BgLeader::LeaderState& leader, ReceiveInput const& in, std::vector<RawAnswer> const& answers,
                    Report&& report)
{
    OnLeaderEpoch(ai, leader.election.epoch);
    for (RawAnswer const& answer : answers)
    {
        PendingAsk const pending = ai.pending;

        JudgeContext ctx;
        ctx.nowMs = in.nowMs;
        ctx.minConfidencePct = in.minConfidencePct;
        ctx.matchOver = in.matchOver;
        ctx.key = in.key;
        ctx.side = in.side;
        ctx.leaderEpoch = leader.election.epoch;
        ctx.current = CurrentOffer(leader, ai, pending.point);   // the list that was asked about; another point is refused
        Judgement const verdict = Judge(pending, answer, ctx);

        Finish(ai, answer.requestId);
        ai.costUsd += answer.costUsd;
        if (verdict.accept)
        {
            if (answer.point == DecisionPoint::D1_STANCE)
                BgLeader::SetLayerStance(leader, static_cast<BgLeader::Stance>(verdict.optionId), in.nowMs, in.lifeMs);
            else
                SetChoice(ai.book, answer.point, verdict.optionId, in.nowMs, in.lifeMs);
            ++ai.accepted;
            Remember(ai, answer.decisionId);
        }
        else
            ++ai.rejected;

        report(pending, answer, verdict);
    }
}
}  // namespace AiPvp

#endif
