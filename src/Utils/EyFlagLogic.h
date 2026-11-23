/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_EYFLAGLOGIC_H
#define _PLAYERBOT_EYFLAGLOGIC_H

// BT-583: the flag in the middle of Eye of the Storm (docs/bg-sheets/eye-of-the-storm.md sections 6 and 16) as pure logic, free of
// game types so that tests/ can run it: the pickup action (decision point D15), the carrier's destination among the bases its
// team holds (D16), the answer to an enemy carrier (D17) and the duty of every other bot of the plan for each state of the flag.
// Every decision is a list of candidate options with an integer score, the first is the rule; a decision layer may later pick
// another from the same list (design decision 0014 and 0026).

#include "EyPlanLogic.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace EyFlag
{
using Tier = BotSkillMath::Tier;
using EyLogic::BASE_COUNT;
using EyLogic::BASE_NONE;

// BattlegroundEY's flag states (BG_EY_FlagState).
constexpr std::uint8_t FLAG_ON_BASE = 0;
constexpr std::uint8_t FLAG_WAIT_RESPAWN = 1;
constexpr std::uint8_t FLAG_ON_PLAYER = 2;
constexpr std::uint8_t FLAG_ON_GROUND = 3;

// ---------------------------------------------------------------------------------------------------------------------
// D15, the pickup action. The flag is picked up with an 8 s cast (the capture spell), which an enemy hit interrupts: it is
// started only when the stand is clear. The flag scores only at a base the team holds, so with no base it is taken to deny.
// ---------------------------------------------------------------------------------------------------------------------
enum class Action : std::uint8_t
{
    TAKE_NOW = 0,
    WAIT_CLEAR = 1,
    DENY_ONLY = 2,
    IGNORE = 3
};

inline char const* ActionName(Action a)
{
    switch (a)
    {
        case Action::TAKE_NOW: return "take_now";
        case Action::WAIT_CLEAR: return "wait_clear";
        case Action::DENY_ONLY: return "deny_only";
        case Action::IGNORE: return "ignore";
    }
    return "?";
}

constexpr float STAND_CLEAR_YARDS = 25.0f;   // the pickup cast is interrupted by a hit: no enemy within the cast's reach
constexpr int SCORE_TAKE_NOW = 100;
constexpr int SCORE_WAIT_CLEAR = 90;
constexpr int SCORE_DENY_ONLY = 80;
constexpr int SCORE_IGNORE = 40;

struct ActionInput
{
    unsigned ourBases = 0;
    unsigned theirBases = 0;
    unsigned enemiesNearStand = 0;    // seen within STAND_CLEAR_YARDS of the stand
    int oddsAtStand = 0;              // friends minus seen enemies within 40 yd of the stand
    Tier tier = Tier::GOOD;
};

struct ActionOption
{
    Action action;
    int score;
};

inline std::vector<ActionOption> Actions(ActionInput const& in)
{
    std::vector<ActionOption> out;
    bool const clear = in.enemiesNearStand == 0;
    if (in.ourBases >= 1)
        out.push_back({ Action::TAKE_NOW, clear ? SCORE_TAKE_NOW : SCORE_TAKE_NOW - 60 });
    if (!clear && in.oddsAtStand >= 0)
        out.push_back({ Action::WAIT_CLEAR, SCORE_WAIT_CLEAR });
    else if (!clear)
        out.push_back({ Action::WAIT_CLEAR, SCORE_WAIT_CLEAR - 60 });
    // Deny: good and expert bots take it with no base of their own while the enemy holds one (core bots do not).
    if (in.ourBases == 0 && in.theirBases >= 1 && BotSkillMath::Uses(in.tier, Tier::GOOD))
        out.push_back({ Action::DENY_ONLY, clear ? SCORE_DENY_ONLY + 30 : SCORE_DENY_ONLY - 30 });
    out.push_back({ Action::IGNORE, SCORE_IGNORE });
    std::stable_sort(out.begin(), out.end(), [](ActionOption const& a, ActionOption const& b)
    {
        if (a.score != b.score)
            return a.score > b.score;
        return static_cast<std::uint8_t>(a.action) < static_cast<std::uint8_t>(b.action);
    });
    return out;
}

inline Action RuleAction(ActionInput const& in)
{
    return Actions(in).front().action;
}

// ---------------------------------------------------------------------------------------------------------------------
// D16, where the carrier scores. Candidates are the bases the team holds; a base nobody guards or that the enemy is already
// neutralising is not offered while another one is. The score is the sheet's: 200 - distance / 3 + 40 per guard - 50 per enemy
// seen - 120 when the base is about to fall + 60 for a base next to our start room (the way back is short and the dead come
// out there).
// ---------------------------------------------------------------------------------------------------------------------
constexpr std::uint8_t HOLD_BACK = 0xFE;   // wait for the escorts (40 s at most)
constexpr int DEST_BASE_SCORE = 200;
constexpr float DEST_YARDS_PER_POINT = 3.0f;
constexpr int DEST_GUARD_BONUS = 40;
constexpr int DEST_ENEMY_PENALTY = 50;
constexpr int DEST_FALLING_PENALTY = 120;
constexpr int DEST_NEAR_OWN_BONUS = 60;
constexpr int DEST_HOLD_BACK_SCORE = 60;
constexpr std::uint32_t HOLD_BACK_MAX_MS = 40000;
constexpr std::uint32_t HOLD_BACK_BLOCKED_ENEMIES = 3;   // seen near the carrier: the path is blocked

struct BaseFact
{
    bool owned = false;
    float distance = 0.0f;        // from the carrier to the capture trigger
    unsigned guards = 0;          // friends in the base's circle, the carrier not counted
    unsigned enemies = 0;         // seen in the circle
    int margin = 0;               // how far the owner's bar is from the enemy's reach (EyLogic::Margin)
    bool nearOwnStart = false;
};

struct DestOption
{
    std::uint8_t base;            // a base id, or HOLD_BACK
    int score;
};

inline bool Falling(BaseFact const& f)
{
    return f.enemies > f.guards || (f.enemies > 0 && f.margin <= 1);
}

inline int DestScore(BaseFact const& f)
{
    int score = DEST_BASE_SCORE - static_cast<int>(f.distance / DEST_YARDS_PER_POINT) + DEST_GUARD_BONUS * static_cast<int>(f.guards) -
                DEST_ENEMY_PENALTY * static_cast<int>(f.enemies);
    if (Falling(f))
        score -= DEST_FALLING_PENALTY;
    if (f.nearOwnStart)
        score += DEST_NEAR_OWN_BONUS;
    return score;
}

// The list, best first; empty when the team holds no base (the carrier then keeps clear). `escortNear` and `enemiesNearCarrier`
// decide whether waiting for the escorts is offered.
inline std::vector<DestOption> Destinations(BaseFact const (&facts)[BASE_COUNT], bool escortNear, unsigned enemiesNearCarrier)
{
    std::vector<DestOption> out;
    bool anyEligible = false;
    for (std::uint8_t b = 0; b < BASE_COUNT; ++b)
        if (facts[b].owned && facts[b].guards > 0 && !Falling(facts[b]))
            anyEligible = true;
    for (std::uint8_t b = 0; b < BASE_COUNT; ++b)
    {
        if (!facts[b].owned)
            continue;
        bool const eligible = facts[b].guards > 0 && !Falling(facts[b]);
        if (anyEligible && !eligible)
            continue;
        out.push_back({ b, DestScore(facts[b]) });
    }
    if (out.empty())
        return out;
    if (escortNear && enemiesNearCarrier >= HOLD_BACK_BLOCKED_ENEMIES)
        out.push_back({ HOLD_BACK, DEST_HOLD_BACK_SCORE });
    std::stable_sort(out.begin(), out.end(), [](DestOption const& a, DestOption const& b)
    {
        if (a.score != b.score)
            return a.score > b.score;
        return a.base < b.base;
    });
    return out;
}

// The base the rule chooses: the first candidate that is a base (waiting is never the rule's answer unless it is all there is).
inline std::uint8_t RuleDestination(std::vector<DestOption> const& list)
{
    for (DestOption const& o : list)
        if (o.base != HOLD_BACK)
            return o.base;
    return BASE_NONE;
}

// ---------------------------------------------------------------------------------------------------------------------
// D17, the answer to an enemy carrier. With four bases the capture is worth 500: everybody in reach hunts. With fewer the two
// nearest attack and two cut the way to the base it needs; an expert may neutralise that base first (a base captured at the
// threshold falls to one enemy in one 2 s tick, and the capture cannot be made there).
// ---------------------------------------------------------------------------------------------------------------------
enum class EfcAction : std::uint8_t
{
    HUNT_ALL = 0,
    HUNT_TWO_CUT = 1,
    NEUTRALIZE_TARGET_BASE = 2,
    IGNORE = 3
};

inline char const* EfcActionName(EfcAction a)
{
    switch (a)
    {
        case EfcAction::HUNT_ALL: return "hunt_all";
        case EfcAction::HUNT_TWO_CUT: return "hunt_two_cut";
        case EfcAction::NEUTRALIZE_TARGET_BASE: return "neutralize_target_base";
        case EfcAction::IGNORE: return "ignore";
    }
    return "?";
}

constexpr float HUNT_ALL_YARDS = 100.0f;
constexpr std::uint32_t FRESH_BASE_MS = 60000;      // held for less than this: not hardened
constexpr unsigned NEUTRALIZE_MAX_GUARDS = 1;

struct EfcInput
{
    unsigned theirBases = 0;
    float distanceToEfc = 0.0f;     // from the bot's team's nearest free bot
    bool expertAvailable = false;   // an expert near enough to reach the target base first
    bool haveTarget = false;        // the base the carrier runs to is known and is theirs
    bool targetFresh = false;       // held for under FRESH_BASE_MS or not hardened
    unsigned targetGuards = 0;      // seen guards at the target base
};

struct EfcOption
{
    EfcAction action;
    int score;
};

inline std::vector<EfcOption> EfcActions(EfcInput const& in)
{
    std::vector<EfcOption> out;
    if (in.theirBases == 0)
    {
        // No base to score at: it cannot capture; hunting stays possible, nothing is urgent.
        out.push_back({ EfcAction::IGNORE, 100 });
        out.push_back({ EfcAction::HUNT_TWO_CUT, 60 });
        out.push_back({ EfcAction::HUNT_ALL, 50 });
        return out;
    }
    bool const all = in.theirBases >= BASE_COUNT || in.distanceToEfc <= HUNT_ALL_YARDS;
    out.push_back({ EfcAction::HUNT_ALL, all ? 100 : 80 });
    out.push_back({ EfcAction::HUNT_TWO_CUT, in.theirBases >= BASE_COUNT ? 85 : all ? 90 : 100 });
    if (in.expertAvailable && in.haveTarget && (in.targetFresh || in.targetGuards <= NEUTRALIZE_MAX_GUARDS))
        out.push_back({ EfcAction::NEUTRALIZE_TARGET_BASE, 110 });
    out.push_back({ EfcAction::IGNORE, 30 });
    std::stable_sort(out.begin(), out.end(), [](EfcOption const& a, EfcOption const& b)
    {
        if (a.score != b.score)
            return a.score > b.score;
        return static_cast<std::uint8_t>(a.action) < static_cast<std::uint8_t>(b.action);
    });
    return out;
}

// ---------------------------------------------------------------------------------------------------------------------
// The duty of a free bot (not a guard, not a raider, not in a retreat) for each state of the flag.
// ---------------------------------------------------------------------------------------------------------------------
enum class Duty : std::uint8_t
{
    NONE,            // the node plan decides
    TAKE_FLAG,       // cast the pickup at the centre stand
    STAGE,           // stand off the stand (beyond the cast's 25 yd), ready to guard the taker
    ESCORT,          // within 12 yd of our carrier, peel its attackers
    HEAL_CARRIER,    // a healer follows our carrier
    HUNT,            // attack the enemy carrier
    CUT,             // wait for the enemy carrier at the base it runs to
    NEUTRALIZE,      // go into the circle of the base it runs to and neutralise it before it arrives
    CLICK_DROPPED    // click the dropped flag at once (an instant click)
};

inline char const* DutyName(Duty d)
{
    switch (d)
    {
        case Duty::NONE: return "none";
        case Duty::TAKE_FLAG: return "take-flag";
        case Duty::STAGE: return "stage";
        case Duty::ESCORT: return "escort";
        case Duty::HEAL_CARRIER: return "heal-carrier";
        case Duty::HUNT: return "hunt";
        case Duty::CUT: return "cut";
        case Duty::NEUTRALIZE: return "neutralize";
        case Duty::CLICK_DROPPED: return "click-dropped";
    }
    return "?";
}

constexpr std::uint32_t STAND_GROUP = 4;       // the bots nearest the stand: the taker and its cover
constexpr std::uint32_t ESCORTS = 2;
constexpr std::uint32_t HEALER_ESCORTS = 2;
constexpr std::uint32_t HUNTERS = 2;
constexpr std::uint32_t CUTTERS = 2;
constexpr std::uint32_t HUNTERS_ALL = 5;
constexpr std::uint32_t DROPPED_CLICKERS = 2;

struct DutyInput
{
    std::uint8_t flagState = FLAG_ON_BASE;
    bool weCarry = false;            // a friend (not the bot) carries it
    bool theyCarry = false;
    bool isHealer = false;
    bool free = true;                // not a guard, a raider or in a retreat
    bool yields = false;             // the plan is taking the fourth base or panicking: the flag waits
    Action action = Action::TAKE_NOW;
    EfcAction efc = EfcAction::HUNT_TWO_CUT;
    std::uint32_t rankStand = 99;    // place among the free bots by distance to the stand
    std::uint32_t rankCarrier = 99;  // ... to our carrier
    std::uint32_t rankHealer = 99;   // healers only, to our carrier
    std::uint32_t rankEfc = 99;      // ... to the enemy carrier
    std::uint32_t rankDropped = 99;  // ... to the dropped flag
    bool neutralizer = false;        // the expert chosen to go for the target base
};

inline Duty DutyFor(DutyInput const& in)
{
    if (!in.free)
        return Duty::NONE;

    // A dropped flag is an instant click and anyone's: the two nearest take it at once, whatever the plan is doing.
    if (in.flagState == FLAG_ON_GROUND)
        return in.rankDropped < DROPPED_CLICKERS ? Duty::CLICK_DROPPED : Duty::NONE;

    if (in.flagState == FLAG_ON_PLAYER && in.weCarry)
    {
        if (in.isHealer)
            return in.rankHealer < HEALER_ESCORTS ? Duty::HEAL_CARRIER : Duty::NONE;
        return in.rankCarrier < ESCORTS ? Duty::ESCORT : Duty::NONE;
    }

    if (in.flagState == FLAG_ON_PLAYER && in.theyCarry)
    {
        if (in.yields)
            return Duty::NONE;
        switch (in.efc)
        {
            case EfcAction::HUNT_ALL:
                return in.rankEfc < HUNTERS_ALL ? Duty::HUNT : Duty::NONE;
            case EfcAction::NEUTRALIZE_TARGET_BASE:
                if (in.neutralizer)
                    return Duty::NEUTRALIZE;
                [[fallthrough]];
            case EfcAction::HUNT_TWO_CUT:
                if (in.rankEfc < HUNTERS)
                    return Duty::HUNT;
                return in.rankEfc < HUNTERS + CUTTERS ? Duty::CUT : Duty::NONE;
            case EfcAction::IGNORE:
                return Duty::NONE;
        }
        return Duty::NONE;
    }

    // The flag on its stand or waiting to respawn after a capture: the bots nearest the stand are its taker and its cover.
    if (in.yields || in.action == Action::IGNORE)
        return Duty::NONE;
    if (in.rankStand >= STAND_GROUP)
        return Duty::NONE;
    bool const takes = in.action == Action::TAKE_NOW || in.action == Action::DENY_ONLY;
    if (takes && in.rankStand == 0 && in.flagState == FLAG_ON_BASE)
        return Duty::TAKE_FLAG;
    return Duty::STAGE;
}

// Duties that follow a moving target: the objective is refreshed while the duty lasts.
inline bool IsDynamic(Duty d)
{
    return d == Duty::ESCORT || d == Duty::HEAL_CARRIER || d == Duty::HUNT || d == Duty::CLICK_DROPPED;
}

}  // namespace EyFlag

#endif
