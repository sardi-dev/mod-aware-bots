/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_PVPBURSTPOLICY_H
#define _PLAYERBOT_PVPBURSTPOLICY_H

// BT-620: when a bot may spend its big offensive cooldowns on an enemy PLAYER (design decision 0032, owner decision 2026-11-19).
//
// Why this exists. The dungeon gates of the damage cooldowns ask for an elite, a boss or a pack of three attackers, so against
// a player (one target, no pack) Feral Berserk, Arcane Power, Mirror Image, Pillar of Frost and Demonology Metamorphosis never
// fired. The player guides want the opposite: hold the burst for a kill window (the enemy healer locked, the target held or
// without its medallion, the partner going in), then spend it all at once. The PvE gates stay as they are; this file only adds
// the window for a player target.
//
// Every decision is a list of candidate reasons with a deterministic integer score (design decision 0014 and 0026: a later selector may
// pick among them); the window is open when the best reason reaches the tier's threshold. Tier gating (fundamentals sheet 14):
//   - expert: understands everything, waits for a real window, and only falls back to a long fight after FALLBACK_EXPERT_MS;
//   - good:   understands the shared windows, the held target and a hurt target, falls back after FALLBACK_GOOD_MS;
//   - casual: spends the burst as soon as the target is a player (it does not read the fight).
//
// Free of game types, so tests/ can run it (tests/test_pvp_burst_policy.cpp).

#include "BotSkillMath.h"

#include <algorithm>
#include <cstdint>

