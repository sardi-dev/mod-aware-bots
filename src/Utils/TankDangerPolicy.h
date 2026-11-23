/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * BT-292: one danger reading per tank (role sheet tank.md, gap 8), free of game types so tests/ can run it. Every tank
 * defensive read only the tank's own health (Shield Wall 40, Ardent Defender 40, Icebound Fortitude 50, ...): right for
 * a slow fight, late for a burst, where a tank at 90 percent dies to one cast chain before the health rule fires. A
 * player tank presses a cooldown when the pull looks bad, not when the bar is already low.
 */

#ifndef _PLAYERBOT_TANKDANGERPOLICY_H
#define _PLAYERBOT_TANKDANGERPOLICY_H

#include <cstddef>
#include <cstdint>

namespace TankDangerPolicy
{
// Mobs on the tank at once: three is a big trash pack, five a pack plus a patrol.
constexpr unsigned MANY_ATTACKERS = 3;
constexpr unsigned SWARM_ATTACKERS = 5;
// Health points lost (peak in the window minus now) within DROP_WINDOW_MS: a quarter is a hard hitting pack, two fifths
// a burst no heal can catch up with.
constexpr float FAST_DROP_PCT = 25.0f;
constexpr float BURST_DROP_PCT = 40.0f;
constexpr std::uint32_t DROP_WINDOW_MS = 5000;
constexpr unsigned MAX_DANGER_LEVEL = 3;

struct Reading
{
    unsigned attackersOnTank = 0;   // hostile units whose victim is the tank
    float healthDropPct = 0.0f;     // HealthDropWindow
    bool healerAvailable = true;    // a living healer of the group in heal range and sight of the tank
    bool bossOnTank = false;        // a dungeon or world boss has the tank as its victim
};

// 0 calm, 1 heavy, 2 dangerous, 3 deadly. Each fact adds points; none alone makes a fight deadly.
constexpr unsigned Level(Reading const& r)
{
    unsigned points = 0;
    points += r.attackersOnTank >= SWARM_ATTACKERS ? 2u : r.attackersOnTank >= MANY_ATTACKERS ? 1u : 0u;
    points += r.healthDropPct >= BURST_DROP_PCT ? 2u : r.healthDropPct >= FAST_DROP_PCT ? 1u : 0u;
    points += r.healerAvailable ? 0u : 1u;
    points += r.bossOnTank ? 1u : 0u;
    return points > MAX_DANGER_LEVEL ? MAX_DANGER_LEVEL : points;
}

// The small defensive (Shield Barrier, Divine Protection, Rune Tap, Barkskin, Elusive Brew): about 10 points earlier
// than the class rules at level 1, early at 2, at once at 3.
constexpr float SMALL_AT_LEVEL1_PCT = 75.0f;
constexpr float SMALL_AT_LEVEL2_PCT = 90.0f;
constexpr bool SmallDefensive(float healthPct, unsigned level)
{
    return (level >= 1 && healthPct < SMALL_AT_LEVEL1_PCT) || (level >= 2 && healthPct < SMALL_AT_LEVEL2_PCT) ||
        level >= 3;
}

// The big defensive (Shield Wall, Ardent Defender, Icebound Fortitude, Survival Instincts, Fortifying Brew): at 60
// percent when it is dangerous (the class rules wait for 40 to 50), at 80 when it is deadly.
constexpr float BIG_AT_LEVEL2_PCT = 60.0f;
constexpr float BIG_AT_LEVEL3_PCT = 80.0f;
constexpr bool BigDefensive(float healthPct, unsigned level)
{
    return (level >= 2 && healthPct < BIG_AT_LEVEL2_PCT) || (level >= 3 && healthPct < BIG_AT_LEVEL3_PCT);
}

// Two big defensives at once waste the second: after one, the next waits this long unless the fight is deadly.
constexpr std::uint32_t BIG_GAP_MS = 10000;
constexpr bool BigAllowed(std::uint32_t lastBigMs, std::uint32_t nowMs, unsigned level)
{
    return !lastBigMs || level >= MAX_DANGER_LEVEL || nowMs - lastBigMs >= BIG_GAP_MS;
}

// A healer's external on the tank (Pain Suppression, Guardian Spirit, Ironbark, Life Cocoon, Hand of Sacrifice): the
// class rules cast it at 30 to 40 percent; with a dangerous reading at 50, with a deadly one at 70.
constexpr float EXTERNAL_AT_LEVEL2_PCT = 50.0f;
constexpr float EXTERNAL_AT_LEVEL3_PCT = 70.0f;
constexpr bool ExternalOnTank(float healthPct, unsigned level)
{
    return (level >= 2 && healthPct < EXTERNAL_AT_LEVEL2_PCT) || (level >= 3 && healthPct < EXTERNAL_AT_LEVEL3_PCT);
}
// Hand of Sacrifice (6940) moves 30 percent of the tank's damage to the paladin for 12 s: the holy paladin gives it only
// while she can carry that share herself (half her health or more) and never on herself.
constexpr float SACRIFICE_OWN_HEALTH_PCT = 50.0f;
constexpr bool HandOfSacrificeSafe(float ownHealthPct, bool tankIsSelf)
{
    return !tankIsSelf && ownHealthPct >= SACRIFICE_OWN_HEALTH_PCT;
}
// A healer reads the tank's level only while it is this fresh (the tank writes it every half second in a fight).
constexpr std::uint32_t READING_FRESH_MS = 2000;
constexpr bool ReadingFresh(std::uint32_t readAtMs, std::uint32_t nowMs)
{
    return readAtMs != 0 && nowMs - readAtMs <= READING_FRESH_MS;
}

// The health the tank lost lately: the highest health seen within DROP_WINDOW_MS minus the health now. Fed from the
// tank's own reading of the value, so it costs no hook and no scan; a hit healed back before the next reading is not
// seen, which is right here: what kills a tank is the health it does not get back.
class HealthDropWindow
{
public:
    static constexpr std::size_t CAPACITY = 16;
    // Readings closer than this are folded into the last one, so the window always spans its full time.
    static constexpr std::uint32_t MIN_GAP_MS = DROP_WINDOW_MS / CAPACITY;

    float Observe(float healthPct, std::uint32_t nowMs)
    {
        if (m_count && nowMs - m_samples[Last()].ms < MIN_GAP_MS)
        {
            Sample& last = m_samples[Last()];
            if (healthPct > last.healthPct)
                last.healthPct = healthPct;
        }
        else
        {
            m_samples[m_next] = Sample{ nowMs, healthPct };
            m_next = (m_next + 1) % CAPACITY;
            if (m_count < CAPACITY)
                ++m_count;
        }

        float peak = healthPct;
        for (std::size_t i = 0; i < m_count; ++i)
            if (nowMs - m_samples[i].ms <= DROP_WINDOW_MS && m_samples[i].healthPct > peak)
                peak = m_samples[i].healthPct;
        return peak - healthPct;
    }

    void Reset() { m_count = 0; m_next = 0; }

private:
    struct Sample
    {
        std::uint32_t ms = 0;
        float healthPct = 0.0f;
    };

    std::size_t Last() const { return (m_next + CAPACITY - 1) % CAPACITY; }

    Sample m_samples[CAPACITY];
    std::size_t m_count = 0;
    std::size_t m_next = 0;
};
}  // namespace TankDangerPolicy

#endif
