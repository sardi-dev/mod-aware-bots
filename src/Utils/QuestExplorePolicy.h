/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef AWARE_BOTS_QUEST_EXPLORE_POLICY_H
#define AWARE_BOTS_QUEST_EXPLORE_POLICY_H

#include <cmath>
#include <cstdint>
#include <vector>

// BT-372: exploration quests ("The Forgotten Pools", "The Fargodeep Mine", "Investigate An'daroth") complete when the
// player enters an area trigger: the game client sees the trigger and sends CMSG_AREATRIGGER, and the core credits the
// quest of that trigger (areatrigger_involvedrelation). A bot has no client, so no random bot ever finished one; on
// 2026-11-17 80 such entries sat in the bots' logs, unchanged since build 5. The bot now walks into the trigger of its
// open exploration objective and sends the packet a client would send there; the core's own handler checks the
// position and gives the credit.
namespace QuestExplorePolicy
{
// The trigger's shape from AreaTrigger.dbc: a sphere when radius > 0, else a box turned by orientation.
struct Shape
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float radius = 0.0f;
    float boxX = 0.0f;
    float boxY = 0.0f;
    float boxZ = 0.0f;
    float boxOrientation = 0.0f;
};

// Inside the shape, with the same geometry as the core's HandleAreaTriggerOpcode but without its 5 yd tolerance, so
// the handler never refuses a packet the bot sends.
inline bool Inside(Shape const& shape, float px, float py, float pz)
{
    if (shape.radius > 0.0f)
    {
        float const dx = px - shape.x;
        float const dy = py - shape.y;
        float const dz = pz - shape.z;
        return dx * dx + dy * dy + dz * dz <= shape.radius * shape.radius;
    }

    double const rotation = 2 * M_PI - shape.boxOrientation;
    double const sinVal = std::sin(rotation);
    double const cosVal = std::cos(rotation);
    float const distX = px - shape.x;
    float const distY = py - shape.y;
    float const rotX = float(distX * cosVal - distY * sinVal);
    float const rotY = float(distY * cosVal + distX * sinVal);
    return std::fabs(rotX) <= shape.boxX / 2 && std::fabs(rotY) <= shape.boxY / 2 &&
           std::fabs(pz - shape.z) <= shape.boxZ / 2;
}

// At the trigger's centre (2d) this long without being inside it: the trigger is out of reach (a ledge, a roof, a
// cave the path does not enter). The quest goes to the retry book.
constexpr float ARRIVE_DIST = 8.0f;
// Within ARRIVE_DIST the bot steps onto the centre (small spheres sit partly above the ground).
constexpr float CENTRE_DIST = 1.5f;
constexpr uint32_t ARRIVAL_WAIT_MS = 60 * 1000;
// Triggers farther than this are not this zone's: the quest POI walk takes the bot nearer first.
constexpr float MAX_TRIP_DIST = 1500.0f;

// Index of the nearest shape within MAX_TRIP_DIST (2d), or -1.
inline int Nearest(std::vector<Shape> const& shapes, float px, float py)
{
    int best = -1;
    float bestSq = MAX_TRIP_DIST * MAX_TRIP_DIST;
    for (std::size_t i = 0; i < shapes.size(); ++i)
    {
        float const dx = shapes[i].x - px;
        float const dy = shapes[i].y - py;
        float const sq = dx * dx + dy * dy;
        if (sq <= bestSq)
        {
            best = int(i);
            bestSq = sq;
        }
    }
    return best;
}
}  // namespace QuestExplorePolicy

#endif
