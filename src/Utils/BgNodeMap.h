/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_BGNODEMAP_H
#define _PLAYERBOT_BGNODEMAP_H

// BT-551: the node maps as data. One table row per battleground that is won by holding nodes: Arathi Basin and Battle for
// Gilneas and Eye of the Storm. The node plan (AbPlanLogic.h rules, AbPlan.cpp record and glue) reads everything that
// differs between the maps from here: how many nodes, the score limit and the tick table, the positions, the words of the
// call-outs, the near node of each team, the hub and the thresholds of the regimes. The numbers are the core's own
// (BattlegroundAB.h, BattlegroundBFG.h in the core) and the owner's decisions; sheets: docs/bg-sheets/arathi-basin.md and
// docs/bg-sheets/battle-for-gilneas.md.
//
// Free of game types, so tests/ can run it.

#include "BotSkillMath.h"

#include <cstdint>

namespace BgNodeMap
{
struct Pt
{
    float x;
    float y;
    float z;
};

// The most nodes a map has (Arathi Basin): the size of every per-node array of the plan.
constexpr std::uint8_t MAX_NODES = 5;
constexpr std::uint8_t NODE_NONE = 0xFF;

enum class Kind : std::uint8_t
{
    ARATHI,
    GILNEAS,
    EYE
};

// How a node is taken. CLICK: a banner is clicked after an 8 s cast and then contested for 60 s (Arathi Basin, Battle for
// Gilneas). PRESENCE: a progress bar moves by the net number of players inside a circle round the node every 2 s, there is
// no cast and no contest timer (Eye of the Storm, BT-582).
enum class CaptureModel : std::uint8_t
{
    CLICK,
    PRESENCE
};

struct MapData
{
    Kind kind;
    std::uint32_t mapId;
    char const* tag;            // the log tag: [BGAB], [BGBFG]
    char const* voicePrefix;    // the rows of the text table: bg_ab_, bg_bfg_
    char const* mapName;        // what the decision layer is told
    std::uint8_t nodeCount;
    std::uint32_t maxScore;
    // Tick interval and points per tick by occupied nodes (index 0 to nodeCount).
    std::uint32_t tickIntervalMs[MAX_NODES + 1];
    std::uint32_t tickPoints[MAX_NODES + 1];
    Pt pos[MAX_NODES];
    char const* word[MAX_NODES];     // the call-out word of a node ("bs", "ww")
    char const* name[MAX_NODES];     // the decision layer's word ("blacksmith", "waterworks")
    std::uint8_t home[2];            // the near node of each team (Alliance, Horde)
    std::uint8_t hub;                // the node with the shortest walks to the others, where the reserve stages
    Pt start[2];                     // the start rooms (waiting spots)
    bool exitPortals;                // the script has exit portals a bot must keep clear of (Arathi Basin)
    // Regime thresholds (AbLogic::ChooseRegime).
    std::uint8_t closeOutNodes;      // nodes held at which the one left ends the match: 4 of 5, 2 of 3
    bool holdAtAllNodes;             // HOLD means all nodes held (Gilneas); else ahead on the clock with holdMinNodes or more
    std::uint8_t holdMinNodes;
    std::uint8_t lastNodeOurMin;     // LAST-NODE: we hold this many nodes or fewer, at least lastNodeOurMin ...
    std::uint8_t lastNodeOurMax;
    std::uint8_t lastNodeEnemyMin;   // ... and the enemy holds this many or more
    BotSkillMath::Tier closeOutTier; // who plays the close-out option (Arathi: expert; Gilneas: good, the sheet's third-node urgency)
    // BT-582 (Eye of the Storm): the presence model and what only that map has.
    CaptureModel captureModel;
    std::uint8_t nearNodes[2][2];    // the two nodes nearest each team's start room (maps with one near node repeat it)
    Pt centre;                       // a point of interest that is not a node: the middle of the map (the flag stand)
    float circleRadius;              // presence maps: players within this 3D distance of cap[n] count at node n (0 for click maps)
    Pt cap[MAX_NODES];               // the centres of those circles
    std::uint32_t flagPoints[MAX_NODES + 1];   // points of a flag capture by nodes held (0 for maps with no flag)
    std::uint8_t holdAtNodes;        // hold state from this many nodes held whatever the clock says (0: use holdAtAllNodes / the clock)
};

// ---------------------------------------------------------------------------------------------------------------------
// Arathi Basin (map 529): BattlegroundAB.h:117-121, :175-182, :192-193.
// ---------------------------------------------------------------------------------------------------------------------
inline constexpr MapData ARATHI = {
    Kind::ARATHI, 529, "BGAB", "bg_ab_", "ab", 5, 1600,
    { 0, 12000, 9000, 6000, 3000, 1000 },
    { 0, 10, 10, 10, 10, 30 },
    { { 1166.785f, 1200.132f, -56.70859f },   // stables
      { 977.0156f, 1046.616f, -44.80923f },   // blacksmith
      { 806.1821f, 874.2723f, -55.99371f },   // farm
      { 856.1419f, 1148.902f, 11.18469f },    // lumber mill (plateau)
      { 1146.923f, 848.1782f, -110.917f } },  // gold mine (pit)
    { "st", "bs", "farm", "lm", "gm" },
    { "stables", "blacksmith", "farm", "lumber_mill", "gold_mine" },
    { 0, 2 },        // Stables for the Alliance, Farm for the Horde
    1,               // the Blacksmith
    { { 1313.90f, 1310.74f, 0.0f }, { 684.01f, 681.22f, 0.0f } },
    true,
    4, false, 3, 1, 1, 3,
    BotSkillMath::Tier::EXPERT,
    CaptureModel::CLICK, { { 0, 0 }, { 2, 2 } }, { 0.0f, 0.0f, 0.0f }, 0.0f,
    { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } },
    { 0, 0, 0, 0, 0, 0 }, 0
};

