/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef AWARE_BOTS_HEIGAN_DANCE_LOGIC_H
#define AWARE_BOTS_HEIGAN_DANCE_LOGIC_H

#include <cstdint>

// Naxxramas, Heigan the Unclean: the phase clock of the safety dance (strategy/raids/Naxx/NaxxBossHelper.h), with the
// timings of the SkyFire 5.4.8 boss_heigan.cpp. Slow dance: 90 s, eruptions at 15 s then every 10 s. Fast dance: 45 s,
// eruptions at 8 s then every 3 s. The core erupts every section but one and moves the spared one 3, 2, 1, 0, 1, 2, 3
// from the start of each phase; waypoint 0 is section 3.
namespace HeiganDance
{
constexpr uint32_t WAYPOINT_COUNT = 4;

constexpr uint32_t SLOW_FIRST_ERUPTION_MS = 15000;
constexpr uint32_t SLOW_ERUPTION_INTERVAL_MS = 10000;
constexpr uint32_t SLOW_ERUPTION_COUNT = 8;
constexpr uint32_t SLOW_PHASE_DURATION_MS = 90000;
constexpr uint32_t FAST_FIRST_ERUPTION_MS = 8000;
constexpr uint32_t FAST_ERUPTION_INTERVAL_MS = 3000;
constexpr uint32_t FAST_ERUPTION_COUNT = 13;
constexpr uint32_t FAST_PHASE_DURATION_MS = 45000;
// Do not leave the current safe spot until this long after the computed eruption time (scheduler jitter)
constexpr uint32_t ERUPTION_SAFETY_MARGIN_MS = 300;

// Eruptions of the current phase that already happened (safety margin applied)
inline uint32_t EruptionIndex(bool fast, uint32_t phaseElapsedMs)
{
    uint32_t const first = fast ? FAST_FIRST_ERUPTION_MS : SLOW_FIRST_ERUPTION_MS;
    uint32_t const interval = fast ? FAST_ERUPTION_INTERVAL_MS : SLOW_ERUPTION_INTERVAL_MS;
    if (phaseElapsedMs < first + ERUPTION_SAFETY_MARGIN_MS)
        return 0;
    return (phaseElapsedMs - ERUPTION_SAFETY_MARGIN_MS - first) / interval + 1;
}

// Waypoint (0..3) to stand at now: 0, 1, 2, 3, 2, 1, 0, ... and 0 again after the phase's last eruption, the first
// safe spot of the next phase
inline uint32_t SafeWaypoint(bool fast, uint32_t phaseElapsedMs)
{
    uint32_t const index = EruptionIndex(fast, phaseElapsedMs);
    if (index >= (fast ? FAST_ERUPTION_COUNT : SLOW_ERUPTION_COUNT))
        return 0;
    uint32_t const r = index % (2 * (WAYPOINT_COUNT - 1));
    return r < WAYPOINT_COUNT ? r : 2 * (WAYPOINT_COUNT - 1) - r;
}

inline uint32_t MsUntilNextEruption(bool fast, uint32_t phaseElapsedMs)
{
    uint32_t const first = fast ? FAST_FIRST_ERUPTION_MS : SLOW_FIRST_ERUPTION_MS;
    uint32_t const interval = fast ? FAST_ERUPTION_INTERVAL_MS : SLOW_ERUPTION_INTERVAL_MS;
    uint32_t const count = fast ? FAST_ERUPTION_COUNT : SLOW_ERUPTION_COUNT;
    uint32_t const happened = phaseElapsedMs < first ? 0 : (phaseElapsedMs - first) / interval + 1;
    uint32_t next;
    if (happened >= count)
    {
        // No more eruptions this phase; the next one is the first of the following phase
        uint32_t const phaseLength = fast ? FAST_PHASE_DURATION_MS : SLOW_PHASE_DURATION_MS;
        uint32_t const nextFirst = fast ? SLOW_FIRST_ERUPTION_MS : FAST_FIRST_ERUPTION_MS;
        next = phaseLength + nextFirst;
    }
    else
        next = first + happened * interval;

    return next > phaseElapsedMs ? next - phaseElapsedMs : 0;
}
}

#endif
