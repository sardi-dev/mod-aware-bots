/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef AWARE_BOTS_ACTIVITY_SCALE_POLICY_H
#define AWARE_BOTS_ACTIVITY_SCALE_POLICY_H

#include <cstdint>

// The smart activity scale (botActiveAloneSmartScale) thins the random bots' rotation when the world lags (BT-370).
// It used to read the longest world tick of the last 500 to 1,000 ticks: one slow tick (a map load, a save) above the
// 200 ms ceiling stopped every rotating bot for up to a minute. On this realm one tick in eight takes 100 ms or more
// (world update average 53 ms, 2026-11-17), so a tick above 200 ms came in nearly every table and the rotation stood
// at zero: no random bot away from a player quested at all. The scale now reads the mean tick of the last full table,
// which follows the load the bots add without reacting to single stalls.
namespace ActivityScale
{
constexpr uint32_t TABLE_TICKS = 500;

// AC's formula: full rotation up to the floor, none above the ceiling, linear in between.
inline uint32_t Scale(uint32_t mod, uint32_t lagMs, uint32_t floorMs, uint32_t ceilingMs)
{
    if (ceilingMs <= floorMs)
        return lagMs > ceilingMs ? 0 : mod;
    if (lagMs > ceilingMs)
        return 0;
    if (lagMs <= floorMs)
        return mod;
    double const progress = double(lagMs - floorMs) / double(ceilingMs - floorMs);
    return uint32_t(double(mod) * (1.0 - progress));
}

// Mean world tick over tables of TABLE_TICKS diffs. Lag() is the mean of the last full table, or of the ticks seen so
// far before the first table is full.
struct Table
{
    uint64_t sum = 0;
    uint32_t count = 0;
    uint32_t lastMean = 0;
    bool full = false;

    void Add(uint32_t diff)
    {
        sum += diff;
        if (++count < TABLE_TICKS)
            return;
        lastMean = uint32_t(sum / count);
        full = true;
        sum = 0;
        count = 0;
    }

    uint32_t Lag() const
    {
        if (full)
            return lastMean;
        return count ? uint32_t(sum / count) : 0;
    }
};
}  // namespace ActivityScale

#endif
