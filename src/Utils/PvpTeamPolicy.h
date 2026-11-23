/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_PVPTEAMPOLICY_H
#define _PLAYERBOT_PVPTEAMPOLICY_H

// BT-628 to BT-631: the shared team state of an arena or battleground skirmish (rules R4 and R6 of docs/pvp, design decision 0032,
// owner decisions 2026-11-19): who holds the enemy healer, when the kill window is open, which bot casts which control, and how
// the partners go in together. Pure data and decisions, free of game types, so tests/ can run it (tests/test_pvp_team_policy.cpp);
// the game side (PvP/PvpTeam.cpp) fills the inputs from what a player sees (auras, unit states, the bot's own awareness book).
//
// What is shared and what is not. The team table holds the plan of the side (the open window, the control offers and the pending
// control casts), the same thing the arena plan's shared focus already is. It never holds what one bot saw of the enemy: a bot
// reads the healer's medallion and the diminishing returns from its OWN book (PvpAwarenessPolicy.h) and passes the answer in. The
// state of the healer's crowd control is public (auras) and is read directly.
//
// Every decision is a candidate with a deterministic integer score (design decision 0014 and 0026); ties go to the lower member id.
// Tier gating (fundamentals sheet 14): experts do all of it, good bots the first link and the shared window, casual bots none.

