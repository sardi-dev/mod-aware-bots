/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_TRAILMATH_H
#define _PLAYERBOT_TRAILMATH_H

// The arithmetic of the dungeon trail follow (Formations.cpp), free of game types so that tests/ can run it.

#include <cmath>
#include <cstddef>
#include <deque>

namespace TrailMath
{
struct Point
{
    float x;
    float y;
    float z;
};

struct Limits
{
    float step;           // a new point when the leader moved this far
    float resetDistance;  // a gap this long starts the trail again
    float resetHeight;    // so does this much height between two readings
    size_t maxPoints;
};

// Adds the leader's position to the front of the trail. True when the trail was started again (it was empty, or the
// leader jumped), so the caller can note the direction the leader faces.
inline bool Record(std::deque<Point>& trail, Point const& head, Limits const& limits)
{
    if (!trail.empty())
    {
        float const moved = std::hypot(head.x - trail.front().x, head.y - trail.front().y);
        if (moved > limits.resetDistance || std::fabs(head.z - trail.front().z) > limits.resetHeight)
            trail.clear();
        else if (moved >= limits.step)
            trail.push_front(head);
    }

    bool const restarted = trail.empty();
    if (restarted)
        trail.push_front(head);
    while (trail.size() > limits.maxPoints)
        trail.pop_back();
    return restarted;
}

struct Place
{
    enum Kind
    {
        NONE,      // the trail is shorter than the bot's distance
        SPOT,      // the bot's own place on the trail; sideX and sideY point to the left of the way walked
        CATCH_UP   // a trail point on the way: the bot is far behind its place
    };

    Kind kind;
    Point point;
    float sideX;
    float sideY;
};

// The place `behind` yards back along the trail from the leader. A bot whose nearest trail point lies at least
// catchUpPoints behind that place is sent to the point catchUpStep ahead of its nearest one instead, so it walks
// the leader's way point by point.
inline Place Find(std::deque<Point> const& trail, Point const& head, float behind, float botX, float botY,
                  size_t catchUpPoints, size_t catchUpStep)
{
    Point from = head;
    float left = behind;
    for (size_t index = 0; index < trail.size(); ++index)
    {
        Point const& point = trail[index];
        float const dx = from.x - point.x;
        float const dy = from.y - point.y;
        float const length = std::hypot(dx, dy);
        if (length > 0.01f && length >= left)
        {
            size_t nearest = index;
            float nearestDistance = std::hypot(botX - point.x, botY - point.y);
            for (size_t older = index + 1; older < trail.size(); ++older)
            {
                float const distance = std::hypot(botX - trail[older].x, botY - trail[older].y);
                if (distance < nearestDistance)
                {
                    nearestDistance = distance;
                    nearest = older;
                }
            }
            if (nearest >= index + catchUpPoints && nearest >= catchUpStep)
                return { Place::CATCH_UP, trail[nearest - catchUpStep], 0.0f, 0.0f };

            float const part = left / length;
            Point const spot = { from.x - dx * part, from.y - dy * part, from.z - (from.z - point.z) * part };
            return { Place::SPOT, spot, -dy / length, dx / length };
        }
        left -= length;
        from = point;
    }
    return { Place::NONE, head, 0.0f, 0.0f };
}
}  // namespace TrailMath

#endif
