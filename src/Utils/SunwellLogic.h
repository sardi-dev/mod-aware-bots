/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef AWARE_BOTS_SUNWELL_LOGIC_H
#define AWARE_BOTS_SUNWELL_LOGIC_H

#include "RaidGeometry.h"

#include <cmath>
#include <vector>

// The decisions of the Sunwell Plateau strategy that need no game objects, from the SkyFire 5.4.8 boss scripts:
// - Brutallus casts Meteor Slash (45150) on his victim every 11 s: a 90 degree frontal cone, 65 yd, 20000 damage
//   shared by everyone in it, and a debuff (75% more damage taken per stack, 40 s, refreshed by every hit).
// - Felmyst's flight: she climbs 10 yd, sends two Demonic Vapors (they chase a player and leave a trail that turns
//   into Unyielding Dead), then three times flies to a spot 70 yd from a player, faces that player, waits 10 s and
//   crosses the room through that player's spot, dropping a Fog of Corruption cloud (charms within 20 yd) every
//   second. Then she flies to her top threat target and lands.
// - Kil'jaeden's Sinister Reflections attack the raid member they copy; the Shield Orbs circle 17 yd above the floor.
namespace SunwellLogic
{
using RaidGeometry::Circle;
using RaidGeometry::Point;

// Brutallus
inline constexpr float METEOR_CONE_WIDTH = RaidGeometry::PI / 2.0f;
inline constexpr float METEOR_CONE_RANGE = 65.0f;
// Stay this far (radians) outside the cone's edge, so a small turn of the boss does not catch a bystander.
inline constexpr float METEOR_CONE_MARGIN = RaidGeometry::PI / 9.0f;
// Soakers stand within 25 degrees of his facing, well inside the 45 degree edge.
inline constexpr float SOAK_SPREAD = RaidGeometry::PI * 50.0f / 180.0f;
inline constexpr float RANGED_SOAK_DISTANCE = 15.0f;
// A soaker takes this many hits, then the other side takes over once its stacks have fallen off (40 s, four hits).
inline constexpr int SOAK_SWAP_STACKS = 4;
inline constexpr int TANK_SWAP_STACKS = 4;

// Which of two sides (two soak teams, or the tank Brutallus hits and the other tank) takes the next Meteor Slash.
// Side 0 wins ties, so every bot that looks at the same stacks picks the same side. The side already carrying more
// stacks keeps soaking until it reaches swapAt; then the other side takes over. Returns -1 when neither side has
// anyone to soak.
inline int ChooseSoaker(int stacks0, int stacks1, int swapAt, bool has0, bool has1)
{
    if (!has0 && !has1)
        return -1;
    if (!has1)
        return 0;
    if (!has0)
        return 1;

    int const keep = stacks0 >= stacks1 ? 0 : 1;
    int const keepStacks = keep == 0 ? stacks0 : stacks1;
    int const restStacks = keep == 0 ? stacks1 : stacks0;
    if (keepStacks < swapAt || restStacks >= keepStacks)
        return keep;
    return 1 - keep;
}

// The soak spot of `slot` out of `count`, fanned out across the front of the boss at `distance` from him.
inline Point SoakSpot(Point boss, float facing, int slot, int count, float distance)
{
    float offset = 0.0f;
    if (count > 1)
        offset = -SOAK_SPREAD / 2.0f + SOAK_SPREAD * (float(slot) + 0.5f) / float(count);
    return { boss.x + std::cos(facing + offset) * distance, boss.y + std::sin(facing + offset) * distance };
}

// Inside Meteor Slash's cone, widened by `margin` radians on each side.
inline bool InMeteorCone(Point boss, float facing, Point point, float margin)
{
    if (std::hypot(point.x - boss.x, point.y - boss.y) > METEOR_CONE_RANGE)
        return false;
    return RaidGeometry::InFrontCone(boss, facing, point, METEOR_CONE_WIDTH + 2.0f * margin);
}

// Where a bot that must not be hit goes: melee to the rear flank at melee distance, ranged to the side at their
// current distance (10 to 30 yd), on the side nearer to them.
inline Point MeteorAvoidSpot(Point boss, float facing, Point from, bool melee, float meleeDistance)
{
    if (melee)
        return RaidGeometry::FlankPoint(boss, facing, RaidGeometry::PI * 3.0f / 4.0f, meleeDistance, from);

    float const distance = std::fmax(10.0f, std::fmin(30.0f, std::hypot(from.x - boss.x, from.y - boss.y)));
    return RaidGeometry::FlankPoint(boss, facing, RaidGeometry::PI * 5.0f / 9.0f, distance, from);
}

// Felmyst
enum class FelmystPhase
{
    Ground,
    AirVapor,   // a Demonic Vapor or its trail is out
    AirBreath,  // hovering before a breath (facing the lane) or crossing the room with Speed Burst
    AirMoving   // climbing, flying to a breath spot or back to land
};

// She lands at ground level; in the air she is 10 yd above where she took off or above her breath target.
inline constexpr float FELMYST_AIR_HEIGHT = 6.0f;
// 70 yd to the player she aims at and as far again past them, plus a little.
inline constexpr float BREATH_LANE_LENGTH = 150.0f;
inline constexpr float BREATH_LANE_SPACING = 8.0f;
// How long a lane seen while she hovered still stands for her crossing (10 s wait plus the crossing).
inline constexpr unsigned BREATH_LANE_MEMORY_MS = 20000;

inline FelmystPhase ClassifyFelmyst(float heightAboveBot, bool vaporOut, bool speedBurst, bool moving)
{
    if (heightAboveBot < FELMYST_AIR_HEIGHT)
        return FelmystPhase::Ground;
    if (vaporOut)
        return FelmystPhase::AirVapor;
    if (speedBurst || !moving)
        return FelmystPhase::AirBreath;
    return FelmystPhase::AirMoving;
}

// The breath lane as a row of circles from `start` along `facing`: every circle a fog cloud could reach.
inline std::vector<Circle> BreathLaneCircles(Point start, float facing, float length, float radius, float spacing)
{
    std::vector<Circle> circles;
    if (spacing <= 0.0f || length < 0.0f)
        return circles;

    float const dx = std::cos(facing);
    float const dy = std::sin(facing);
    for (float along = 0.0f; along <= length + 0.001f; along += spacing)
        circles.push_back({ { start.x + dx * along, start.y + dy * along }, radius });
    return circles;
}

// In the air everyone but the ranged damage dealers gathers where she took off, so she lands among them.
inline bool NeedsRegroup(float distanceToRally, bool rangedDps)
{
    return distanceToRally > (rangedDps ? 25.0f : 10.0f);
}

// A step away from what chases the bot (a Demonic Vapor, a Volatile Felfire Fiend), bent away from the raid so the
// chaser and its trail stay off the others.
inline Point KitePoint(Point bot, Point chaser, Point raidCenter, float step)
{
    float ax = bot.x - chaser.x;
    float ay = bot.y - chaser.y;
    float const awayLength = std::hypot(ax, ay);
    if (awayLength > 0.01f)
    {
        ax /= awayLength;
        ay /= awayLength;
    }
    else
    {
        ax = 1.0f;
        ay = 0.0f;
    }

    float rx = bot.x - raidCenter.x;
    float ry = bot.y - raidCenter.y;
    float const raidLength = std::hypot(rx, ry);
    if (raidLength > 0.01f)
    {
        rx /= raidLength;
        ry /= raidLength;
    }
    else
    {
        rx = 0.0f;
        ry = 0.0f;
    }

    float dx = ax + 0.5f * rx;
    float dy = ay + 0.5f * ry;
    float const length = std::hypot(dx, dy);
    if (length < 0.01f)
    {
        dx = ax;
        dy = ay;
    }
    else
    {
        dx /= length;
        dy /= length;
    }

    return { bot.x + dx * step, bot.y + dy * step };
}

// Kil'jaeden: Sinister Reflections on healers die first, then those on other casters, then the rest.
inline int ReflectionRank(bool victimHeals, bool victimRanged)
{
    if (victimHeals)
        return 0;
    if (victimRanged)
        return 1;
    return 2;
}

// True when reflection A should die before reflection B: lower rank first, then lower health.
inline bool ReflectionBefore(int rankA, float healthPctA, int rankB, float healthPctB)
{
    if (rankA != rankB)
        return rankA < rankB;
    return healthPctA < healthPctB;
}
}

#endif
