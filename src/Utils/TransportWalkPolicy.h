#pragma once

namespace TransportWalkPolicy
{
// Progress must be measured against a stable route point, even when movement looks ahead.
inline bool Improved(float best, float distance)
{
    return best < 0.0f || distance + 3.0f < best;
}

inline bool UseDockNode(bool alreadySelected, bool hasLandPoint, float landDistance, float reachedDistance)
{
    return alreadySelected || !hasLandPoint || landDistance < reachedDistance;
}
}
