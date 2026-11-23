/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_PVPDEFENSIVEPOLICY_H
#define _PLAYERBOT_PVPDEFENSIVEPOLICY_H

// BT-522, rule R2 of the class rules map (fundamentals sheet topics 2 and 9): the defensive cooldowns and the breaks from
// control in a PvP fight, by tier and role.
//
// Defensives. A player spends the smaller defensive EARLY in an enemy go, while health is still high, one per go, and keeps the
// immunity for last (Divine Shield, Ice Block, Deterrence). So:
//   - a "go" is on when somebody hits the bot and it lost 20 percent of its health in 3 s, or two enemies hit it;
//   - under a go a tempo defensive goes at the tier's line (expert 80 percent, good 65, casual 35); without a go at 50, 45, 35;
//   - an immunity goes only at 30 percent (casual 25), or when the bot would die within about two seconds;
//   - one defensive per go: not while one runs or within 4 s of the last, unless death is predicted; then the immunity;
//   - a defensive that a silence blocks (Divine Protection) goes before a silence lands, when the bot expects one (expert);
//   - Evasion answers melee, Cloak of Shadows and Anti-Magic Shell casters;
//   - the shortest cooldown first within the tempo defensives (the smaller one).
//   A good bot and a casual bot sometimes press it late (a mistake on purpose: the line sinks by 15 points for that go).
//
// Breaks. When the bot is stunned, feared, polymorphed or rooted it picks the cheapest way out that removes the effect: a
// narrow class spell first (Berserker Rage, Lichborne), then a racial or class spell with its own cooldown (Will of the
// Forsaken, Unbound Will, Escape Artist), then the racial that shares the medallion's cooldown, and the medallion last. The
// medallion is a decision by role:
//   - a healer trinkets during the enemy's kill window (a teammate at or under 40 percent, or enemy burst running) and
//     otherwise eats the control; it also trinkets when the control would take it to 35 percent;
//   - a damage dealer trinkets a control when the enemy healer is locked for 2 s more and its kill target is at or under 35
//     percent, or when it is in danger (it would be at or under 35 percent when the control ends);
//   - an expert holds the medallion when the enemy has a follow-up control of a fresh category ready, unless it would die;
//   - an effect with under 1.2 s left is eaten; a mistake on purpose trinkets at full health.
// A movement impairment alone (a root or a snare) is broken by Escape Artist or the medallion only when the bot has somewhere
// to be (a kited ranged bot, a chasing melee bot, a carrier).
//
// Pure logic, free of game types (tests/test_pvp_defensive.cpp). The glue is strategy/generic/PvpDefense.cpp.

