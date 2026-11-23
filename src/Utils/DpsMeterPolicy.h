/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_DPSMETERPOLICY_H
#define _PLAYERBOT_DPSMETERPOLICY_H

// One damage dealer's fight, measured from the core's own damage hook (BotDamageWatch.cpp) and the bot's casts, free of
// game types so that tests/ can run it. The module has no damage meter, so "the damage class sheets are applied" could
// only be read from the code; the [DPSFIGHT] line this feeds makes each specialisation's damage per second, its
// cooldown use and its fight weight readable from a proof run's log (tools/dps_by_spec.py).

#include <array>
#include <cstddef>
#include <cstdint>

namespace DpsMeterPolicy
{
// A fight shorter than this says nothing about a rotation (a pull that ends in two hits, a bot that flickers in and out
// of combat at the edge of a pack), so it is not written.
constexpr std::uint32_t MIN_FIGHT_MS = 8000;

// Spells with at least this recovery time count as cooldowns: the fight line lists how often each was cast, which is
// how "cooldowns never used" and "cooldowns spent on trash" show up.
constexpr std::uint32_t COOLDOWN_MIN_MS = 60000;

constexpr std::size_t MAX_COOLDOWNS = 6;

struct CooldownCast
{
    std::uint32_t spell = 0;
    std::uint16_t casts = 0;
};

struct Fight
{
    bool active = false;
    std::uint32_t startMs = 0;
    // The bot's and its pet's damage counters when the fight began (the counters only grow, see BotDamageWatch.cpp).
    std::uint64_t damageBase = 0;
    std::uint64_t petBase = 0;
    std::uint16_t casts = 0;
    // The heaviest picture of the fight seen when a cooldown was cast: attackers on the group, elites among them, a boss.
    std::uint8_t attackers = 0;
    std::uint8_t elites = 0;
    bool boss = false;
    std::uint8_t cooldownKinds = 0;
    std::array<CooldownCast, MAX_COOLDOWNS> cooldowns{};
};

inline void Begin(Fight& fight, std::uint32_t nowMs, std::uint64_t damage, std::uint64_t petDamage)
{
    fight = Fight{};
    fight.active = true;
    fight.startMs = nowMs;
    fight.damageBase = damage;
    fight.petBase = petDamage;
}

inline void NoteSituation(Fight& fight, std::uint32_t attackers, std::uint32_t elites, bool boss)
{
    if (!fight.active)
        return;
    std::uint8_t const a = attackers > 255 ? 255 : static_cast<std::uint8_t>(attackers);
    std::uint8_t const e = elites > 255 ? 255 : static_cast<std::uint8_t>(elites);
    if (a > fight.attackers)
        fight.attackers = a;
    if (e > fight.elites)
        fight.elites = e;
    fight.boss = fight.boss || boss;
}

// Whether a cast should refresh the fight weight: the first cooldown cast of a spell, which is when a rotation's choice
// to spend it can be judged against the fight.
inline bool IsCooldown(std::uint32_t recoveryMs)
{
    return recoveryMs >= COOLDOWN_MIN_MS;
}

// Returns true when this cast is the first of its spell in the fight (the caller then reads the fight weight).
inline bool NoteCast(Fight& fight, std::uint32_t spell, std::uint32_t recoveryMs)
{
    if (!fight.active)
        return false;
    if (fight.casts < 65535)
        ++fight.casts;
    if (!IsCooldown(recoveryMs))
        return false;
    for (std::size_t i = 0; i < fight.cooldownKinds; ++i)
        if (fight.cooldowns[i].spell == spell)
        {
            if (fight.cooldowns[i].casts < 65535)
                ++fight.cooldowns[i].casts;
            return false;
        }
    if (fight.cooldownKinds >= MAX_COOLDOWNS)
        return false;
    fight.cooldowns[fight.cooldownKinds] = { spell, 1 };
    ++fight.cooldownKinds;
    return true;
}

struct Summary
{
    bool write = false;
    std::uint32_t seconds = 0;
    std::uint64_t damage = 0;
    std::uint64_t petDamage = 0;
    std::uint32_t dps = 0;
    std::uint32_t petDps = 0;
};

inline std::uint32_t PerSecond(std::uint64_t damage, std::uint32_t ms)
{
    return ms == 0 ? 0 : static_cast<std::uint32_t>(damage * 1000 / ms);
}

// Ends the fight. The counters are the bot's and its pet's damage totals now; a counter that went down (a reset) reads
// as no damage rather than a wrap.
inline Summary End(Fight& fight, std::uint32_t nowMs, std::uint64_t damage, std::uint64_t petDamage)
{
    Summary summary;
    if (!fight.active)
        return summary;
    fight.active = false;
    std::uint32_t const ms = nowMs - fight.startMs;
    summary.seconds = ms / 1000;
    summary.damage = damage >= fight.damageBase ? damage - fight.damageBase : 0;
    summary.petDamage = petDamage >= fight.petBase ? petDamage - fight.petBase : 0;
    summary.dps = PerSecond(summary.damage, ms);
    summary.petDps = PerSecond(summary.petDamage, ms);
    // A long fight with no damage is written too: a damage dealer that stood idle while the group fought is the case
    // the line exists to find.
    summary.write = ms >= MIN_FIGHT_MS;
    return summary;
}
}  // namespace DpsMeterPolicy

#endif
