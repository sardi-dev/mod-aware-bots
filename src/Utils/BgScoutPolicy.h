/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_BGSCOUTPOLICY_H
#define _PLAYERBOT_BGSCOUTPOLICY_H

// BT-483: what an expert bot calls out beyond the events the plans always said: the enemy group it sees on its own half of the
// map before it reaches the base, and the early warning at a node (Arathi Basin). Bots share what they see only through
// visible call-outs (owner 2026-11-18, experts call out more often), so these two lines are how the team learns of a group
// that is still on its way. Pure, so that tests/ can run it (tests/test_bg_scout.cpp).

#include <cstddef>
#include <cstdint>
#include <vector>

namespace BgScout
{
// A group of this many enemies in one zone is worth a call (the same number as the base's "inc").
constexpr std::uint32_t SCOUT_MIN_ENEMIES = 3;
// The expert must be this close to the group to have seen it (a player's view of other players).
constexpr float SCOUT_SIGHT_YARDS = 80.0f;
// The same zone is called at most this often.
constexpr std::uint32_t SCOUT_REPEAT_MS = 20000;

struct Enemy
{
    float x = 0.0f;
    float y = 0.0f;
    std::uint8_t place = 0xFF;   // the zone of the map (WsgLogic::Place as a number), 0xFF for none
};

struct Zone
{
    std::uint8_t place = 0xFF;
    std::uint32_t count = 0;
    float x = 0.0f;
    float y = 0.0f;
};

// An enemy is on our half when it is nearer our flag stand than theirs.
inline bool OnOurHalf(float x, float y, float ownX, float ownY, float foeX, float foeY)
{
    float const own = (x - ownX) * (x - ownX) + (y - ownY) * (y - ownY);
    float const foe = (x - foeX) * (x - foeX) + (y - foeY) * (y - foeY);
    return own < foe;
}

// The zone with the most enemies on our half, at least SCOUT_MIN_ENEMIES, never the zone `skipPlace` (the flag room, which has
// its own call), ties to the lower place number. `enemies` must hold only enemies on our half and in a zone.
inline bool BestZone(std::vector<Enemy> const& enemies, std::uint8_t skipPlace, Zone& out)
{
    bool found = false;
    for (Enemy const& first : enemies)
    {
        if (first.place == 0xFF || first.place == skipPlace)
            continue;

        Zone zone;
        zone.place = first.place;
        for (Enemy const& e : enemies)
        {
            if (e.place != first.place)
                continue;
            ++zone.count;
            zone.x += e.x;
            zone.y += e.y;
        }
        if (zone.count < SCOUT_MIN_ENEMIES)
            continue;

        zone.x /= static_cast<float>(zone.count);
        zone.y /= static_cast<float>(zone.count);
        if (!found || zone.count > out.count || (zone.count == out.count && zone.place < out.place))
        {
            out = zone;
            found = true;
        }
    }
    return found;
}

inline bool RepeatDue(std::uint32_t lastMs, std::uint32_t nowMs)
{
    return !lastMs || static_cast<std::int32_t>(nowMs - lastMs) >= static_cast<std::int32_t>(SCOUT_REPEAT_MS);
}

// Arathi Basin: an expert warns of a group that is 60 yards from our node before it is 50 yards from it (where every tier's
// ordinary "inc" starts).
inline bool EarlyWarning(std::uint32_t enemiesWithin50, std::uint32_t enemiesWithin60, std::uint32_t threshold)
{
    return enemiesWithin60 >= threshold && enemiesWithin50 < threshold;
}
}  // namespace BgScout

#endif
