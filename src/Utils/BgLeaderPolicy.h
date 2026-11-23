/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_BGLEADERPOLICY_H
#define _PLAYERBOT_BGLEADERPOLICY_H

// BT-481: the battleground leader (owner decisions 2026-11-18, docs/bg-sheets/twin-peaks.md section 8 and 16). One leader bot
// per side picks or confirms the stance the deterministic plan already computes and calls it in battleground chat. The map
// plans keep owning roles and execution; the leader never assigns a bot a position.
//
//  - Election: the best bot of the side by skill tier, then level, then the GUID hash (the same hash the skill tiers use);
//    a human is never the leader. The incumbent stays while it is eligible once the gates are open (a leader that changes
//    every refresh is no leader); before that the best bot present wins, so the team's first bot to tick does not decide.
//    A leader that dies, leaves or logs out is replaced at the next refresh (an epoch counts every change).
//  - Stance: what the team plays (balanced, offense, defense, all in, turtle; for Arathi Basin hold, push, close out, last
//    node, panic). The rule's answer is the first candidate of a scored list (the decision point D1 of the sheet); a later
//    decision layer may answer with another id of the same list, accepted only when it is for this match, side, leader epoch
//    and state version, and one of the offered ids.
//  - Drift: casual bots take a stance change one refresh late, and about a quarter of them keep their old role one more
//    refresh (owner 2026-11-18). A carrier, a flag taker and the last guard never drift.
//
// Free of game types so that tests/ can run it (tests/test_bg_leader.cpp).

