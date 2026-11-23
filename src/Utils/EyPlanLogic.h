/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_EYPLANLOGIC_H
#define _PLAYERBOT_EYPLANLOGIC_H

// BT-580: the Eye of the Storm playbook (docs/bg-sheets/eye-of-the-storm.md) as pure logic, free of game types so that tests/
// can run it. This part is what the old objective case of BgTacticsEY.cpp needs without a team record: the map data, the
// capture bar arithmetic of the core, the opening squads and pinned defenders picked from the roster, and one answer per bot
// (carrier, pinned defender, squad, hold state, the third and fourth base, the flag in the middle). The node plan
// (AbPlanLogic.h over the Eye of the Storm row of BgNodeMap.h) and the flag plan build on the same data.
//
// The core's own numbers: BattlegroundEY.h and .cpp in the core (after a bar clamp fix in the core, which clamps
// the net count of players at a base to -5..5 on both sides).

#include "BfgPlanLogic.h"
#include "BgPlanCommon.h"
#include "BotSkillMath.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace EyLogic
{
using Tier = BotSkillMath::Tier;
using Member = BfgLogic::Member;
namespace Ab = AbLogic;

// ---------------------------------------------------------------------------------------------------------------------
// Map data (sheet section 2). Point ids are the core's EYBattlegroundPoints.
// ---------------------------------------------------------------------------------------------------------------------
constexpr std::uint32_t MAP_ID = 566;
constexpr std::uint8_t BASE_COUNT = 4;
constexpr std::uint8_t BASE_NONE = 0xFF;
constexpr std::uint8_t TEAM_NONE = 0xFF;      // a base nobody owns

enum Base : std::uint8_t
{
    FEL_REAVER = 0,
    BLOOD_ELF = 1,
    DRAENEI_RUINS = 2,
    MAGE_TOWER = 3
};

struct Pt
{
    float x;
    float y;
    float z;
};

// The flag capture triggers (AreaTrigger.dbc, radius 3 yd): where a carrier stands to score.
constexpr Pt TRIGGER_POS[BASE_COUNT] = {
    { 2044.00f, 1729.73f, 1190.03f },   // Fel Reaver Ruins
    { 2048.48f, 1393.60f, 1194.54f },   // Blood Elf Tower
    { 2286.56f, 1402.36f, 1197.29f },   // Draenei Ruins
    { 2284.78f, 1731.32f, 1189.64f }    // Mage Tower
};
constexpr std::uint32_t TRIGGER_ID[BASE_COUNT] = { 4514, 4476, 4518, 4516 };

// The cap objects: the centres of the 70 yd circles that count the players of a base (3D distance).
constexpr Pt CAP_POS[BASE_COUNT] = {
    { 2024.60f, 1742.82f, 1195.16f },
    { 2050.49f, 1372.24f, 1194.56f },
    { 2301.01f, 1386.93f, 1197.18f },
    { 2282.12f, 1760.01f, 1189.71f }
};
constexpr float CIRCLE_RADIUS = 70.0f;
constexpr float TRIGGER_RADIUS = 3.0f;
// The owner's rule (2026-11-18): the bot carrier scores from at most this far from the trigger (a human needs the 3 yd).
constexpr float CAPTURE_RANGE = 2.5f;

constexpr Pt CENTRE_FLAG = { 2174.78f, 1569.05f, 1160.36f };
constexpr Pt START_ROOM[2] = { { 2523.76f, 1596.13f, 1247.70f }, { 1803.02f, 1539.97f, 1249.19f } };

constexpr std::uint8_t TEAM_IDX_ALLIANCE = Ab::TEAM_IDX_ALLIANCE;
constexpr std::uint8_t TEAM_IDX_HORDE = Ab::TEAM_IDX_HORDE;

inline char const* BaseWord(std::uint8_t base)
{
    static char const* const words[BASE_COUNT] = { "fr", "be", "dr", "mt" };
    return base < BASE_COUNT ? words[base] : "?";
}

// The two bases nearest a team's start room (275 to 307 yd; the centre is 350 to 373 yd away).
inline void NearBases(std::uint8_t team, std::uint8_t (&out)[2])
{
    if (team == TEAM_IDX_ALLIANCE)
    {
        out[0] = MAGE_TOWER;
        out[1] = DRAENEI_RUINS;
    }
    else
    {
        out[0] = FEL_REAVER;
        out[1] = BLOOD_ELF;
    }
}

inline bool IsNearBase(std::uint8_t team, std::uint8_t base)
{
    std::uint8_t nb[2];
    NearBases(team, nb);
    return base == nb[0] || base == nb[1];
}

inline float Dist2d(float ax, float ay, float bx, float by)
{
    return std::sqrt((ax - bx) * (ax - bx) + (ay - by) * (ay - by));
}

inline float Dist3d(float ax, float ay, float az, float bx, float by, float bz)
{
    return std::sqrt((ax - bx) * (ax - bx) + (ay - by) * (ay - by) + (az - bz) * (az - bz));
}

// Inside the 70 yd circle of a base (the core's own test, IsWithinDistInMap).
inline bool InCircle(std::uint8_t base, float x, float y, float z)
{
    return base < BASE_COUNT && Dist3d(x, y, z, CAP_POS[base].x, CAP_POS[base].y, CAP_POS[base].z) <= CIRCLE_RADIUS;
}

// ---------------------------------------------------------------------------------------------------------------------
// The capture bar (sheet section 3; BattlegroundEY.cpp UpdatePointStatuses). 0 to 100, 50 at the start, the Alliance owns a base at
// 70 or more and the Horde at 30 or less, every 2 s the bar moves by the net number of players at the base, at most 5.
// ---------------------------------------------------------------------------------------------------------------------
constexpr int BAR_START = 50;
constexpr int BAR_ALLIANCE_OWNS = 70;
constexpr int BAR_HORDE_OWNS = 30;
constexpr int MAX_NET = 5;
constexpr std::uint32_t BAR_TICK_MS = 2000;
constexpr int TAKE_POINTS = 20;                // from the neutral start to the owner's threshold
// A base the owner has hardened to this margin or more needs the enemy about 14 s at a net of 5 to neutralise.
constexpr int HARDENED_MARGIN = 15;
constexpr int MAX_MARGIN = 31;

inline int NetPerTick(int alliancePlayers, int hordePlayers)
{
    return std::max(-MAX_NET, std::min(alliancePlayers - hordePlayers, MAX_NET));
}

// Seconds to take a neutral base with this net number of players (the first tick counts, up to 2 s more to arrive).
inline std::uint32_t TakeSeconds(int net)
{
    int const n = std::max(1, std::min(std::abs(net), MAX_NET));
    return static_cast<std::uint32_t>((TAKE_POINTS + n - 1) / n) * (BAR_TICK_MS / 1000u);
}

// Points the enemy needs, net, to neutralise a base its owner holds: 1 for a base captured at the threshold, 31 at the most.
inline int Margin(int bar, std::uint8_t owner)
{
    if (owner == TEAM_IDX_ALLIANCE)
        return std::max(0, std::min(bar - (BAR_ALLIANCE_OWNS - 1), MAX_MARGIN));
    if (owner == TEAM_IDX_HORDE)
        return std::max(0, std::min((BAR_HORDE_OWNS + 1) - bar, MAX_MARGIN));
    return 0;
}

inline bool Hardened(int bar, std::uint8_t owner)
{
    return Margin(bar, owner) >= HARDENED_MARGIN;
}

// Seconds to neutralise a base of this margin at this net (the owner's presence adds to the margin, so a lower bound).
inline std::uint32_t NeutraliseSeconds(int margin, int net)
{
    int const n = std::max(1, std::min(std::abs(net), MAX_NET));
    return static_cast<std::uint32_t>((std::max(margin, 1) + n - 1) / n) * (BAR_TICK_MS / 1000u);
}

// ---------------------------------------------------------------------------------------------------------------------
// Rates and flag values (sheet section 3), bases held 0 to 4. The owner's ruling: the core's values stay.
// ---------------------------------------------------------------------------------------------------------------------
constexpr std::uint32_t MAX_SCORE = 1600;
constexpr std::uint32_t TICK_POINTS[BASE_COUNT + 1] = { 0, 1, 2, 5, 10 };
constexpr std::uint32_t FLAG_POINTS[BASE_COUNT + 1] = { 0, 75, 85, 100, 500 };

inline float PointsPerSecond(unsigned bases)
{
    return static_cast<float>(TICK_POINTS[std::min<unsigned>(bases, BASE_COUNT)]) * 1000.0f / static_cast<float>(BAR_TICK_MS);
}

inline std::uint32_t FlagPoints(unsigned bases)
{
    return FLAG_POINTS[std::min<unsigned>(bases, BASE_COUNT)];
}

// The change in (our rate - their rate) of taking a base nobody holds, and of neutralising one the enemy holds (they lose it at
// once); a flip is both.
inline float TakeValue(unsigned ours)
{
    return ours >= BASE_COUNT ? 0.0f : PointsPerSecond(ours + 1) - PointsPerSecond(ours);
}

inline float NeutraliseValue(unsigned theirs)
{
    return theirs == 0 ? 0.0f : PointsPerSecond(theirs) - PointsPerSecond(theirs - 1);
}

// ---------------------------------------------------------------------------------------------------------------------
// The opening squads and the pinned defenders (sheet section 4). A squad is 3 or more (the owner's capture rule: a net of 3
// takes a neutral base in 14 s, a lone bot needs 40 s and loses it to one enemy in 2 s); after the first wave one pinned
// defender per near base stays (two in the defense plan) and the others walk on to the middle.
// ---------------------------------------------------------------------------------------------------------------------
constexpr std::uint32_t SQUAD_SIZE = 3;
constexpr std::uint32_t SQUAD_SIZE_DEFENSE = 4;
constexpr std::uint32_t DEFENSE_SQUAD_MIN_TEAM = 12;
constexpr std::uint32_t TWO_SQUADS_MIN = 2 * SQUAD_SIZE;   // fewer bots than this take one base only

enum class Role : std::uint8_t
{
    MIDDLE,
    SQUAD,
    PINNED
};

struct Slot
{
    Role role = Role::MIDDLE;
    std::uint8_t base = BASE_NONE;
};

struct Assigned
{
    std::uint32_t guidLow = 0;
    Slot slot;
};

inline std::uint32_t SquadSize(BgPlan::Plan plan, std::uint32_t bots)
{
    return plan == BgPlan::Plan::DEFENSE && bots >= DEFENSE_SQUAD_MIN_TEAM ? SQUAD_SIZE_DEFENSE : SQUAD_SIZE;
}

inline std::uint32_t PinnedPerBase(BgPlan::Plan plan)
{
    return plan == BgPlan::Plan::DEFENSE ? 2u : 1u;
}

inline bool HashBefore(std::uint32_t a, std::uint32_t b)
{
    std::uint32_t const ha = BotSkillMath::Mix(a);
    std::uint32_t const hb = BotSkillMath::Mix(b);
    return ha != hb ? ha < hb : a < b;
}

// The slots of a team's bots at the start: pinned defenders by kit, tier and level (the Gilneas order: rogue, mage, hunter
// first), one per near base alternately; the squads from the rest by GUID hash; the others are the middle group. Fewer than 3
// bots take no base at all (the capture rule), fewer than 6 take one.
inline std::vector<Assigned> AssignOpening(BgPlan::Plan plan, std::uint8_t team, std::vector<Member> const& bots)
{
    std::vector<Assigned> out;
    std::uint32_t const count = static_cast<std::uint32_t>(bots.size());
    for (Member const& m : bots)
        out.push_back({ m.guidLow, Slot() });
    if (count < SQUAD_SIZE)
        return out;

    std::uint8_t nb[2];
    NearBases(team, nb);
    std::uint32_t const squads = count >= TWO_SQUADS_MIN ? 2u : 1u;
    std::uint32_t const size = SquadSize(plan, count);
    std::uint32_t const pinnedPer = std::min(PinnedPerBase(plan), size - 1);

    auto const find = [&out](std::uint32_t guid) -> Assigned& {
        for (Assigned& a : out)
            if (a.guidLow == guid)
                return a;
        return out.front();
    };

    std::vector<std::uint32_t> const pinned = BfgLogic::PickDefenders(bots, static_cast<std::size_t>(pinnedPer * squads));
    std::uint32_t placed[2] = { 0, 0 };
    for (std::size_t i = 0; i < pinned.size(); ++i)
    {
        std::uint32_t const s = static_cast<std::uint32_t>(i % squads);
        find(pinned[i]).slot = { Role::PINNED, nb[s] };
        ++placed[s];
    }

    std::vector<std::uint32_t> rest;
    for (Member const& m : bots)
        if (find(m.guidLow).slot.role == Role::MIDDLE)
            rest.push_back(m.guidLow);
    std::sort(rest.begin(), rest.end(), HashBefore);

    std::size_t next = 0;
    for (std::uint32_t s = 0; s < squads; ++s)
    {
        std::uint32_t const need = size > placed[s] ? size - placed[s] : 0u;
        for (std::uint32_t i = 0; i < need && next < rest.size(); ++i, ++next)
            find(rest[next]).slot = { Role::SQUAD, nb[s] };
    }
    return out;
}

inline Slot SlotOf(std::vector<Assigned> const& all, std::uint32_t guidLow)
{
    for (Assigned const& a : all)
        if (a.guidLow == guidLow)
            return a.slot;
    return Slot();
}

// ---------------------------------------------------------------------------------------------------------------------
// The old objective case: one answer per bot from the base owners, the capture bars and the flag.
// ---------------------------------------------------------------------------------------------------------------------
enum class What : std::uint8_t
{
    NOTHING,
    POST,      // stand within 12 yd of the base's trigger, inside the circle: pinned defender, squad, guard of the hold state
    TAKE,      // walk into the circle of a base nobody holds or the enemy holds, and stay in it
    CARRY,     // the carrier runs to the base where it scores
    RETREAT,   // the carrier holds no base to score at: it keeps clear of the enemy on its own side
    FLAG,      // the taker goes to the centre stand
    STAGE      // the rest of the middle group: a ring around the centre stand, out of the 25 yd of the pickup cast
};

struct Choice
{
    What what = What::NOTHING;
    std::uint8_t base = BASE_NONE;
    std::uint32_t rank = 0;      // the bot's place in the group it belongs to (a post or stage angle)
};

// What the old case reads of the match.
struct World
{
    std::uint8_t team = TEAM_IDX_ALLIANCE;
    std::uint8_t owner[BASE_COUNT] = { TEAM_NONE, TEAM_NONE, TEAM_NONE, TEAM_NONE };   // team index of the owner, TEAM_NONE if neutral
    int bar[BASE_COUNT] = { BAR_START, BAR_START, BAR_START, BAR_START };
    std::uint8_t flagState = 0;       // 0 on its stand, 1 waits to respawn, 2 carried, 3 on the ground (BG_EY_FlagState)
    bool teamCarries = false;
    bool enemyCarries = false;
};

struct Me
{
    std::uint32_t guidLow = 0;
    float x = 0.0f;
    float y = 0.0f;
    std::uint8_t level = 80;
    bool carrying = false;
};

constexpr float WALK_SPEED = 7.0f;
constexpr float MOUNT_SPEED = 11.2f;
constexpr std::uint8_t MOUNT_LEVEL = 20;
constexpr float PATH_DETOUR = 1.4f;
constexpr float TARGET_ETA_SOFTENER_S = 20.0f;
constexpr float TARGET_VALUE_SCALE = 10000.0f;
// Free bots needed to start a take (the owner's rule), the group that goes, and the bots needed for the fourth base.
constexpr std::uint32_t TAKE_GROUP_MIN = 3;
constexpr std::uint32_t TAKE_GROUP_MAX = 4;
constexpr std::uint32_t FOURTH_BASE_MIN_FREE = 6;
// The hold state (sheet section 6): guards per held base.
constexpr std::uint32_t HOLD_GUARDS = 2;

inline unsigned Held(World const& w, std::uint8_t team)
{
    unsigned n = 0;
    for (std::uint8_t b = 0; b < BASE_COUNT; ++b)
        if (w.owner[b] == team)
            ++n;
    return n;
}

inline float EtaSeconds(float yards, std::uint8_t level)
{
    float const speed = level >= MOUNT_LEVEL ? MOUNT_SPEED : WALK_SPEED;
    return yards * PATH_DETOUR / speed;
}

// The base a carrier scores at: the nearest base of its team (the old case; the node plan scores the choice by guards and enemies
// seen). BASE_NONE when the team holds none.
inline std::uint8_t NearestOwnedBase(World const& w, float x, float y)
{
    std::uint8_t best = BASE_NONE;
    float bestDist = 0.0f;
    for (std::uint8_t b = 0; b < BASE_COUNT; ++b)
    {
        if (w.owner[b] != w.team)
            continue;
        float const d = Dist2d(x, y, TRIGGER_POS[b].x, TRIGGER_POS[b].y);
        if (best == BASE_NONE || d < bestDist)
        {
            best = b;
            bestDist = d;
        }
    }
    return best;
}

// The base to take or flip: the best swing per second of walking among the bases the team does not hold.
inline std::uint8_t BestTakeBase(World const& w, float x, float y, std::uint8_t level)
{
    unsigned const ours = Held(w, w.team);
    unsigned const theirs = Held(w, Ab::OtherTeam(w.team));
    std::uint8_t best = BASE_NONE;
    int bestScore = 0;
    for (std::uint8_t b = 0; b < BASE_COUNT; ++b)
    {
        if (w.owner[b] == w.team)
            continue;
        float value = TakeValue(ours);
        if (w.owner[b] != TEAM_NONE)
            value += NeutraliseValue(theirs);
        float const eta = EtaSeconds(Dist2d(x, y, TRIGGER_POS[b].x, TRIGGER_POS[b].y), level);
        int const score = static_cast<int>(value * TARGET_VALUE_SCALE / (eta + TARGET_ETA_SOFTENER_S));
        if (best == BASE_NONE || score > bestScore)
        {
            best = b;
            bestScore = score;
        }
    }
    return best;
}

// The bases of a team in the order the hold state fills them: the one nearest the enemy's start room first (their dead walk
// from there).
inline std::vector<std::uint8_t> HoldOrder(World const& w)
{
    std::vector<std::uint8_t> bases;
    for (std::uint8_t b = 0; b < BASE_COUNT; ++b)
        if (w.owner[b] == w.team)
            bases.push_back(b);
    Pt const room = START_ROOM[Ab::OtherTeam(w.team) & 1];
    std::stable_sort(bases.begin(), bases.end(), [&room](std::uint8_t a, std::uint8_t b)
    {
        return Dist2d(TRIGGER_POS[a].x, TRIGGER_POS[a].y, room.x, room.y) < Dist2d(TRIGGER_POS[b].x, TRIGGER_POS[b].y, room.x, room.y);
    });
    return bases;
}

// What a bot of the old case does. Rules, in order (sheet sections 4 to 6):
//  1. a carrier runs to the nearest base of its team (none: it keeps clear on its own side);
//  2. a pinned defender stands at its near base, always;
//  3. a squad bot stands in the circle of its base until the base is ours and hardened (margin 15 or more);
//  4. the rest (the middle group and released squads) in hash order:
//     with three or more bases held the hold state: 2 guards per held base (counting the pinned ones), the others one group for
//     the fourth base only when 6 or more are free; otherwise a take group of 3 to 4 goes to the best base to take (never fewer
//     than 3: a lone bot loses the base to one enemy in 2 s), and the others take the flag in the middle when it is free and
//     the team holds a base, else stage around it.
// Nothing chases a player across the map and nothing walks to the enemy start room.
inline Choice Choose(World const& w, BgPlan::Plan plan, std::vector<Member> const& bots, Me const& me)
{
    Choice c;
    if (me.carrying)
    {
        std::uint8_t const base = NearestOwnedBase(w, me.x, me.y);
        c.what = base == BASE_NONE ? What::RETREAT : What::CARRY;
        c.base = base;
        return c;
    }

    std::vector<Assigned> const slots = AssignOpening(plan, w.team, bots);
    Slot const mine = SlotOf(slots, me.guidLow);

    if (mine.role == Role::PINNED)
    {
        c.what = What::POST;
        c.base = mine.base;
        return c;
    }
    auto const released = [&w](Slot const& s) {
        return s.role == Role::SQUAD && w.owner[s.base] == w.team && Hardened(w.bar[s.base], w.team);
    };
    if (mine.role == Role::SQUAD && !released(mine))
    {
        c.what = w.owner[mine.base] == w.team ? What::POST : What::TAKE;
        c.base = mine.base;
        return c;
    }

    // The free bots: the middle group and the released squads, in a stable order.
    std::vector<std::uint32_t> freeBots;
    std::uint32_t guardsAt[BASE_COUNT] = { 0, 0, 0, 0 };
    for (Assigned const& a : slots)
    {
        if (a.slot.role == Role::PINNED || (a.slot.role == Role::SQUAD && !released(a.slot)))
        {
            if (a.slot.base < BASE_COUNT)
                ++guardsAt[a.slot.base];
            continue;
        }
        freeBots.push_back(a.guidLow);
    }
    bool const inList = std::find(freeBots.begin(), freeBots.end(), me.guidLow) != freeBots.end();
    if (!inList)
        freeBots.push_back(me.guidLow);   // a bot the roster missed plays with the free ones, last
    std::sort(freeBots.begin(), freeBots.end(), HashBefore);
    std::uint32_t rank = static_cast<std::uint32_t>(std::find(freeBots.begin(), freeBots.end(), me.guidLow) - freeBots.begin());
    std::uint32_t const free = static_cast<std::uint32_t>(freeBots.size());
    c.rank = rank;

    unsigned const held = Held(w, w.team);
    std::uint32_t guardRanks = 0;
    if (held >= 3)
    {
        for (std::uint8_t b : HoldOrder(w))
        {
            std::uint32_t const need = HOLD_GUARDS > guardsAt[b] ? HOLD_GUARDS - guardsAt[b] : 0u;
            if (rank >= guardRanks && rank < guardRanks + need)
            {
                c.what = What::POST;
                c.base = b;
                c.rank = rank - guardRanks;
                return c;
            }
            guardRanks += need;
        }
    }

    // The take group: ranks after the guards, 3 to 4 of them, only when enough are free (the fourth base needs more).
    std::uint32_t const spare = free > guardRanks ? free - guardRanks : 0u;
    bool const mayTake = held < 3 ? spare >= TAKE_GROUP_MIN : spare >= FOURTH_BASE_MIN_FREE;
    std::uint8_t const target = BestTakeBase(w, me.x, me.y, me.level);
    if (mayTake && target != BASE_NONE)
    {
        std::uint32_t const group = std::min(TAKE_GROUP_MAX, spare);
        if (rank >= guardRanks && rank < guardRanks + group)
        {
            c.what = What::TAKE;
            c.base = target;
            c.rank = rank - guardRanks;
            return c;
        }
        rank -= std::min(rank, guardRanks + group);
    }
    else
    {
        rank -= std::min(rank, guardRanks);
    }
    c.rank = rank;

    // The flag in the middle: the first of the rest takes it when it is free and a base is held (the flag scores only at a base we
    // hold); everybody else stands off the stand.
    bool const flagFree = w.flagState == 0 && !w.teamCarries && !w.enemyCarries;
    if (flagFree && held >= 1 && rank == 0)
    {
        c.what = What::FLAG;
        return c;
    }
    c.what = What::STAGE;
    return c;
}

}  // namespace EyLogic

#endif
