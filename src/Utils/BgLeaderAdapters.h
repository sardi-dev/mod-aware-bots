/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_BGLEADERADAPTERS_H
#define _PLAYERBOT_BGLEADERADAPTERS_H

// BT-481: how the leader (BgLeaderPolicy.h) reads and writes the map plans. The plans already compute what a team plays:
// the two-flag plan (Warsong Gulch and Twin Peaks) a draw (balanced, offense, defense) and a situation from score and
// clock, the Arathi Basin plan the same draw and a regime from the race clock. The leader's stance is a NAME for that state,
// so the leader confirms what the deterministic code computes and calls it; it does not run a second allocator.
//
//  - StanceOf: the stance the rule plays now.
//  - StanceOffer: the decision point D1 as a scored list whose first option is the rule's stance, so that a decision layer
//    may answer with another id of the list.
//  - Apply: a stance answered by the layer, written back as the plan and situation the plan's counts are built from. The
//    rule's own answer is never applied (the plan keeps its exact state), only another one is.
//  - OfferFrom: the decision lists that already exist as TpLogic::Scored lists (D4 carrier lane, D6 floater, D7 wipe reply)
//    become offers of the same shape.
//  - The Arathi Basin stance list (hold, push, last node, panic and the three plans) and the list of target nodes (D9).
//  - CandidateName: the word the decision layer reads for an option id (a fixed vocabulary, never a GUID or a coordinate).
//
// Free of game types so that tests/ can run it (tests/test_bg_leader.cpp).

#include "AbPlanLogic.h"
#include "EyFlagLogic.h"
#include "BgLeaderPolicy.h"
#include "SmPlanLogic.h"
#include "TpPlanLogic.h"
#include "WsgPlanLogic.h"

#include <cstdint>
#include <vector>

