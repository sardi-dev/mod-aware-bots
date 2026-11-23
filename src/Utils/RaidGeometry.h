/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 *
 * Plane geometry for raid strategies (Sunwell Plateau): cone tests around a boss and the step that takes a bot out of a
 * circle or a cone; and the heading and lane matches that tell where a seen moving unit goes (Al'ar, Illidan, Eye
 * Blast). Header-only and free of core types, so tests/ can check it.
 */

#ifndef PLAYERBOTS_RAIDGEOMETRY_H
#define PLAYERBOTS_RAIDGEOMETRY_H

#include <cmath>
#include <vector>

namespace RaidGeometry
{
inline constexpr float PI = 3.14159265358979f;

struct Point
{
    float x;
    float y;
};

// The angle in -pi..pi.
inline float SignedAngle(float angle)
{
    angle = std::fmod(angle, 2.0f * PI);
    if (angle > PI)
        angle -= 2.0f * PI;
    else if (angle < -PI)
        angle += 2.0f * PI;
    return angle;
}

// How far the point lies off the boss's facing, 0 (straight ahead) to pi (straight behind).
inline float OffFacing(Point boss, float facing, Point point)
{
    float const toPoint = std::atan2(point.y - boss.y, point.x - boss.x);
    return std::fabs(SignedAngle(toPoint - facing));
}

// Inside the frontal cone of the given full width (a 90 degree cone is 45 degrees to each side).
inline bool InFrontCone(Point boss, float facing, Point point, float coneWidth)
{
    return OffFacing(boss, facing, point) <= coneWidth / 2.0f;
}

// Inside the cone of the given full width behind the boss.
inline bool InRearCone(Point boss, float facing, Point point, float coneWidth)
{
    return OffFacing(boss, facing, point) >= PI - coneWidth / 2.0f;
}

// The point at `distance` from the boss, `offAngle` off his facing on the side nearer to `from` (left or right).
inline Point FlankPoint(Point boss, float facing, float offAngle, float distance, Point from)
{
    Point const left{ boss.x + std::cos(facing + offAngle) * distance, boss.y + std::sin(facing + offAngle) * distance };
    Point const right{ boss.x + std::cos(facing - offAngle) * distance, boss.y + std::sin(facing - offAngle) * distance };
    float const toLeft = std::hypot(left.x - from.x, left.y - from.y);
    float const toRight = std::hypot(right.x - from.x, right.y - from.y);
    return toLeft <= toRight ? left : right;
}

struct Circle
{
    Point center;
    float radius;
};

inline bool InAnyCircle(Point point, std::vector<Circle> const& circles)
{
    for (Circle const& circle : circles)
        if (std::hypot(point.x - circle.center.x, point.y - circle.center.y) < circle.radius)
            return true;
    return false;
}

// The nearest point to `from` that lies outside every circle: rings of 24 points every `step` yards out to
// `maxDistance`. False when every candidate is inside a circle.
inline bool NearestPointOutside(Point from, std::vector<Circle> const& circles, float step, float maxDistance, Point& out)
{
    constexpr int angles = 24;
    for (float distance = step; distance <= maxDistance + 0.001f; distance += step)
    {
        bool found = false;
        float best = 0.0f;
        for (int i = 0; i < angles; ++i)
        {
            float const angle = 2.0f * PI * float(i) / float(angles);
            Point const candidate{ from.x + std::cos(angle) * distance, from.y + std::sin(angle) * distance };
            if (InAnyCircle(candidate, circles))
                continue;

            // On one ring every candidate is equally far from the bot; prefer the one farthest from the circles' edge.
            float clearance = 1000.0f;
            for (Circle const& circle : circles)
                clearance = std::fmin(clearance,
                    std::hypot(candidate.x - circle.center.x, candidate.y - circle.center.y) - circle.radius);
            if (!found || clearance > best)
            {
                out = candidate;
                best = clearance;
                found = true;
            }
        }

        if (found)
            return true;
    }

    return false;
}

// The point straight away from `hazard` at `distance` from it; a bot standing on the hazard steps out along +x.
inline Point AwayFrom(Point hazard, Point from, float distance)
{
    float const dx = from.x - hazard.x;
    float const dy = from.y - hazard.y;
    float const length = std::hypot(dx, dy);
    if (length < 0.01f)
        return { hazard.x + distance, hazard.y };
    return { hazard.x + dx / length * distance, hazard.y + dy / length * distance };
}

struct Point3
{
    float x;
    float y;
    float z;
};

// The cosine between a unit heading and the direction from `from` to `to`; -1 when the points coincide, as nothing
// lies ahead there.
inline float HeadingCosine(Point3 from, Point3 heading, Point3 to)
{
    float const tx = to.x - from.x;
    float const ty = to.y - from.y;
    float const tz = to.z - from.z;
    float const distance = std::sqrt(tx * tx + ty * ty + tz * tz);
    if (distance < 0.01f)
        return -1.0f;
    return (heading.x * tx + heading.y * ty + heading.z * tz) / distance;
}

// The index of the target straight ahead of a heading: the best cosine, at least minCosine; -1 for none.
inline int BestHeadingMatch(Point3 from, Point3 heading, std::vector<Point3> const& targets, float minCosine)
{
    int best = -1;
    float bestCosine = minCosine;
    for (size_t i = 0; i < targets.size(); ++i)
    {
        float const cosine = HeadingCosine(from, heading, targets[i]);
        if (cosine >= bestCosine)
        {
            bestCosine = cosine;
            best = static_cast<int>(i);
        }
    }
    return best;
}

struct Lane
{
    Point start;
    Point end;
};

// The lane a unit moves along: its direction (any length) within acos(minCosine) of the lane's and it stands less than
// maxOffLane off the lane's line; the nearest such lane, or -1.
inline int MatchLane(Point at, Point direction, std::vector<Lane> const& lanes, float maxOffLane, float minCosine)
{
    float const directionLength = std::hypot(direction.x, direction.y);
    if (directionLength < 0.01f)
        return -1;

    int best = -1;
    float bestOffLane = maxOffLane;
    for (size_t i = 0; i < lanes.size(); ++i)
    {
        float const laneX = lanes[i].end.x - lanes[i].start.x;
        float const laneY = lanes[i].end.y - lanes[i].start.y;
        float const laneLength = std::hypot(laneX, laneY);
        if (laneLength < 0.01f)
            continue;

        if ((direction.x * laneX + direction.y * laneY) / (directionLength * laneLength) < minCosine)
            continue;

        float const relX = at.x - lanes[i].start.x;
        float const relY = at.y - lanes[i].start.y;
        float const offLane = std::fabs(relX * laneY - relY * laneX) / laneLength;
        if (offLane < bestOffLane)
        {
            bestOffLane = offLane;
            best = static_cast<int>(i);
        }
    }
    return best;
}
}

#endif
