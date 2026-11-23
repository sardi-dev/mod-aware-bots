#ifndef AWARE_BOTS_PULL_SIGHT_POLICY_H
#define AWARE_BOTS_PULL_SIGHT_POLICY_H

#include <cmath>

// BT-333: where a tank stands to pull a mark it cannot see. Deadmines proof runs (2026-11-17): a Goblin Overseer behind a
// rail stayed unseen from 5 yd and from 27 yd, the walk to it ended "move refused" and the pull waited for the healer's
// step to land the tank on a spot with sight (94 s once, never twice). A player tank looks for a place on the way round
// from where the mob is in sight and pulls from there. The bot tries spots on rings round the mark, nearest first, and
// walks to the first one that sees it and has a short path.
namespace PullSightPolicy
{
// Rings round the mark in yards (pull spells reach 30 or more, melee pulls use the walk above) and spots per ring.
constexpr float RING_YARDS[] = { 8.0f, 14.0f, 20.0f };
constexpr unsigned RINGS = sizeof(RING_YARDS) / sizeof(RING_YARDS[0]);
constexpr unsigned SPOTS_PER_RING = 12;
constexpr unsigned SPOT_COUNT = RINGS * SPOTS_PER_RING;
// A spot on another floor does not count (a ledge over the mark sees it and cannot be reached the same way).
constexpr float MAX_SPOT_DZ = 3.0f;
// The best few spots by straight distance get a path search; the rest are never searched, so one try costs at most this.
constexpr unsigned PATH_SEARCHES = 4;
// A path longer than this many times the straight line (plus the slack) goes round half the room: not worth it.
constexpr float MAX_DETOUR_FACTOR = 2.0f;
constexpr float DETOUR_SLACK_YARDS = 15.0f;
// One search per this many seconds per bot, so a tank that finds nothing does not search every tick.
constexpr long RETRY_SECONDS = 10;

// Offset of spot `spot` (0 to SPOTS_PER_RING - 1) on a ring of `ringYards`. Other rings of spots (BT-377: the step away
// from an attacker on another floor, FloorAttackerPolicy.h) use the same angles, so the spots of every search lie alike.
inline void RingOffset(float ringYards, unsigned spot, float& dx, float& dy)
{
    constexpr float TWO_PI = 6.28318530718f;
    float const angle = TWO_PI * float(spot % SPOTS_PER_RING) / float(SPOTS_PER_RING);
    dx = std::cos(angle) * ringYards;
    dy = std::sin(angle) * ringYards;
}

// Offset of spot `index` (0 to SPOT_COUNT - 1) from the mark.
inline void Offset(unsigned index, float& dx, float& dy)
{
    RingOffset(RING_YARDS[(index / SPOTS_PER_RING) % RINGS], index, dx, dy);
}

constexpr bool SameFloor(float spotZ, float markZ)
{
    return std::fabs(spotZ - markZ) <= MAX_SPOT_DZ;
}

constexpr bool PathIsShort(float pathLength, float straightDistance)
{
    return pathLength <= MAX_DETOUR_FACTOR * straightDistance + DETOUR_SLACK_YARDS;
}

constexpr bool MaySearch(long lastSearch, long now)
{
    return lastSearch == 0 || now - lastSearch >= RETRY_SECONDS;
}
}  // namespace PullSightPolicy

#endif
