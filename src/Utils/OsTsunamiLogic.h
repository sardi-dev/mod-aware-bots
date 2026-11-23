/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef AWARE_BOTS_OS_TSUNAMI_LOGIC_H
#define AWARE_BOTS_OS_TSUNAMI_LOGIC_H

#include <cmath>
#include <vector>

// Obsidian Sanctum, Sartharion's Flame Tsunami on the SkyFire 5.4.8 script: one wave creature per lane runs straight
// along X across the platform and casts Flame Tsunami (57491, 7 yard radius) every half second. A bot in a lane steps
// along Y to the nearer side of it that no other lane covers.
namespace OsTsunami
{
constexpr float DAMAGE_RADIUS = 7.0f;
// Lane half width the bot must leave: the damage radius plus room for the bot's own size and a late step
constexpr float LANE_HALF_WIDTH = 9.0f;
// Where to stand from a lane centre, a little past the half width so the bot does not stop on the edge
constexpr float SIDE_STEP = 11.0f;

// A wave moving towards +X has passed a bot that stands at a lower X, and the other way round.
inline bool WavePassed(float waveX, float botX, bool waveMovesTowardsPlusX)
{
    return waveMovesTowardsPlusX ? botX < waveX : botX > waveX;
}

inline bool InLane(float y, std::vector<float> const& laneYs)
{
    for (float lane : laneYs)
        if (std::fabs(y - lane) < LANE_HALF_WIDTH)
            return true;
    return false;
}

// True when botY lies in a lane; safeY is then the nearest spot beside that lane that no lane covers.
inline bool DodgeY(float botY, std::vector<float> const& laneYs, float& safeY)
{
    for (float lane : laneYs)
    {
        if (std::fabs(botY - lane) >= LANE_HALF_WIDTH)
            continue;

        float const towards = botY >= lane ? 1.0f : -1.0f;
        float const nearSide = lane + towards * SIDE_STEP;
        float const farSide = lane - towards * SIDE_STEP;
        if (!InLane(nearSide, laneYs))
            safeY = nearSide;
        else if (!InLane(farSide, laneYs))
            safeY = farSide;
        else
            return false;  // lanes too close together to stand between; keep the move to the edge to the caller
        return true;
    }
    return false;
}
}

#endif
