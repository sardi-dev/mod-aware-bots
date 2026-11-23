#ifndef BOT_START_AREA_POLICY_H
#define BOT_START_AREA_POLICY_H

#include <cstdint>
#include <vector>

// BT-236: random bots belong to their own race's start area, not another race's (owner 2026-11-16: a pandaren stood in
// the tauren start at Camp Narache). Pure rules; the caller fills the start points from playercreateinfo.
namespace StartAreaPolicy
{
struct StartPoint
{
    std::uint32_t race;
    std::uint32_t map;
    float x;
    float y;
};

// How close to a race's start position counts as its start area, in yards (a start valley is a few hundred yards).
constexpr float START_AREA_RADIUS = 400.0f;

// True when (map, x, y) lies in another race's start area and not in the bot's own (races that share a start keep it).
inline bool InForeignStartArea(std::vector<StartPoint> const& starts, std::uint32_t botRace, std::uint32_t map, float x,
                               float y, float radius = START_AREA_RADIUS)
{
    bool foreign = false;
    for (StartPoint const& start : starts)
    {
        if (start.map != map)
            continue;

        float const dx = x - start.x;
        float const dy = y - start.y;
        if (dx * dx + dy * dy > radius * radius)
            continue;

        if (start.race == botRace)
            return false;

        foreign = true;
    }
    return foreign;
}

// A neutral pandaren picks its faction where a player does: with "A New Fate" (31450) in its log at the end of the
// Wandering Isle. A bot that never gets there leaves the isle once it is well past it (fallbackLevel), so it cannot be
// stuck there for good.
inline bool ShouldPickPandarenFaction(bool hasNewFateQuest, std::uint32_t level, std::uint32_t fallbackLevel)
{
    return hasNewFateQuest || level >= fallbackLevel;
}
}

#endif
