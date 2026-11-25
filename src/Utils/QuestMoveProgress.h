#ifndef QUEST_MOVE_PROGRESS_H
#define QUEST_MOVE_PROGRESS_H

#include <cstdint>

namespace QuestMoveProgress
{
// A detour can refresh the stall clock, but cannot postpone recovery indefinitely.
constexpr uint32_t MAX_DETOUR_MS = 180000;
struct Watch
{
    float x = 0, y = 0, z = 0;
    float best = 0;
    bool hasTarget = false;
    uint32_t destinationProgressMs = 0;
};

inline void DestinationProgress(Watch& watch, uint32_t now)
{
    watch.destinationProgressMs = now;
}

inline bool LegProgress(Watch& watch, float x, float y, float z, float remaining, uint32_t now)
{
    if (!watch.hasTarget || x != watch.x || y != watch.y || z != watch.z)
    {
        watch.x = x; watch.y = y; watch.z = z;
        watch.best = remaining;
        watch.hasTarget = true;
        return false;
    }
    if (remaining + 5.0f >= watch.best)
        return false;
    watch.best = remaining;
    return uint32_t(now - watch.destinationProgressMs) < MAX_DETOUR_MS;
}

}
#endif
