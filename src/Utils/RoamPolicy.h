/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef AWARE_BOTS_ROAM_POLICY_H
#define AWARE_BOTS_ROAM_POLICY_H

#include <cstddef>
#include <cstdint>

// BT-252 (owner 2026-11-16, "aimless roaming: up a mountain, down, up again"): WANDER_RANDOM and the stay at a quest POI
// picked each step around the bot's own position (a random walk that drifts anywhere) with no look at the terrain, and the
// POI stay took 3 to 8 yd shuffles. A patrol now stays within a leash of a fixed anchor (where the wander began, or the
// quest POI), takes steps of at least MIN_HOP_YARDS, and refuses steps that climb or detour more than a player would to
// look for mobs.
namespace RoamPolicy
{
// Radius of a wander around where it began (the grind spot); the grind pick looks further for mobs anyway.
constexpr float WANDER_LEASH_YARDS = 40.0f;
// Radius of the search around a quest POI.
constexpr float POI_LEASH_YARDS = 30.0f;
// No step shorter than this: shorter ones read as pacing on the spot.
constexpr float MIN_HOP_YARDS = 12.0f;
// Free climb of any step, then this much rise per yard of distance (about 24 degrees).
constexpr float FREE_CLIMB_YARDS = 3.0f;
constexpr float MAX_CLIMB_PER_YARD = 0.45f;
// A step whose walk is this much longer than the straight line goes around a ridge or a wall: refused.
constexpr float MAX_DETOUR = 1.6f;
constexpr float DETOUR_SLACK_YARDS = 5.0f;
// A wander that brought no experience for this long has nothing to fight here: it ends and the bot picks a real task.
constexpr uint32_t NO_WORK_MS = 120000;

// The candidate step's distance from the anchor, as a fraction of the leash, from a roll in [0, 1).
constexpr float AnchorRadius(float leash, float roll)
{
    return (0.35f + 0.65f * roll) * leash;
}

// Sum of the upward steps along a path's heights (a path down and up again climbs too).
template <class Heights>
inline float TotalClimb(Heights const& z)
{
    float climb = 0.0f;
    for (std::size_t i = 1; i < z.size(); ++i)
        if (z[i] > z[i - 1])
            climb += z[i] - z[i - 1];
    return climb;
}

// horizontal: straight 2-D distance of the step; climb: TotalClimb of its path; pathLength: the walked length.
constexpr bool StepTerrainOk(float horizontal, float climb, float pathLength)
{
    return climb <= FREE_CLIMB_YARDS + MAX_CLIMB_PER_YARD * horizontal &&
           pathLength <= MAX_DETOUR * horizontal + DETOUR_SLACK_YARDS;
}

// The bot was pulled further than this from the anchor (a chase, a flight from a fight): it walks back first.
constexpr bool OutsideLeash(float distToAnchor, float leash)
{
    return distToAnchor > leash + 10.0f;
}

// Whether a wander has had nothing to do: no experience gained since it began, for NO_WORK_MS.
constexpr bool WanderHasNoWork(uint32_t sinceStartMs, bool gainedExperience)
{
    return !gainedExperience && sinceStartMs >= NO_WORK_MS;
}

// BT-254 (the "move stuck" trigger of mod-playerbots, ported): a bot whose status wants it to move (a trip, a patrol, a
// quest, a gathering round) and that stayed within STILL_YARDS for STILL_MS out of combat is stuck where its paths fail
// (a ledge, a pocket, a door). It steps aside like a player would and picks a new task; never a teleport.
constexpr float STILL_YARDS = 5.0f;
constexpr uint32_t STILL_MS = 180000;

struct StillWatch
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    uint32_t sinceMs = 0;  // 0 = not watching
};

// One reading. Returns true when the bot has been still for STILL_MS; the watch then starts over from here.
// watching: the status wants movement and nothing excuses standing (combat, a break, a ride, a dock wait).
inline bool StillTooLong(StillWatch& watch, bool watching, float x, float y, float z, uint32_t nowMs)
{
    if (!watching)
    {
        watch.sinceMs = 0;
        return false;
    }
    float const dx = x - watch.x;
    float const dy = y - watch.y;
    float const dz = z - watch.z;
    if (!watch.sinceMs || dx * dx + dy * dy + dz * dz > STILL_YARDS * STILL_YARDS)
    {
        watch = StillWatch{x, y, z, nowMs ? nowMs : 1};
        return false;
    }
    if (nowMs - watch.sinceMs < STILL_MS)
        return false;
    watch = StillWatch{x, y, z, nowMs ? nowMs : 1};
    return true;
}
}  // namespace RoamPolicy

#endif
