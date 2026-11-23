/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_BFGPLANLOGIC_H
#define _PLAYERBOT_BFGPLANLOGIC_H

// BT-550: the Battle for Gilneas playbook (docs/bg-sheets/battle-for-gilneas.md) as pure logic, free of game types so that
// tests/ can run it. This first part is what the old objective case of BgTacticsBFG.cpp needs without a team record: the
// map data, the three-node rate table, the near-node defender and the hold state at three nodes. The node plan (opening
// counts per size, regimes, retreat, ghost) builds on the same data.
//
// The generic node rules (the click rule, who occupies and contests a node) are Arathi Basin's (AbPlanLogic.h): the core's
// BattlegroundBFG::EventPlayerClickedOnFlag is the same code.

#include "AbPlanLogic.h"
#include "BgNodeMap.h"
#include "BgPlanCommon.h"
#include "BotSkillMath.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace BfgLogic
{
using Tier = BotSkillMath::Tier;
namespace Ab = AbLogic;

// ---------------------------------------------------------------------------------------------------------------------
// Numbers from the core (BattlegroundBFG.h / .cpp in the core) and the owner's decisions of 2026-11-18.
// ---------------------------------------------------------------------------------------------------------------------
// The map data is BgNodeMap::GILNEAS (the core's own numbers); the names below are what this file's rules read.
inline constexpr auto const& MAP = BgNodeMap::GILNEAS;
constexpr std::uint32_t MAP_ID = BgNodeMap::GILNEAS.mapId;
constexpr std::uint32_t MAX_SCORE = BgNodeMap::GILNEAS.maxScore;
inline constexpr auto const& TICK_INTERVAL_MS = BgNodeMap::GILNEAS.tickIntervalMs;   // index = nodes held
inline constexpr auto const& TICK_POINTS = BgNodeMap::GILNEAS.tickPoints;
constexpr std::uint8_t NODE_COUNT = BgNodeMap::GILNEAS.nodeCount;
constexpr std::uint8_t NODE_NONE = BgNodeMap::NODE_NONE;

enum Node : std::uint8_t
{
    NODE_LIGHTHOUSE = 0,
    NODE_WATERWORKS = 1,
    NODE_MINES = 2
};

inline constexpr auto const& NODE_POS = BgNodeMap::GILNEAS.pos;

// The start rooms: WorldSafeLocs 1740 (Alliance) and 1739 (Horde) are also the graveyards a team with no node comes out of.
inline constexpr auto const& START_ROOM = BgNodeMap::GILNEAS.start;
// A bot stays this far from the enemy's start graveyard unless passing (the dwell rule of the Warsong Gulch and Twin Peaks
// sheets, owner: no camping). The far node is 152 to 161 yd from it, so no post lies inside.
constexpr float START_ROOM_AVOID_YARDS = 45.0f;

inline char const* NodeWord(std::uint8_t node)
{
    return Ab::NodeWord(node, BgNodeMap::GILNEAS);
}

// The node next to a team's start room (161 yd for the Alliance, 152 yd for the Horde) and the node next to the enemy's.
constexpr std::uint8_t NearNode(std::uint8_t team)
{
    return BgNodeMap::GILNEAS.home[team & 1];
}

constexpr std::uint8_t FarNode(std::uint8_t team)
{
    return NearNode(Ab::OtherTeam(team));
}

inline float Dist2d(float ax, float ay, float bx, float by)
{
    return Ab::Dist2d(ax, ay, bx, by);
}

inline float DistToNode(std::uint8_t node, float x, float y)
{
    return Ab::DistToNode(node, x, y, BgNodeMap::GILNEAS);
}

// ---------------------------------------------------------------------------------------------------------------------
// The rate table (sheet section 3): 0.83, 1.67 and 30 points per second for one, two and three nodes.
// ---------------------------------------------------------------------------------------------------------------------
inline float PointsPerSecond(unsigned occupiedNodes)
{
    return Ab::PointsPerSecond(occupiedNodes, BgNodeMap::GILNEAS);
}

// The change in (our rate - their rate) of an action, from ours / theirs occupied nodes now.
inline float SwingPerSecond(unsigned ours, unsigned theirs, Ab::SwingKind kind)
{
    return Ab::SwingValue(ours, theirs, kind, BgNodeMap::GILNEAS);
}

// ---------------------------------------------------------------------------------------------------------------------
// The near-node defender (sheet section 4): the guide trusts three kits to hold a node alone.
// ---------------------------------------------------------------------------------------------------------------------
struct Member
{
    std::uint32_t guidLow = 0;
    std::uint8_t cls = 0;
    std::uint8_t level = 0;
    bool healer = false;
    bool tank = false;
    bool feral = false;
    Tier tier = Tier::GOOD;
};

// Class fit for the lone defender, 0 to 30: rogue, mage and hunter first; then kits with a self-heal or an escape; the rest;
// healers last.
inline int DefenderFit(Member const& m)
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

constexpr int DEFENDER_LEVEL_STEP = 2;     // a level counts less than the kit (the three kits are one class step apart)
constexpr int DEFENDER_TIER_GOOD = 4;      // a lone defender who drifts loses the node: good and expert hold better
constexpr int DEFENDER_TIER_EXPERT = 6;

inline int DefenderScore(Member const& m, std::uint8_t minLevel)
{
    int const tier = m.tier == Tier::EXPERT ? DEFENDER_TIER_EXPERT : m.tier == Tier::GOOD ? DEFENDER_TIER_GOOD : 0;
    return DefenderFit(m) + tier + (int(m.level) - int(minLevel)) * DEFENDER_LEVEL_STEP;
}

// The defenders of a team: the `count` best by fit, tier and level, ties by the GUID hash then the GUID. Deterministic, so
// every bot of the team computes the same list from the same roster.
inline std::vector<std::uint32_t> PickDefenders(std::vector<Member> const& team, std::size_t count)
{
    std::uint8_t minLevel = 255;
    for (Member const& m : team)
        minLevel = std::min(minLevel, m.level);

    std::vector<std::pair<int, Member const*>> scored;
    for (Member const& m : team)
        scored.emplace_back(DefenderScore(m, minLevel), &m);
    std::sort(scored.begin(), scored.end(), [](auto const& a, auto const& b)
    {
        if (a.first != b.first)
            return a.first > b.first;
        std::uint32_t const ha = BotSkillMath::Mix(a.second->guidLow);
        std::uint32_t const hb = BotSkillMath::Mix(b.second->guidLow);
        return ha != hb ? ha < hb : a.second->guidLow < b.second->guidLow;
    });

    std::vector<std::uint32_t> out;
    for (std::size_t i = 0; i < scored.size() && i < count; ++i)
        out.push_back(scored[i].second->guidLow);
    return out;
}

// How many defenders a plan puts at the near node: two in the defense plan, from 6 players on.
inline std::size_t DefenderCount(BgPlan::Plan plan, std::uint32_t teamSize)
{
    return plan == BgPlan::Plan::DEFENSE && teamSize >= 6 ? 2 : 1;
}

// ---------------------------------------------------------------------------------------------------------------------
// The hold state (sheet section 6): all three nodes held. Guards 2, 2, 3 (near, Waterworks, far), the rest staged at the far
// node's approach, the node next to the enemy's start room where their dead come out.
// ---------------------------------------------------------------------------------------------------------------------
constexpr std::uint32_t HOLD_GUARDS_NEAR = 2;
constexpr std::uint32_t HOLD_GUARDS_HUB = 2;
constexpr std::uint32_t HOLD_GUARDS_FAR = 3;

struct HoldPost
{
    std::uint8_t node = NODE_NONE;
    bool staged = false;   // a reserve bot: stands 12 to 20 yd from the banner, answers an assault
};

// The posts of a team of `size` bots, in the order they are filled: far, Waterworks, near, then the reserve, repeating the
// guard table when the team is large. At least one guard per node while there are bots enough.
inline std::vector<HoldPost> HoldPosts(std::uint8_t team, std::uint32_t size)
{
    std::uint8_t const nearNode = NearNode(team);
    std::uint8_t const farNode = FarNode(team);
    std::vector<BgPlan::GroupWish> const wishes = {
        { HOLD_GUARDS_FAR, 1 }, { HOLD_GUARDS_HUB, 1 }, { HOLD_GUARDS_NEAR, 1 }, { 3, 0 }
    };
    std::vector<std::uint32_t> const parts = BgPlan::Apportion(wishes, size);

    std::vector<HoldPost> out;
    std::uint8_t const nodes[3] = { farNode, NODE_WATERWORKS, nearNode };
    for (std::size_t g = 0; g < 3; ++g)
        for (std::uint32_t i = 0; i < parts[g]; ++i)
            out.push_back({ nodes[g], false });
    for (std::uint32_t i = 0; i < parts[3]; ++i)
        out.push_back({ farNode, true });
    return out;
}

// ---------------------------------------------------------------------------------------------------------------------
// The old objective case without a team record: one answer per bot from the node states.
// ---------------------------------------------------------------------------------------------------------------------
enum class What : std::uint8_t
{
    NOTHING,
    TAKE,    // go to the banner and click it (a neutral, enemy or enemy-contested node)
    POST     // stand at the node: defender, capping group, guard of the hold state
};

struct Choice
{
    What what = What::NOTHING;
    std::uint8_t node = NODE_NONE;
    bool staged = false;
};

struct Input
{
    std::uint8_t team = Ab::TEAM_IDX_ALLIANCE;
    std::uint8_t state[NODE_COUNT] = { 0, 0, 0 };   // BattlegroundBFG::GetNodeStatus
    bool defender = false;                          // the bot is one of the team's near-node defenders
    std::uint32_t rank = 0;                         // the bot's place among the team's non-defenders (hash order)
    std::uint32_t others = 0;                       // the team's non-defender bots
    float x = 0.0f;
    float y = 0.0f;
    std::uint8_t level = 80;
};

constexpr float WALK_SPEED = 7.0f;
constexpr float MOUNT_SPEED = 11.2f;
constexpr std::uint8_t MOUNT_LEVEL = 20;
constexpr float PATH_DETOUR = 1.4f;
constexpr float TARGET_ETA_SOFTENER_S = 20.0f;
constexpr float TARGET_VALUE_SCALE = 10000.0f;
// A bot within this distance of a node its team is capturing stays with the capping group (good rule of the sheet, applied
// to every bot of the old case: the capture is the 60 s that decide the match).
constexpr float CAPPING_STAY_YARDS = 45.0f;

inline unsigned Occupied(Input const& in, std::uint8_t team)
{
    unsigned n = 0;
    for (std::uint8_t node = 0; node < NODE_COUNT; ++node)
        if (Ab::IsOccupiedBy(in.state[node], team))
            ++n;
    return n;
}

inline float EtaSeconds(float yards, std::uint8_t level)
{
    float const speed = level >= MOUNT_LEVEL ? MOUNT_SPEED : WALK_SPEED;
    return yards * PATH_DETOUR / speed;
}

// What a bot of the old case does. Rules, in order:
//  1. the near-node defender clicks its node when it can (the opening, a restore), else stands at it;
//  2. a bot near a node its team is capturing stays with it (a node we are capturing counts as ours);
//  3. the best node to click by swing value per second of effort (the third node and the flip of the enemy's last node
//     weigh 28 to 29 points per second against 0.8 to 1.7 for the rest); the near node is the defender's while neutral;
//  4. nothing to click: the hold state, a post by rank (guards 2, 2, 3, the rest staged at the far node).
inline Choice Choose(Input const& in)
{
    std::uint8_t const team = in.team;
    std::uint8_t const nearNode = NearNode(team);
    Choice choice;

    if (in.defender)
    {
        choice.node = nearNode;
        choice.what = Ab::CanClickNode(in.state[nearNode], team) ? What::TAKE : What::POST;
        return choice;
    }

    for (std::uint8_t node = 0; node < NODE_COUNT; ++node)
    {
        if (Ab::IsContestedBy(in.state[node], team) && DistToNode(node, in.x, in.y) <= CAPPING_STAY_YARDS)
        {
            choice.what = What::POST;
            choice.node = node;
            return choice;
        }
    }

    unsigned const ours = Occupied(in, team);
    unsigned const theirs = Occupied(in, Ab::OtherTeam(team));
    int bestScore = 0;
    for (std::uint8_t node = 0; node < NODE_COUNT; ++node)
    {
        std::uint8_t const state = in.state[node];
        if (!Ab::CanClickNode(state, team))
            continue;
        if (node == nearNode && state == Ab::STATE_NEUTRAL)
            continue;   // the defender's node while nobody has taken it

        Ab::SwingKind const kind = Ab::IsOccupiedBy(state, Ab::OtherTeam(team)) ? Ab::SwingKind::TAKE_ENEMY : Ab::SwingKind::TAKE_FREE;
        float const eta = EtaSeconds(DistToNode(node, in.x, in.y), in.level);
        int const score = static_cast<int>(SwingPerSecond(ours, theirs, kind) * TARGET_VALUE_SCALE / (eta + TARGET_ETA_SOFTENER_S));
        if (choice.what == What::NOTHING || score > bestScore)
        {
            bestScore = score;
            choice.what = What::TAKE;
            choice.node = node;
        }
    }
    if (choice.what == What::TAKE)
        return choice;

    // The hold state. `rank` indexes the posts; the defenders are not in the count.
    std::vector<HoldPost> const posts = HoldPosts(team, in.others);
    if (posts.empty())
    {
        choice.what = What::POST;
        choice.node = NODE_WATERWORKS;
        return choice;
    }
    HoldPost const post = posts[in.rank % posts.size()];
    choice.what = What::POST;
    choice.node = post.node;
    choice.staged = post.staged;
    return choice;
}

// True when a point lies where an enemy team with no node comes out (its start graveyard): no post and no walk ends there.
inline bool NearEnemyStartRoom(std::uint8_t team, float x, float y)
{
    Ab::Pt const room = START_ROOM[Ab::OtherTeam(team) & 1];
    return Dist2d(x, y, room.x, room.y) <= START_ROOM_AVOID_YARDS;
}

}  // namespace BfgLogic

#endif