// ---------------------------------------------------------------------------------------------------------------------
// Battle for Gilneas (map 761): BattlegroundBFG.h:100-104, :154-159, :170-171. Owner decision 2026-11-18: the core's
// rates and 2000 to win stay.
// ---------------------------------------------------------------------------------------------------------------------
inline constexpr MapData GILNEAS = {
    Kind::GILNEAS, 761, "BGBFG", "bg_bfg_", "bfg", 3, 2000,
    { 0, 12000, 6000, 1000, 0, 0 },
    { 0, 10, 10, 30, 0, 0 },
    { { 1057.790f, 1278.285f, 3.1500f },      // Lighthouse
      { 980.0446f, 948.7411f, 12.650f },      // Waterworks
      { 1251.010f, 958.2685f, 5.6000f },      // Mines
      { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } },
    { "lh", "ww", "mines", "?", "?" },
    { "lighthouse", "waterworks", "mines", "unknown", "unknown" },
    { 0, 2 },        // Lighthouse for the Alliance, Mines for the Horde
    1,               // Waterworks
    { { 908.274f, 1338.60f, 27.6449f }, { 1401.38f, 977.125f, 7.44215f } },
    false,
    2, true, 3, 0, 1, 2,
    BotSkillMath::Tier::GOOD,
    CaptureModel::CLICK, { { 0, 0 }, { 2, 2 } }, { 0.0f, 0.0f, 0.0f }, 0.0f,
    { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } },
    { 0, 0, 0, 0, 0, 0 }, 0
};

// ---------------------------------------------------------------------------------------------------------------------
// Eye of the Storm (map 566): BattlegroundEY.h (BG_EY_TickPoints {1, 2, 5, 10} every 2 s, BG_EY_FlagPoints {75, 85, 100, 500},
// BG_EY_MAX_TEAM_SCORE 1600, BG_EY_POINT_RADIUS 70, the point order of EYBattlegroundPoints). Owner ruling 2026-11-18: the core's
// values stay. Positions are the flag capture triggers (AreaTrigger.dbc), the circles' centres the tower cap objects.
// ---------------------------------------------------------------------------------------------------------------------
inline constexpr MapData EYE = {
    Kind::EYE, 566, "BGEY", "bg_ey_", "eots", 4, 1600,
    { 0, 2000, 2000, 2000, 2000, 0 },
    { 0, 1, 2, 5, 10, 0 },
    { { 2044.00f, 1729.73f, 1190.03f },       // Fel Reaver Ruins
      { 2048.48f, 1393.60f, 1194.54f },       // Blood Elf Tower
      { 2286.56f, 1402.36f, 1197.29f },       // Draenei Ruins
      { 2284.78f, 1731.32f, 1189.64f },       // Mage Tower
      { 0.0f, 0.0f, 0.0f } },
    { "fr", "be", "dr", "mt", "?" },
    { "fel_reaver", "blood_elf", "draenei_ruins", "mage_tower", "unknown" },
    { 3, 0 },        // Mage Tower for the Alliance, Fel Reaver for the Horde (the first of each team's two near nodes)
    3,               // no hub: the node nearest the middle, where the reserve stages by default
    { { 2523.76f, 1596.13f, 1247.70f }, { 1803.02f, 1539.97f, 1249.19f } },
    false,
    3, false, 3, 0, 1, 3,
    BotSkillMath::Tier::GOOD,
    CaptureModel::PRESENCE, { { 3, 2 }, { 0, 1 } }, { 2174.78f, 1569.05f, 1160.36f }, 70.0f,
    { { 2024.60f, 1742.82f, 1195.16f }, { 2050.49f, 1372.24f, 1194.56f }, { 2301.01f, 1386.93f, 1197.18f },
      { 2282.12f, 1760.01f, 1189.71f }, { 0.0f, 0.0f, 0.0f } },
    { 0, 75, 85, 100, 500, 0 }, 3
};

inline MapData const* ByMapId(std::uint32_t mapId)
{
    if (mapId == ARATHI.mapId)
        return &ARATHI;
    if (mapId == GILNEAS.mapId)
        return &GILNEAS;
    if (mapId == EYE.mapId)
        return &EYE;
    return nullptr;
}
}  // namespace BgNodeMap

#endif
