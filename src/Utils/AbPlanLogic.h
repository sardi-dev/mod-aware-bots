/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_ABPLANLOGIC_H
#define _PLAYERBOT_ABPLANLOGIC_H

// BT-156: the Arathi Basin playbook (docs/bg-sheets/arathi-basin.md) as pure logic, free of game types so that tests/ can
// run it: the click rule, the resource rates and the race clock, the node situations, the regimes, the opening counts,
// the guard slots, the slot assignment, the scored options, the responders, the assault and raid targets, the flip
// limiter, the call-out kinds and the reading of a human's call.
//
// Every decision is a list of candidate options with a deterministic score, so the rules stay the fallback and a later
// decision layer can pick among the same options. Each rule says its tier: core rules are played by every bot, good
// rules by good and expert bots, expert rules by expert bots only (BotSkillMath.h, owner 2026-11-12).
//
// BT-551: the node maps are data (BgNodeMap.h). Every function that depends on the map takes the map as its last argument and
// defaults to Arathi Basin, so the Arathi code and its tests read as before; the second map is Battle for Gilneas.
//
// The pieces shared with Warsong Gulch (plan draw, tier lag, call limiter, call chance, candidate pattern, human call
// words, records) are in BgPlanCommon.h.

#include "BgNodeMap.h"
#include "BgPlanCommon.h"
#include "BotSkillMath.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace AbLogic
{
using Tier = BotSkillMath::Tier;
using BgPlan::Plan;
using Map = BgNodeMap::MapData;
using Pt = BgNodeMap::Pt;

// ---------------------------------------------------------------------------------------------------------------------
// Numbers from the core (BattlegroundAB.h / .cpp in the core) and the owner's decisions of 2026-11-12.
// ---------------------------------------------------------------------------------------------------------------------

// The match ends at this many resources (BG_AB_MAX_TEAM_SCORE, BattlegroundAB.h:117-121); there is no time limit.
constexpr std::uint32_t MAX_SCORE = BgNodeMap::ARATHI.maxScore;
// A node contested by one team becomes theirs after this long (BG_AB_FLAG_CAPTURING_TIME, BattlegroundAB.h:111-114).
constexpr std::uint32_t CAPTURE_TIME_MS = 60000;
// Spell 21651 (capture banner) is an 8 second cast (SpellCastTimes 8000 ms), from within 10 yd of the banner
// (BattlegroundAB.cpp:398).
constexpr std::uint32_t CAST_TIME_MS = 8000;
constexpr float CLICK_RANGE = 10.0f;
// Tick interval and points per tick for 0 to 5 occupied nodes (BG_AB_TickIntervals / BG_AB_TickPoints, BattlegroundAB.h).
inline constexpr auto const& TICK_INTERVAL_MS = BgNodeMap::ARATHI.tickIntervalMs;
inline constexpr auto const& TICK_POINTS = BgNodeMap::ARATHI.tickPoints;

// The size of every per-node array: the most nodes a map has (Arathi Basin). A map with fewer nodes uses the first ones.
constexpr std::uint8_t NODE_COUNT = BgNodeMap::MAX_NODES;
constexpr std::uint8_t NODE_NONE = BgNodeMap::NODE_NONE;

// BG_AB_BattlegroundNodes: the order is the core's and must not change.
enum Node : std::uint8_t
{
    NODE_STABLES = 0,
    NODE_BLACKSMITH = 1,
    NODE_FARM = 2,
    NODE_LUMBER_MILL = 3,
    NODE_GOLD_MINE = 4
};

// BattlegroundAB::m_Nodes: 0 neutral, 1 and 2 contested by the Alliance and the Horde, 3 and 4 occupied by them.
constexpr std::uint8_t STATE_NEUTRAL = 0;
constexpr std::uint8_t STATE_MAX = 4;

// Team indexes as the core's TeamId: Alliance 0, Horde 1.
constexpr std::uint8_t TEAM_IDX_ALLIANCE = 0;
constexpr std::uint8_t TEAM_IDX_HORDE = 1;

// The click rule of BattlegroundAB::EventPlayerClickedOnFlag (BattlegroundAB.cpp:413): `m_Nodes == 0 || teamIndex ==
// m_Nodes % 2`. A neutral node is anyone's; a contested or occupied node only the opposing team's. The owner of a node
// and the team that has it contested cannot click it: a cast on it wastes 8 seconds and changes nothing.
constexpr bool CanClickNode(std::uint8_t state, std::uint8_t team)
{
    return team <= TEAM_IDX_HORDE && (state == STATE_NEUTRAL || (state <= STATE_MAX && team == state % 2));
}

constexpr bool IsOccupiedBy(std::uint8_t state, std::uint8_t team)
{
    return team <= TEAM_IDX_HORDE && state == 3 + team;
}

constexpr bool IsContestedBy(std::uint8_t state, std::uint8_t team)
{
    return team <= TEAM_IDX_HORDE && state == 1 + team;
}

// The attack pool of the old objective code: the nodes a team may take or flip are exactly the nodes it may click
// (neutral, occupied by the enemy, contested by the enemy). The team's own contested node is not one of them.
constexpr bool InAttackPool(std::uint8_t state, std::uint8_t team)
{
    return CanClickNode(state, team);
}

constexpr std::uint8_t OtherTeam(std::uint8_t team)
{
    return team == TEAM_IDX_ALLIANCE ? TEAM_IDX_HORDE : TEAM_IDX_ALLIANCE;
}

// The near node of a team (Arathi Basin: Stables and Farm; Battle for Gilneas: Lighthouse and Mines).
constexpr std::uint8_t HomeNode(std::uint8_t team, Map const& map = BgNodeMap::ARATHI)
{
    return map.home[team & 1];
}

inline char const* NodeWord(std::uint8_t node, Map const& map = BgNodeMap::ARATHI)
{
    return node < map.nodeCount ? map.word[node] : "?";
}

// BT-582: a map where a node is taken by standing in its circle (Eye of the Storm) and not by a click and a contest timer.
constexpr bool IsPresence(Map const& map)
{
    return map.captureModel == BgNodeMap::CaptureModel::PRESENCE;
}

// A node of the two nearest a team's start room (Eye of the Storm; on the click maps the home node twice).
constexpr bool IsNearNode(std::uint8_t team, std::uint8_t node, Map const& map = BgNodeMap::ARATHI)
{
    return node == map.nearNodes[team & 1][0] || node == map.nearNodes[team & 1][1];
}

// Inside the circle that counts the players of a presence node (the core's own test, 3D distance to the cap object).
inline bool InCircle(std::uint8_t node, float x, float y, float z, Map const& map)
{
    if (node >= map.nodeCount || map.circleRadius <= 0.0f)
        return false;
    float const dx = x - map.cap[node].x;
    float const dy = y - map.cap[node].y;
    float const dz = z - map.cap[node].z;
    return dx * dx + dy * dy + dz * dz <= map.circleRadius * map.circleRadius;
}

// ---------------------------------------------------------------------------------------------------------------------
// Map data: the server's own (BG_AB_NodePositions, BG_AB_BuffPositions, BG_AB_SpiritGuidePos, safe locations 889-899).
// ---------------------------------------------------------------------------------------------------------------------
inline constexpr auto const& NODE_POS = BgNodeMap::ARATHI.pos;

// The base exit portals (area triggers 3948 and 3949): a bot that enters one leaves the match. No post or camp point may
// lie within EXIT_CLEARANCE of them (BattlegroundAB.cpp:208-219).
constexpr Pt EXIT_PORTAL[2] = { { 1394.36f, 1303.64f, 0.0f }, { 627.19f, 699.96f, 0.0f } };
constexpr float EXIT_CLEARANCE = 25.0f;

inline float Dist2d(float ax, float ay, float bx, float by)
{
    float const dx = ax - bx;
    float const dy = ay - by;
    return std::sqrt(dx * dx + dy * dy);
}

inline float DistToNode(std::uint8_t node, float x, float y, Map const& map = BgNodeMap::ARATHI)
{
    return node < map.nodeCount ? Dist2d(x, y, map.pos[node].x, map.pos[node].y) : 100000.0f;
}

// The node whose banner is within `radius` of a point (2D), the nearest one; NODE_NONE when there is none. The banner
// objects of the core all stand at BG_AB_NodePositions, so a game object is matched to its node by position.
inline std::uint8_t NodeNear(float x, float y, float radius, Map const& map = BgNodeMap::ARATHI)
{
    std::uint8_t best = NODE_NONE;
    float bestDist = radius;
    for (std::uint8_t n = 0; n < map.nodeCount; ++n)
    {
        float const d = DistToNode(n, x, y, map);
        if (d <= bestDist)
        {
            bestDist = d;
            best = n;
        }
    }
    return best;
}

inline bool NearExitPortal(float x, float y, Map const& map = BgNodeMap::ARATHI)
{
    if (!map.exitPortals)
        return false;   // the Battle for Gilneas script has none
    for (Pt const& p : EXIT_PORTAL)
        if (Dist2d(x, y, p.x, p.y) < EXIT_CLEARANCE)
            return true;
    return false;
}

// The nearer of Gold Mine and Lumber Mill to a team's base by the module's recorded paths (Alliance: 462 / 482 yd,
// Horde: 470 / 518 yd, sheet section 2). Paths within TIE_YARDS of each other are a tie and the match parity decides
// (owner pick of the sheet: ties by match parity).
constexpr float BASE_TO_GOLD_MINE[2] = { 462.0f, 470.0f };
constexpr float BASE_TO_LUMBER_MILL[2] = { 482.0f, 518.0f };
constexpr float NEARER_TIE_YARDS = 25.0f;

inline std::uint8_t NearerContested(std::uint8_t team, std::uint32_t matchParity)
{
    float const gm = BASE_TO_GOLD_MINE[team & 1];
    float const lm = BASE_TO_LUMBER_MILL[team & 1];
    if (std::fabs(gm - lm) <= NEARER_TIE_YARDS)
        return (matchParity & 1u) ? NODE_LUMBER_MILL : NODE_GOLD_MINE;
    return gm < lm ? NODE_GOLD_MINE : NODE_LUMBER_MILL;
}

// Walking speed in yards per second: the base run speed, and mounted from level 20 (sheet section 2).
constexpr float WALK_SPEED = 7.0f;
constexpr float MOUNT_SPEED = 11.2f;
constexpr std::uint8_t MOUNT_LEVEL = 20;
// A recorded path is longer than the straight line by about this factor (Stables to Blacksmith 393 against 244 yd,
// Blacksmith to Farm 255 against 242: the average of the sheet's table). Inference; the live audit refines it.
constexpr float PATH_DETOUR = 1.4f;

inline std::uint32_t TravelMs(float straightYards, std::uint8_t level)
{
    float const speed = level >= MOUNT_LEVEL ? MOUNT_SPEED : WALK_SPEED;
    return static_cast<std::uint32_t>(straightYards * PATH_DETOUR / speed * 1000.0f);
}

// ---------------------------------------------------------------------------------------------------------------------
// Resource rates and the race clock (sheet sections 3 and 6).
// ---------------------------------------------------------------------------------------------------------------------
constexpr std::int32_t ETA_NEVER = 24 * 3600;

inline float PointsPerSecond(unsigned occupiedNodes, Map const& map = BgNodeMap::ARATHI)
{
    unsigned const n = std::min(occupiedNodes, unsigned(map.nodeCount));
    return map.tickIntervalMs[n] ? static_cast<float>(map.tickPoints[n]) * 1000.0f / static_cast<float>(map.tickIntervalMs[n]) : 0.0f;
}

// Seconds until a team reaches MAX_SCORE at its present rate; ETA_NEVER with no node.
inline std::int32_t EtaSeconds(std::uint32_t score, unsigned occupiedNodes, Map const& map = BgNodeMap::ARATHI)
{
    float const rate = PointsPerSecond(occupiedNodes, map);
    if (rate <= 0.0f)
        return ETA_NEVER;
    std::uint32_t const remaining = score >= map.maxScore ? 0 : map.maxScore - score;
    return std::min(ETA_NEVER, static_cast<std::int32_t>(std::ceil(static_cast<float>(remaining) / rate)));
}

enum class SwingKind : std::uint8_t
{
    TAKE_ENEMY,   // flip a node they occupy: we gain a node and they lose one
    TAKE_FREE,    // take a neutral node or one that is contested by them and was never occupied: gain only
    DENY          // assault a node they occupy without holding it afterwards: they lose its share at once
};

// The change in (our rate - their rate) in points per second, from ours / theirs occupied nodes now.
inline float SwingValue(unsigned ours, unsigned theirs, SwingKind kind, Map const& map = BgNodeMap::ARATHI)
{
    float const gain = PointsPerSecond(std::min(ours + 1, unsigned(map.nodeCount)), map) - PointsPerSecond(ours, map);
    float const loss = theirs ? PointsPerSecond(theirs, map) - PointsPerSecond(theirs - 1, map) : 0.0f;
    switch (kind)
    {
        case SwingKind::TAKE_ENEMY: return gain + loss;
        case SwingKind::TAKE_FREE: return gain;
        case SwingKind::DENY: return loss;
    }
    return 0.0f;
}

// ---------------------------------------------------------------------------------------------------------------------
// Node situations for one team.
// ---------------------------------------------------------------------------------------------------------------------
enum class NodeSituation : std::uint8_t
{
    NEUTRAL,         // nobody clicked it yet
    OURS,            // occupied by us: scoring
    UNDER_ASSAULT,   // contested by them and it was ours: an 8 s click restores it at once
    THEIRS,          // occupied by them
    CAPPING,         // contested by us: 60 s from our cast
    FLIPPED_BACK     // contested by them and it was never ours: no instant restore, a click starts our own 60 s
};

inline char const* SituationName(NodeSituation s)
{
    switch (s)
    {
        case NodeSituation::NEUTRAL: return "neutral";
        case NodeSituation::OURS: return "ours";
        case NodeSituation::UNDER_ASSAULT: return "assault";
        case NodeSituation::THEIRS: return "theirs";
        case NodeSituation::CAPPING: return "capping";
        case NodeSituation::FLIPPED_BACK: return "flipped";
    }
    return "?";
}

// prevState is the state before the present one (BattlegroundAB::m_prevNodes, which is private; the record keeps its own
// observation of it). The core restores a contested node at once only when it was occupied before the contest
// (BattlegroundAB.cpp:443, `m_prevNodes[node] < BG_AB_NODE_TYPE_OCCUPIED`).
inline NodeSituation Classify(std::uint8_t state, std::uint8_t prevState, std::uint8_t team)
{
    if (state == STATE_NEUTRAL || state > STATE_MAX)
        return NodeSituation::NEUTRAL;
    if (IsOccupiedBy(state, team))
        return NodeSituation::OURS;
    if (IsContestedBy(state, team))
        return NodeSituation::CAPPING;
    if (IsOccupiedBy(state, OtherTeam(team)))
        return NodeSituation::THEIRS;
    // Contested by them.
    return IsOccupiedBy(prevState, team) ? NodeSituation::UNDER_ASSAULT : NodeSituation::FLIPPED_BACK;
}

// BT-582: the situation of a presence node. There is no contest state: the core says neutral, held by the Alliance (state 3) or
// held by the Horde (state 4). A node that was ours and is neutral now was neutralised: UNDER_ASSAULT, to be retaken (the guards
// stay and the restore responders go); CAPPING (our group in the circle with a net edge) is set by the record from the counts.
inline NodeSituation ClassifyPresence(std::uint8_t state, std::uint8_t prevState, std::uint8_t team)
{
    if (IsOccupiedBy(state, team))
        return NodeSituation::OURS;
    if (IsOccupiedBy(state, OtherTeam(team)))
        return NodeSituation::THEIRS;
    return IsOccupiedBy(prevState, team) ? NodeSituation::UNDER_ASSAULT : NodeSituation::NEUTRAL;
}

inline NodeSituation ClassifyFor(Map const& map, std::uint8_t state, std::uint8_t prevState, std::uint8_t team)
{
    return IsPresence(map) ? ClassifyPresence(state, prevState, team) : Classify(state, prevState, team);
}

// A node a team holds: its guards stand there (occupied, being captured by us, or under assault and to be restored).
inline bool IsHeld(NodeSituation s)
{
    return s == NodeSituation::OURS || s == NodeSituation::CAPPING || s == NodeSituation::UNDER_ASSAULT;
}

// A node an assault group may go to take or flip.
inline bool IsTargetable(NodeSituation s)
{
    return s == NodeSituation::NEUTRAL || s == NodeSituation::THEIRS || s == NodeSituation::FLIPPED_BACK;
}

// The time left of a contest, inferred from the moment the record saw the state change (the core keeps it private).
inline std::uint32_t ContestRemainingMs(std::uint32_t sinceChangeMs)
{
    return sinceChangeMs >= CAPTURE_TIME_MS ? 0 : CAPTURE_TIME_MS - sinceChangeMs;
}

// ---------------------------------------------------------------------------------------------------------------------
// Regimes from the race clock (sheet section 6).
// ---------------------------------------------------------------------------------------------------------------------
enum class Regime : std::uint8_t
{
    EVEN,
    HOLD,        // ahead on the clock with three or more nodes: keep what we have
    PUSH,        // behind on the clock: flip a node or deny
    CLOSE_OUT,   // four nodes: the fifth ends the match in 53 seconds
    LAST_NODE,   // one node against three or more: never lose it
    PANIC        // the enemy is about to win the race
};

inline char const* RegimeName(Regime r)
{
    switch (r)
    {
        case Regime::EVEN: return "even";
        case Regime::HOLD: return "hold";
        case Regime::PUSH: return "push";
        case Regime::CLOSE_OUT: return "closeout";
        case Regime::LAST_NODE: return "lastnode";
        case Regime::PANIC: return "panic";
    }
    return "even";
}

constexpr std::int32_t REGIME_MARGIN_S = 90;
constexpr std::int32_t REGIME_HYSTERESIS_S = 30;
constexpr std::int32_t PANIC_ETA_S = 180;
// "The last node" rule only applies against a strong enemy; in the opening a team holds one node against one and that is
// normal (deviation from the sheet's "exactly one", which would fire in the first minutes).
constexpr unsigned LAST_NODE_ENEMY_MIN = 3;

struct RaceInput
{
    std::uint32_t ourScore = 0;
    std::uint32_t theirScore = 0;
    bool closeOutOk = true;    // Eye of the Storm: the fourth base only when the odds there are 0 or better and 4 or more are free
    unsigned ourNodes = 0;     // occupied nodes only
    unsigned theirNodes = 0;
    Map const* map = &BgNodeMap::ARATHI;
};

// Deterministic; `previous` gives the hysteresis so that the plan does not flap at the margin.
inline Regime ChooseRegime(RaceInput const& in, Regime previous)
{
    Map const& map = *in.map;
    std::int32_t const etaUs = EtaSeconds(in.ourScore, in.ourNodes, map);
    std::int32_t const etaThem = EtaSeconds(in.theirScore, in.theirNodes, map);
    std::int32_t const dd = etaThem - etaUs;

    std::int32_t const panicLimit = PANIC_ETA_S + (previous == Regime::PANIC ? REGIME_HYSTERESIS_S : 0);
    if (etaThem <= panicLimit && etaUs > etaThem)
        return Regime::PANIC;

    // The last node but one decides: four of five at Arathi Basin, two of three at Battle for Gilneas (the third node, 28 points
    // a second against 0.8 to 1.7, ends a match in about a minute).
    if (in.ourNodes == map.closeOutNodes && in.closeOutOk)
        return Regime::CLOSE_OUT;
    if (in.ourNodes >= map.lastNodeOurMin && in.ourNodes <= map.lastNodeOurMax && in.theirNodes >= map.lastNodeEnemyMin)
        return Regime::LAST_NODE;

    // Hold: all nodes held at Battle for Gilneas (whatever the clock says); ahead on the clock with most nodes at Arathi Basin.
    std::int32_t const holdMargin = REGIME_MARGIN_S - (previous == Regime::HOLD ? REGIME_HYSTERESIS_S : 0);
    if (map.holdAtNodes ? in.ourNodes >= map.holdAtNodes :
        map.holdAtAllNodes ? in.ourNodes >= map.nodeCount : (dd >= holdMargin && in.ourNodes >= map.holdMinNodes))
        return Regime::HOLD;

    std::int32_t const pushMargin = REGIME_MARGIN_S - (previous == Regime::PUSH ? REGIME_HYSTERESIS_S : 0);
    if (dd <= -pushMargin)
        return Regime::PUSH;

    return Regime::EVEN;
}

// ---------------------------------------------------------------------------------------------------------------------
// Guard counts, opening counts, slots (sheet section 4).
// ---------------------------------------------------------------------------------------------------------------------
enum class Job : std::uint8_t
{
    GUARD,
    RAIDER,
    ASSAULT,
    MOBILE
};

inline char const* JobName(Job job)
{
    switch (job)
    {
        case Job::GUARD: return "guard";
        case Job::RAIDER: return "raider";
        case Job::ASSAULT: return "assault";
        case Job::MOBILE: return "mobile";
    }
    return "?";
}

struct Slot
{
    Job job = Job::MOBILE;
    std::uint8_t node = NODE_NONE;   // GUARD: the guarded node; ASSAULT: the opening group's node, NODE_NONE once free

    bool operator==(Slot const& o) const { return job == o.job && node == o.node; }
    bool operator!=(Slot const& o) const { return !(*this == o); }
};

// Guard leash: a guard fights what comes but returns to its post; it leaves only to restore or to answer an "inc" within
// reach (owner, 2026-11-12). Casual bots drift: a wider leash.
constexpr float GUARD_LEASH = 25.0f;
constexpr float GUARD_LEASH_CASUAL = 40.0f;
// Posts stand within this distance of the banner (sheet: within 10 yd, the click range).
constexpr float POST_RADIUS = 8.0f;

constexpr std::uint32_t LOW_BRACKET_BELOW_LEVEL = 20;

struct GuardInput
{
    Regime regime = Regime::EVEN;
    bool lowBracket = false;
    bool hasHealer = true;
    std::uint32_t enemyNear50 = 0;
    bool hardened = true;   // Eye of the Storm: the owner's bar is far enough from the enemy's reach (margin 15 or more)
};

inline bool IsContestedNode(std::uint8_t node)
{
    return node == NODE_LUMBER_MILL || node == NODE_GOLD_MINE;
}

// Guards per held node (sheet section 4, steady state table). Balanced: 1 at home, 2 at the Blacksmith, Gold Mine and
// Lumber Mill. Offense: 1 everywhere, 2 at the Blacksmith. Defense: 2 at home, 3 at the Blacksmith and at the far node.
// Battle for Gilneas (sheet section 4, steady state): guards per held node, near / Waterworks / far. Balanced 1, 2, 1; offense 1,
// 1, 1; defense 2, 3, 2; the hold state (all three held) 2, 2, 3 whatever the plan: the far node is the one next to the enemy's
// start room, where their dead come out.
constexpr std::uint32_t GILNEAS_HOLD_NEAR = 2;
constexpr std::uint32_t GILNEAS_HOLD_HUB = 2;
constexpr std::uint32_t GILNEAS_HOLD_FAR = 3;

inline std::uint32_t GilneasGuardCount(Plan plan, std::uint8_t team, std::uint8_t node, Regime regime, Map const& map)
{
    bool const isNear = node == HomeNode(team, map);
    bool const isHub = node == map.hub;
    if (regime == Regime::HOLD)
        return isNear ? GILNEAS_HOLD_NEAR : isHub ? GILNEAS_HOLD_HUB : GILNEAS_HOLD_FAR;
    if (plan == Plan::DEFENSE)
        return isHub ? 3 : 2;
    if (plan == Plan::OFFENSE)
        return 1;
    return isHub ? 2 : 1;
}

// Eye of the Storm (sheet section 4, steady state): one pinned defender per near base (two in the defense plan), one at a far
// base we hold (two in the defense plan); the hold state 2 per held base and 3 at the base the enemy is at; a base the owner has
// not hardened yet keeps the squad of 3 (a base captured at the threshold falls to one enemy in one 2 s tick).
constexpr std::uint32_t EYE_THIN_GUARDS = 3;
constexpr std::uint32_t EYE_HOLD_GUARDS = 2;
constexpr std::uint32_t EYE_HOLD_GUARDS_ATTACKED = 3;

inline std::uint32_t EyeGuardCount(Plan plan, std::uint8_t team, std::uint8_t node, GuardInput const& in, Map const& map)
{
    bool const isNear = IsNearNode(team, node, map);
    std::uint32_t n = plan == Plan::DEFENSE ? 2u : isNear ? 1u : 1u;
    switch (in.regime)
    {
        case Regime::HOLD:
            n = in.enemyNear50 > 0 ? EYE_HOLD_GUARDS_ATTACKED : EYE_HOLD_GUARDS;
            break;
        case Regime::PANIC:
            return 1;
        case Regime::LAST_NODE:
            return 4;
        case Regime::PUSH:
            n = in.enemyNear50 > 0 ? std::min(n, 2u) : 1u;
            break;
        case Regime::CLOSE_OUT:
            n = 1;
            break;
        default:
            break;
    }
    if (!in.hardened)
        n = std::max(n, EYE_THIN_GUARDS);
    return n;
}

inline std::uint32_t GuardCount(Plan plan, std::uint8_t team, std::uint8_t node, GuardInput const& in,
                                Map const& map = BgNodeMap::ARATHI)
{
    if (map.kind == BgNodeMap::Kind::EYE)
        return EyeGuardCount(plan, team, node, in, map);
    if (map.kind == BgNodeMap::Kind::GILNEAS)
    {
        std::uint32_t g = GilneasGuardCount(plan, team, node, in.regime, map);
        switch (in.regime)
        {
            case Regime::CLOSE_OUT:   // the third node: everybody who is not a guard goes, one guard stays at each held node
            case Regime::PANIC:
                return 1;
            case Regime::LAST_NODE:
                return 4;
            case Regime::PUSH:
                return in.enemyNear50 > 0 ? std::min(g, 2u) : 1u;
            default:
                return g;
        }
    }

    std::uint32_t n = 1;
    if (node == HomeNode(team, map))
        n = plan == Plan::DEFENSE ? 2 : 1;
    else if (node == HomeNode(OtherTeam(team), map))
        n = 1;
    else if (node == NODE_BLACKSMITH)
        n = plan == Plan::DEFENSE ? 3 : 2;
    else
        n = plan == Plan::DEFENSE ? 3 : plan == Plan::OFFENSE ? 1 : 2;

    // Low brackets have no mounts: the walks are 1.6 times longer, so the far nodes get two guards (sheet section 7).
    if (in.lowBracket && IsContestedNode(node))
        n = std::max(n, 2u);
    // No healer on the team: one more at the Blacksmith.
    if (!in.hasHealer && node == NODE_BLACKSMITH)
        ++n;

    switch (in.regime)
    {
        case Regime::LAST_NODE:
            n = 4;
            break;
        case Regime::PANIC:
            n = 1;
            break;
        case Regime::PUSH:
            n = in.enemyNear50 > 0 ? std::min(n, 2u) : 1u;
            break;
        default:
            break;
    }
    return n;
}

// Opening groups (sheet section 4): the number of bots that go for each node, the raiders and the reserve.
struct OpeningCounts
{
    std::uint32_t byNode[NODE_COUNT] = { 0, 0, 0, 0, 0 };
    std::uint32_t raiders = 0;
    std::uint32_t reserve = 0;

    std::uint32_t Total() const
    {
        std::uint32_t total = raiders + reserve;
        for (std::uint32_t v : byNode)
            total += v;
        return total;
    }
};

// A team of this many or more plays the full table (scaled to the team size); a smaller team opens the home node, the
// Blacksmith and the nearer contested node only, with one raider from 8 players on.
constexpr std::uint32_t FULL_TEAM = 10;
constexpr std::uint32_t RAIDER_MIN_TEAM = 8;

// Battle for Gilneas (sheet section 4): the offense plan has two raiders from 8 players on and one from 6; the ghost pair needs two
// stealth bots, so a team with fewer stealth bots has none (the slots go to the Waterworks group).
inline std::uint32_t GilneasRaiderCount(Plan plan, std::uint32_t size, std::uint32_t stealthBots)
{
    if (plan != Plan::OFFENSE || stealthBots < 2)
        return 0;
    std::uint32_t const wanted = size >= 8 ? 2u : size >= 6 ? 1u : 0u;
    return std::min(wanted, stealthBots);
}

// Battle for Gilneas (sheet section 4): near node 1 (2 in the defense plan from 6 players), raiders in the offense plan (stealth
// limited), the rest at Waterworks, never fewer than 4 there while the team has them.
constexpr std::uint32_t GILNEAS_HUB_MIN = 4;

inline OpeningCounts GilneasOpening(Plan plan, std::uint8_t team, std::uint32_t size, std::uint32_t stealthBots, Map const& map)
{
    OpeningCounts out;
    if (size == 0)
        return out;
    std::uint32_t nearCount = plan == Plan::DEFENSE && size >= 6 ? 2u : 1u;
    nearCount = std::min(nearCount, size);
    std::uint32_t raiders = GilneasRaiderCount(plan, size, stealthBots);
    while (raiders > 0 && size - nearCount - raiders < std::min(GILNEAS_HUB_MIN, size - nearCount))
        --raiders;
    out.byNode[HomeNode(team, map)] = nearCount;
    out.byNode[map.hub] = size - nearCount - raiders;
    out.raiders = raiders;
    return out;
}

// Eye of the Storm (sheet section 4): a squad of 3 at each near base (4 in the defense plan from 12 players), with two stealth
// raiders in the offense plan from 12 players; the rest is the middle group. Fewer than 6 bots take one base only, fewer than 3
// none (a net of 3 takes a neutral base in 14 s, a lone bot needs 40 s and loses it to one enemy in 2 s).
constexpr std::uint32_t EYE_SQUAD = 3;
constexpr std::uint32_t EYE_SQUAD_DEFENSE = 4;
constexpr std::uint32_t EYE_DEFENSE_MIN_TEAM = 12;
constexpr std::uint32_t EYE_RAIDER_MIN_TEAM = 12;

inline std::uint32_t EyeRaiderCount(Plan plan, std::uint32_t size, std::uint32_t stealthBots)
{
    if (plan != Plan::OFFENSE || size < EYE_RAIDER_MIN_TEAM || stealthBots < 2)
        return 0;
    return std::min(2u, stealthBots);
}

inline OpeningCounts EyeOpening(Plan plan, std::uint8_t team, std::uint32_t size, std::uint32_t stealthBots, Map const& map)
{
    OpeningCounts out;
    if (size < EYE_SQUAD)
        return out;
    std::uint32_t const squad = plan == Plan::DEFENSE && size >= EYE_DEFENSE_MIN_TEAM ? EYE_SQUAD_DEFENSE : EYE_SQUAD;
    std::uint32_t const squads = size >= 2 * EYE_SQUAD ? 2u : 1u;
    for (std::uint32_t i = 0; i < squads; ++i)
        out.byNode[map.nearNodes[team & 1][i]] = squad;
    out.raiders = std::min(EyeRaiderCount(plan, size, stealthBots), size - squads * squad);
    return out;
}

inline OpeningCounts OpeningFor(Plan plan, std::uint8_t team, std::uint32_t size, std::uint8_t nearerContested)
{
    OpeningCounts out;
    if (size == 0)
        return out;
    if (nearerContested != NODE_LUMBER_MILL)
        nearerContested = NODE_GOLD_MINE;

    std::uint8_t const home = HomeNode(team);
    std::uint8_t const other = nearerContested == NODE_GOLD_MINE ? NODE_LUMBER_MILL : NODE_GOLD_MINE;

    // Weights are the 15 player table: balanced "four cap" 2 / 4 / 4 / 4 + 1 raider, offense "push" 1 / 4 / 4 / 4 + 2
    // raiders, defense "hold three" 2 / 5 / 5 + 3 in reserve. Index: home, Blacksmith, nearer, other, raiders, reserve.
    std::uint32_t homeW = 2, bsW = 4, nearW = 4, otherW = 4, raidW = 1, reserveW = 0;
    if (plan == Plan::OFFENSE)
    {
        homeW = 1;
        raidW = 2;
    }
    else if (plan == Plan::DEFENSE)
    {
        bsW = 5;
        nearW = 5;
        otherW = 0;
        raidW = 0;
        reserveW = 3;
    }

    if (size < FULL_TEAM)
    {
        // Small team: three nodes only, one raider from 8 players, no separate reserve.
        homeW = 2;
        bsW = 4;
        nearW = 4;
        otherW = 0;
        raidW = size >= RAIDER_MIN_TEAM && plan != Plan::DEFENSE ? 1 : 0;
        reserveW = 0;
    }

    // Keep at least 1 at home and at least 2 at each opened node when the table has them (sheet: scale by
    // round(n / 15 * count)).
    std::vector<BgPlan::GroupWish> const wishes = {
        { homeW, 1 }, { bsW, 2 }, { nearW, 2 }, { otherW, 2 }, { raidW, 0 }, { reserveW, 0 }
    };
    std::vector<std::uint32_t> const parts = BgPlan::Apportion(wishes, size);
    out.byNode[home] = parts[0];
    out.byNode[NODE_BLACKSMITH] = parts[1];
    out.byNode[nearerContested] = parts[2];
    out.byNode[other] = parts[3];
    out.raiders = parts[4];
    out.reserve = parts[5];
    return out;
}

// After this long the opening groups are over and the steady assault count rules (sheet: the centre nodes are occupied at
// 135 to 145 s on foot).
constexpr std::uint32_t OPENING_SLOT_MS = 240u * 1000u;

// Steady state sizes (sheet section 4): ASSAULT and reserve per plan, the cap of one assault group, and the regime
// changes.
constexpr std::uint32_t ASSAULT_BALANCED = 4;
constexpr std::uint32_t ASSAULT_OFFENSE = 5;
constexpr std::uint32_t ASSAULT_DEFENSE = 2;
constexpr std::uint32_t RESERVE_BALANCED = 3;
constexpr std::uint32_t RESERVE_OFFENSE = 3;
constexpr std::uint32_t RESERVE_DEFENSE = 5;
constexpr std::uint32_t ASSAULT_HOLD = 2;
constexpr std::uint32_t ASSAULT_GROUP_MAX = 6;
constexpr std::uint32_t ASSAULT_PUSH_EXTRA = 2;

inline std::uint32_t PlanAssault(Plan plan)
{
    return plan == Plan::OFFENSE ? ASSAULT_OFFENSE : plan == Plan::DEFENSE ? ASSAULT_DEFENSE : ASSAULT_BALANCED;
}

// The reserve of a plan; Battle for Gilneas (sheet section 4): balanced 3, offense 1, defense 3.
inline std::uint32_t PlanReserve(Plan plan, Map const& map = BgNodeMap::ARATHI)
{
    if (map.kind == BgNodeMap::Kind::GILNEAS)
        return plan == Plan::OFFENSE ? 1 : 3;
    if (map.kind == BgNodeMap::Kind::EYE)
        return plan == Plan::OFFENSE ? 1 : plan == Plan::DEFENSE ? 3 : 2;
    return plan == Plan::OFFENSE ? RESERVE_OFFENSE : plan == Plan::DEFENSE ? RESERVE_DEFENSE : RESERVE_BALANCED;
}

// The free assault group: `freeBots` are the bots left after the guards and the raiders.
inline std::uint32_t AssaultCount(Plan plan, Regime regime, std::uint32_t freeBots, Map const& map = BgNodeMap::ARATHI)
{
    switch (regime)
    {
        case Regime::HOLD:
            // Battle for Gilneas: nothing to take with all three nodes held, the rest is the reserve at the far node. Eye of the Storm:
            // the middle group is the flag group, the reserve stays at the bases.
            if (map.kind == BgNodeMap::Kind::EYE)
            {
                std::uint32_t const reserve = PlanReserve(plan, map);
                return freeBots > reserve ? freeBots - reserve : 0u;
            }
            return map.kind == BgNodeMap::Kind::GILNEAS ? 0u : std::min(freeBots, ASSAULT_HOLD);
        case Regime::CLOSE_OUT:
        case Regime::PANIC:
        case Regime::LAST_NODE:
            return freeBots;   // everybody who is not a guard goes for the node that matters
        case Regime::PUSH:
        {
            std::uint32_t const wish = std::max(PlanAssault(plan) + ASSAULT_PUSH_EXTRA, freeBots > 1 ? freeBots - 1 : 0u);
            return std::min(freeBots, wish);
        }
        case Regime::EVEN:
            break;
    }
    std::uint32_t const reserve = PlanReserve(plan, map);
    std::uint32_t const surplus = freeBots > reserve ? freeBots - reserve : 0u;
    return std::min(std::min(freeBots, ASSAULT_GROUP_MAX), std::max(PlanAssault(plan), surplus));
}

// Raiders per plan (owner: one stealth class in balanced, two in offense, none in defense); a small team has one at most.
inline std::uint32_t RaiderCount(Plan plan, std::uint32_t size)
{
    if (size < RAIDER_MIN_TEAM)
        return 0;
    return plan == Plan::OFFENSE ? 2 : plan == Plan::BALANCED ? 1 : 0;
}

struct SlotInput
{
    std::uint8_t team = TEAM_IDX_ALLIANCE;
    Plan plan = Plan::BALANCED;
    Regime regime = Regime::EVEN;
    std::uint32_t size = 0;          // everybody on the team, humans included
    std::uint32_t humans = 0;        // a human takes a slot and is never commanded
    NodeSituation sit[NODE_COUNT] = { NodeSituation::NEUTRAL, NodeSituation::NEUTRAL, NodeSituation::NEUTRAL,
                                      NodeSituation::NEUTRAL, NodeSituation::NEUTRAL };
    std::uint32_t enemyNear50[NODE_COUNT] = { 0, 0, 0, 0, 0 };
    bool hardened[NODE_COUNT] = { true, true, true, true, true };   // Eye of the Storm: the owner's bar is hardened at the node
    bool lowBracket = false;
    bool hasHealer = true;
    std::uint32_t playMs = 0;
    std::uint8_t nearerContested = NODE_GOLD_MINE;
    Map const* map = &BgNodeMap::ARATHI;
    std::uint32_t stealthBots = 0;   // alive bots of a stealth class (Battle for Gilneas: the ghost pair)
};

// The order guards are filled in: the Blacksmith first (the centre), then the contested nodes, the enemy's home and our
// own home last (it is the safest).
inline std::vector<std::uint8_t> GuardOrder(std::uint8_t team, Map const& map = BgNodeMap::ARATHI)
{
    // Battle for Gilneas: the hub (Waterworks) first, then the node next to the enemy's start room (where their dead come out),
    // then ours.
    if (map.kind == BgNodeMap::Kind::GILNEAS)
        return { map.hub, HomeNode(OtherTeam(team), map), HomeNode(team, map) };
    // Eye of the Storm: the bases nearest the enemy's start room first (their dead walk from there), our near bases last.
    if (map.kind == BgNodeMap::Kind::EYE)
    {
        std::vector<std::uint8_t> order;
        for (std::uint8_t n = 0; n < map.nodeCount; ++n)
            order.push_back(n);
        Pt const room = map.start[OtherTeam(team) & 1];
        std::stable_sort(order.begin(), order.end(), [&](std::uint8_t a, std::uint8_t b)
        {
            return Dist2d(map.pos[a].x, map.pos[a].y, room.x, room.y) < Dist2d(map.pos[b].x, map.pos[b].y, room.x, room.y);
        });
        return order;
    }
    return { NODE_BLACKSMITH, NODE_GOLD_MINE, NODE_LUMBER_MILL, HomeNode(OtherTeam(team), map), HomeNode(team, map) };
}

// The slots of a team in fill order (sheet section 4): guards of the nodes held, raiders, the opening groups (while the
// opening lasts), the free assault group, then the reserve. The list has `size - humans` entries: a human takes the
// last slots (the reserve first) and is never commanded.
inline std::vector<Slot> BuildSlots(SlotInput const& in)
{
    std::vector<Slot> slots;
    std::uint32_t const wanted = in.size > in.humans ? in.size - in.humans : 0u;
    if (wanted == 0)
        return slots;

    // 1. Guards of the nodes we hold.
    for (std::uint8_t node : GuardOrder(in.team, *in.map))
    {
        if (!IsHeld(in.sit[node]))
            continue;
        GuardInput g;
        g.regime = in.regime;
        g.lowBracket = in.lowBracket;
        g.hasHealer = in.hasHealer;
        g.enemyNear50 = in.enemyNear50[node];
        g.hardened = in.hardened[node];
        for (std::uint32_t i = GuardCount(in.plan, in.team, node, g, *in.map); i > 0; --i)
            slots.push_back({ Job::GUARD, node });
    }
    std::uint32_t const guards = static_cast<std::uint32_t>(slots.size());

    // 2. Raiders.
    bool const gilneas = in.map->kind == BgNodeMap::Kind::GILNEAS;
    bool const eye = in.map->kind == BgNodeMap::Kind::EYE;
    std::uint32_t const raiders = eye ? EyeRaiderCount(in.plan, in.size, in.stealthBots) :
                                  gilneas ? GilneasRaiderCount(in.plan, in.size, in.stealthBots) : RaiderCount(in.plan, in.size);
    for (std::uint32_t i = 0; i < raiders; ++i)
        slots.push_back({ Job::RAIDER, NODE_NONE });

    // 3. The opening groups, while the opening lasts: the plan's table, for the nodes we do not hold yet.
    std::uint32_t openingAssault = 0;
    if (in.playMs < OPENING_SLOT_MS)
    {
        OpeningCounts const opening = eye ? EyeOpening(in.plan, in.team, in.size, in.stealthBots, *in.map) :
                                      gilneas ? GilneasOpening(in.plan, in.team, in.size, in.stealthBots, *in.map) :
                                                OpeningFor(in.plan, in.team, in.size, in.nearerContested);
        static std::uint8_t const nodeOrder[NODE_COUNT] = { NODE_BLACKSMITH, NODE_GOLD_MINE, NODE_LUMBER_MILL,
                                                            NODE_STABLES, NODE_FARM };
        // The near node's slot is filled first at Battle for Gilneas: the lone defender is picked from the whole team by its kit
        // before the Waterworks group takes the rest.
        std::uint8_t const gilneasOrder[3] = { HomeNode(in.team, *in.map), in.map->hub, HomeNode(OtherTeam(in.team), *in.map) };
        // Eye of the Storm: the near bases, the squads (the pinned defender is the squad's best kit and stays as its guard).
        std::uint8_t const eyeOrder[4] = { in.map->nearNodes[in.team & 1][0], in.map->nearNodes[in.team & 1][1], 0, 0 };
        // The squads are dealt one slot at a time so that the two best kits stand at different bases.
        if (eye)
        {
            for (std::uint32_t round = 0; round < EYE_DEFENSE_MIN_TEAM; ++round)
                for (std::uint8_t k = 0; k < 2; ++k)
                    if (!IsHeld(in.sit[eyeOrder[k]]) && round < opening.byNode[eyeOrder[k]])
                    {
                        slots.push_back({ Job::ASSAULT, eyeOrder[k] });
                        ++openingAssault;
                    }
        }
        for (std::uint8_t i = 0; i < (eye ? 0 : gilneas ? in.map->nodeCount : NODE_COUNT); ++i)
        {
            std::uint8_t const node = eye ? eyeOrder[i] : gilneas ? gilneasOrder[i] : nodeOrder[i];
            if (IsHeld(in.sit[node]))
                continue;
            for (std::uint32_t i = 0; i < opening.byNode[node]; ++i)
            {
                slots.push_back({ Job::ASSAULT, node });
                ++openingAssault;
            }
        }
    }

    // 4. The free assault group (in the opening only what the opening groups do not already supply), then the reserve:
    // the leftover bots get no slot and are mobile.
    std::uint32_t const freeBots = wanted > guards + raiders ? wanted - guards - raiders : 0u;
    std::uint32_t const assault = AssaultCount(in.plan, in.regime, freeBots, *in.map);
    for (std::uint32_t i = openingAssault; i < assault; ++i)
        slots.push_back({ Job::ASSAULT, NODE_NONE });
    while (slots.size() < wanted)
        slots.push_back({ Job::MOBILE, NODE_NONE });
    slots.resize(wanted);
    return slots;
}

// ---------------------------------------------------------------------------------------------------------------------
// Slot assignment: candidates, scores, stability. (Warsong's pattern, BgPlanCommon.h.)
// ---------------------------------------------------------------------------------------------------------------------
using BgPlan::CLS_WARRIOR;
using BgPlan::CLS_PALADIN;
using BgPlan::CLS_HUNTER;
using BgPlan::CLS_ROGUE;
using BgPlan::CLS_PRIEST;
using BgPlan::CLS_DEATH_KNIGHT;
using BgPlan::CLS_SHAMAN;
using BgPlan::CLS_MAGE;
using BgPlan::CLS_WARLOCK;
using BgPlan::CLS_MONK;
using BgPlan::CLS_DRUID;

struct Candidate : BgPlan::CandidateBase
{
    float x = 0.0f;
    float y = 0.0f;
    bool hasPrevious = false;
    Slot previous;
    bool pinned = false;     // casting the capture: never moved to another slot
};

struct AssignContext
{
    Map const* map = &BgNodeMap::ARATHI;
    std::uint8_t team = TEAM_IDX_ALLIANCE;
    std::uint8_t minLevel = 0;
    std::uint8_t maxLevel = 0;
    std::uint32_t healersTotal = 0;
    std::uint32_t healersPlaced = 0;
    std::uint32_t healersAt[NODE_COUNT] = { 0, 0, 0, 0, 0 };
    std::uint32_t assaultHealers = 0;
};

constexpr int LEVEL_STEP = 20;               // per level over the lowest in the team: level beats class fit (as in Warsong)
constexpr int STICKY_SLOT = 60;              // keeps the exact slot
constexpr int STICKY_NODE = 45;              // same node, another job (a capper becomes a guard)
constexpr int STICKY_JOB = 20;
constexpr int PIN_BONUS = 100000;
constexpr int DISTANCE_CAP = 30;             // points
constexpr float DISTANCE_YARDS_PER_POINT = 10.0f;
constexpr int HEALER_SPREAD_BONUS = 150;     // the first healer of a group
constexpr int HEALER_EXTRA_PENALTY = -300;   // a second healer in the same group, or a third in the assault group
constexpr int HEALER_RESERVE_PENALTY = -250; // the last free healer is kept for the assault group
constexpr int RAIDER_FIT = 40;

// Class and spec fit of a job, 0 to about 30.
inline int ClassFit(Candidate const& c, Job job)
{
    switch (job)
    {
        case Job::GUARD:
        {
            if (c.healer)
                return 20;
            if (c.tank)
                return 28;
            switch (c.cls)
            {
                case CLS_WARRIOR: return 24;
                case CLS_PALADIN: return 24;
                case CLS_DEATH_KNIGHT: return 22;
                case CLS_HUNTER: return 22;
                case CLS_MAGE: return 22;
                case CLS_WARLOCK: return 22;
                case CLS_SHAMAN: return 18;
                case CLS_DRUID: return 18;
                case CLS_PRIEST: return 16;
                case CLS_MONK: return 16;
                case CLS_ROGUE: return 8;
                default: return 10;
            }
        }
        case Job::ASSAULT:
        {
            if (c.healer)
                return 10;
            switch (c.cls)
            {
                case CLS_WARRIOR: return 26;
                case CLS_ROGUE: return 24;
                case CLS_DEATH_KNIGHT: return 24;
                case CLS_DRUID: return c.feral ? 24 : 14;
                case CLS_MONK: return 22;
                case CLS_PALADIN: return 22;
                case CLS_SHAMAN: return 18;
                case CLS_HUNTER: return 14;
                case CLS_MAGE: return 14;
                case CLS_WARLOCK: return 12;
                case CLS_PRIEST: return 6;
                default: return 10;
            }
        }
        case Job::MOBILE:
        {
            if (c.healer)
                return 18;
            switch (c.cls)
            {
                case CLS_SHAMAN: return 22;
                case CLS_DRUID: return 22;
                case CLS_HUNTER: return 20;
                case CLS_MAGE: return 20;
                case CLS_PALADIN: return 18;
                case CLS_WARLOCK: return 16;
                case CLS_PRIEST: return 16;
                case CLS_MONK: return 16;
                case CLS_DEATH_KNIGHT: return 14;
                case CLS_WARRIOR: return 14;
                case CLS_ROGUE: return 10;
                default: return 10;
            }
        }
        case Job::RAIDER:
            return BgPlan::IsStealthClass(c) ? RAIDER_FIT : 0;
    }
    return 0;
}

// Battle for Gilneas (sheet section 4): the near-node defender. The guide trusts three kits to hold a node alone, rogue, mage and
// hunter; then kits with a self-heal or an escape; healers last. For this one slot the kit, the tier and the level count in the
// ratio 10 : 1 : 2 per point (a kit step is worth about 9 levels), not the level-first order of the other slots.
inline int DefenderFit(BgPlan::CandidateBase const& m)
{
    if (m.healer)
        return 4;
    switch (m.cls)
    {
        case BgPlan::CLS_ROGUE: return 30;
        case BgPlan::CLS_MAGE: return 30;
        case BgPlan::CLS_HUNTER: return 30;
        case BgPlan::CLS_DRUID: return 18;
        case BgPlan::CLS_DEATH_KNIGHT: return 18;
        case BgPlan::CLS_PALADIN: return 18;
        case BgPlan::CLS_MONK: return 16;
        case BgPlan::CLS_WARLOCK: return 16;
        case BgPlan::CLS_WARRIOR: return m.tank ? 18 : 12;
        case BgPlan::CLS_SHAMAN: return 12;
        default: return 8;
    }
}

constexpr int DEFENDER_FIT_WEIGHT = 10;
constexpr int EYE_DEFENDER_FIT_WEIGHT = 2;     // Eye of the Storm: 0 to 60 points, a kit step is worth less than the stickiness
constexpr int DEFENDER_LEVEL_STEP = 20;     // per level over the lowest in the team
constexpr int DEFENDER_TIER_GOOD = 40;      // a lone defender who drifts loses the node: good and expert hold better
constexpr int DEFENDER_TIER_EXPERT = 60;

inline int DefenderScore(BgPlan::CandidateBase const& m, std::uint8_t minLevel)
{
    int const tier = m.tier == Tier::EXPERT ? DEFENDER_TIER_EXPERT : m.tier == Tier::GOOD ? DEFENDER_TIER_GOOD : 0;
    return DefenderFit(m) * DEFENDER_FIT_WEIGHT + tier + (int(m.level) - int(minLevel)) * DEFENDER_LEVEL_STEP;
}

inline int TierFit(Tier tier, Job job)
{
    // Expert bots react at once: they take the jobs where a late reaction costs most. Casual bots hurt least at a post.
    return job == Job::GUARD ? BgPlan::TierFitPost(tier) : BgPlan::TierFitCritical(tier);
}

inline int SlotScore(Candidate const& c, Slot const& slot, AssignContext const& ctx)
{
    if (c.pinned && c.hasPrevious)
        return slot == c.previous ? PIN_BONUS : BgPlan::NOT_ELIGIBLE;

    // Only a stealth class raids; a raider slot without one stays empty and its bot goes to the reserve.
    if (slot.job == Job::RAIDER && !BgPlan::IsStealthClass(c))
        return BgPlan::NOT_ELIGIBLE;

    int score = ClassFit(c, slot.job) + TierFit(c.tier, slot.job);
    int const level = static_cast<int>(c.level) - static_cast<int>(ctx.minLevel);
    int const levelFromTop = static_cast<int>(ctx.maxLevel) - static_cast<int>(c.level);

    // The near-node slot of Battle for Gilneas, the lone defender (its opening group and its guard): by kit first.
    bool const defenderSlot = ctx.map->kind == BgNodeMap::Kind::GILNEAS && slot.node == HomeNode(ctx.team, *ctx.map) &&
                              (slot.job == Job::GUARD || slot.job == Job::ASSAULT);

    // Eye of the Storm: the squad at a near base and its pinned guard take the best kit as a bonus (a rogue, mage or hunter stands
    // alone best), but the bot already there keeps the slot: the stickiness below outweighs the bonus of a bot far away.
    bool const eyeNear = ctx.map->kind == BgNodeMap::Kind::EYE && slot.node < ctx.map->nodeCount &&
                         IsNearNode(ctx.team, slot.node, *ctx.map) && (slot.job == Job::GUARD || slot.job == Job::ASSAULT);

    if (defenderSlot)
        score = DefenderScore(c, ctx.minLevel);
    else if (eyeNear)
        score += DefenderFit(c) * EYE_DEFENDER_FIT_WEIGHT;
    if (!defenderSlot)
    switch (slot.job)
    {
        case Job::GUARD:
            // The highest levels take the Blacksmith; the lowest stand at home next to a healer (sheet section 7).
            if (slot.node == ctx.map->hub)
                score += level * LEVEL_STEP;
            else if (slot.node == HomeNode(ctx.team, *ctx.map))
                score += levelFromTop * (LEVEL_STEP / 2);
            else
                score += level * (LEVEL_STEP / 2);
            break;
        case Job::ASSAULT:
            score += level * LEVEL_STEP;
            break;
        case Job::MOBILE:
        case Job::RAIDER:
            score += level * (LEVEL_STEP / 4);
            break;
    }

    // Distance cost: the bots that arrive first hold the node.
    if ((slot.job == Job::GUARD || slot.job == Job::ASSAULT) && slot.node < ctx.map->nodeCount)
    {
        int const cost = static_cast<int>(DistToNode(slot.node, c.x, c.y, *ctx.map) / DISTANCE_YARDS_PER_POINT);
        score -= std::min(cost, DISTANCE_CAP);
    }

    // Healers: one per group, the first one at the Blacksmith, one with the assault group (never more than two there);
    // the last free healer is kept for the assault group.
    if (c.healer && !defenderSlot)
    {
        if (slot.job == Job::GUARD && slot.node < ctx.map->nodeCount)
        {
            score += ctx.healersAt[slot.node] == 0 ? HEALER_SPREAD_BONUS : HEALER_EXTRA_PENALTY;
            if (slot.node != ctx.map->hub && ctx.healersTotal >= 2 && ctx.assaultHealers == 0 &&
                ctx.healersTotal - ctx.healersPlaced <= 1)
                score += HEALER_RESERVE_PENALTY;
        }
        else if (slot.job == Job::ASSAULT)
            score += ctx.assaultHealers == 0 ? HEALER_SPREAD_BONUS : HEALER_EXTRA_PENALTY;
    }

    if (c.hasPrevious)
    {
        if (slot == c.previous)
            score += STICKY_SLOT;
        else if (slot.node != NODE_NONE && slot.node == c.previous.node)
            score += STICKY_NODE;
        else if (slot.job == c.previous.job)
            score += STICKY_JOB;
    }
    return score;
}

struct Assignment
{
    std::uint32_t guidLow = 0;
    Slot slot;
};

// Fills the slots in order, each with its best-scoring free candidate (ties by the GUID hash, then the GUID). Candidates
// without a slot become the mobile reserve; a slot nobody fits (a raider slot without a stealth class) stays empty.
inline std::vector<Assignment> Assign(std::vector<Candidate> const& candidates, std::vector<Slot> const& slots,
                                      std::uint8_t team, Map const& map = BgNodeMap::ARATHI)
{
    AssignContext ctx;
    ctx.map = &map;
    ctx.team = team;
    BgPlan::LevelSpan const span = BgPlan::SpanOf(candidates);
    ctx.minLevel = span.minLevel;
    ctx.maxLevel = span.maxLevel;
    for (Candidate const& c : candidates)
        if (c.healer)
            ++ctx.healersTotal;

    std::vector<bool> taken(candidates.size(), false);
    std::vector<Assignment> result;
    result.reserve(candidates.size());

    for (Slot const& slot : slots)
    {
        std::size_t const best = BgPlan::PickBest(candidates, taken, [&](Candidate const& c) { return SlotScore(c, slot, ctx); });
        if (best == candidates.size())
            continue;

        taken[best] = true;
        result.push_back({ candidates[best].guidLow, slot });
        if (candidates[best].healer)
        {
            ++ctx.healersPlaced;
            if (slot.job == Job::GUARD && slot.node < map.nodeCount)
                ++ctx.healersAt[slot.node];
            else if (slot.job == Job::ASSAULT)
                ++ctx.assaultHealers;
        }
    }

    for (std::size_t i = 0; i < candidates.size(); ++i)
        if (!taken[i])
            result.push_back({ candidates[i].guidLow, Slot() });
    return result;
}

// ---------------------------------------------------------------------------------------------------------------------
// Targets for the assault group and for the raiders.
// ---------------------------------------------------------------------------------------------------------------------
constexpr float TARGET_ETA_SOFTENER_S = 20.0f;
constexpr float TARGET_VALUE_SCALE = 10000.0f;   // a 3 / 2 flip scores about 240 at 60 s away, a fifth node about 3400
constexpr int TARGET_EXCESS_PENALTY = 50;        // per enemy near the banner beyond our group
constexpr std::uint32_t CAST_AND_CAPTURE_S = 8;

struct TargetInput
{
    std::uint8_t node = NODE_NONE;
    NodeSituation sit = NodeSituation::NEUTRAL;
    std::uint32_t enemyNear40 = 0;
    std::uint32_t friendNear40 = 0;
    float etaS = 0.0f;           // for the group to reach the banner
    bool flipBlocked = false;    // flipped back twice in the last 3 minutes (expert rule)
};

struct TargetScore
{
    std::uint8_t node;
    int score;
};

// The value of taking a node per second of effort, minus a penalty for each enemy beyond the group's size. Nodes that
// are not targetable, and nodes the flip limiter blocks for an expert, are left out. Highest score first; ties by node.
inline std::vector<TargetScore> ScoreTargets(std::vector<TargetInput> const& inputs, unsigned ourNodes, unsigned theirNodes,
                                             std::uint32_t groupSize, Tier tier, Map const& map = BgNodeMap::ARATHI)
{
    std::vector<TargetScore> out;
    for (TargetInput const& t : inputs)
    {
        if (t.node >= map.nodeCount || !IsTargetable(t.sit))
            continue;
        if (t.flipBlocked && BotSkillMath::Uses(tier, Tier::EXPERT))
            continue;

        SwingKind const kind = t.sit == NodeSituation::THEIRS ? SwingKind::TAKE_ENEMY : SwingKind::TAKE_FREE;
        float const value = SwingValue(ourNodes, theirNodes, kind, map);
        std::uint32_t const excess = t.enemyNear40 > groupSize ? t.enemyNear40 - groupSize : 0u;
        int const score = static_cast<int>(value * TARGET_VALUE_SCALE / (t.etaS + TARGET_ETA_SOFTENER_S)) -
                          static_cast<int>(excess) * TARGET_EXCESS_PENALTY;
        out.push_back({ t.node, score });
    }

    std::stable_sort(out.begin(), out.end(), [](TargetScore const& a, TargetScore const& b)
    {
        if (a.score != b.score)
            return a.score > b.score;
        return a.node < b.node;
    });
    return out;
}

// A group keeps its target while it is nearly as good as the best one: the distances change as the group walks, and a
// target that flips every two seconds sends the whole team back and forth. `previous` is the node chosen last time.
constexpr int TARGET_KEEP_MARGIN = 30;           // points
constexpr int TARGET_KEEP_PERCENT = 20;          // or this share of the best score, whichever is more

inline std::uint8_t StickyTarget(std::vector<TargetScore> const& sorted, std::uint8_t previous)
{
    if (sorted.empty())
        return NODE_NONE;
    if (previous == NODE_NONE || previous == sorted.front().node)
        return sorted.front().node;

    for (TargetScore const& t : sorted)
    {
        if (t.node != previous)
            continue;
        int const margin = std::max(TARGET_KEEP_MARGIN, sorted.front().score * TARGET_KEEP_PERCENT / 100);
        return t.score + margin >= sorted.front().score ? previous : sorted.front().node;
    }
    return sorted.front().node;   // the previous target is no longer one
}

// The node a casual group takes: the nearest one it can click. (Casual bots do not weigh the swing.)
inline std::uint8_t NearestTarget(std::vector<TargetInput> const& inputs, Map const& map = BgNodeMap::ARATHI)
{
    std::uint8_t best = NODE_NONE;
    float bestEta = 0.0f;
    for (TargetInput const& t : inputs)
    {
        if (t.node >= map.nodeCount || !IsTargetable(t.sit))
            continue;
        if (best == NODE_NONE || t.etaS < bestEta || (t.etaS == bestEta && t.node < best))
        {
            best = t.node;
            bestEta = t.etaS;
        }
    }
    return best;
}

// The fifth node when we hold four: the one node that is not ours and that a group can still click.
inline std::uint8_t CloseOutNode(NodeSituation const (&sit)[NODE_COUNT], Map const& map = BgNodeMap::ARATHI)
{
    std::uint8_t found = NODE_NONE;
    for (std::uint8_t n = 0; n < map.nodeCount; ++n)
    {
        if (sit[n] == NodeSituation::OURS)
            continue;
        if (found != NODE_NONE)
            return NODE_NONE;   // more than one node is not ours: not a close-out
        found = n;
    }
    return found;
}

// The raider's choice: an enemy-occupied node with at most one defender near its banner, the fewest defenders first,
// the enemy home first on a tie (it is the farthest from their reserve), then the lower node. One defender is only
// attempted by an expert (Sap or Cheap Shot, sheet section 6); two or more is never attempted alone.
struct RaidInput
{
    std::uint8_t node = NODE_NONE;
    NodeSituation sit = NodeSituation::NEUTRAL;
    std::uint32_t enemyNear40 = 0;
};

constexpr std::uint32_t RAID_MAX_DEFENDERS = 1;

inline std::uint8_t PickRaidNode(std::vector<RaidInput> const& inputs, std::uint8_t team, Tier tier,
                                 Map const& map = BgNodeMap::ARATHI)
{
    std::uint8_t best = NODE_NONE;
    std::uint32_t bestDefenders = 0;
    bool bestIsHome = false;
    for (RaidInput const& r : inputs)
    {
        if (r.node >= map.nodeCount || r.sit != NodeSituation::THEIRS)
            continue;
        // Eye of the Storm: a lone defender in the circle neutralises the pair's net of 2 to 1 and the take needs 40 s, so only a base
        // with no defender seen is raided.
        std::uint32_t const maxDefenders = IsPresence(map) ? 0u : RAID_MAX_DEFENDERS;
        if (r.enemyNear40 > maxDefenders || (r.enemyNear40 == maxDefenders && maxDefenders && !BotSkillMath::Uses(tier, Tier::EXPERT)))
            continue;

        bool const isHome = r.node == HomeNode(OtherTeam(team), map);
        bool const better = best == NODE_NONE || r.enemyNear40 < bestDefenders ||
                            (r.enemyNear40 == bestDefenders && isHome && !bestIsHome) ||
                            (r.enemyNear40 == bestDefenders && isHome == bestIsHome && r.node < best);
        if (better)
        {
            best = r.node;
            bestDefenders = r.enemyNear40;
            bestIsHome = isHome;
        }
    }
    return best;
}

// A raider stays for the full 60 seconds only when nobody is within 60 yd of the banner; otherwise it leaves when the
// cast ends (the contest runs without it).
constexpr float RAID_STAY_CLEAR_YARDS = 60.0f;

// ---------------------------------------------------------------------------------------------------------------------
// Responders: who answers a node that is assaulted or called.
// ---------------------------------------------------------------------------------------------------------------------
constexpr std::uint32_t INC_ENEMIES_MIN = 3;            // enemies near a held banner that make a guard call "inc"
constexpr std::uint32_t INC_ENEMIES_MIN_LOW = 2;        // the 15 to 19 bracket (no mounts, small health pools)
constexpr float INC_RADIUS = 50.0f;
constexpr std::uint32_t RESPONDERS = 2;
constexpr std::uint32_t RESPONDERS_BIG = 3;             // six or more enemies
constexpr std::uint32_t BIG_ATTACK_ENEMIES = 6;
constexpr std::uint32_t RESPONDER_MAX_ETA_MS = 90000;   // farther than this does not answer
constexpr std::uint32_t RESTORE_DEADLINE_MS = 50000;    // the sheet's deadline: assault time + 50 s

// A bot that answered a node at the last refresh keeps the answer unless another bot is clearly nearer: its walk time
// counts this much shorter (the responders must not change at every refresh while an attack goes on).
constexpr std::uint32_t RESPONDER_STICKY_MS = 15000;

inline std::uint32_t ResponderCount(std::uint32_t enemies)
{
    return enemies >= BIG_ATTACK_ENEMIES ? RESPONDERS_BIG : RESPONDERS;
}

inline std::uint32_t IncThreshold(bool lowBracket)
{
    return lowBracket ? INC_ENEMIES_MIN_LOW : INC_ENEMIES_MIN;
}

struct ResponderCandidate
{
    std::uint32_t guidLow = 0;
    Job job = Job::MOBILE;
    std::uint8_t slotNode = NODE_NONE;
    std::uint32_t etaMs = 0;      // for the bot to reach the banner
};

// The bots that answer one node: the nearest reserve bots first, then assault bots (never more than half of the group),
// never a guard of another node and never a raider. A guard of the node itself is always a candidate. `maxEtaMs` is the
// farthest an answer may be away (for a restore: what is left of the contest minus the cast).
inline std::vector<std::uint32_t> ChooseResponders(std::vector<ResponderCandidate> const& all, std::uint8_t node,
                                                   std::size_t count, std::uint32_t maxEtaMs, std::uint32_t assaultAlive)
{
    std::vector<ResponderCandidate> mobile;
    std::vector<ResponderCandidate> assault;
    std::vector<ResponderCandidate> guards;
    for (ResponderCandidate const& c : all)
    {
        if (c.etaMs > maxEtaMs)
            continue;
        if (c.job == Job::GUARD && c.slotNode == node)
            guards.push_back(c);
        else if (c.job == Job::MOBILE)
            mobile.push_back(c);
        else if (c.job == Job::ASSAULT)
            assault.push_back(c);
    }

    auto const byEta = [](ResponderCandidate const& a, ResponderCandidate const& b)
    {
        if (a.etaMs != b.etaMs)
            return a.etaMs < b.etaMs;
        return a.guidLow < b.guidLow;
    };
    std::sort(mobile.begin(), mobile.end(), byEta);
    std::sort(assault.begin(), assault.end(), byEta);
    std::sort(guards.begin(), guards.end(), byEta);

    std::vector<std::uint32_t> out;
    for (ResponderCandidate const& c : mobile)
        if (out.size() < count)
            out.push_back(c.guidLow);
    std::size_t const assaultLimit = std::min<std::size_t>(count - std::min(count, out.size()), assaultAlive / 2);
    for (std::size_t i = 0; i < assault.size() && i < assaultLimit; ++i)
        out.push_back(assault[i].guidLow);
    // The nearest guard of the node itself restores it too (one extra, on top of the count).
    if (!guards.empty())
        out.push_back(guards.front().guidLow);
    return out;
}

// ---------------------------------------------------------------------------------------------------------------------
// The flip limiter (expert rule, sheet section 6): a node flipped back twice in 3 minutes is not re-assaulted unless the
// local odds improved.
// ---------------------------------------------------------------------------------------------------------------------
constexpr std::uint32_t FLIP_WINDOW_MS = 180000;
constexpr std::uint32_t FLIP_LIMIT = 2;
constexpr int ODDS_IMPROVED_MARGIN = 2;      // more of us than of them near the banner

struct FlipLimiter
{
    std::vector<std::uint32_t> flips[NODE_COUNT];

    void Record(std::uint8_t node, std::uint32_t nowMs)
    {
        if (node >= NODE_COUNT)
            return;
        Prune(node, nowMs);
        flips[node].push_back(nowMs ? nowMs : 1);
    }

    bool Blocked(std::uint8_t node, std::uint32_t nowMs, int oddsOurMinusTheir) const
    {
        if (node >= NODE_COUNT)
            return false;
        std::uint32_t recent = 0;
        for (std::uint32_t t : flips[node])
            if (nowMs - t < FLIP_WINDOW_MS)
                ++recent;
        return recent >= FLIP_LIMIT && oddsOurMinusTheir < ODDS_IMPROVED_MARGIN;
    }

private:
    void Prune(std::uint8_t node, std::uint32_t nowMs)
    {
        flips[node].erase(std::remove_if(flips[node].begin(), flips[node].end(),
                                         [nowMs](std::uint32_t t) { return nowMs - t >= FLIP_WINDOW_MS; }),
                          flips[node].end());
    }
};

// ---------------------------------------------------------------------------------------------------------------------
// Retreat and ghost (Battle for Gilneas, sheet section 6). A group that loses a local fight walks to the nearest node we hold,
// where the dead come out in the next 30 s wave, and pushes again as one group; two expert stealth bots may ink the enemy's near
// node instead. Both read what the team has seen (call-outs and friends in sight), never the whole enemy roster.
// ---------------------------------------------------------------------------------------------------------------------
constexpr int RETREAT_ODDS = -3;               // alive friends minus seen enemies within FIGHT_RADIUS of the fight
constexpr int RETREAT_ODDS_NO_HEALER = -2;     // the same with no healer alive near the fight
constexpr float FIGHT_RADIUS = 40.0f;
constexpr float RETREAT_REACH = 60.0f;         // a bot this close to the fight belongs to the group
constexpr std::uint32_t RETREAT_REGROUP_MEMBERS = 4;
constexpr std::uint32_t RETREAT_MAX_MS = 25000;
constexpr std::uint32_t NO_RETREAT_BEFORE_FLIP_MS = 15000;   // a capping group within 15 s of its flip does not retreat
constexpr std::uint32_t GHOST_PAIR = 2;
constexpr std::uint32_t GHOST_MAX_DEFENDERS = 1;             // seen at the enemy's near node

inline int FightOdds(unsigned friends, unsigned enemies)
{
    return int(friends) - int(enemies);
}

inline bool Losing(int odds, bool friendHealerAlive)
{
    return odds <= RETREAT_ODDS || (!friendHealerAlive && odds <= RETREAT_ODDS_NO_HEALER);
}

// The retreat ends when enough of the group stands at the rally node, or after the time limit.
inline bool RetreatOver(unsigned membersAtRally, std::uint32_t sinceMs)
{
    return membersAtRally >= RETREAT_REGROUP_MEMBERS || sinceMs >= RETREAT_MAX_MS;
}

// A capping group within NO_RETREAT_BEFORE_FLIP_MS of its flip stays and fights.
inline bool CappingNearFlip(NodeSituation sit, std::uint32_t captureAgeMs)
{
    return sit == NodeSituation::CAPPING && captureAgeMs + NO_RETREAT_BEFORE_FLIP_MS >= CAPTURE_TIME_MS;
}

// The node a losing group falls back to: the nearest held node other than the one it fights at; NODE_NONE when there is none
// (then the group fights on).
inline std::uint8_t RallyNode(NodeSituation const (&sit)[NODE_COUNT], std::uint8_t from, float x, float y,
                              Map const& map = BgNodeMap::ARATHI)
{
    std::uint8_t best = NODE_NONE;
    float bestDist = 0.0f;
    for (std::uint8_t n = 0; n < map.nodeCount; ++n)
    {
        if (n == from || !IsHeld(sit[n]))
            continue;
        float const d = DistToNode(n, x, y, map);
        if (best == NODE_NONE || d < bestDist)
        {
            best = n;
            bestDist = d;
        }
    }
    return best;
}

// The ghost (expert): the fight goes badly, two expert stealth bots are alive and the enemy's near node has at most one defender
// in sight.
inline bool GhostWanted(int odds, bool friendHealerAlive, unsigned stealthExperts, unsigned enemyDefendersSeen)
{
    return Losing(odds, friendHealerAlive) && stealthExperts >= GHOST_PAIR && enemyDefendersSeen <= GHOST_MAX_DEFENDERS;
}

// ---------------------------------------------------------------------------------------------------------------------
// Team intel comes from living observers within sight range and from call-outs.
// The visibility check is supplied by the caller so game-specific detection stays outside pure logic.
// ---------------------------------------------------------------------------------------------------------------------
constexpr float SEEN_RADIUS = 70.0f;

inline bool SeenFrom(float enemyX, float enemyY, float friendX, float friendY)
{
    return Dist2d(enemyX, enemyY, friendX, friendY) <= SEEN_RADIUS;
}

inline bool WithinSight(float enemyX, float enemyY, float enemyZ, float friendX, float friendY, float friendZ)
{
    float const dx = enemyX - friendX;
    float const dy = enemyY - friendY;
    float const dz = enemyZ - friendZ;
    return dx * dx + dy * dy + dz * dz <= SEEN_RADIUS * SEEN_RADIUS;
}

template <typename Observers, typename Alive, typename InRange, typename CanSee>
bool SeenByTeam(bool enemyAlive, Observers const& observers, Alive alive, InRange inRange, CanSee canSee)
{
    if (!enemyAlive)
        return false;
    for (auto const& observer : observers)
        if (alive(observer) && inRange(observer) && canSee(observer))
            return true;
    return false;
}

// ---------------------------------------------------------------------------------------------------------------------
// Options: every decision of a bot is a scored list.
// ---------------------------------------------------------------------------------------------------------------------
enum class Kind : std::uint8_t
{
    RESTORE,         // click our own node back (contested by them, and it was ours)
    CLOSE_OUT,       // take the fifth node
    REINFORCE,       // walk to a held node under pressure ("inc")
    SUPPORT_HUMAN,   // join a human who is capturing or called a node
    HOLD_CAPTURE,    // stay at a node we are capturing until it flips
    GATHER,          // wait short of the node for the group
    TAKE,            // take or flip a node
    RAID,            // hit-and-run assault on an enemy node
    HOLD_POST,       // the guard post at the banner
    HOLD_LOOSE,      // a casual guard: anywhere inside the wider leash
    STAGE,           // the reserve's staging post
    HOLD_QUIET,      // nothing to do
    RETREAT,         // BT-552: leave a lost fight for the nearest node we hold and regroup there (Battle for Gilneas)
    GHOST            // BT-552: the expert pair that inks the enemy's near node while the main fight goes badly
};

inline char const* KindName(Kind k)
{
    switch (k)
    {
        case Kind::RESTORE: return "restore";
        case Kind::CLOSE_OUT: return "close-out";
        case Kind::REINFORCE: return "reinforce";
        case Kind::SUPPORT_HUMAN: return "support-human";
        case Kind::HOLD_CAPTURE: return "hold-capture";
        case Kind::GATHER: return "gather";
        case Kind::TAKE: return "take";
        case Kind::RAID: return "raid";
        case Kind::HOLD_POST: return "post";
        case Kind::HOLD_LOOSE: return "post-loose";
        case Kind::STAGE: return "stage";
        case Kind::HOLD_QUIET: return "quiet";
        case Kind::RETREAT: return "retreat";
        case Kind::GHOST: return "ghost";
    }
    return "?";
}

struct Option
{
    Kind kind;
    std::uint8_t node;
    int score;
};

// A non-guard that has begun a capture stays at least this long (good rule: the capping group stays 15 s; guards stay
// until the node flips).
constexpr std::uint32_t STAY_AFTER_CAST_MS = 15000;
constexpr std::uint32_t EYE_STAY_MS = 10u * 60u * 1000u;
// Battle for Gilneas: the capping group stays until the node flips (the 60 s that decide the match, sheet section 6).
inline std::uint32_t StayAfterCastMs(Map const& map)
{
    // Eye of the Storm: a group in the circle stays while it has the net edge there (the bar decides, not a timer).
    return map.kind == BgNodeMap::Kind::EYE ? EYE_STAY_MS : map.kind == BgNodeMap::Kind::GILNEAS ? CAPTURE_TIME_MS : STAY_AFTER_CAST_MS;
}
// The opening group waits this long for its members, at the gather point 40 yd short of the banner (sheet section 5).
constexpr std::uint32_t GATHER_WAIT_MS = 15000;
constexpr float GATHER_DISTANCE = 40.0f;
constexpr float EYE_GATHER_BEYOND_CIRCLE = 30.0f;   // Eye of the Storm: the squad waits this far outside the circle's edge
constexpr float GATHER_RADIUS = 25.0f;
constexpr std::uint32_t GATHER_MIN_MEMBERS = 3;

// Where an opening group waits: GATHER_DISTANCE from the banner, and outside the circle on a presence map (a bot in it already
// moves the bar alone).
inline float GatherDistance(Map const& map)
{
    return IsPresence(map) ? map.circleRadius + EYE_GATHER_BEYOND_CIRCLE : GATHER_DISTANCE;
}

struct DecisionContext
{
    Map const* map = &BgNodeMap::ARATHI;
    Job job = Job::MOBILE;
    std::uint8_t slotNode = NODE_NONE;
    Tier tier = Tier::GOOD;
    Regime regime = Regime::EVEN;
    NodeSituation sit[NODE_COUNT] = { NodeSituation::NEUTRAL, NodeSituation::NEUTRAL, NodeSituation::NEUTRAL,
                                      NodeSituation::NEUTRAL, NodeSituation::NEUTRAL };
    std::uint32_t captureAgeMs[NODE_COUNT] = { 0, 0, 0, 0, 0 };   // since our contest of the node began
    bool restoreResponder = false;
    std::uint8_t restoreNode = NODE_NONE;
    bool incResponder = false;
    std::uint8_t incNode = NODE_NONE;
    bool supportHuman = false;
    std::uint8_t supportNode = NODE_NONE;
    std::uint8_t assaultNode = NODE_NONE;     // the team's swing-chosen target
    std::uint8_t nearestNode = NODE_NONE;     // what a casual group goes for
    std::uint8_t closeOutNode = NODE_NONE;
    std::uint8_t raidNode = NODE_NONE;
    bool raidStay = false;
    std::uint8_t stageNode = NODE_BLACKSMITH;
    std::uint8_t retreatNode = NODE_NONE;     // the rally node while the bot's group is losing its fight (good rule)
    std::uint8_t ghostNode = NODE_NONE;       // the enemy's near node while the bot is one of the ghost pair (expert rule)
    bool groupGathered = true;                // the opening group of the slot node is together (or waited long enough)
};

inline bool Plays(DecisionContext const& c, Tier line)
{
    return BotSkillMath::Uses(c.tier, line);
}

// Score bands: 900s reactions to an event, 700s to 800s the duty of the job, 600s waiting.
constexpr int SCORE_RESTORE = 960;
constexpr int SCORE_CLOSE_OUT = 940;
constexpr int SCORE_RETREAT = 930;      // below the third node, above the reinforcement of a held node
constexpr int SCORE_GHOST = 925;
constexpr int SCORE_REINFORCE = 900;
constexpr int SCORE_SUPPORT_HUMAN = 880;
constexpr int SCORE_HOLD_CAPTURE = 820;
constexpr int SCORE_GATHER = 770;
constexpr int SCORE_TAKE = 760;
constexpr int SCORE_TAKE_MOBILE = 740;
constexpr int SCORE_RAID = 750;
constexpr int SCORE_HOLD_POST = 700;
constexpr int SCORE_HOLD_LOOSE = 690;
constexpr int SCORE_STAGE = 650;
constexpr int SCORE_QUIET = 300;

inline std::vector<Option> Options(DecisionContext const& c)
{
    std::vector<Option> out;
    auto const add = [&out](Kind kind, std::uint8_t node, int score) { out.push_back({ kind, node, score }); };

    // Casual bots ignore the race regimes and the swing (they take the nearest node); good bots play the regimes.
    Regime const regime = Plays(c, Tier::GOOD) ? c.regime : Regime::EVEN;

    // Core: a guard (or whoever the record names) restores the node assaulted next to it. The reserve and assault bots
    // that the record sends are good-rule responders.
    bool const mayRespond = c.job == Job::GUARD || Plays(c, Tier::GOOD);
    if (c.restoreResponder && c.restoreNode < NODE_COUNT && mayRespond)
        add(Kind::RESTORE, c.restoreNode, SCORE_RESTORE);

    switch (c.job)
    {
        case Job::GUARD:
        {
            if (c.slotNode >= NODE_COUNT)
            {
                add(Kind::STAGE, c.stageNode, SCORE_STAGE);
                break;
            }
            if (c.sit[c.slotNode] == NodeSituation::CAPPING && Plays(c, Tier::GOOD))
                add(Kind::HOLD_CAPTURE, c.slotNode, SCORE_HOLD_CAPTURE);
            // Casual guards drift: the loose post is inside the wider leash.
            if (Plays(c, Tier::GOOD))
                add(Kind::HOLD_POST, c.slotNode, SCORE_HOLD_POST);
            else
                add(Kind::HOLD_LOOSE, c.slotNode, SCORE_HOLD_LOOSE + 10);
            break;
        }
        case Job::ASSAULT:
        case Job::MOBILE:
        {
            if (c.incResponder && c.incNode < NODE_COUNT && Plays(c, Tier::GOOD))
                add(Kind::REINFORCE, c.incNode, SCORE_REINFORCE);
            if (c.supportHuman && c.supportNode < NODE_COUNT && Plays(c, Tier::GOOD))
                add(Kind::SUPPORT_HUMAN, c.supportNode, SCORE_SUPPORT_HUMAN);
            if (c.retreatNode < c.map->nodeCount && Plays(c, Tier::GOOD))
                add(Kind::RETREAT, c.retreatNode, SCORE_RETREAT);
            if (c.ghostNode < c.map->nodeCount && Plays(c, Tier::EXPERT))
                add(Kind::GHOST, c.ghostNode, SCORE_GHOST);

            bool const closeOut = (regime == Regime::CLOSE_OUT || regime == Regime::PANIC) && c.closeOutNode < c.map->nodeCount;
            if (closeOut && Plays(c, c.map->closeOutTier))
                add(Kind::CLOSE_OUT, c.closeOutNode, SCORE_CLOSE_OUT);

            // The node the group is after: the opening group's own node, else the team's choice (casual: the nearest).
            bool const openingGroup = c.job == Job::ASSAULT && c.slotNode < NODE_COUNT;
            std::uint8_t target = openingGroup ? c.slotNode : Plays(c, Tier::GOOD) ? c.assaultNode : c.nearestNode;
            if (target == NODE_NONE)
                target = c.nearestNode;

            if (c.job == Job::ASSAULT && target < NODE_COUNT)
            {
                if (c.sit[target] == NodeSituation::CAPPING && c.captureAgeMs[target] < StayAfterCastMs(*c.map) &&
                    Plays(c, Tier::GOOD))
                    add(Kind::HOLD_CAPTURE, target, SCORE_HOLD_CAPTURE);
                else if (openingGroup && !c.groupGathered && Plays(c, Tier::GOOD))
                    add(Kind::GATHER, target, SCORE_GATHER);
                else if (c.sit[target] != NodeSituation::CAPPING)
                    add(Kind::TAKE, target, SCORE_TAKE);
            }
            else if (c.job == Job::MOBILE && target < NODE_COUNT && Plays(c, Tier::GOOD) &&
                     (regime == Regime::PUSH || regime == Regime::PANIC || regime == Regime::CLOSE_OUT ||
                      regime == Regime::LAST_NODE) &&
                     c.sit[target] != NodeSituation::CAPPING)
                add(Kind::TAKE, target, SCORE_TAKE_MOBILE);

            add(Kind::STAGE, c.stageNode, SCORE_STAGE);
            break;
        }
        case Job::RAIDER:
        {
            if (c.ghostNode < c.map->nodeCount && Plays(c, Tier::EXPERT))
                add(Kind::GHOST, c.ghostNode, SCORE_GHOST);
            if (c.raidStay && c.raidNode < NODE_COUNT && c.sit[c.raidNode] == NodeSituation::CAPPING && Plays(c, Tier::GOOD))
                add(Kind::HOLD_CAPTURE, c.raidNode, SCORE_HOLD_CAPTURE);
            if (c.raidNode < NODE_COUNT && c.sit[c.raidNode] != NodeSituation::CAPPING)
                add(Kind::RAID, c.raidNode, SCORE_RAID);
            add(Kind::STAGE, c.stageNode, SCORE_STAGE);
            break;
        }
    }

    add(Kind::HOLD_QUIET, NODE_NONE, SCORE_QUIET);

    // Highest score first; equal scores in the order of the Kind list, so the choice is deterministic.
    std::stable_sort(out.begin(), out.end(), [](Option const& a, Option const& b)
    {
        if (a.score != b.score)
            return a.score > b.score;
        return static_cast<std::uint8_t>(a.kind) < static_cast<std::uint8_t>(b.kind);
    });
    return out;
}

// The opening group is "gathered" when enough of its members stand at the gather point, or it waited long enough.
inline bool GroupGathered(std::uint32_t aliveMembers, std::uint32_t membersAtPoint, std::uint32_t waitedMs)
{
    if (aliveMembers == 0)
        return true;
    std::uint32_t const need = std::min(GATHER_MIN_MEMBERS, aliveMembers);
    return membersAtPoint >= need || waitedMs >= GATHER_WAIT_MS;
}

// ---------------------------------------------------------------------------------------------------------------------
// Call-outs (sheet section 8).
// ---------------------------------------------------------------------------------------------------------------------
enum class Call : std::uint8_t
{
    OPENING_GROUP,
    INCOMING,
    ASSAULTED,
    ON_MY_WAY,
    NODE_BACK,
    NODE_LOST,
    NEED_FOURTH,
    SCORE,
    ANSWER,
    STANCE,       // BT-481: the leader says what the team plays
    RETREAT,      // BT-552: the losing group falls back to the node named
    GHOST         // BT-552: the expert pair goes for the enemy's near node
};
constexpr std::uint32_t CALL_KIND_COUNT = 12;

// "bs assaulted" is only said if no "inc" went out in the last 10 s (sheet section 8).
constexpr std::uint32_t CALL_ASSAULT_AFTER_INC_MS = 10000;
constexpr std::uint32_t SCORE_CALL_FIRST = 1000;
// The leader's stance call (BT-481, the same number as BgLeader::STANCE_CALL_GAP_MS).
constexpr std::uint32_t CALL_STANCE_GAP_MS = 15000;

inline BgPlan::KindGap KindGapFor(Call kind)
{
    if (kind == Call::ASSAULTED)
        return { true, static_cast<std::uint8_t>(Call::INCOMING), CALL_ASSAULT_AFTER_INC_MS };
    if (kind == Call::STANCE)
        return { true, static_cast<std::uint8_t>(Call::STANCE), CALL_STANCE_GAP_MS };
    return BgPlan::KindGap();
}

struct CallLimiter : BgPlan::CallLimiter
{
    bool Allow(std::uint32_t guidLow, Call kind, std::uint32_t nowMs) const
    {
        return BgPlan::CallLimiter::Allow(guidLow, static_cast<std::uint8_t>(kind), nowMs, KindGapFor(kind));
    }

    void Commit(std::uint32_t guidLow, Call kind, std::uint32_t nowMs)
    {
        BgPlan::CallLimiter::Commit(guidLow, static_cast<std::uint8_t>(kind), nowMs);
    }
};

// What a human's call does to the assault group once it is read (BgCalloutPolicy.h): a claim ("take lm") or a lost node ("st
// lost") weighs on the targets of the good bots, an "inc" or "help" sends responders. Never an order to anybody.
enum class CallVerb : std::uint8_t
{
    INC,     // enemies are coming to the node (also "incoming", "help")
    TAKE,    // a friend takes the node ("take", "cap", "capture")
    LOST     // the node is lost
};

// How long a human's call steers the bots (the answer is valid this long).
constexpr std::uint32_t HUMAN_CALL_MS = 20000;
// A human who casts the capture within this distance of a banner is supported, and the two nearest non-guard bots go.
constexpr float HUMAN_SUPPORT_RADIUS = 40.0f;
constexpr std::uint32_t SUPPORTERS = 2;

}  // namespace AbLogic

#endif