#include "BgPlanCommon.h"
#include "BotSkillMath.h"
#include "PvpObservationMath.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace BgLeader
{
using Tier = BotSkillMath::Tier;

// ---------------------------------------------------------------------------------------------------------------------
// Election.
// ---------------------------------------------------------------------------------------------------------------------
struct Candidate
{
    std::uint32_t guidLow = 0;
    Tier tier = Tier::GOOD;
    std::uint8_t level = 0;
    bool bot = false;        // a human is never the leader
    bool alive = false;
    bool present = false;    // in the world and in the match
};

inline bool Eligible(Candidate const& c)
{
    return c.bot && c.alive && c.present;
}

// True when a ranks above b: the better tier, the higher level, the lower GUID hash, the lower GUID.
inline bool Outranks(Candidate const& a, Candidate const& b)
{
    if (a.tier != b.tier)
        return static_cast<std::uint8_t>(a.tier) < static_cast<std::uint8_t>(b.tier);
    if (a.level != b.level)
        return a.level > b.level;
    std::uint32_t const hashA = BotSkillMath::Mix(a.guidLow);
    std::uint32_t const hashB = BotSkillMath::Mix(b.guidLow);
    if (hashA != hashB)
        return hashA < hashB;
    return a.guidLow < b.guidLow;
}

// The GUID of the best eligible candidate, 0 for none.
inline std::uint32_t Best(std::vector<Candidate> const& candidates)
{
    Candidate const* best = nullptr;
    for (Candidate const& c : candidates)
        if (Eligible(c) && (!best || Outranks(c, *best)))
            best = &c;
    return best ? best->guidLow : 0;
}

struct Election
{
    std::uint32_t leader = 0;      // GUID low, 0 for none
    std::uint32_t epoch = 0;       // counts every change of leader, including to none
    std::uint32_t sinceMs = 0;
};

enum class Change : std::uint8_t
{
    NONE,
    ELECTED,     // from no leader to one
    HANDOVER,    // from one leader to another
    LOST         // the leader went and nobody can follow
};

inline Change Elect(Election& state, std::vector<Candidate> const& candidates, std::uint32_t nowMs, bool keepIncumbent)
{
    std::uint32_t next = 0;
    if (keepIncumbent && state.leader)
        for (Candidate const& c : candidates)
            if (c.guidLow == state.leader && Eligible(c))
                next = state.leader;
    if (!next)
        next = Best(candidates);

    if (next == state.leader)
        return Change::NONE;

    Change const change = !next ? Change::LOST : state.leader ? Change::HANDOVER : Change::ELECTED;
    state.leader = next;
    ++state.epoch;
    state.sinceMs = PvpObs::Stamp(nowMs);
    return change;
}

// ---------------------------------------------------------------------------------------------------------------------
// Stance.
// ---------------------------------------------------------------------------------------------------------------------
enum class Stance : std::uint8_t
{
    BALANCED,
    OFFENSE,
    DEFENSE,
    ALL_IN,       // behind late: home down to one, the rest strike
    TURTLE,       // ahead late: everybody home, a strike pair only to deny
    HOLD,         // Arathi Basin: ahead on the race clock, keep what we have
    PUSH,         // behind on the race clock: flip a node or deny
    CLOSE_OUT,    // four nodes: the fifth ends the match
    LAST_NODE,    // one node against three or more: never lose it
    PANIC         // the enemy is about to win the race
};

inline char const* StanceName(Stance stance)
{
    switch (stance)
    {
        case Stance::BALANCED: return "balanced";
        case Stance::OFFENSE: return "offense";
        case Stance::DEFENSE: return "defense";
        case Stance::ALL_IN: return "all_in";
        case Stance::TURTLE: return "turtle";
        case Stance::HOLD: return "hold";
        case Stance::PUSH: return "push";
        case Stance::CLOSE_OUT: return "close_out";
        case Stance::LAST_NODE: return "last_node";
        case Stance::PANIC: return "panic";
    }
    return "balanced";
}

// The stance of a plan draw (balanced, offense, defense).
inline Stance StanceOfPlan(BgPlan::Plan plan)
{
    return plan == BgPlan::Plan::OFFENSE ? Stance::OFFENSE : plan == BgPlan::Plan::DEFENSE ? Stance::DEFENSE : Stance::BALANCED;
}

// A stance the leader says aloud. Close out has its own call that names the node (Arathi Basin "take st, 5 nodes ends it").
inline bool IsCallable(Stance stance)
{
    return stance != Stance::CLOSE_OUT;
}

// ---------------------------------------------------------------------------------------------------------------------
// Decision points and the hook for the decision layer. Every decision point is a list of options with an integer score,
// best first, ties by the lower id; the FIRST is the rule's answer and the fallback. The layer answers with one id of the
// list. Options never carry free coordinates or GUIDs, only ids the plan already knows.
// ---------------------------------------------------------------------------------------------------------------------
enum class DecisionPoint : std::uint8_t
{
    D1_STANCE = 1,
    D2_PICKUP,
    D3_ROUTE,
    D4_CARRIER_LANE,
    D5_HUNT_SIZE,
    D6_FLOATER,
    D7_WIPE_REPLY,
    D8_STALEMATE,
    D9_NODE_TARGET,   // Arathi Basin: the node the assault group goes for (a list of the targetable nodes)
    // The arena asks of a later step (design decision 0032 point 9): the kill target and the crowd control target at the key moments
    // of a fight. Not wired: nothing offers or applies them yet.
    D10_ARENA_KILL_TARGET,
    D11_ARENA_CC_TARGET,
    // Battle for Gilneas (BT-553, sheet section 16): the Horde's route to Waterworks (not asked until the routes are measured), the
    // answer to a fight that is going badly, and where the reserve stages.
    D12_BFG_ROUTE,
    D13_BFG_FIGHT,
    D14_BFG_STAGE,
    // Eye of the Storm (BT-585, sheet section 16): the pickup of the flag in the middle, the base our carrier runs to, and the
    // answer to an enemy carrier.
    D15_EYE_FLAG,
    D16_EYE_CARRIER,
    D17_EYE_EFC,
    // Silvershard Mines (BT-613, sheet section 16): which cart the main group contests, and what the switch holder does.
    D18_SM_CART,
    D19_SM_SWITCH
};

struct Option
{
    std::uint8_t id = 0;
    int score = 0;
};

struct Offer
{
    DecisionPoint point = DecisionPoint::D1_STANCE;
    std::uint32_t stateVersion = 0;
    std::vector<Option> options;   // best first

    std::uint8_t Fallback() const { return options.empty() ? std::uint8_t(0) : options.front().id; }
};

inline void SortOptions(std::vector<Option>& options)
{
    std::stable_sort(options.begin(), options.end(), [](Option const& a, Option const& b)
    {
        if (a.score != b.score)
            return a.score > b.score;
        return a.id < b.id;
    });
}

// A request is worth sending to a decision layer only when a materially different option exists.
constexpr int MEANINGFUL_ALTERNATIVE_SCORE = 45;

inline bool IsMeaningful(Offer const& offer)
{
    return offer.options.size() >= 2 && offer.options[1].score >= MEANINGFUL_ALTERNATIVE_SCORE;
}

struct Answer
{
    PvpObs::MatchKey key;
    std::uint8_t side = 0;
    std::uint32_t leaderEpoch = 0;
    std::uint32_t stateVersion = 0;
    DecisionPoint point = DecisionPoint::D1_STANCE;
    std::uint8_t optionId = 0;
};

enum class Verdict : std::uint8_t
{
    ACCEPT,
    STALE_MATCH,     // another match or another generation of the same instance
    WRONG_SIDE,
    STALE_LEADER,    // the leader changed since the request
    STALE_STATE,     // the state the request described has moved on
    WRONG_POINT,
    UNKNOWN_OPTION   // not one of the offered ids
};

inline Verdict CheckAnswer(Offer const& offer, Answer const& answer, PvpObs::MatchKey const& key, std::uint8_t side,
                           std::uint32_t leaderEpoch)
{
    if (!(answer.key == key))
        return Verdict::STALE_MATCH;
    if (answer.side != side)
        return Verdict::WRONG_SIDE;
    if (answer.leaderEpoch != leaderEpoch)
        return Verdict::STALE_LEADER;
    if (answer.stateVersion != offer.stateVersion)
        return Verdict::STALE_STATE;
    if (answer.point != offer.point)
        return Verdict::WRONG_POINT;
    for (Option const& o : offer.options)
        if (o.id == answer.optionId)
            return Verdict::ACCEPT;
    return Verdict::UNKNOWN_OPTION;
}

// ---------------------------------------------------------------------------------------------------------------------
// The leader state of one side, and its tick.
// ---------------------------------------------------------------------------------------------------------------------
// The leader says its stance at most this often: a second call (a handover, a change) within it waits.
constexpr std::uint32_t STANCE_CALL_GAP_MS = 15000;

// A stance that was changed this long ago and never got its call (the limiter said no) is dropped: it would be old news.
constexpr std::uint32_t STANCE_CALL_STALE_MS = 20000;

struct LeaderState
{
    Election election;

    Stance rule = Stance::BALANCED;          // the stance the deterministic plan plays
    bool ruleKnown = false;
    std::uint32_t stanceVersion = 0;         // counts every change of the rule's stance
    std::uint32_t stanceSeq = 0;             // counts changes after the first reading (drift rolls)
    std::uint32_t stanceChangedMs = 0;

    bool layerChosen = false;                // a decision layer's answer for the current version
    Stance layer = Stance::BALANCED;
    std::uint32_t layerUntilMs = 0;          // the answer holds until then (0 = until the rule's stance changes)
    bool effectiveKnown = false;
    Stance lastEffective = Stance::BALANCED; // the stance played at the last tick: a change of it is a stance change

    Stance called = Stance::BALANCED;        // the stance last said aloud
    bool callWasMade = false;
    bool callOwed = false;                   // the leader should say the stance now
    std::uint32_t owedSinceMs = 0;
    Offer offer;                             // D1 for the current version
};

inline Stance Effective(LeaderState const& s)
{
    return s.layerChosen ? s.layer : s.rule;
}

struct TickResult
{
    Change change = Change::NONE;
    bool stanceChanged = false;     // the rule's stance moved this tick (after the first reading)
    bool callDue = false;           // the leader should say the stance now
    Stance stance = Stance::BALANCED;
    std::uint32_t leader = 0;
};

// One refresh of one side. `rule` is the stance the deterministic code plays now, `offer` the D1 list built from the same
// facts (its first option is the rule). `started` is true once the gates are open: the stance is called from then on, and the
// incumbent leader is kept from then on.
template <class AfterOffer>
TickResult TickWith(LeaderState& s, std::vector<Candidate> const& candidates, Stance rule, Offer offer, std::uint32_t nowMs,
                    bool started, AfterOffer&& afterOffer)
{
    TickResult result;
    result.change = Elect(s.election, candidates, nowMs, started);

    if (!s.ruleKnown || rule != s.rule)
    {
        bool const first = !s.ruleKnown;
        s.rule = rule;
        s.ruleKnown = true;
        ++s.stanceVersion;
        s.stanceChangedMs = PvpObs::Stamp(nowMs);
        s.layerChosen = false;      // an answer is for the version it was asked about
        s.layerUntilMs = 0;
        if (!first)
        {
            ++s.stanceSeq;
            result.stanceChanged = true;
        }
    }
    offer.stateVersion = s.stanceVersion;
    s.offer = std::move(offer);

    // A decision layer's answer lives for a while (the layer asks again every interval), then the rule plays again.
    if (s.layerChosen && s.layerUntilMs && PvpObs::Reached(nowMs, s.layerUntilMs))
        s.layerChosen = false;
    // The hook of the decision layer: the answers that came in are judged against the offer just published, so an answer is
    // never accepted for a list that is already old.
    afterOffer(s);

    Stance const stance = Effective(s);
    // A change of the stance the team plays counts as a stance change whoever made it, the rule or the layer: the casual bots
    // drift after both. The first reading is the baseline.
    if (s.effectiveKnown && stance != s.lastEffective && !result.stanceChanged)
    {
        ++s.stanceSeq;
        result.stanceChanged = true;
        s.stanceChangedMs = PvpObs::Stamp(nowMs);
    }
    s.lastEffective = stance;
    s.effectiveKnown = true;
    bool owed = s.callOwed;
    // A new leader says the stance again: the team must hear it from whoever leads it now.
    if (started && (result.change == Change::HANDOVER || result.change == Change::ELECTED))
        owed = true;
    if (started && (!s.callWasMade || stance != s.called))
        owed = true;
    if (owed && !s.callOwed)
        s.owedSinceMs = PvpObs::Stamp(nowMs);
    s.callOwed = owed;

    if (!IsCallable(stance) && s.callOwed)
    {
        // A silent stance counts as said (close out has its own call).
        s.called = stance;
        s.callWasMade = true;
        s.callOwed = false;
    }
    else if (s.callOwed && s.callWasMade && PvpObs::Reached(nowMs, s.owedSinceMs + STANCE_CALL_STALE_MS))
    {
        // It never got its turn (the limiter said no for too long): drop it, the next change is the news.
        s.called = stance;
        s.callOwed = false;
    }

    result.stance = stance;
    result.leader = s.election.leader;
    result.callDue = s.callOwed && s.election.leader != 0;
    return result;
}

inline TickResult Tick(LeaderState& s, std::vector<Candidate> const& candidates, Stance rule, Offer offer,
                       std::uint32_t nowMs, bool started)
{
    return TickWith(s, candidates, rule, std::move(offer), nowMs, started, [](LeaderState&) {});
}

// The caller queued the stance call: it is no longer owed.
inline void CallCommitted(LeaderState& s)
{
    s.called = Effective(s);
    s.callWasMade = true;
    s.callOwed = false;
}

// The stance a decision layer chose, held for lifeMs (0 = until the rule's stance changes). The answer was already judged.
inline void SetLayerStance(LeaderState& s, Stance stance, std::uint32_t nowMs, std::uint32_t lifeMs)
{
    s.layerChosen = true;
    s.layer = stance;
    s.layerUntilMs = lifeMs ? PvpObs::Stamp(nowMs + lifeMs) : 0;
}

// The hook a decision layer uses: takes an answer for the current D1 offer. The stance is then the answered one for lifeMs
// (0 = until the rule's stance changes).
inline Verdict AcceptStanceAnswer(LeaderState& s, Answer const& answer, PvpObs::MatchKey const& key, std::uint8_t side,
                                  std::uint32_t nowMs = 0, std::uint32_t lifeMs = 0)
{
    Verdict const verdict = CheckAnswer(s.offer, answer, key, side, s.election.epoch);
    if (verdict == Verdict::ACCEPT)
        SetLayerStance(s, static_cast<Stance>(answer.optionId), nowMs, lifeMs);
    return verdict;
}

// ---------------------------------------------------------------------------------------------------------------------
// Drift (owner 2026-11-18): casual bots adopt a stance change one refresh late (the adoption delay of the plan's epoch gets
// one more refresh) and about a quarter of them also keep their old role one more refresh. The roll is fixed per bot and per
// stance change, so a replay gives the same answer. Expert and good bots do not drift.
// ---------------------------------------------------------------------------------------------------------------------
constexpr std::uint32_t DRIFT_KEEP_PERCENT = 25;
constexpr std::uint32_t DRIFT_ROLL_MIX = 2246822519u;

inline std::uint32_t StanceExtraLagMs(Tier tier)
{
    return tier == Tier::CASUAL ? BgPlan::REFRESH_MS : 0;
}

// The refreshes a bot keeps its old role after a stance change: 0 or 1.
inline std::uint32_t RoleKeepRefreshes(Tier tier, std::uint32_t guidLow, std::uint32_t stanceSeq)
{
    if (tier != Tier::CASUAL)
        return 0;
    return BotSkillMath::Mix(guidLow ^ (stanceSeq * DRIFT_ROLL_MIX)) % 100 < DRIFT_KEEP_PERCENT ? 1 : 0;
}

// A bot may keep its old role this refresh: it still has refreshes to keep and the duty is not critical (the carrier, a flag
// taker, the last guard).
inline bool MayKeepRole(std::uint32_t keepLeft, bool critical)
{
    return keepLeft > 0 && !critical;
}
}  // namespace BgLeader

#endif