#include "BotSkillMath.h"
#include "PvpAwarenessPolicy.h"
#include "PvpSpellCatalog.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace PvpDefense
{
using Tier = BotSkillMath::Tier;
using PvpSpells::Fit;
using PvpSpells::Kind;

// A go: this much of the bot's health lost in 3 s while somebody hits it, or this many enemies on it.
constexpr float GO_LOSS_PERCENT = 20.0f;
constexpr std::uint32_t GO_ATTACKERS = 2;
// One defensive per go: no second while one runs or within this long of the last, unless death is predicted.
constexpr std::uint32_t GO_WINDOW_MS = 4000;
// A mistake on purpose: the line sinks by this many points for that go (a late press).
constexpr float LATE_PRESS_DROP = 15.0f;
// An expert expecting a silence presses a silence-blocked defensive this many points above its line.
constexpr float SILENCE_EARLY_RISE = 10.0f;
// Death is predicted when this share of the current health was lost in the last three seconds.
constexpr float PREDICTED_LETHAL_SHARE = 0.8f;

struct Lines
{
    float tempoUnderGo;  // a tempo defensive under a go at or under this health
    float tempoQuiet;    // without a go
    float immunity;      // an immunity at or under this health
};

inline Lines LinesOf(Tier tier)
{
    switch (tier)
    {
        case Tier::EXPERT: return { 80.0f, 50.0f, 30.0f };
        case Tier::GOOD: return { 65.0f, 45.0f, 30.0f };
        case Tier::CASUAL: return { 35.0f, 35.0f, 25.0f };
    }
    return { 65.0f, 45.0f, 30.0f };
}

struct Candidate
{
    std::uint32_t spellId = 0;
    Kind kind = Kind::DEFENSIVE_TEMPO;
    Fit fit = Fit::ALL;
    bool silenceBlocked = false;
    std::uint32_t cooldownMs = 0;
    bool ready = false;  // known, off cooldown, not running and the core accepts the cast now
};

struct DefFacts
{
    Tier tier = Tier::GOOD;
    std::uint32_t guid = 0;
    std::uint32_t goKey = 0;           // identifies the go (the mistake roll is fixed per go)
    float hpPct = 100.0f;
    float lossPct3s = 0.0f;            // health lost in the last three seconds, percent of the maximum
    std::uint32_t attackers = 0;       // enemies hitting the bot now
    bool meleeThreat = false;          // a melee enemy is on it
    bool casterThreat = false;         // a caster is on it
    std::uint32_t sinceLastDefensiveMs = 0xFFFFFFFFu;  // 0xFFFFFFFF: none yet
    bool defensiveRunning = false;     // one of its defensives is active now
    bool silenceExpected = false;      // a silence is coming (cast at it, or a silencer in reach with its silence not seen used)
};

struct DefPick
{
    bool use = false;
    int index = -1;                    // into the candidates
    std::int32_t score = 0;
    char const* why = "none";
};

inline bool GoOn(DefFacts const& f)
{
    return f.attackers >= 1 && (f.lossPct3s >= GO_LOSS_PERCENT || f.attackers >= GO_ATTACKERS);
}

inline bool PredictedLethal(DefFacts const& f)
{
    return f.attackers >= 1 && f.hpPct > 0.0f && f.lossPct3s >= f.hpPct * PREDICTED_LETHAL_SHARE;
}

inline bool FitsThreat(Fit fit, bool meleeThreat, bool casterThreat)
{
    switch (fit)
    {
        case Fit::ALL: return true;
        case Fit::PHYSICAL: return meleeThreat;
        case Fit::MAGIC: return casterThreat;
    }
    return true;
}

// Which of the candidates to press, when `due` says a defensive is wanted now by the caller's own rule (an arena plan or the
// engine's health line); the rest of the rules (one per go, immunity last, the fit, the shortest first) still apply.
inline DefPick Pick(DefFacts const& f, std::vector<Candidate> const& candidates, bool due)
{
    DefPick none;
    if (f.hpPct <= 0.0f)
        return none;

    bool const lethal = PredictedLethal(f);
    // One per go.
    if ((f.defensiveRunning || f.sinceLastDefensiveMs < GO_WINDOW_MS) && !lethal)
    {
        none.why = "one-per-go";
        return none;
    }

    Lines lines = LinesOf(f.tier);
    // A good or casual bot presses late in some of its goes: one stable roll per go.
    if (f.tier != Tier::EXPERT && PvpAware::MakesMistake(f.tier, f.guid, f.goKey))
    {
        lines.tempoUnderGo -= LATE_PRESS_DROP;
        lines.tempoQuiet -= LATE_PRESS_DROP;
    }

    bool const go = GoOn(f);
    float const tempoLine = go ? lines.tempoUnderGo : lines.tempoQuiet;

    // Immunity: last. Only low, or when death is predicted.
    bool const immunityNow = f.hpPct <= lines.immunity || lethal;

    int bestTempo = -1;
    int bestImmunity = -1;
    for (std::size_t i = 0; i < candidates.size(); ++i)
    {
        Candidate const& c = candidates[i];
        if (!c.ready || !FitsThreat(c.fit, f.meleeThreat, f.casterThreat))
            continue;
        if (c.kind == Kind::DEFENSIVE_IMMUNITY)
        {
            if (bestImmunity < 0)
                bestImmunity = static_cast<int>(i);
            continue;
        }
        if (c.kind != Kind::DEFENSIVE_TEMPO)
            continue;
        // A defensive that a silence blocks is pressed before the silence lands (expert).
        float line = tempoLine;
        if (f.tier == Tier::EXPERT && c.silenceBlocked && f.silenceExpected && f.attackers >= 1)
            line += SILENCE_EARLY_RISE;
        bool const wanted = due || f.hpPct <= line;
        if (!wanted)
            continue;
        // The shortest cooldown first: the smaller defensive goes before the big one.
        if (bestTempo < 0 || c.cooldownMs < candidates[static_cast<std::size_t>(bestTempo)].cooldownMs)
            bestTempo = static_cast<int>(i);
    }

    if (lethal && bestImmunity >= 0)
        return { true, bestImmunity, 900, "predicted-lethal-immunity" };
    if (bestTempo >= 0)
        return { true, bestTempo, go ? 800 : 700, go ? "tempo-under-go" : "tempo-low" };
    if (immunityNow && bestImmunity >= 0)
        return { true, bestImmunity, 850, "immunity-last" };
    return none;
}

// ---------------------------------------------------------------------------------------------------------------------
// Breaks from control.
// ---------------------------------------------------------------------------------------------------------------------

// An effect with less than this left is eaten (the spell and the reaction would cost more than the time it saves).
constexpr std::uint32_t MIN_BREAK_MS = 1200;
// The medallion for a healer needs at least this much of the effect left.
constexpr std::uint32_t HEALER_MIN_BREAK_MS = 3000;
// Health at or under which a teammate puts the enemy in its kill window.
constexpr float TEAMMATE_LOW_HP = 40.0f;
// A damage dealer trinkets for a kill when the enemy healer is held this much longer and the kill target is this low.
constexpr std::uint32_t HEALER_HELD_MS = 2000;
constexpr float KILL_TARGET_LOW_HP = 35.0f;
// Danger: the bot would be at or under this health when the effect ends. Lethal: at or under this.
constexpr float DANGER_HP = 35.0f;
constexpr float LETHAL_HP = 20.0f;
// The loss of health in a go that makes the enemy's burst "running".
constexpr float BURST_LOSS_PERCENT = 20.0f;

struct BreakCandidate
{
    std::uint32_t spellId = 0;
    std::uint32_t breaks = 0;    // the CcKind mask it removes
    std::uint8_t cost = 0;       // BREAK_COST_*
    bool ready = false;
};

struct BreakFacts
{
    Tier tier = Tier::GOOD;
    std::uint32_t guid = 0;
    std::uint32_t effectKey = 0;       // identifies the effect (the mistake roll is fixed per effect)

    std::uint32_t kinds = 0;           // the CcKind mask on the bot
    std::uint32_t worstKind = 0;       // the hard kind with the longest time left (0: only movement impairment)
    std::uint32_t remainingMs = 0;     // of the worst hard kind, or of the movement impairment when nothing is hard
    float hpPct = 100.0f;
    float lossPctPerSec = 0.0f;        // recent health loss, percent of the maximum per second

    bool isHealer = false;
    bool teammateLow = false;          // a teammate at or under 40 percent
    bool enemyBurst = false;           // the enemy's damage is running (a go on the bot or on a teammate)
    bool enemyHealerHeld = false;  // the enemy healer is controlled for 2 s more
    float killTargetHp = 100.0f;
    bool followUpPossible = false;     // the awareness book says the enemy has a fresh-category control ready (expert only)
    bool needsToMove = false;          // a movement impairment matters (kited, chasing, carrying)
};

struct BreakPick
{
    bool use = false;
    int index = -1;
    char const* why = "none";
};

inline float ProjectedHp(BreakFacts const& f)
{
    return f.hpPct - f.lossPctPerSec * (static_cast<float>(f.remainingMs) / 1000.0f);
}

// Whether the bot wants to spend the medallion (or the racial that shares its cooldown) on this effect.
inline bool WantsMedallion(BreakFacts const& f)
{
    float const projected = ProjectedHp(f);
    bool const danger = projected <= DANGER_HP;
    bool const lethal = projected <= LETHAL_HP;

    if (lethal)
        return true;
    // An expert holds it for a follow-up effect it cannot shrug off.
    if (f.tier == Tier::EXPERT && f.followUpPossible)
        return false;

    bool const wantsByRole = f.isHealer
        ? ((f.teammateLow || f.enemyBurst) && f.remainingMs >= HEALER_MIN_BREAK_MS) || danger
        : (f.enemyHealerHeld && f.killTargetHp <= KILL_TARGET_LOW_HP) || danger;
    if (wantsByRole)
        return true;
    // A mistake on purpose: the medallion at full health, on an effect that did not need it.
    return PvpAware::MakesMistake(f.tier, f.guid, f.effectKey);
}

inline BreakPick PickBreak(BreakFacts const& f, std::vector<BreakCandidate> const& candidates)
{
    BreakPick none;
    std::uint32_t const hard = f.kinds & PvpSpells::CC_HARD;
    std::uint32_t const moving = f.kinds & (PvpSpells::CC_ROOT | PvpSpells::CC_SNARE);
    if (hard == 0 && moving == 0)
        return none;
    if (f.remainingMs < MIN_BREAK_MS)
    {
        none.why = "eat-short";
        return none;
    }
    // Movement impairment alone: only when the bot has somewhere to be.
    std::uint32_t target = f.worstKind;
    if (hard == 0)
    {
        if (!f.needsToMove)
        {
            none.why = "eat-slow";
            return none;
        }
        target = moving & PvpSpells::CC_ROOT ? PvpSpells::CC_ROOT : PvpSpells::CC_SNARE;
    }
    if (target == 0)
        target = hard;

    // The cheapest ready candidate that removes the worst effect; the medallion family only when the role wants it.
    int best = -1;
    for (std::size_t i = 0; i < candidates.size(); ++i)
    {
        BreakCandidate const& c = candidates[i];
        if (!c.ready || !(c.breaks & target))
            continue;
        if (best < 0 || c.cost < candidates[static_cast<std::size_t>(best)].cost)
            best = static_cast<int>(i);
    }
    if (best < 0)
    {
        none.why = "no-break";
        return none;
    }
    BreakCandidate const& c = candidates[static_cast<std::size_t>(best)];
    if (c.cost >= PvpSpells::BREAK_COST_SHARED && !WantsMedallion(f))
    {
        none.why = f.followUpPossible && f.tier == Tier::EXPERT ? "hold-for-followup" : "eat-it";
        return none;
    }
    return { true, best, c.cost >= PvpSpells::BREAK_COST_SHARED ? "medallion" : "cheap-break" };
}

// ---------------------------------------------------------------------------------------------------------------------
// The recent loss of health: samples of (time, health) kept by the glue; the policy reads the rates.
// ---------------------------------------------------------------------------------------------------------------------
struct HealthSample
{
    std::uint32_t atMs = 0;
    float hpPct = 100.0f;
};

// The health lost over the last `windowMs`: the highest sample inside the window less the newest.
inline float LossOver(std::vector<HealthSample> const& samples, std::uint32_t nowMs, std::uint32_t windowMs)
{
    if (samples.empty())
        return 0.0f;
    float highest = samples.back().hpPct;
    for (HealthSample const& s : samples)
        if (nowMs - s.atMs <= windowMs)
            highest = std::max(highest, s.hpPct);
    return std::max(0.0f, highest - samples.back().hpPct);
}
}  // namespace PvpDefense

#endif