#include "BotSkillMath.h"
#include "PvpAwarenessPolicy.h"
#include "PvpSpellCatalog.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace PvpTeam
{
using BotSkillMath::Tier;
using PvpSpells::DrCategory;

// ---------------------------------------------------------------------------------------------------------------------
// The kill window (rule R6): open while the enemy healer is locked for 4 s or more, or locked with no medallion to break it, and
// the kill target has no immunity (owner, 2026-11-19).
// ---------------------------------------------------------------------------------------------------------------------
constexpr std::uint32_t WINDOW_MIN_LOCK_MS = 4000;
// With the medallion seen used the lock cannot be shortened, so a lock that long already holds for its whole time.
constexpr std::uint32_t WINDOW_NO_TRINKET_MIN_LOCK_MS = 1500;
// An opened window stays shared this long (the burst of the guides lasts 15 to 20 s; the opening half is the go).
constexpr std::uint32_t GO_HOLD_MS = 12000;

enum class WindowReason : std::uint8_t
{
    NONE,
    NO_HEALER,          // nobody to lock: the local burst rules judge the fight
    TARGET_IMMUNE,      // the kill target cannot be hurt
    NOT_LOCKED,         // the healer is free
    LOCKED,             // locked for WINDOW_MIN_LOCK_MS or more
    LOCKED_NO_TRINKET   // locked and the medallion was seen used
};

struct WindowInputs
{
    bool healerPresent = false;
    std::uint32_t healerLockedMs = 0;   // the longest time the healer cannot heal: hard control or silence left on it
    bool healerTrinketDown = false;     // the OBSERVER's own book: the medallion was seen used and is still down
    bool targetImmune = false;          // the kill target
};

struct WindowVerdict
{
    bool open = false;
    WindowReason reason = WindowReason::NONE;
};

inline char const* WindowReasonName(WindowReason reason)
{
    switch (reason)
    {
        case WindowReason::NO_HEALER: return "no-healer";
        case WindowReason::TARGET_IMMUNE: return "target-immune";
        case WindowReason::NOT_LOCKED: return "not-locked";
        case WindowReason::LOCKED: return "locked";
        case WindowReason::LOCKED_NO_TRINKET: return "locked-no-trinket";
        default: return "none";
    }
}

// Casual bots do not read a window; a good bot reads the lock time only; an expert also reads the medallion.
inline WindowVerdict EvaluateWindow(WindowInputs const& in, Tier tier)
{
    if (tier == Tier::CASUAL)
        return {};
    if (!in.healerPresent)
        return { false, WindowReason::NO_HEALER };
    if (in.targetImmune)
        return { false, WindowReason::TARGET_IMMUNE };
    if (in.healerLockedMs >= WINDOW_MIN_LOCK_MS)
        return { true, WindowReason::LOCKED };
    if (tier == Tier::EXPERT && in.healerTrinketDown && in.healerLockedMs >= WINDOW_NO_TRINKET_MIN_LOCK_MS)
        return { true, WindowReason::LOCKED_NO_TRINKET };
    return { false, WindowReason::NOT_LOCKED };
}

// The window the side opened, kept for GO_HOLD_MS so that every member sees the same go (a window that was open a moment ago is
// still the go even when the lock has run down: the burst is under way).
struct Go
{
    bool open = false;
    std::uint32_t targetLow = 0;
    std::uint32_t openedMs = 0;
    WindowReason reason = WindowReason::NONE;
};

inline bool GoActive(Go const& go, std::uint32_t targetLow, std::uint32_t nowMs)
{
    return go.open && go.targetLow == targetLow && nowMs - go.openedMs < GO_HOLD_MS;
}

// Opens (or keeps) the go on this target. Returns true when this call opened it.
inline bool OpenGo(Go& go, std::uint32_t targetLow, std::uint32_t nowMs, WindowReason reason)
{
    if (GoActive(go, targetLow, nowMs))
        return false;
    go.open = true;
    go.targetLow = targetLow;
    go.openedMs = nowMs ? nowMs : 1;
    go.reason = reason;
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// Partners going in together (rule R6): a partner's burst on the same target within PARTNER_HOLD_MS counts for the bot, and a
// bot that joins later than ALIGN_MS after the first one is late (logged, never blocked: the cooldowns of the partners are not
// known to each other, so the go is shared the moment it opens and each member goes as soon as its own burst is ready).
// ---------------------------------------------------------------------------------------------------------------------
constexpr std::uint32_t ALIGN_MS = 1500;
constexpr std::uint32_t PARTNER_HOLD_MS = 10000;

inline std::uint32_t SkewMs(std::uint32_t firstAtMs, std::uint32_t otherAtMs)
{
    return firstAtMs > otherAtMs ? firstAtMs - otherAtMs : otherAtMs - firstAtMs;
}

inline bool Aligned(std::uint32_t firstAtMs, std::uint32_t otherAtMs)
{
    return SkewMs(firstAtMs, otherAtMs) <= ALIGN_MS;
}

// A partner burst, as the game side reads it off the partner's auras: how long ago it started.
inline bool PartnerBurstCounts(std::uint32_t partnerBurstAgeMs)
{
    return partnerBurstAgeMs < PARTNER_HOLD_MS;
}

// ---------------------------------------------------------------------------------------------------------------------
// Control and its diminishing returns (rule R4).
// ---------------------------------------------------------------------------------------------------------------------
// A link shorter than this is not worth a global cooldown and a trinket bait.
constexpr std::uint32_t MIN_USEFUL_CC_MS = 2500;
// The long crowd controls are capped at 8 s against players.
constexpr std::uint32_t PVP_CC_CAP_MS = 8000;
// A cast that is pending (started, not landed yet) blocks a second one on the same target and category for this long.
constexpr std::uint32_t PENDING_MS = 2500;
// BT-663: an instant control lands with the cast, so it blocks a second one only for the moment the aura needs to show (a stun of
// 3 s must not hold the next link back for 2.5 s of it).
constexpr std::uint32_t INSTANT_PENDING_MS = 600;

// Slack after the cast time at which the next link is cast so that no gap opens (the tier's reaction).
inline std::uint32_t LeadMs(std::uint32_t castMs, Tier tier)
{
    switch (tier)
    {
        case Tier::EXPERT: return castMs + 250;
        case Tier::GOOD: return castMs + 600;
        case Tier::CASUAL: return castMs + 1200;
    }
    return castMs + 600;
}

// The duration an effect of base length lands with at the DR step (0 full, 1 half, 2 quarter, 3 immune), after the PvP cap.
inline std::uint32_t EffectiveMs(std::uint32_t baseMs, std::uint32_t drStep)
{
    std::uint32_t const capped = std::min(baseMs, PVP_CC_CAP_MS);
    return capped * PvpAware::DrPercent(drStep) / 100;
}

struct ControlCandidate
{
    std::uint32_t member = 0;       // the casting bot (guid low)
    std::uint32_t spellId = 0;
    DrCategory category = DrCategory::NONE;
    std::uint32_t baseMs = 0;
    std::uint32_t castMs = 0;       // 0: instant
    bool breakable = false;         // a hit frees the target (sheep, fear, blind ...)
    std::uint32_t drStep = 0;       // the bot's own reading of the category on the target
    // BT-650: a ground control (Ring of Frost) lands this long after the cast on whoever still stands in it. Only a target that is
    // already held that long is a sure catch, so such a control is a link of an expert's chain and never an opener (0: lands at once).
    std::uint32_t armMs = 0;
};

struct ControlTarget
{
    std::uint32_t lockedMs = 0;                  // hard control left on the target (0: free)
    DrCategory lockCategory = DrCategory::NONE;  // the category of the longest running hard control
    bool enemyKickReady = false;                 // the observer's book: a kick that can stop a cast is not seen used
};

constexpr std::int32_t NOT_DUE = -1;
constexpr std::int32_t CONTROL_PER_100MS = 1;       // 8 s of lock are worth 80
constexpr std::int32_t CONTROL_NEW_CATEGORY = 20;   // a link of another category than the one running: no diminishing return
constexpr std::int32_t CONTROL_SAME_CATEGORY = -30;
constexpr std::int32_t CONTROL_INSTANT_VS_KICK = 15;
constexpr std::int32_t CONTROL_CAST_VS_KICK = -10;
// An armed link is cast while the running lock still holds it for the arming time and this much longer (a tick or two of slack).
constexpr std::uint32_t ARMED_LINK_WINDOW_MS = 1200;

// The score of casting this control on the target now, or NOT_DUE: the link is not worth it (immune step, too short) or not due
// yet (the running lock lasts longer than the cast needs, so the link would be wasted or overlap).
inline std::int32_t ScoreControl(ControlCandidate const& c, ControlTarget const& t, Tier tier)
{
    if (tier == Tier::CASUAL || c.spellId == 0 || c.category == DrCategory::NONE)
        return NOT_DUE;
    if (c.drStep >= PvpAware::DR_IMMUNE_STEP)
        return NOT_DUE;
    std::uint32_t const effective = EffectiveMs(c.baseMs, c.drStep);
    if (effective < MIN_USEFUL_CC_MS)
        return NOT_DUE;
    // The first link is cast on a free target; a later link when the running one is about to end. A good bot has no chain
    // (it casts only on a free target), an expert chains.
    if (c.armMs > 0)
    {
        if (tier != Tier::EXPERT || t.lockedMs < c.armMs || t.lockedMs > c.armMs + ARMED_LINK_WINDOW_MS)
            return NOT_DUE;
    }
    else if (t.lockedMs > 0 && (tier != Tier::EXPERT || t.lockedMs > LeadMs(c.castMs, tier)))
        return NOT_DUE;

    std::int32_t score = std::int32_t(effective / 100) * CONTROL_PER_100MS;
    if (t.lockedMs > 0 || t.lockCategory != DrCategory::NONE)
        score += c.category != t.lockCategory ? CONTROL_NEW_CATEGORY : CONTROL_SAME_CATEGORY;
    else
        score += CONTROL_NEW_CATEGORY;
    if (t.enemyKickReady)
        score += c.castMs == 0 ? CONTROL_INSTANT_VS_KICK : CONTROL_CAST_VS_KICK;
    return score;
}

// ---------------------------------------------------------------------------------------------------------------------
// Offers and reservations: the members publish their best control for a target, the best one executes, and a cast that is on its
// way blocks the others (no two bots waste control into each other or into the diminishing return).
// ---------------------------------------------------------------------------------------------------------------------
constexpr std::uint32_t OFFER_TTL_MS = 800;
constexpr std::size_t MAX_OFFERS = 16;
constexpr std::size_t MAX_PENDING = 16;

struct Offer
{
    std::uint32_t member = 0;
    std::uint32_t targetLow = 0;
    std::uint32_t spellId = 0;
    DrCategory category = DrCategory::NONE;
    std::int32_t score = 0;
    std::uint32_t atMs = 0;
};

struct Pending
{
    std::uint32_t member = 0;
    std::uint32_t targetLow = 0;
    DrCategory category = DrCategory::NONE;
    std::uint32_t atMs = 0;
    bool hard = true;   // a control that holds the target (not a silence or a root)
    std::uint32_t holdMs = PENDING_MS;   // how long the cast counts as on its way
};

inline bool Fresh(std::uint32_t atMs, std::uint32_t nowMs, std::uint32_t ttlMs)
{
    return nowMs - atMs < ttlMs;
}

class ControlBook
{
public:
    // A member publishes (or refreshes) its best option for a target.
    void Publish(PvpTeam::Offer const& offer, std::uint32_t nowMs)
    {
        Expire(nowMs);
        for (PvpTeam::Offer& o : m_offers)
            if (o.member == offer.member)
            {
                o = offer;
                return;
            }
        if (m_offers.size() >= MAX_OFFERS)
            m_offers.erase(m_offers.begin());
        m_offers.push_back(offer);
    }

    void Withdraw(std::uint32_t member)
    {
        m_offers.erase(std::remove_if(m_offers.begin(), m_offers.end(), [&](PvpTeam::Offer const& o) { return o.member == member; }),
            m_offers.end());
    }

    // The member whose offer on this target is best (score, then the lower member id) among the fresh ones: the executor.
    bool IsExecutor(std::uint32_t member, std::uint32_t targetLow, std::uint32_t nowMs) const
    {
        PvpTeam::Offer const* best = nullptr;
        for (PvpTeam::Offer const& o : m_offers)
        {
            if (o.targetLow != targetLow || !Fresh(o.atMs, nowMs, OFFER_TTL_MS))
                continue;
            if (!best || o.score > best->score || (o.score == best->score && o.member < best->member))
                best = &o;
        }
        return best && best->member == member;
    }

    // Is a cast of this category on this target already on its way, from another member or from this one (a bot whose cast bar is
    // still filling must not start a second one)? Or, for a hard control, any hard control on its way (a second hard control on
    // the same target while one is pending wastes one of them).
    bool Blocked(std::uint32_t /*member*/, std::uint32_t targetLow, DrCategory category, bool hard, std::uint32_t nowMs) const
    {
        for (Pending const& p : m_pending)
        {
            if (p.targetLow != targetLow || !Fresh(p.atMs, nowMs, p.holdMs))
                continue;
            if (p.category == category || (hard && p.hard))
                return true;
        }
        return false;
    }

    // castMs: the cast time of the control (0: instant, counted as on its way for INSTANT_PENDING_MS only; the default counts a cast).
    void NoteCast(std::uint32_t member, std::uint32_t targetLow, DrCategory category, bool hard, std::uint32_t nowMs,
        std::uint32_t castMs = PENDING_MS)
    {
        Expire(nowMs);
        if (m_pending.size() >= MAX_PENDING)
            m_pending.erase(m_pending.begin());
        m_pending.push_back({ member, targetLow, category, nowMs ? nowMs : 1, hard, castMs == 0 ? INSTANT_PENDING_MS : PENDING_MS });
        Withdraw(member);
    }

    void Expire(std::uint32_t nowMs)
    {
        m_offers.erase(std::remove_if(m_offers.begin(), m_offers.end(),
            [&](PvpTeam::Offer const& o) { return !Fresh(o.atMs, nowMs, OFFER_TTL_MS * 4); }), m_offers.end());
        m_pending.erase(std::remove_if(m_pending.begin(), m_pending.end(),
            [&](Pending const& p) { return !Fresh(p.atMs, nowMs, PENDING_MS); }), m_pending.end());
    }

    std::size_t OfferCount() const { return m_offers.size(); }
    std::size_t PendingCount() const { return m_pending.size(); }

private:
    std::vector<PvpTeam::Offer> m_offers;
    std::vector<Pending> m_pending;
};

// ---------------------------------------------------------------------------------------------------------------------
// Not breaking a breakable control with damage (rule R4): a damage cast at a target that a team member holds in a sheep, a fear or
// a sleep frees it. Casual bots do not look.
// ---------------------------------------------------------------------------------------------------------------------
constexpr std::uint32_t HOLD_DAMAGE_MIN_LEFT_MS = 800;

inline bool HoldDamage(bool targetInBreakableCc, std::uint32_t ccLeftMs, bool actionDamages, Tier tier)
{
    return tier != Tier::CASUAL && targetInBreakableCc && ccLeftMs >= HOLD_DAMAGE_MIN_LEFT_MS && actionDamages;
}

// ---------------------------------------------------------------------------------------------------------------------
// What the team state hands the rest of the module.
// ---------------------------------------------------------------------------------------------------------------------
// The arena plan's focus score reads two facts (ArenaPlanMath::FocusInput): whether the burst window is open and whether the
// enemy healer is held (the other targets then count for more).
constexpr std::uint32_t HEALER_HELD_MIN_MS = 2500;

inline bool HealerHeld(std::uint32_t healerLockedMs)
{
    return healerLockedMs >= HEALER_HELD_MIN_MS;
}

// The state a side keeps for its members (one per match and side, map thread only; see PvpMatchState.h).
struct TeamState
{
    Go go;
    ControlBook control;
};
}  // namespace PvpTeam

#endif
