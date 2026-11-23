/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * BT-256: a random bot on the activity rotation that a real player walks up to slept out its last PassiveDelay (10 s)
 * plus up to 5 s of the AllowActivity cache before its first full tick. Each real player's manager now looks around it
 * once a second and wakes the bots inside the force radius that still hold an "inactive" answer. Free of game types.
 */

#ifndef _PLAYERBOT_ACTIVITYWAKEPOLICY_H
#define _PLAYERBOT_ACTIVITYWAKEPOLICY_H

#include <cstdint>

namespace ActivityWakePolicy
{
// One look around a real player per second: a walking player covers 7 yd in that time, well inside the 150 yd radius.
constexpr std::uint32_t WAKE_SCAN_MS = 1000;

constexpr bool ScanDue(std::uint32_t lastScanMs, std::uint32_t nowMs, bool radiusEnabled)
{
    return radiusEnabled && (lastScanMs == 0 || nowMs - lastScanMs >= WAKE_SCAN_MS);
}

// A bot is woken when its cached answer said "inactive" and a fresh one says "active". A running cast keeps its own
// wait (the duel and battleground wakes do the same).
constexpr bool Wake(bool cachedActive, bool activeNow, bool casting)
{
    return !cachedActive && activeNow && !casting;
}
}  // namespace ActivityWakePolicy

#endif
