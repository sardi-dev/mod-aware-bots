/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_BGFLAGMAP_H
#define _PLAYERBOT_BGFLAGMAP_H

// BT-470: the map adapter of the two-flag plan (docs/bg-sheets/twin-peaks.md gap G1). The plan of Warsong Gulch
// (WsgPlan.cpp) used to be written against one core class and one set of coordinates. Twin Peaks is the same game on
// other ground, with its own core class, so what differs is gathered here, free of game types so that tests/ can run it:
//
//  - MapData: the sides (flag stand, entrance posts, hide posts, own graveyard, the point where the own lane leaves the
//    base), the middle of the map and the match clock. One value per map, picked by map id (the Random Battleground
//    queue keeps its own type id, the map id is reliable).
//  - FlagView: the flag state both core classes fill (BattlegroundWS and BattlegroundTP carry the same four values per
//    team, in two unrelated classes). The game-bound fill lives in strategy/bg/BgFlagView.cpp.
//  - Clock: the match limit of the map, with the limit an older core still uses (see Clock below).
//
// Warsong Gulch is the reference: its data is moved here unchanged from WsgPlan.cpp, and its tests pin it.

#include <cstdint>

namespace BgFlagMap
{
struct Pos
{
    float x;
    float y;
    float z;
};

// One side of the map. Index of the sides array = TeamId (TEAM_ALLIANCE 0, TEAM_HORDE 1).
struct Side
{
    Pos standWalk;        // player height next to the flag (the old tactics' WS_FLAG_POS_*)
    Pos tunnelExit;       // where a bot of this team leaves its graveyard for the open ground (the graveyard hop)
    Pos mainGraveyard;    // the graveyard a dead player of this team respawns at in play
    Pos post[3];          // flag room entrances
    Pos hide[3];          // hide posts while the enemy holds our flag
    bool hideUsable[3];   // a post left out until the live audit is marked false
    Pos gather;           // where the ENEMY strike group gathers outside this base, on the lane it arrives by
};

// The match clock. The core ends the match when the instance clock reaches limitMs (the clock also runs in the
// 2 minute preparation). A core that still plays on to legacyLimitMs (Twin Peaks today ends at 30 minutes, the fork is
// to move it to 27, BT-461) is read live: while the match is still running past limitMs, the longer limit is the
// clock, so the plan is right before and after the core change. When the core ends the match at limitMs the longer
// limit is never reached.
struct Clock
{
    std::uint32_t limitMs;
    std::uint32_t legacyLimitMs;   // equal to limitMs when the core never changed
};

struct MapData
{
    std::uint32_t mapId;
    Side sides[2];
    Pos mid;              // the middle of the map: the call-out zone and the first hold post
    Clock clock;
    float roofAboveStand; // a point this far above a flag stand and near it is "the roof" in a call-out; 0 = no roof zone
};

// Time left in the match in milliseconds; negative past the limit (as the Warsong plan always had it).
inline std::int32_t LeftMs(std::uint32_t elapsedMs, Clock const& clock)
{
    std::uint32_t limit = clock.limitMs;
    if (clock.legacyLimitMs > limit && elapsedMs >= limit)
        limit = clock.legacyLimitMs;
    return static_cast<std::int32_t>(limit) - static_cast<std::int32_t>(elapsedMs);
}

// The flag state of one match: index = TeamId of the team whose flag it is. The state numbers are the core's
// BG_WS_FlagState and BG_TP_FlagState (the same four values): 0 on its stand, 1 waiting to respawn after a capture,
// 2 carried, 3 on the ground. picker = guid of the player carrying that team's flag, dropped = guid of the dropped
// flag object, 0 for none.
struct FlagView
{
    std::uint8_t state[2] = { 0, 0 };
    std::uint64_t picker[2] = { 0, 0 };
    std::uint64_t dropped[2] = { 0, 0 };
};

// ---------------------------------------------------------------------------------------------------------------------
// Warsong Gulch (map 489). Everything here is the server's own data: the flag stands from
// BattlegroundWS::SetupBattleground (BattlegroundWS.cpp:658-659), graveyards from WorldSafeLocs 771 / 772, the tunnel
// exits are the tunnel-side ends of the authored paths (vPath_WSG_*Tunnel_*, BattleGroundTactics.cpp), the entrance posts
// are the waiting spots inside each flag room (WS_WAITING_POS_*).
// ---------------------------------------------------------------------------------------------------------------------
constexpr std::uint32_t WSG_MAP_ID = 489;
// Eye of the Storm (BT-583): one flag in the middle of the map, read through the same view.
constexpr std::uint32_t EYE_MAP_ID = 566;
// The Warsong script ends the match when the instance clock reaches 27 minutes (BattlegroundWS.cpp:52).
constexpr std::uint32_t WSG_LIMIT_MS = 27u * 60u * 1000u;
// The roof of a Warsong flag room lies more than this above the stand.
constexpr float WSG_ROOF_ABOVE_STAND = 12.0f;

inline MapData const& Warsong()
{
    static MapData const data = {
        WSG_MAP_ID,
        {
            {
                { 1539.219f, 1481.747f, 352.458f },
                { 1348.020f, 1461.060f, 323.167f },
                { 1406.650f, 1564.740f, 328.330f },
                { { 1496.578f, 1457.900f, 344.442f }, { 1510.502f, 1493.385f, 351.995f }, { 1521.235f, 1480.951f, 352.007f } },
                { { 1529.249f, 1456.470f, 353.040f }, { 1540.286f, 1476.026f, 352.692f }, { 1495.807f, 1466.774f, 352.350f } },
                { true, true, true },
                { 1348.020f, 1461.060f, 323.167f },
            },
            {
                { 915.958f, 1433.925f, 346.193f },
                { 1124.370f, 1462.280f, 315.853f },
                { 1061.600f, 1377.090f, 328.560f },
                { { 948.488f, 1459.834f, 343.066f }, { 944.981f, 1423.478f, 345.434f }, { 933.484f, 1433.726f, 345.535f } },
                { { 926.142f, 1460.641f, 346.116f }, { 925.166f, 1458.084f, 355.966f }, { 924.922f, 1423.672f, 345.524f } },
                { true, false, true },
                { 1124.370f, 1462.280f, 315.853f },
            }
        },
        { 1227.446f, 1476.235f, 307.484f },
        { WSG_LIMIT_MS, WSG_LIMIT_MS },
        WSG_ROOF_ABOVE_STAND,
    };
    return data;
}
}  // namespace BgFlagMap

#endif
