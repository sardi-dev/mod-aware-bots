/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * BT-157: the mana guardrails and the pull decision of the proactive healer, free of game types so that tests/ can run
 * them. The owner's rule (2026-11-12): upkeep on the tank before and during every pull while mana is above 30 percent;
 * upkeep on other members only above 60 percent mana or when they take damage; at 30 percent or below only reactive
 * heals, and the healer asks the group for time to drink.
 */

#ifndef _PLAYERBOT_HEALERGUARDRAILS_H
#define _PLAYERBOT_HEALERGUARDRAILS_H

#include <cstddef>
#include <cstdint>

namespace HealerGuardrails
{
// At or below this mana percentage a healer only answers damage (reactive heals) and asks for time to drink.
constexpr std::uint32_t REACTIVE_ONLY_MANA_PCT = 30;
// Above this mana percentage the healer tops up any hurt member, not only the ones that just took damage.
constexpr std::uint32_t PROACTIVE_MEMBER_MANA_PCT = 60;
// A member counts as "took damage" for this long after the healer saw his health drop.
constexpr std::uint32_t RECENT_DAMAGE_WINDOW_MS = 6000;
// A hurt member below this health percentage is worth a damage over time heal in the proactive band.
constexpr std::uint32_t PROACTIVE_HEALTH_CEILING_PCT = 98;
// How long the healer prepares the tank for one pull target before it stops (a long wait is a group that drinks).
constexpr std::uint32_t PRE_PULL_WINDOW_MS = 20000;
// How far from the tank a mob may stand to count as the next pull, in yards (the range of the tank's pull).
constexpr float PRE_PULL_TARGET_RANGE = 40.0f;

// BT-175: a member under this health percentage for this long without a heal is a heal that did not happen.
// Deadmines run of 2026-11-13: the leader fell from 94 to 0 percent in 47 s with the healer 11 yd away and no heal, and in
// the run before it stood at 33 yd (the heal distance is 25) for 41 s. The healer walks to regain sight or range and the
// log says why it did not heal.
constexpr std::uint32_t UNHEALED_HEALTH_PCT = 60;
constexpr std::uint32_t UNHEALED_SECONDS = 5;
constexpr bool MemberStaysHurt(std::uint32_t healthPct, std::uint32_t hurtSeconds)
{
    return healthPct < UNHEALED_HEALTH_PCT && hurtSeconds >= UNHEALED_SECONDS;
}
// Why the healer cannot heal a member now ("" when it can): sight and range first, then mana.
constexpr char const* NoHealReason(bool inSight, bool inHealRange, std::uint32_t manaPct)
{
    return !inSight ? "no line of sight" : !inHealRange ? "out of heal range" : manaPct <= 5 ? "no mana" : "";
}
// The healer walks to such a member only for the first two reasons.
constexpr bool ReasonNeedsWalk(bool inSight, bool inHealRange) { return !inSight || !inHealRange; }

// Upkeep on the tank (shield, damage over time heal) needs more than the reactive-only mana.
constexpr bool TankUpkeepAllowed(std::uint32_t manaPct) { return manaPct > REACTIVE_ONLY_MANA_PCT; }

// Upkeep on a member who is not the tank. Below the reactive-only mana never; above the proactive mana always; between
// the two only for a member who took damage a moment ago.
constexpr bool MemberProactiveAllowed(std::uint32_t manaPct, bool tookDamageRecently)
{
    return manaPct > REACTIVE_ONLY_MANA_PCT && (manaPct > PROACTIVE_MEMBER_MANA_PCT || tookDamageRecently);
}

// The healer tells the group it needs a drink at or below the reactive-only mana.
constexpr bool ShouldAskToDrink(std::uint32_t manaPct) { return manaPct <= REACTIVE_ONLY_MANA_PCT; }

// What the healer knows before a pull.
struct PrePullFacts
{
    bool hasTank;               // the group has a main tank in sight of the healer
    bool tankInCombat;          // the fight has begun: the combat upkeep takes over
    bool markOnFreshHostile;    // the group's mark is on a living hostile that is not fighting yet, near the tank
    bool tankTargetsFreshHostile;  // the tank has such a mob selected
};

// The next pull is about to start: the tank is out of combat and either the leader's mark or the tank's own target is a
// hostile that has not been engaged.
constexpr bool PullImminent(PrePullFacts const& facts)
{
    return facts.hasTank && !facts.tankInCombat && (facts.markOnFreshHostile || facts.tankTargetsFreshHostile);
}

// Bounds the preparation of one pull target in time, so a tank that keeps a far mob selected while the group drinks does not
// make the healer renew the shield every fifteen seconds.
class PrePullWindow
{
public:
    // Call with the pull target's id while a pull is imminent and 0 when it is not. True while the healer may still prepare.
    bool Allows(std::uint64_t targetId, std::uint32_t nowMs)
    {
        if (!targetId)
        {
            m_targetId = 0;
            return false;
        }
        if (targetId != m_targetId)
        {
            m_targetId = targetId;
            m_firstSeenMs = nowMs;
        }
        return nowMs - m_firstSeenMs < PRE_PULL_WINDOW_MS;
    }

private:
    std::uint64_t m_targetId = 0;
    std::uint32_t m_firstSeenMs = 0;
};

// Remembers the last health the healer saw per member and whether it dropped lately. The healer reads the heal target only
// (the value it already computes), so there is no scan of the group: a drop of a member it is not looking at is not seen.
class DamageWindow
{
public:
    static constexpr std::size_t CAPACITY = 8;

