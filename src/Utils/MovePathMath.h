/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef AWARE_BOTS_MOVE_PATH_MATH_H
#define AWARE_BOTS_MOVE_PATH_MATH_H

#include <cmath>
#include <cstddef>

// BT-251 (owner 2026-11-16, "stop-and-go" and "up a mountain, down, up again"): the core's PathGenerator builds at most
// 74 smoothed points 4 yd apart and answers PATHFIND_SHORT for any longer route, which MoveFarTo refused. Every trip
// over about 250 yd of walking therefore fell back to the cone step: a random point 35 to 70 yd away within 90 degrees
// of the destination, with no look at the terrain. A long trip became a zig-zag of short legs over hills. MoveFarTo now
// asks for the route's corners (a straight path has a corner only where the route turns) and walks up to LEG_YARDS of
// that real route per leg. These helpers cut the corner list; any point type with x, y and z members works.
namespace MovePathMath
{
// One leg of a long trip. MoveTo checks the leg again with a smoothed path, so it must stay under the 74 x 4 yd cap with
// room for the corners the smoothing adds.
constexpr float LEG_YARDS = 180.0f;
// A trip this far away (straight line) takes the corner route; closer ones keep the smoothed path, which fits.
constexpr float LONG_TRIP_YARDS = 200.0f;
// A leg must bring the bot at least this much closer to the destination, or the route is no help (a dead end).
constexpr float MIN_PROGRESS_YARDS = 5.0f;

template <class P>
inline float Dist(P const& a, P const& b)
{
    float const dx = a.x - b.x;
    float const dy = a.y - b.y;
    float const dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// The point maxLength yards along the path from its first point. Returns false for a path of fewer than 2 points.
// cut: the path is longer than maxLength (the point lies between two corners, interpolated, its z only a guess);
// otherwise the point is the path's last one.
template <class P, class Points>
inline bool PointAlong(Points const& points, float maxLength, P& out, bool& cut)
{
    cut = false;
    if (points.size() < 2)
        return false;
    float walked = 0.0f;
    for (std::size_t i = 1; i < points.size(); ++i)
    {
        float const step = Dist(points[i - 1], points[i]);
        if (walked + step > maxLength)
        {
            if (step <= 0.0f)
                break;
            float const t = (maxLength - walked) / step;
            out = points[i - 1];
            out.x = points[i - 1].x + (points[i].x - points[i - 1].x) * t;
            out.y = points[i - 1].y + (points[i].y - points[i - 1].y) * t;
            out.z = points[i - 1].z + (points[i].z - points[i - 1].z) * t;
            cut = true;
            return true;
        }
        walked += step;
    }
    out = points[points.size() - 1];
    return true;
}

// Whether a leg ending at legEnd is worth walking toward a destination now botToDest yards away.
constexpr bool LegMakesProgress(float legEndToDest, float botToDest)
{
    return legEndToDest + MIN_PROGRESS_YARDS < botToDest;
}

// The index of the first path point (after the start) that passes `good`, or points.size() when none does. A healer
// walking around a corner to a tank it lost sight of stops at the first point it sees him from.
template <class Points, class Pred>
inline std::size_t FirstPointWhere(Points const& points, Pred good)
{
    for (std::size_t i = 1; i < points.size(); ++i)
        if (good(points[i]))
            return i;
    return points.size();
}
}  // namespace MovePathMath

#endif