namespace PvpBurst
{
using Tier = BotSkillMath::Tier;

enum class Reason : std::uint8_t
{
    NONE,
    IMMUNE,            // closed: the target cannot be hurt
    BREAKABLE_CC,      // closed: a hit would free the target from a crowd control
    DUEL,              // a duel or a lone fight: the whole fight is the window
    TEAM_WINDOW,       // the team's kill window is open (the enemy healer is locked, R6)
    PARTNER_BURST,     // a partner went in on this target a moment ago
    TARGET_CONTROLLED, // the target is stunned or rooted
    TRINKET_DOWN,      // the bot saw the target's medallion used and it is still on cooldown
    TARGET_LOW,        // the target is hurt: finish it
    LONG_FIGHT,        // the fight went on long enough that holding costs more than it gains
    OPPORTUNITY        // a casual bot: no window needed
};

constexpr std::int32_t SCORE_DUEL = 100;
constexpr std::int32_t SCORE_TEAM_WINDOW = 90;
constexpr std::int32_t SCORE_PARTNER_BURST = 85;
constexpr std::int32_t SCORE_TARGET_CONTROLLED = 70;
constexpr std::int32_t SCORE_TRINKET_DOWN = 65;
constexpr std::int32_t SCORE_TARGET_LOW = 55;
constexpr std::int32_t SCORE_LONG_FIGHT = 50;
constexpr std::int32_t SCORE_OPPORTUNITY = 40;

// The target counts as hurt below this health percentage.
constexpr std::int32_t TARGET_LOW_HP_PERCENT = 50;

// How long a bot of the tier holds its burst in a fight with no window before it spends it anyway.
constexpr std::uint32_t FALLBACK_EXPERT_MS = 12000;
constexpr std::uint32_t FALLBACK_GOOD_MS = 8000;

// A partner's burst counts for this long (the burst lasts 15 to 20 s; the go is shared for the opening half of it).
constexpr std::uint32_t PARTNER_BURST_HOLD_MS = 10000;

struct TierGate
{
    std::int32_t threshold;     // the best reason must reach this score
    bool readsImmunity;         // does not burst an immune target
    bool readsBreakableCc;      // does not burst a target held by a breakable crowd control
    bool readsTeam;             // understands the team window and the partner's burst
    bool readsTrinket;          // uses what it saw of the enemy medallion
    std::uint32_t fallbackMs;   // 0: never needs it (the opportunity score suffices)
};

inline TierGate GateOf(Tier tier)
{
    switch (tier)
    {
        case Tier::EXPERT:
            return { SCORE_LONG_FIGHT, true, true, true, true, FALLBACK_EXPERT_MS };
        case Tier::GOOD:
            return { SCORE_LONG_FIGHT, true, true, true, false, FALLBACK_GOOD_MS };
        case Tier::CASUAL:
            return { SCORE_OPPORTUNITY, false, false, false, false, 0 };
    }
    return GateOf(Tier::GOOD);
}

struct Inputs
{
    bool targetIsEnemyPlayer = false;  // an attackable, living enemy player
    bool immune = false;               // an immunity aura or a deflect on the target
    bool breakableCc = false;          // polymorph, sap, fear, sleep, banish or disorient on the target
    bool controlled = false;           // stunned or rooted (and not by a breakable crowd control)
    bool lone = false;                 // a duel or a lone 1v1 fight
    bool teamWindow = false;           // the shared kill window is open (R6)
    bool partnerBurst = false;         // a partner burst on this target within PARTNER_BURST_HOLD_MS
    bool trinketKnownDown = false;     // the bot's own book: the medallion was seen used and is still down
    std::int32_t targetHpPercent = 100;
    std::uint32_t fightMs = 0;         // how long the bot has fought this target
};

struct Verdict
{
    bool open = false;
    Reason reason = Reason::NONE;
    std::int32_t score = 0;
};

inline char const* ReasonName(Reason reason)
{
    switch (reason)
    {
        case Reason::IMMUNE: return "immune";
        case Reason::BREAKABLE_CC: return "breakable-cc";
        case Reason::DUEL: return "duel";
        case Reason::TEAM_WINDOW: return "team-window";
        case Reason::PARTNER_BURST: return "partner-burst";
        case Reason::TARGET_CONTROLLED: return "target-controlled";
        case Reason::TRINKET_DOWN: return "trinket-down";
        case Reason::TARGET_LOW: return "target-low";
        case Reason::LONG_FIGHT: return "long-fight";
        case Reason::OPPORTUNITY: return "opportunity";
        default: return "none";
    }
}

// The window for a burst on an enemy player. A target that is not an enemy player is not this file's business: the caller
// keeps its PvE gate and this returns closed with Reason::NONE.
inline Verdict Decide(Inputs const& in, Tier tier)
{
    Verdict verdict;
    if (!in.targetIsEnemyPlayer)
        return verdict;

    TierGate const gate = GateOf(tier);
    if (gate.readsImmunity && in.immune)
        return { false, Reason::IMMUNE, 0 };
    if (gate.readsBreakableCc && in.breakableCc)
        return { false, Reason::BREAKABLE_CC, 0 };

    Reason best = Reason::NONE;
    std::int32_t bestScore = 0;
    auto offer = [&](Reason reason, std::int32_t score)
    {
        // Ties go to the earlier reason in the enum (the stronger one), which the call order below already respects.
        if (score > bestScore)
        {
            best = reason;
            bestScore = score;
        }
    };

    if (in.lone)
        offer(Reason::DUEL, SCORE_DUEL);
    if (gate.readsTeam && in.teamWindow)
        offer(Reason::TEAM_WINDOW, SCORE_TEAM_WINDOW);
    if (gate.readsTeam && in.partnerBurst)
        offer(Reason::PARTNER_BURST, SCORE_PARTNER_BURST);
    if (in.controlled)
        offer(Reason::TARGET_CONTROLLED, SCORE_TARGET_CONTROLLED);
    if (gate.readsTrinket && in.trinketKnownDown)
        offer(Reason::TRINKET_DOWN, SCORE_TRINKET_DOWN);
    if (in.targetHpPercent <= TARGET_LOW_HP_PERCENT)
        offer(Reason::TARGET_LOW, SCORE_TARGET_LOW);
    if (gate.fallbackMs != 0 && in.fightMs >= gate.fallbackMs)
        offer(Reason::LONG_FIGHT, SCORE_LONG_FIGHT);
    if (tier == Tier::CASUAL)
        offer(Reason::OPPORTUNITY, SCORE_OPPORTUNITY);

    verdict.reason = best;
    verdict.score = bestScore;
    verdict.open = bestScore >= gate.threshold;
    return verdict;
}

// ---------------------------------------------------------------------------------------------------------------------
// Demonology Metamorphosis (the PvE trigger asks for a pack of three or a boss, 500 Fury and every Hand of Gul'dan charge spent).
// The guide: burst from 300 Fury with the trinket or Dark Soul up, keep the Hand of Gul'dan charges for the go.
// ---------------------------------------------------------------------------------------------------------------------
constexpr std::uint32_t META_PVP_ENTER_FURY = 300;
// The PvE trigger drops the form at 300 Fury; against a player the form is kept until the Fury is nearly gone, or the entry
// at 300 would be undone on the next tick.
constexpr std::uint32_t META_PVP_LEAVE_FURY = 60;

inline bool MetamorphosisEnterVsPlayer(Verdict const& window, std::uint32_t fury, bool alreadyMetamorphosed)
{
    return window.open && !alreadyMetamorphosed && fury >= META_PVP_ENTER_FURY;
}

inline bool MetamorphosisLeave(std::uint32_t fury, bool vsPlayer, std::uint32_t pveLeaveFury)
{
    return fury <= (vsPlayer ? META_PVP_LEAVE_FURY : pveLeaveFury);
}

// ---------------------------------------------------------------------------------------------------------------------
// Fire Presence of Mind. The guide spends it on an instant crowd control (Polymorph on the healer or an off damage dealer,
// Frostjaw on the target), never on a Pyroblast, which does a quarter less than a Hot Streak one.
// ---------------------------------------------------------------------------------------------------------------------
enum class PomRoute : std::uint8_t
{
    HOLD,       // keep it
    POLYMORPH,  // an instant Polymorph on the control target
    FROSTJAW    // an instant Frostjaw on the kill target
};

struct PomInputs
{
    bool windowOpen = false;        // the burst window on the kill target is open
    bool polymorphDue = false;      // the team's control plan wants a Polymorph now (R4) and the bot can cast it
    bool frostjawReady = false;     // the bot knows Frostjaw, it is ready and the kill target is in range
    bool targetAlreadyHeld = false; // the kill target is already stunned, rooted or silenced
};

inline PomRoute RoutePresenceOfMind(PomInputs const& in)
{
    // The control the team plan asked for wins: it is the link of a chain and a late link breaks the chain.
    if (in.polymorphDue)
        return PomRoute::POLYMORPH;
    // Frostjaw is the freeze setup of the burst, so it needs the window and a target that is not already held.
    if (in.windowOpen && in.frostjawReady && !in.targetAlreadyHeld)
        return PomRoute::FROSTJAW;
    return PomRoute::HOLD;
}
}  // namespace PvpBurst

#endif