    // True when this member's health went down within the last RECENT_DAMAGE_WINDOW_MS.
    bool Observe(std::uint64_t memberId, std::uint32_t healthPct, std::uint32_t nowMs)
    {
        Entry* entry = Find(memberId);
        if (!entry || nowMs - entry->seenMs > RECENT_DAMAGE_WINDOW_MS)
        {
            entry = entry ? entry : Claim();
            *entry = Entry{ memberId, healthPct, nowMs, 0, false, true };
            return false;
        }

        if (healthPct < entry->healthPct)
        {
            entry->dropped = true;
            entry->droppedMs = nowMs;
        }
        entry->healthPct = healthPct;
        entry->seenMs = nowMs;
        return entry->dropped && nowMs - entry->droppedMs <= RECENT_DAMAGE_WINDOW_MS;
    }

private:
    struct Entry
    {
        std::uint64_t id = 0;
        std::uint32_t healthPct = 0;
        std::uint32_t seenMs = 0;
        std::uint32_t droppedMs = 0;
        bool dropped = false;
        bool used = false;
    };

    Entry* Find(std::uint64_t id)
    {
        for (Entry& entry : m_entries)
            if (entry.used && entry.id == id)
                return &entry;
        return nullptr;
    }

    // A free slot, else the member seen the longest ago.
    Entry* Claim()
    {
        Entry* oldest = &m_entries[0];
        for (Entry& entry : m_entries)
        {
            if (!entry.used)
                return &entry;
            if (entry.seenMs < oldest->seenMs)
                oldest = &entry;
        }
        return oldest;
    }

    Entry m_entries[CAPACITY];
};

// BT-295: dispels run at ACTION_DISPEL (50), above every heal (critical heals at 30), so a healer cleansed a poison off a
// damage dealer while a member stood at 15 percent. A player heals the dying member first. A dispel waits while a member
// the healer can heal now is under this health; a dispel on the tank never waits (a crowd control on the tank loses the
// pack).
constexpr float TRIAGE_HEALTH_PCT = 35.0f;
constexpr bool DispelWaits(float lowestHealthPct, bool lowestHealableNow, bool dispelOnTank)
{
    return lowestHealableNow && lowestHealthPct < TRIAGE_HEALTH_PCT && !dispelOnTank;
}

// BT-192: the mistweaver's fallback auto attack "failed" on most idle ticks (Deadmines run 4: 101 samples, run 5: 31):
// with no target, a target the bot already attacks, or, in a group, one out of melee reach where the healer must not
// walk. It is tried only when it can start something. A solo monk keeps the old reach.
constexpr bool MeleeFallbackWorthTrying(bool hasTarget, bool alreadyAttacking, bool inGroup, bool inMeleeRange)
{
    return hasTarget && !alreadyAttacking && (!inGroup || inMeleeRange);
}
}  // namespace HealerGuardrails

#endif
