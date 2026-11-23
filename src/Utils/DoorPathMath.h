#ifndef MOD_AWARE_BOTS_DOOR_PATH_MATH_H
#define MOD_AWARE_BOTS_DOOR_PATH_MATH_H

#include <cmath>
#include <cstddef>

// BT-158: retreat along the path, including earlier segments if the hit is close to a corner.
namespace DoorPathMath
{
template<class Points, class Point>
void StopShort(Points& path, std::size_t segment, Point hit, float distance)
{
    path.erase(path.begin() + segment + 1, path.end());
    while (true)
    {
        Point const start = path.back();
        float const dx = hit.x - start.x, dy = hit.y - start.y, dz = hit.z - start.z;
        float const length = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (length > distance && length > 0.0f)
        {
            float const t = (length - distance) / length;
            path.push_back(Point(start.x + dx * t, start.y + dy * t, start.z + dz * t));
            return;
        }
        distance -= length;
        if (path.size() == 1)
            return;
        hit = start;
        path.pop_back();
    }
}
}
#endif
