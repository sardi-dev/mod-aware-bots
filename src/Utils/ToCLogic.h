/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef AWARE_BOTS_TOC_LOGIC_H
#define AWARE_BOTS_TOC_LOGIC_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

// Trial of the Crusader (map 649) on the SkyFire 5.4.8 scripts: the geometry and the choices the bot strategy makes,
// kept free of the game core so they can be tested. Ids and positions come from
// src/server/scripts/Northrend/CrusadersColiseum/TrialOfTheCrusader/ (boss_*.cpp, trial_of_the_crusader.h).
namespace ToCLogic
{
struct Point
{
    float x = 0.0f;
    float y = 0.0f;
};

inline float Distance(Point a, Point b)
{
    return std::hypot(a.x - b.x, a.y - b.y);
}

// ToCCommonLoc[1] "Center" (trial_of_the_crusader.h): the middle of the coliseum floor, where Icehowl jumps for Massive
// Crash and the orb paths of the twins are centred.
constexpr Point ARENA_CENTER = {563.672974f, 139.571f};
// The twins' orbs fly on a circle of 47 around the centre (boss_twin_valkyr.cpp MoveToNextPoint); standing 7 inside
// the wall keeps a bot clear of it.
constexpr float ARENA_RUN_RADIUS = 40.0f;

inline Point ClampToArena(Point p, Point center = ARENA_CENTER, float radius = ARENA_RUN_RADIUS)
{
    float const d = Distance(p, center);
    if (d <= radius || d < 0.001f)
        return p;
    float const k = radius / d;
    return {center.x + (p.x - center.x) * k, center.y + (p.y - center.y) * k};
}

// The point `dist` yards from `from` in the direction away from `danger`; the bot's own spot when they coincide.
inline Point AwayFrom(Point self, Point danger, float dist)
{
    float const d = Distance(self, danger);
    if (d < 0.001f)
        return {self.x + dist, self.y};
    float const k = dist / d;
    return {self.x + (self.x - danger.x) * k, self.y + (self.y - danger.y) * k};
}

inline float DistanceToSegment(Point p, Point a, Point b)
{
    float const dx = b.x - a.x;
    float const dy = b.y - a.y;
    float const len2 = dx * dx + dy * dy;
    if (len2 < 0.0001f)
        return Distance(p, a);
    float const t = std::clamp(((p.x - a.x) * dx + (p.y - a.y) * dy) / len2, 0.0f, 1.0f);
    return Distance(p, {a.x + t * dx, a.y + t * dy});
}

// ---- Northrend Beasts: Icehowl (boss_northrend_beasts.cpp) ---------------------------------------------------------

// Stage 3 of the script: Icehowl stands still for 4 s facing a random player, then hops backwards (to 2 * boss - target)
// and charges to the spot the player had when the hop started (MoveCharge), casting Trample (66734, radius 12) on
// arrival and on anyone within 6 yards on the way. So the zone to leave is the segment from the hop point to the target
// plus the Trample circle around the target.
constexpr float TRAMPLE_RADIUS = 12.0f;
constexpr float CHARGE_HALF_WIDTH = 6.0f;
// A bot's own size and a late step
constexpr float CHARGE_MARGIN = 2.0f;

inline Point ChargeStart(Point boss, Point target)
{
    return {2.0f * boss.x - target.x, 2.0f * boss.y - target.y};
}

inline bool InChargeZone(Point p, Point boss, Point target)
{
    if (DistanceToSegment(p, ChargeStart(boss, target), target) < CHARGE_HALF_WIDTH + CHARGE_MARGIN)
        return true;
    return Distance(p, target) < TRAMPLE_RADIUS + CHARGE_MARGIN;
}

// The nearest spot outside the charge zone: tried 3 yards at a time along the two sides of the charge line and away
// from the target. False when the bot is already clear.
inline bool EscapeCharge(Point p, Point boss, Point target, Point& out)
{
    if (!InChargeZone(p, boss, target))
        return false;

    float lx = target.x - boss.x;
    float ly = target.y - boss.y;
    float const len = std::hypot(lx, ly);
    if (len < 0.001f)
    {
        lx = 1.0f;
        ly = 0.0f;
    }
    else
    {
        lx /= len;
        ly /= len;
    }
    float const dirs[4][2] = {{-ly, lx}, {ly, -lx}, {lx, ly}, {-lx, -ly}};

    for (float step = 3.0f; step <= 30.0f; step += 3.0f)
    {
        bool found = false;
        float bestDist = 1.0e9f;
        for (auto const& dir : dirs)
        {
            Point const cand = ClampToArena({p.x + dir[0] * step, p.y + dir[1] * step});
            if (InChargeZone(cand, boss, target))
                continue;
            float const d = Distance(cand, p);
            if (d < bestDist)
            {
                bestDist = d;
                out = cand;
                found = true;
            }
        }
        if (found)
            return true;
    }
    out = ClampToArena(AwayFrom(p, target, 30.0f));
    return true;
}

// The player Icehowl faces runs from the boss: in the 4 s before the hop it covers about 28 yards, and the further it
// is, the longer the charge and the fewer bots near the end. Kept inside the coliseum.
constexpr float TRAMPLE_TARGET_RUN = 28.0f;

inline Point TrampleTargetSpot(Point self, Point boss)
{
    return ClampToArena(AwayFrom(self, boss, TRAMPLE_TARGET_RUN));
}

// ---- Northrend Beasts: the worms ------------------------------------------------------------------------------------

// A worm that dies enrages the other (SNAKES_SPECIAL), so the damage dealers keep the two together. The skull moves to
// the other worm only when it has this much more health left (in percent), so the group does not flap between them.
constexpr float WORM_SWITCH_MARGIN = 5.0f;

inline bool SwitchWorm(float currentHpPct, float otherHpPct)
{
    return otherHpPct > currentHpPct + WORM_SWITCH_MARGIN;
}

// Radius a bot keeps from a ground effect, by creature entry; 0 when the entry is none.
constexpr uint32_t NPC_FIRE_BOMB = 34854;
constexpr uint32_t NPC_SLIME_POOL = 35176;
constexpr uint32_t NPC_LEGION_FLAME = 34784;

// Fire Bomb 66320 ticks in 5 yards (heroic effect 1) and the bomb explodes in 8 (66317); Slime Pool is 10 yards across
// when summoned and grows to 40 across (script comment on SPELL_SLIME_POOL_EFFECT), so its radius starts at 5 and
// reaches 20; Legion Flame 66877 has 3. One yard is added for the bot's own size.
constexpr float FIRE_BOMB_AVOID = 6.0f;
constexpr float SLIME_POOL_AVOID = 11.0f;
constexpr float LEGION_FLAME_AVOID = 4.0f;

inline float HazardRadius(uint32_t entry)
{
    switch (entry)
    {
        case NPC_FIRE_BOMB:
            return FIRE_BOMB_AVOID;
        case NPC_SLIME_POOL:
            return SLIME_POOL_AVOID;
        case NPC_LEGION_FLAME:
            return LEGION_FLAME_AVOID;
        default:
            return 0.0f;
    }
}

// ---- Lord Jaraxxus -------------------------------------------------------------------------------------------------

// The player Legion Flame (66197) was cast on drops a flame trail as he runs, so he leaves the raid. It is a trail of
// 3 yard flames, so 20 yards away from the others is plenty.
constexpr float LEGION_FLAME_RUN_FROM_GROUP = 20.0f;

inline Point Centroid(std::vector<Point> const& points)
{
    if (points.empty())
        return {};
    float sx = 0.0f;
    float sy = 0.0f;
    for (Point const& p : points)
    {
        sx += p.x;
        sy += p.y;
    }
    float const n = static_cast<float>(points.size());
    return {sx / n, sy / n};
}

// Where the flame carrier runs: away from the others' centre until LEGION_FLAME_RUN_FROM_GROUP yards from it.
inline bool LegionFlameSpot(Point self, std::vector<Point> const& others, Point& out)
{
    if (others.empty())
        return false;
    Point const centre = Centroid(others);
    float const d = Distance(self, centre);
    if (d >= LEGION_FLAME_RUN_FROM_GROUP)
        return false;
    out = ClampToArena(AwayFrom(self, centre, LEGION_FLAME_RUN_FROM_GROUP - d + 3.0f));
    return true;
}

// ---- Faction Champions (boss_faction_champions.cpp, trial_of_the_crusader.h CreatureIds) ------------------------------

// Lower is killed first: healers, then the casters, then the rest. Pets and unknown entries come last.
constexpr int CHAMPION_TIER_HEALER = 0;
constexpr int CHAMPION_TIER_CASTER = 1;
constexpr int CHAMPION_TIER_OTHER = 2;
constexpr int CHAMPION_TIER_NONE = 3;

inline int ChampionTier(uint32_t entry)
{
    switch (entry)
    {
        // Restoration druid, holy paladin, discipline priest and restoration shaman: alliance, then horde
        case 34469: case 34465: case 34466: case 34470:
        case 34459: case 34445: case 34447: case 34444:
            return CHAMPION_TIER_HEALER;
        // Balance druid, mage, shadow priest, warlock
        case 34460: case 34468: case 34473: case 34474:
        case 34451: case 34449: case 34441: case 34450:
            return CHAMPION_TIER_CASTER;
        // Death knight, hunter, retribution paladin, rogue, enhancement shaman, warrior
        case 34461: case 34467: case 34471: case 34472: case 34463: case 34475:
        case 34458: case 34448: case 34456: case 34454: case 34455: case 34453:
            return CHAMPION_TIER_OTHER;
        default:
            return CHAMPION_TIER_NONE;
    }
}

// Healers first, then the lowest health among equals
inline bool ChampionBetter(int tierA, float hpPctA, int tierB, float hpPctB)
{
    if (tierA != tierB)
        return tierA < tierB;
    return hpPctA < hpPctB;
}

// ---- Twin Val'kyr (boss_twin_valkyr.cpp) -----------------------------------------------------------------------------

enum class Twin
{
    Light,  // Fjola Lightbane, aura state 22
    Dark    // Eydis Darkbane, aura state 19
};

// Light Essence (65686) gives +50% damage against aura state 19 (Eydis) and -50% against 22 (Fjola); Dark Essence the
// other way round (spell effects 65811 and 65827, aura 303). So a bot that attacks one twin wants the essence of the
// other colour, and a bot that holds an essence attacks the twin of the other colour. The map is its own inverse.
inline Twin Opposite(Twin colour)
{
    return colour == Twin::Light ? Twin::Dark : Twin::Light;
}

// The twin a bot works on while neither twin is shielded: the main tank holds Fjola, the assist tank Eydis, everyone
// else splits by character number so both twins lose health together (Twin Empathy 66132/66133 shares damage 100%
// either way, so the split is about threat and healing, not about the damage).
inline Twin AssignedTwin(bool mainTank, bool assistTank, uint32_t guidLow)
{
    if (mainTank)
        return Twin::Light;
    if (assistTank)
        return Twin::Dark;
    return (guidLow % 2 == 0) ? Twin::Light : Twin::Dark;
}

// An orb is harmless to a bot that holds the essence of its colour (the essence absorbs it, boss_twin_valkyr.cpp
// spell_valkyr_essences); to everyone else it is the Unleashed hit.
inline bool OrbDangerous(Twin orbColour, bool hasEssence, Twin essence)
{
    return !hasEssence || orbColour != essence;
}

// Unleashed Light/Dark (65795/65808) hits in 6 yards and the orb explodes when a player is within 3 (range check of the
// orb scripts); 8 keeps a bot clear of both.
constexpr float ORB_AVOID_RADIUS = 8.0f;
// How far a ranged bot goes for an orb of its own colour (it stacks Powering Up, 67590)
constexpr float ORB_FETCH_RADIUS = 12.0f;

// Step away from a set of dangerous orbs: out of the nearest one by the missing distance.
inline bool StepOutOfOrbs(Point self, std::vector<Point> const& orbs, Point& out)
{
    float nearest = ORB_AVOID_RADIUS;
    Point nearestPoint;
    bool found = false;
    for (Point const& orb : orbs)
    {
        float const d = Distance(self, orb);
        if (d < nearest)
        {
            nearest = d;
            nearestPoint = orb;
            found = true;
        }
    }
    if (!found)
        return false;
    out = ClampToArena(AwayFrom(self, nearestPoint, ORB_AVOID_RADIUS - nearest + 2.0f));
    return true;
}

// ---- Anub'arak (boss_anubarak_trial.cpp) -----------------------------------------------------------------------------

// Leeching Swarm starts when the boss falls below 30% (HealthBelowPct(30)).
constexpr float ANUBARAK_PHASE3_HP_PCT = 30.0f;

// The spike chases the marked player and stops when it comes within 7 yards of a Frost Sphere (MoveInLineOfSight of the
// spike script), so the marked player runs to the far side of a sphere.
constexpr float SPIKE_SPHERE_STOP = 7.0f;
constexpr float SPIKE_BEYOND_SPHERE = 4.0f;
constexpr float SPIKE_MAX_RUN = 35.0f;
constexpr float SPIKE_CLEARANCE = 6.0f;
// Without a usable sphere the bot just runs
constexpr float SPIKE_FLEE_DISTANCE = 25.0f;

// The Nerubian Burrowers (summoned in phase 1) are tanked this far from the boss, so the group that kills them does
// not stand in Anub'arak's melee and Freezing Slash; chosen for the boss's melee reach and a margin, the script has no
// position for them.
constexpr float BURROWER_TANK_DISTANCE = 20.0f;

inline Point BehindSphere(Point sphere, Point spike)
{
    return AwayFrom(sphere, spike, SPIKE_BEYOND_SPHERE);
}

// Index of the sphere to run behind: the nearest whose far side can be reached without passing the spike; the spike
// then chases the bot through the sphere. -1 when none.
inline int SpikeSphere(Point self, Point spike, std::vector<Point> const& spheres)
{
    int best = -1;
    float bestRun = SPIKE_MAX_RUN;
    for (size_t i = 0; i < spheres.size(); ++i)
    {
        Point const dest = BehindSphere(spheres[i], spike);
        float const run = Distance(self, dest);
        if (run >= bestRun)
            continue;
        if (DistanceToSegment(spike, self, dest) < SPIKE_CLEARANCE)
            continue;
        // A sphere the spike is already at has stopped it (or is about to) and gives nothing more
        if (Distance(spike, spheres[i]) <= SPIKE_SPHERE_STOP)
            continue;
        best = static_cast<int>(i);
        bestRun = run;
    }
    return best;
}
}  // namespace ToCLogic

#endif
