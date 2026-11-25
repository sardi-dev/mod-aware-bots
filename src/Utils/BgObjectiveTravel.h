#ifndef _PLAYERBOT_BGOBJECTIVETRAVEL_H
#define _PLAYERBOT_BGOBJECTIVETRAVEL_H

#include <cstdint>

namespace BgObjectiveTravel
{
constexpr float HOP_YARDS = 70.0f;

// Maps whose battleground has no waypoint routes: Battle for Gilneas (761) and Deepwind Gorge (1105).
inline bool UseHops(std::uint32_t mapId, float distance)
{
    return (mapId == 761 || mapId == 1105) && distance > HOP_YARDS;
}
}

#endif