namespace BgLeader
{
// ---------------------------------------------------------------------------------------------------------------------
// Warsong Gulch and Twin Peaks.
// ---------------------------------------------------------------------------------------------------------------------
struct FlagPlanState
{
    BgPlan::Plan plan = BgPlan::Plan::BALANCED;
    WsgLogic::Situation situation = WsgLogic::Situation::NORMAL;
};

// The names of the situations: ahead by two plays defense, ahead by one late turtles, level late goes for the last capture,
// behind late is all in.
inline Stance StanceOf(FlagPlanState const& state)
{
    switch (state.situation)
    {
        case WsgLogic::Situation::LEAD2: return Stance::DEFENSE;
        case WsgLogic::Situation::ALL_IN: return Stance::ALL_IN;
        case WsgLogic::Situation::LEAD1_LATE: return Stance::TURTLE;
        case WsgLogic::Situation::LEVEL_LATE: return Stance::OFFENSE;
        case WsgLogic::Situation::NORMAL: break;
    }
    return StanceOfPlan(state.plan);
}

// The plan state a stance means. The plain stances play the situation-free table of their plan; all in and turtle keep the
// drawn plan and ask for their situation. A stance the two-flag plan does not know (the Arathi Basin ones) changes nothing.
inline FlagPlanState Apply(Stance stance, FlagPlanState const& rule)
{
    FlagPlanState out = rule;
    switch (stance)
    {
        case Stance::BALANCED:
            out = { BgPlan::Plan::BALANCED, WsgLogic::Situation::NORMAL };
            break;
        case Stance::OFFENSE:
            out = { BgPlan::Plan::OFFENSE, WsgLogic::Situation::NORMAL };
            break;
        case Stance::DEFENSE:
            out = { BgPlan::Plan::DEFENSE, WsgLogic::Situation::NORMAL };
            break;
        case Stance::ALL_IN:
            out.situation = WsgLogic::Situation::ALL_IN;
            break;
        case Stance::TURTLE:
            out.situation = WsgLogic::Situation::LEAD1_LATE;
            break;
        default:
            break;
    }
    return out;
}

// D1. The rule's stance scores 100; the other stances that make sense for the situation score below it, never above (the
// first option is the fallback). Legal alternatives: in normal play the other two plans (balanced 60, offense and defense 50);
// ahead by two, balanced (40); behind late, plain offense (40); level late, balanced (40); ahead late, defense (45) or
// balanced (30). A second option scores 45 or more only where the choice is real (IsMeaningful), so a decision layer is asked
// in normal play and ahead by one late, not in the situations that have one sensible answer. (The Twin Peaks sheet's table of
// section 16 describes the same lists by situation; the numbers here are the ones the tests pin.)
inline Offer StanceOffer(FlagPlanState const& state)
{
    Offer offer;
    offer.point = DecisionPoint::D1_STANCE;

    Stance const rule = StanceOf(state);
    auto const add = [&offer, rule](Stance stance, int score)
    {
        if (stance != rule)
            offer.options.push_back({ static_cast<std::uint8_t>(stance), score });
    };
    offer.options.push_back({ static_cast<std::uint8_t>(rule), 100 });

    switch (state.situation)
    {
        case WsgLogic::Situation::NORMAL:
            add(Stance::BALANCED, 60);
            add(Stance::OFFENSE, 50);
            add(Stance::DEFENSE, 50);
            break;
        case WsgLogic::Situation::LEAD2:
            add(Stance::BALANCED, 40);
            break;
        case WsgLogic::Situation::ALL_IN:
            add(Stance::OFFENSE, 40);
            break;
        case WsgLogic::Situation::LEVEL_LATE:
            add(Stance::BALANCED, 40);
            break;
        case WsgLogic::Situation::LEAD1_LATE:
            add(Stance::DEFENSE, 45);
            add(Stance::BALANCED, 30);
            break;
    }
    SortOptions(offer.options);
    return offer;
}

// ---------------------------------------------------------------------------------------------------------------------
// Arathi Basin.
// ---------------------------------------------------------------------------------------------------------------------
inline Stance StanceOf(BgPlan::Plan plan, AbLogic::Regime regime)
{
    switch (regime)
    {
        case AbLogic::Regime::HOLD: return Stance::HOLD;
        case AbLogic::Regime::PUSH: return Stance::PUSH;
        case AbLogic::Regime::CLOSE_OUT: return Stance::CLOSE_OUT;
        case AbLogic::Regime::LAST_NODE: return Stance::LAST_NODE;
        case AbLogic::Regime::PANIC: return Stance::PANIC;
        case AbLogic::Regime::EVEN: break;
    }
    return StanceOfPlan(plan);
}

// What the Arathi Basin stance list needs besides the rule: the occupied nodes of both sides.
struct AbStanceFacts
{
    unsigned ourNodes = 0;
    unsigned theirNodes = 0;
};

// The plan state an Arathi Basin stance means: the three plans play the even regime of their plan; hold, push, last node and
// panic keep the drawn plan and ask for their regime. Close out is the rule's own and never an answer.
struct AbPlanState
{
    BgPlan::Plan plan = BgPlan::Plan::BALANCED;
    AbLogic::Regime regime = AbLogic::Regime::EVEN;
};

inline AbPlanState Apply(Stance stance, AbPlanState const& rule)
{
    AbPlanState out = rule;
    switch (stance)
    {
        case Stance::BALANCED: out = { BgPlan::Plan::BALANCED, AbLogic::Regime::EVEN }; break;
        case Stance::OFFENSE: out = { BgPlan::Plan::OFFENSE, AbLogic::Regime::EVEN }; break;
        case Stance::DEFENSE: out = { BgPlan::Plan::DEFENSE, AbLogic::Regime::EVEN }; break;
        case Stance::HOLD: out.regime = AbLogic::Regime::HOLD; break;
        case Stance::PUSH: out.regime = AbLogic::Regime::PUSH; break;
        case Stance::LAST_NODE: out.regime = AbLogic::Regime::LAST_NODE; break;
        case Stance::PANIC: out.regime = AbLogic::Regime::PANIC; break;
        default: break;
    }
    return out;
}

// D1 for Arathi Basin. The rule's stance scores 100 and is the fallback. Alternatives that are legal on the node count:
//  - even play (a plan stance): the other two plans (60 and 50); hold with three nodes or more (55); push when they hold a
//    node to flip (52);
//  - hold: balanced (60), push (45) when they hold a node;
//  - push: balanced (60), hold (40) only with three nodes;
//  - last node (one node against three or more): push (50), defense (35);
//  - panic (the enemy is about to win the race): push (55), last node (45) when only one node is ours;
//  - close out (four nodes, the fifth ends the match): the rule alone, nothing to choose.
inline Offer StanceOffer(BgPlan::Plan plan, AbLogic::Regime regime, AbStanceFacts const& facts)
{
    Offer offer;
    offer.point = DecisionPoint::D1_STANCE;

    Stance const rule = StanceOf(plan, regime);
    auto const add = [&offer, rule](Stance stance, int score)
    {
        if (stance != rule)
            offer.options.push_back({ static_cast<std::uint8_t>(stance), score });
    };
    offer.options.push_back({ static_cast<std::uint8_t>(rule), 100 });

    bool const canHold = facts.ourNodes >= 3;
    bool const canPush = facts.theirNodes >= 1;
    switch (regime)
    {
        case AbLogic::Regime::EVEN:
            add(Stance::BALANCED, 60);
            add(Stance::OFFENSE, 50);
            add(Stance::DEFENSE, 50);
            if (canHold)
                add(Stance::HOLD, 55);
            if (canPush)
                add(Stance::PUSH, 52);
            break;
        case AbLogic::Regime::HOLD:
            add(Stance::BALANCED, 60);
            if (canPush)
                add(Stance::PUSH, 45);
            break;
        case AbLogic::Regime::PUSH:
            add(Stance::BALANCED, 60);
            if (canHold)
                add(Stance::HOLD, 40);
            break;
        case AbLogic::Regime::LAST_NODE:
            add(Stance::PUSH, 50);
            add(Stance::DEFENSE, 35);
            break;
        case AbLogic::Regime::PANIC:
            add(Stance::PUSH, 55);
            if (facts.ourNodes == 1)
                add(Stance::LAST_NODE, 45);
            break;
        case AbLogic::Regime::CLOSE_OUT:
            break;
    }
    SortOptions(offer.options);
    return offer;
}

// D9, the node the assault group goes for. `sorted` is the plan's own ScoreTargets list (best first, only nodes that can be
// clicked now); the best scores 100 and the others their share of it (a negative score counts 0). At most three nodes are
// offered: the rest is noise for the decision layer. The rule's answer is the first.
// Battle for Gilneas names its nodes in the same decision point (D9): the candidate ids of its nodes are offset, so that a name
// needs no map argument (Arathi Basin keeps its own ids 0 to 4).
constexpr std::uint8_t BFG_NODE_ID_BASE = 16;
// Eye of the Storm (BT-582): the same decision point, its bases offset by 32.
constexpr std::uint8_t EYE_NODE_ID_BASE = 32;

inline Offer NodeTargetOffer(std::vector<AbLogic::TargetScore> const& sorted, std::uint8_t idBase = 0)
{
    constexpr std::size_t MAX_TARGET_OPTIONS = 3;
    Offer offer;
    offer.point = DecisionPoint::D9_NODE_TARGET;
    for (std::size_t i = 0; i < sorted.size() && i < MAX_TARGET_OPTIONS; ++i)
    {
        int const best = sorted.front().score;
        int const share = best > 0 ? std::max(0, sorted[i].score) * 100 / best : (i == 0 ? 100 : 0);
        offer.options.push_back({ static_cast<std::uint8_t>(sorted[i].node + idBase), i == 0 ? 100 : std::min(share, 99) });
    }
    SortOptions(offer.options);
    return offer;
}

// ---------------------------------------------------------------------------------------------------------------------
// The words the decision layer reads. Fixed vocabulary per decision point; the sidecar's template explains each word once.
// ---------------------------------------------------------------------------------------------------------------------
inline char const* NodeName(std::uint8_t node, AbLogic::Map const& map = BgNodeMap::ARATHI)
{
    return node < map.nodeCount ? map.name[node] : "unknown";
}

// The Battle for Gilneas decision points (sheet section 16). The ids are what the lists below carry.
enum class BfgRoute : std::uint8_t { SHORT_ROUTE = 0, LONG_ROUTE = 1, SPLIT = 2 };
enum class BfgFight : std::uint8_t { FIGHT_ON = 0, RETREAT = 1, GHOST = 2, REINFORCE = 3 };
enum class BfgStage : std::uint8_t { STAY_HUB = 0, TO_FAR_NODE = 1, TO_OWN_NEAR = 2 };

inline char const* CandidateName(DecisionPoint point, std::uint8_t id)
{
    switch (point)
    {
        case DecisionPoint::D1_STANCE:
            return StanceName(static_cast<Stance>(id));
        case DecisionPoint::D4_CARRIER_LANE:
            return id == static_cast<std::uint8_t>(TpLogic::CarrierLane::OWN_LANE) ? "own_lane" : "other_lane";
        case DecisionPoint::D6_FLOATER:
            switch (static_cast<TpLogic::FloaterJob>(id))
            {
                case TpLogic::FloaterJob::TO_STRIKE: return "to_strike";
                case TpLogic::FloaterJob::TO_HOME: return "to_home";
                case TpLogic::FloaterJob::STAY_COVER: return "stay_cover";
            }
            break;
        case DecisionPoint::D7_WIPE_REPLY:
            switch (static_cast<TpLogic::WipeReply>(id))
            {
                case TpLogic::WipeReply::DEFEND_FIRST: return "defend_first";
                case TpLogic::WipeReply::PUSH_NOW: return "push_now";
                case TpLogic::WipeReply::REGROUP_LANE: return "regroup_lane";
            }
            break;
        case DecisionPoint::D9_NODE_TARGET:
            return id >= EYE_NODE_ID_BASE ? NodeName(static_cast<std::uint8_t>(id - EYE_NODE_ID_BASE), BgNodeMap::EYE) :
                   id >= BFG_NODE_ID_BASE ? NodeName(static_cast<std::uint8_t>(id - BFG_NODE_ID_BASE), BgNodeMap::GILNEAS) : NodeName(id);
        case DecisionPoint::D12_BFG_ROUTE:
            return id == static_cast<std::uint8_t>(BfgRoute::SPLIT) ? "split" :
                   id == static_cast<std::uint8_t>(BfgRoute::LONG_ROUTE) ? "long_route" : "short_route";
        case DecisionPoint::D13_BFG_FIGHT:
            switch (static_cast<BfgFight>(id))
            {
                case BfgFight::FIGHT_ON: return "fight_on";
                case BfgFight::RETREAT: return "retreat";
                case BfgFight::GHOST: return "ghost";
                case BfgFight::REINFORCE: return "reinforce";
            }
            break;
        case DecisionPoint::D14_BFG_STAGE:
            switch (static_cast<BfgStage>(id))
            {
                case BfgStage::STAY_HUB: return "stay_hub";
                case BfgStage::TO_FAR_NODE: return "to_far_node";
                case BfgStage::TO_OWN_NEAR: return "to_own_near";
            }
            break;
        case DecisionPoint::D15_EYE_FLAG:
            return EyFlag::ActionName(static_cast<EyFlag::Action>(id));
        case DecisionPoint::D16_EYE_CARRIER:
            return id == EyFlag::HOLD_BACK ? "hold_back" : NodeName(id, BgNodeMap::EYE);
        case DecisionPoint::D17_EYE_EFC:
            return EyFlag::EfcActionName(static_cast<EyFlag::EfcAction>(id));
        case DecisionPoint::D18_SM_CART:
            return id == SmLogic::CART_EAST ? "east_cart_lava" : id == SmLogic::CART_WATERFALL ? "waterfall_cart_mid" : "north_cart_top";
        case DecisionPoint::D19_SM_SWITCH:
            return id == static_cast<std::uint8_t>(SmLogic::SwitchAct::CLICK_NOW) ? "click_now" :
                   id == static_cast<std::uint8_t>(SmLogic::SwitchAct::WAIT_LAST) ? "wait_last" : "leave";
        default:
            break;
    }
    return "unknown";
}

// ---------------------------------------------------------------------------------------------------------------------
// Battle for Gilneas, the lists of D12 to D14 (sheet section 16). The first option is the rule's own answer and the fallback.
// ---------------------------------------------------------------------------------------------------------------------
// D12: the Horde's way to Waterworks. Split from 6 bots in the main group (two columns, so one stun in the water does not catch
// everybody), else the short way; the long way is the alternative. Asked only once the routes are measured (not wired).
inline Offer RouteOffer(std::uint32_t mainGroupSize)
{
    Offer offer;
    offer.point = DecisionPoint::D12_BFG_ROUTE;
    bool const split = mainGroupSize >= 6;
    offer.options.push_back({ static_cast<std::uint8_t>(BfgRoute::SHORT_ROUTE), split ? 70 : 100 });
    offer.options.push_back({ static_cast<std::uint8_t>(BfgRoute::LONG_ROUTE), 70 });
    if (split)
        offer.options.push_back({ static_cast<std::uint8_t>(BfgRoute::SPLIT), 100 });
    SortOptions(offer.options);
    return offer;
}

// D13: the answer to a fight at a node. The rule: the ghost when two expert stealth bots can ink an open, lightly held enemy near
// node (110); the retreat when the fight is lost (100) and a node to fall back to exists; else fight on (100). Reinforce (90) when
// the fight is slightly behind and a reserve bot is near.
struct FightFacts
{
    int odds = 0;                  // alive friends minus seen enemies within 40 yd
    bool healerAlive = true;
    bool rallyAvailable = false;   // a node we hold to fall back to
    bool ghostWanted = false;      // AbLogic::GhostWanted
    bool reinforceAvailable = false;
};

inline Offer FightOffer(FightFacts const& f)
{
    Offer offer;
    offer.point = DecisionPoint::D13_BFG_FIGHT;
    bool const losing = AbLogic::Losing(f.odds, f.healerAlive);
    auto const add = [&offer](BfgFight id, int score) { offer.options.push_back({ static_cast<std::uint8_t>(id), score }); };

    if (f.ghostWanted)
        add(BfgFight::GHOST, 110);
    if (losing && f.rallyAvailable)
        add(BfgFight::RETREAT, 100);
    else if (f.rallyAvailable && f.odds < 0)
        add(BfgFight::RETREAT, 70);
    add(BfgFight::FIGHT_ON, losing && f.rallyAvailable ? 60 : 100);
    if (f.reinforceAvailable && f.odds < 0 && !losing)
        add(BfgFight::REINFORCE, 90);
    SortOptions(offer.options);
    return offer;
}

// D14: where the reserve (the floater healer and the damage dealers kept back) stages. The rule's node scores 100; the other nodes
// we can use score less.
struct StageFacts
{
    std::uint8_t ruleNode = 1;      // the node the rule stages at
    std::uint8_t hubNode = 1;
    std::uint8_t farNode = 2;
    std::uint8_t ownNearNode = 0;
    bool hubUsable = false;         // held or being captured by us
    bool farUsable = false;         // we hold a node and the enemy's near node is not a place to die at once
    bool ownNearUsable = false;
};

inline Offer StageOffer(StageFacts const& f)
{
    Offer offer;
    offer.point = DecisionPoint::D14_BFG_STAGE;
    auto const idOf = [&](std::uint8_t node) -> int
    {
        if (node == f.hubNode) return static_cast<int>(BfgStage::STAY_HUB);
        if (node == f.farNode) return static_cast<int>(BfgStage::TO_FAR_NODE);
        return static_cast<int>(BfgStage::TO_OWN_NEAR);
    };
    auto const add = [&](std::uint8_t node, bool usable, int score)
    {
        if (!usable && node != f.ruleNode)
            return;
        offer.options.push_back({ static_cast<std::uint8_t>(idOf(node)), node == f.ruleNode ? 100 : score });
    };
    add(f.hubNode, f.hubUsable, 60);
    add(f.farNode, f.farUsable, 55);
    add(f.ownNearNode, f.ownNearUsable, 50);
    SortOptions(offer.options);
    return offer;
}

// ---------------------------------------------------------------------------------------------------------------------
// Eye of the Storm, the lists of D15 to D17 (sheet section 16). The first option is the rule's own answer and the fallback.
// ---------------------------------------------------------------------------------------------------------------------
inline Offer EyeFlagOffer(EyFlag::ActionInput const& in)
{
    Offer offer;
    offer.point = DecisionPoint::D15_EYE_FLAG;
    for (EyFlag::ActionOption const& o : EyFlag::Actions(in))
        offer.options.push_back({ static_cast<std::uint8_t>(o.action), o.score });
    SortOptions(offer.options);
    return offer;
}

inline Offer EyeCarrierOffer(std::vector<EyFlag::DestOption> const& list)
{
    Offer offer;
    offer.point = DecisionPoint::D16_EYE_CARRIER;
    for (EyFlag::DestOption const& o : list)
        offer.options.push_back({ o.base, o.score });
    SortOptions(offer.options);
    return offer;
}

inline Offer EyeEfcOffer(EyFlag::EfcInput const& in)
{
    Offer offer;
    offer.point = DecisionPoint::D17_EYE_EFC;
    for (EyFlag::EfcOption const& o : EyFlag::EfcActions(in))
        offer.options.push_back({ static_cast<std::uint8_t>(o.action), o.score });
    SortOptions(offer.options);
    return offer;
}

inline std::uint8_t StageNodeOf(BfgStage id, StageFacts const& f)
{
    return id == BfgStage::STAY_HUB ? f.hubNode : id == BfgStage::TO_FAR_NODE ? f.farNode : f.ownNearNode;
}

// ---------------------------------------------------------------------------------------------------------------------
// The lists of TpPlanLogic.h (D4 carrier lane, D6 floater, D7 wipe reply) as offers.
// ---------------------------------------------------------------------------------------------------------------------
template <class Id>
Offer OfferFrom(DecisionPoint point, std::uint32_t stateVersion, std::vector<TpLogic::Scored<Id>> const& list)
{
    Offer offer;
    offer.point = point;
    offer.stateVersion = stateVersion;
    for (TpLogic::Scored<Id> const& s : list)
        offer.options.push_back({ static_cast<std::uint8_t>(s.id), s.score });
    SortOptions(offer.options);
    return offer;
}
}  // namespace BgLeader

#endif
