/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_THREATMATH_H
#define _PLAYERBOT_THREATMATH_H

// The threat percentage of ThreatValue::Calculate (ThreatValues.cpp), free of game types so that tests/ can run it.

#include <cstdint>

namespace ThreatMath
{
// BT-161: an untouched target must not indefinitely block damage dealers.
inline std::uint8_t Ratio(float botThreat, float maxTankThreat)
{
    if (maxTankThreat <= 0)
        return 0;

    float const ratio = botThreat * 100 / maxTankThreat;
    if (ratio >= 255.0f)
        return 255;
    return ratio <= 0.0f ? 0 : std::uint8_t(ratio);
}
}  // namespace ThreatMath

#endif
