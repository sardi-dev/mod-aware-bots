/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_SMPLANLOGIC_H
#define _PLAYERBOT_SMPLANLOGIC_H

// BT-610: the pure rules of the Silvershard Mines plan (docs/bg-sheets/silvershard-mines.md, owner decisions taken overnight
// 2026-11-19). The map is a payload map: three mine carts ride rails to four depots, every player within 22 yd of a cart moves
// its bar one step a second towards his team, the side of 50 the bar is on keeps the cart and scores a point a second, and a cart
// that docks pays 150 to its keeper. This file holds what the plan decides from that, free of game types so that tests/ can run it
// (tests/test_sm_plan.cpp):
//
//  - the rails (the core's waypoint data) and what they give: the seconds a cart still rides, the point ahead of it, whether it is
//    still before its junction, where a walker meets it;
//  - the model of the bar (the core keeps it private) and the seconds a net of bodies needs to carry it across 50;
//  - the options per cart as a scored list (decision point D18): take, flip, guard, skip;
//  - how many bots hold the main cart and how many tag the off carts, and who goes where (stickiness, class fit);
//  - the track switches: the state each team wants, the window, who clicks and when (decision point D19);
//  - the stance of the leader from the race, and the leash of a bot in a fight.
//
// Every score is a whole number and every list is best first with ties to the lower id, so a decision layer may later answer with
// another id of the same list while the rule stays the fallback.

#include "BgLeaderPolicy.h"
#include "BgPlanCommon.h"
#include "BotSkillMath.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace SmLogic
{
using Tier = BotSkillMath::Tier;

constexpr std::uint32_t MAP_ID = 727;
constexpr std::uint8_t CART_COUNT = 3;
constexpr std::uint8_t CART_NONE = 0xFF;
constexpr std::uint8_t TEAM_IDX_ALLIANCE = 0;
constexpr std::uint8_t TEAM_IDX_HORDE = 1;
constexpr std::uint8_t TEAM_NONE = 2;

// The carts as the core numbers them (BG_SM_MINE_CART_1 to 3) and the names the players use for them: the east cart is "lava", the
// waterfall cart "mid", the north cart "top" (sheet section 8), whatever depot the switch sends it to.
enum Cart : std::uint8_t
{
    CART_EAST = 0,
    CART_WATERFALL = 1,
    CART_NORTH = 2
};

enum Depot : std::uint8_t
{
    DEPOT_WATERFALL = 0,
    DEPOT_LAVA = 1,
    DEPOT_DIAMOND = 2,
    DEPOT_TROLL = 3,
    DEPOT_COUNT = 4
};

// Two track switches: the east one steers the east cart, the north one the north cart (BG_SM_TrackPos).
enum Track : std::uint8_t
{
    TRACK_EAST = 0,
    TRACK_NORTH = 1,
    TRACK_COUNT = 2,
    TRACK_NONE = 0xFF
};

// BattlegroundSM.h:165-169, :138-139, BG_SM_PROGRESS_BAR_*.
constexpr float CONTROL_RADIUS = 22.0f;
constexpr int DOCK_POINTS = 150;
constexpr std::uint32_t WIN_SCORE = 1600;
constexpr float CART_SPEED = 2.0f;             // yd/s: 0.8 of the 2.5 base walk speed (BattlegroundSM.cpp:235); to check live
constexpr float DOCK_CLOSE_S = 3.0f;
constexpr int BAR_START = 50;
constexpr int BAR_MAX = 100;
constexpr float JUNCTION_RADIUS = 0.5f;        // the core picks the path when the cart is this close to the junction
constexpr float SWITCH_COOLDOWN_S = 3.0f;
constexpr float SWITCH_CAST_S = 3.0f;          // the guide's cast; spell 124492's own time is to check live
constexpr float FOOT_SPEED = 7.0f;
constexpr float MOUNT_SPEED = 11.2f;
constexpr float PATH_FACTOR = 1.3f;            // the mine's ramps: the straight line times this is the walk

struct Pt
{
    float x;
    float y;
    float z;
};

inline float Dist2d(float ax, float ay, float bx, float by)
{
    float const dx = ax - bx;
    float const dy = ay - by;
    return std::sqrt(dx * dx + dy * dy);
}

inline float Dist2d(Pt const& a, Pt const& b)
{
    return Dist2d(a.x, a.y, b.x, b.y);
}

inline float Dist3d(Pt const& a, Pt const& b)
{
    float const dz = a.z - b.z;
    float const d2 = Dist2d(a, b);
    return std::sqrt(d2 * d2 + dz * dz);
}

// ---------------------------------------------------------------------------------------------------------------------
// The rails: the core's waypoint_data (sql 2026_11_18_world_silvershard_mines.sql), the start slots of the carts and the depots
// (BattlegroundSM.h), the switches and the junctions.
// ---------------------------------------------------------------------------------------------------------------------
inline constexpr Pt CART_START[CART_COUNT] = {
    { 744.542f, 183.546f, 319.658f }, { 739.400f, 203.599f, 319.603f }, { 760.185f, 198.845f, 319.447f }
};
inline constexpr Pt DEPOT_POS[DEPOT_COUNT] = {
    { 566.951f, 337.058f, 347.295f }, { 619.470f, 79.720f, 299.068f }, { 895.974f, 27.211f, 364.391f }, { 778.445f, 500.950f, 359.739f }
};
inline constexpr Pt SWITCH_POS[TRACK_COUNT] = { { 715.585f, 101.272f, 319.995f }, { 847.482f, 308.033f, 346.573f } };
// The nearest vertex of the mesh to each switch NPC, 1.7 and 1.9 yd away: the bot walks here and clicks (sheet section 18).
inline constexpr Pt SWITCH_STAND[TRACK_COUNT] = { { 715.5f, 102.9f, 320.5f }, { 846.7f, 306.4f, 347.1f } };
inline constexpr Pt JUNCTION_POS[TRACK_COUNT] = { { 717.169f, 114.258f, 320.800f }, { 834.727f, 299.809f, 346.800f } };
// The team starts (the graveyards are on the platforms: AllianceStartLoc 4062, HordeStartLoc 4061).
inline constexpr Pt TEAM_START[2] = { { 846.0f, 144.0f, 329.0f }, { 633.0f, 224.0f, 329.0f } };

// waypoint_data 7270011, 8 points
inline constexpr Pt EAST_FIRST_LEG[8] = {
    { 739.813f, 171.528f, 319.366f }, { 734.332f, 160.725f, 319.002f }, { 729.062f, 152.014f, 319.669f },
    { 725.965f, 146.377f, 319.708f }, { 721.922f, 137.435f, 319.589f }, { 718.783f, 127.548f, 319.538f },
    { 717.631f, 121.792f, 320.194f }, { 717.228f, 114.484f, 320.805f }
};
// waypoint_data 7270012, 18 points
inline constexpr Pt EAST_TO_LAVA[18] = {
    { 715.531f, 108.876f, 320.430f }, { 713.416f, 104.558f, 320.069f }, { 711.746f, 102.690f, 319.640f },
    { 709.642f, 100.842f, 319.294f }, { 708.178f, 100.004f, 319.000f }, { 705.397f, 98.714f, 318.396f },
    { 702.835f, 97.602f, 317.759f }, { 697.029f, 96.584f, 316.458f }, { 683.955f, 95.027f, 313.616f },
    { 680.408f, 94.210f, 312.532f }, { 675.415f, 92.476f, 310.801f }, { 672.125f, 91.021f, 309.622f },
    { 661.984f, 85.079f, 305.739f }, { 653.964f, 82.357f, 303.486f }, { 645.297f, 81.513f, 300.888f },
    { 635.492f, 80.961f, 298.712f }, { 627.331f, 80.279f, 298.515f }, { 624.142f, 79.938f, 298.447f }
};
// waypoint_data 7270013, 35 points
inline constexpr Pt EAST_TO_DIAMOND[35] = {
    { 716.947f, 109.533f, 320.581f }, { 717.158f, 107.275f, 320.577f }, { 717.906f, 104.245f, 320.656f },
    { 719.950f, 101.224f, 320.880f }, { 723.498f, 98.420f, 321.518f }, { 729.429f, 95.950f, 322.576f },
    { 737.529f, 93.723f, 325.273f }, { 748.309f, 91.896f, 328.890f }, { 757.356f, 89.951f, 331.927f },
    { 767.714f, 85.709f, 336.007f }, { 773.101f, 82.823f, 338.373f }, { 779.201f, 80.431f, 340.968f },
    { 790.073f, 78.923f, 344.601f }, { 794.567f, 77.764f, 346.229f }, { 800.644f, 76.028f, 348.893f },
    { 803.136f, 74.812f, 349.561f }, { 806.982f, 72.414f, 350.599f }, { 809.964f, 70.149f, 351.493f },
    { 815.473f, 66.193f, 353.279f }, { 820.528f, 63.702f, 354.622f }, { 824.337f, 62.778f, 355.605f },
    { 828.828f, 62.315f, 356.733f }, { 833.417f, 64.028f, 357.726f }, { 837.725f, 67.406f, 358.628f },
    { 842.537f, 70.267f, 359.654f }, { 846.829f, 71.599f, 360.651f }, { 851.902f, 71.391f, 361.480f },
    { 856.336f, 70.142f, 362.187f }, { 863.724f, 66.298f, 363.305f }, { 872.970f, 58.401f, 364.351f },
    { 878.484f, 52.676f, 364.342f }, { 884.031f, 45.295f, 363.741f }, { 888.753f, 38.639f, 363.604f },
    { 893.231f, 33.042f, 363.857f }, { 894.109f, 31.898f, 363.903f }
};
// waypoint_data 7270021, 22 points
inline constexpr Pt WATERFALL_LEG[22] = {
    { 736.548f, 206.805f, 319.452f }, { 732.146f, 214.183f, 320.143f }, { 729.917f, 219.983f, 320.318f },
    { 729.000f, 226.652f, 320.614f }, { 724.547f, 238.792f, 320.744f }, { 720.361f, 244.472f, 321.131f },
    { 715.631f, 251.413f, 321.044f }, { 710.624f, 261.126f, 320.713f }, { 708.395f, 267.442f, 320.558f },
    { 703.663f, 277.893f, 320.466f }, { 696.197f, 290.645f, 320.586f }, { 687.134f, 300.560f, 320.928f },
    { 675.485f, 310.659f, 321.804f }, { 661.001f, 323.359f, 324.457f }, { 650.727f, 331.558f, 327.443f },
    { 636.847f, 343.759f, 333.088f }, { 631.819f, 345.491f, 335.220f }, { 626.499f, 345.424f, 337.202f },
    { 601.887f, 340.112f, 344.394f }, { 592.689f, 339.309f, 345.928f }, { 580.856f, 337.289f, 346.313f },
    { 571.850f, 336.916f, 346.569f }
};
// waypoint_data 7270031, 17 points
inline constexpr Pt NORTH_FIRST_LEG[17] = {
    { 762.477f, 199.890f, 319.583f }, { 765.152f, 202.359f, 319.897f }, { 767.123f, 204.728f, 319.969f },
    { 770.686f, 210.459f, 321.337f }, { 773.192f, 215.373f, 322.098f }, { 776.042f, 221.111f, 322.779f },
    { 778.197f, 226.702f, 323.663f }, { 780.860f, 235.445f, 326.113f }, { 785.627f, 246.242f, 329.131f },
    { 789.242f, 252.730f, 332.243f }, { 794.445f, 259.695f, 336.467f }, { 800.921f, 267.035f, 339.987f },
    { 807.956f, 274.195f, 342.559f }, { 814.702f, 280.892f, 344.462f }, { 821.345f, 287.324f, 345.470f },
    { 828.885f, 293.503f, 346.333f }, { 834.802f, 299.641f, 346.814f }
};
// waypoint_data 7270032, 31 points
inline constexpr Pt NORTH_TO_TROLL[31] = {
    { 838.299f, 303.651f, 346.869f }, { 841.428f, 308.210f, 346.912f }, { 844.756f, 314.221f, 347.015f },
    { 847.569f, 320.454f, 347.146f }, { 848.459f, 325.685f, 346.852f }, { 848.214f, 330.158f, 347.114f },
    { 846.538f, 340.906f, 347.172f }, { 845.114f, 346.511f, 347.103f }, { 837.194f, 362.910f, 346.901f },
    { 833.275f, 369.036f, 346.856f }, { 830.817f, 374.828f, 346.754f }, { 826.987f, 384.754f, 346.599f },
    { 821.844f, 399.795f, 346.437f }, { 818.729f, 406.383f, 347.448f }, { 816.959f, 413.134f, 348.709f },
    { 816.845f, 419.167f, 349.887f }, { 816.607f, 426.345f, 352.090f }, { 814.784f, 432.833f, 354.160f },
    { 812.513f, 438.346f, 355.590f }, { 810.702f, 442.726f, 356.727f }, { 809.238f, 448.982f, 357.445f },
    { 807.673f, 456.378f, 358.182f }, { 806.689f, 465.029f, 358.637f }, { 804.531f, 471.067f, 358.976f },
    { 801.525f, 476.126f, 359.208f }, { 798.482f, 480.118f, 359.192f }, { 794.889f, 484.412f, 359.333f },
    { 789.291f, 489.534f, 359.266f }, { 785.138f, 493.456f, 359.315f }, { 783.313f, 495.286f, 359.321f },
    { 781.314f, 497.576f, 359.328f }
};
// waypoint_data 7270033, 61 points
inline constexpr Pt NORTH_TO_DIAMOND[61] = {
    { 837.828f, 302.350f, 346.860f }, { 841.867f, 303.567f, 346.890f }, { 845.365f, 303.699f, 346.920f },
    { 851.728f, 303.484f, 346.967f }, { 856.458f, 302.576f, 347.015f }, { 862.241f, 300.385f, 347.063f },
    { 867.288f, 297.620f, 347.007f }, { 872.097f, 293.145f, 347.096f }, { 876.515f, 288.124f, 347.068f },
    { 881.695f, 279.532f, 346.770f }, { 883.794f, 274.745f, 346.361f }, { 886.592f, 269.298f, 346.023f },
    { 888.886f, 262.982f, 345.738f }, { 890.814f, 258.054f, 345.620f }, { 892.383f, 252.914f, 345.493f },
    { 893.526f, 247.444f, 345.519f }, { 894.840f, 241.692f, 345.737f }, { 895.343f, 236.129f, 346.965f },
    { 896.554f, 230.642f, 348.410f }, { 898.720f, 224.644f, 350.104f }, { 900.815f, 219.293f, 351.663f },
    { 903.171f, 213.808f, 353.278f }, { 905.427f, 207.215f, 355.232f }, { 906.886f, 201.117f, 357.014f },
    { 908.490f, 194.291f, 359.015f }, { 910.714f, 182.699f, 362.340f }, { 912.146f, 174.145f, 364.930f },
    { 912.509f, 170.198f, 366.133f }, { 911.992f, 164.993f, 366.294f }, { 911.112f, 160.172f, 366.373f },
    { 909.908f, 155.990f, 366.418f }, { 908.492f, 152.966f, 366.436f }, { 906.588f, 149.528f, 366.431f },
    { 903.286f, 144.345f, 366.412f }, { 900.942f, 140.511f, 366.323f }, { 896.853f, 136.453f, 366.272f },
    { 894.221f, 133.493f, 366.239f }, { 891.690f, 130.435f, 366.216f }, { 888.906f, 126.811f, 366.131f },
    { 887.541f, 124.667f, 366.072f }, { 884.967f, 119.464f, 365.879f }, { 883.253f, 112.678f, 365.593f },
    { 882.509f, 107.984f, 365.393f }, { 882.227f, 104.390f, 365.255f }, { 883.121f, 98.898f, 365.043f },
    { 885.631f, 93.415f, 364.883f }, { 887.995f, 90.279f, 364.806f }, { 889.728f, 87.079f, 364.732f },
    { 891.275f, 81.975f, 364.601f }, { 891.068f, 76.771f, 364.459f }, { 890.494f, 72.639f, 364.551f },
    { 890.062f, 70.263f, 364.539f }, { 890.258f, 62.769f, 364.479f }, { 891.335f, 58.019f, 364.319f },
    { 892.202f, 54.341f, 364.058f }, { 893.220f, 51.170f, 363.776f }, { 893.571f, 44.880f, 363.724f },
    { 893.296f, 40.739f, 363.680f }, { 893.186f, 36.352f, 363.765f }, { 893.328f, 33.141f, 363.858f },
    { 894.002f, 31.853f, 363.900f }
};

struct RailView
{
    Pt const* pts = nullptr;
    std::size_t count = 0;
};

inline RailView FirstLeg(std::uint8_t cart)
{
    switch (cart)
    {
        case CART_EAST: return { EAST_FIRST_LEG, std::size(EAST_FIRST_LEG) };
        case CART_WATERFALL: return { WATERFALL_LEG, std::size(WATERFALL_LEG) };
        case CART_NORTH: return { NORTH_FIRST_LEG, std::size(NORTH_FIRST_LEG) };
        default: break;
    }
    return {};
}

// The second leg of a track after the junction, by the state of its switch: `opened` is the aura 120228 (east: Lava, north:
// Diamond), `closed` the default (east: Diamond, north: Troll). The waterfall cart has none.
inline RailView SecondLeg(std::uint8_t cart, bool opened)
{
    if (cart == CART_EAST)
        return opened ? RailView{ EAST_TO_LAVA, std::size(EAST_TO_LAVA) } : RailView{ EAST_TO_DIAMOND, std::size(EAST_TO_DIAMOND) };
    if (cart == CART_NORTH)
        return opened ? RailView{ NORTH_TO_DIAMOND, std::size(NORTH_TO_DIAMOND) } : RailView{ NORTH_TO_TROLL, std::size(NORTH_TO_TROLL) };
    return {};
}

inline std::uint8_t TrackOf(std::uint8_t cart)
{
    return cart == CART_EAST ? TRACK_EAST : cart == CART_NORTH ? TRACK_NORTH : TRACK_NONE;
}

inline std::uint8_t CartOfTrack(std::uint8_t track)
{
    return track == TRACK_EAST ? CART_EAST : CART_NORTH;
}

// BattlegroundSM.h BG_SM_CartDepots: where a cart ends for a state of its switch.
inline std::uint8_t DepotOf(std::uint8_t cart, bool opened)
{
    if (cart == CART_EAST)
        return opened ? DEPOT_LAVA : DEPOT_DIAMOND;
    if (cart == CART_NORTH)
        return opened ? DEPOT_DIAMOND : DEPOT_TROLL;
    return DEPOT_WATERFALL;
}

// The whole ride of a cart as one polyline: its start slot, the first leg and (for the east and north cart) the second leg of the
// state the switch will have when the cart reaches the junction. `junction` is the index of the junction point (the last of the first leg).
struct Route
{
    std::vector<Pt> pts;
    std::size_t junction = 0;      // index of the junction point, 0 for a cart with no switch
    bool hasJunction = false;
    std::uint8_t depot = DEPOT_WATERFALL;
};

inline Route BuildRoute(std::uint8_t cart, bool opened)
{
    Route route;
    if (cart >= CART_COUNT)
        return route;
    route.pts.push_back(CART_START[cart]);
    RailView const first = FirstLeg(cart);
    for (std::size_t i = 0; i < first.count; ++i)
        route.pts.push_back(first.pts[i]);
    route.depot = DepotOf(cart, opened);
    if (TrackOf(cart) == TRACK_NONE)
        return route;
    route.hasJunction = true;
    route.junction = route.pts.size() - 1;
    RailView const second = SecondLeg(cart, opened);
    for (std::size_t i = 0; i < second.count; ++i)
        route.pts.push_back(second.pts[i]);
    return route;
}

inline float RouteLength(Route const& route)
{
    float total = 0.0f;
    for (std::size_t i = 1; i < route.pts.size(); ++i)
        total += Dist3d(route.pts[i - 1], route.pts[i]);
    return total;
}

// Where along the route a point lies: the distance from the start of the route to its projection on the nearest segment (2D).
struct Projection
{
    float along = 0.0f;       // yards from the start of the route
    float off = 1.0e9f;       // yards from the polyline
    std::size_t segment = 0;
};

inline Projection Project(Route const& route, float x, float y)
{
    Projection best;
    float walked = 0.0f;
    for (std::size_t i = 1; i < route.pts.size(); ++i)
    {
        Pt const& a = route.pts[i - 1];
        Pt const& b = route.pts[i];
        float const segLen = Dist3d(a, b);
        float const dx = b.x - a.x;
        float const dy = b.y - a.y;
        float const len2 = dx * dx + dy * dy;
        float t = len2 > 0.0001f ? ((x - a.x) * dx + (y - a.y) * dy) / len2 : 0.0f;
        t = std::clamp(t, 0.0f, 1.0f);
        float const px = a.x + t * dx;
        float const py = a.y + t * dy;
        float const off = Dist2d(x, y, px, py);
        if (off < best.off)
        {
            best.off = off;
            best.along = walked + t * segLen;
            best.segment = i - 1;
        }
        walked += segLen;
    }
    return best;
}

// The distance along the route to the junction point.
inline float JunctionAlong(Route const& route)
{
    float walked = 0.0f;
    for (std::size_t i = 1; i <= route.junction && i < route.pts.size(); ++i)
        walked += Dist3d(route.pts[i - 1], route.pts[i]);
    return walked;
}

// The point `along` yards from the start of the route (clamped to the ends).
inline Pt PointAlong(Route const& route, float along)
{
    if (route.pts.empty())
        return { 0.0f, 0.0f, 0.0f };
    if (along <= 0.0f)
        return route.pts.front();
    float walked = 0.0f;
    for (std::size_t i = 1; i < route.pts.size(); ++i)
    {
        float const segLen = Dist3d(route.pts[i - 1], route.pts[i]);
        if (walked + segLen >= along && segLen > 0.0001f)
        {
            float const t = (along - walked) / segLen;
            Pt const& a = route.pts[i - 1];
            Pt const& b = route.pts[i];
            return { a.x + t * (b.x - a.x), a.y + t * (b.y - a.y), a.z + t * (b.z - a.z) };
        }
        walked += segLen;
    }
    return route.pts.back();
}

// What a cart at (x, y) still has to ride: the seconds to its depot (the core stops it within 6 yd of the depot and pays 3 s later).
inline float RemainingSeconds(Route const& route, float x, float y)
{
    if (route.pts.size() < 2)
        return 0.0f;
    Projection const p = Project(route, x, y);
    float const left = std::max(0.0f, RouteLength(route) - p.along);
    return left / CART_SPEED;
}

// Before the junction: the cart has not yet come within JUNCTION_RADIUS of it, so a click still decides where it goes.
inline bool BeforeJunction(Route const& route, float x, float y)
{
    if (!route.hasJunction)
        return false;
    return Project(route, x, y).along < JunctionAlong(route) - JUNCTION_RADIUS;
}

inline float SecondsToJunction(Route const& route, float x, float y)
{
    if (!route.hasJunction)
        return 0.0f;
    float const left = JunctionAlong(route) - Project(route, x, y).along;
    return left > 0.0f ? left / CART_SPEED : 0.0f;
}

// The seconds of a ride from the cart's start slot to its depot.
inline float RideSeconds(std::uint8_t cart, bool opened)
{
    return RouteLength(BuildRoute(cart, opened)) / CART_SPEED;
}

// ---------------------------------------------------------------------------------------------------------------------
// Walking to a cart. A bot is not at the cart's position but where the cart will be when the bot gets there: the first second t
// at which the walk to the rail point t seconds ahead takes no longer than t.
// ---------------------------------------------------------------------------------------------------------------------
inline float WalkSeconds(float yards, bool mounted = false)
{
    return yards * PATH_FACTOR / (mounted ? MOUNT_SPEED : FOOT_SPEED);
}

constexpr float INTERCEPT_STEP_S = 2.0f;
constexpr float NEVER_S = 99999.0f;

struct Intercept
{
    bool possible = false;
    float seconds = NEVER_S;     // the walk (and the cart's ride meanwhile)
    Pt where{ 0.0f, 0.0f, 0.0f };
};

// `alongNow` is the cart's place on the route; the walker meets it before the cart is `margin` seconds from its depot at the latest.
inline Intercept InterceptOf(Route const& route, float alongNow, float botX, float botY, bool mounted, float margin = 0.0f)
{
    Intercept out;
    float const length = RouteLength(route);
    float const maxT = std::max(0.0f, (length - alongNow) / CART_SPEED - margin);
    for (float t = 0.0f; t <= maxT + 0.001f; t += INTERCEPT_STEP_S)
    {
        Pt const p = PointAlong(route, alongNow + t * CART_SPEED);
        if (WalkSeconds(Dist2d(botX, botY, p.x, p.y), mounted) <= t + 1.0f)
        {
            out.possible = true;
            out.seconds = t;
            out.where = p;
            return out;
        }
    }
    return out;
}

// ---------------------------------------------------------------------------------------------------------------------
// The bar. The core keeps it private; the plan models it from the keeper (the aura on the cart), the bodies it counts and the time.
// ---------------------------------------------------------------------------------------------------------------------
inline std::uint8_t KeeperOfBar(int bar)
{
    return bar > BAR_START ? TEAM_IDX_ALLIANCE : bar < BAR_START ? TEAM_IDX_HORDE : TEAM_NONE;
}

// `seconds` of presence: every Alliance body moves the bar up one a second, every Horde body down one.
inline int StepBar(int bar, int allianceBodies, int hordeBodies, float seconds)
{
    int const net = allianceBodies - hordeBodies;
    int const moved = static_cast<int>(std::lround(static_cast<float>(net) * seconds));
    return std::clamp(bar + moved, 0, BAR_MAX);
}

// The aura wins over the estimate: it says which side of 50 the bar is on (BattlegroundSM::UpdateCartVisual).
inline int ClampToKeeper(int bar, std::uint8_t keeper)
{
    if (keeper == TEAM_IDX_ALLIANCE)
        return std::clamp(bar, BAR_START + 1, BAR_MAX);
    if (keeper == TEAM_IDX_HORDE)
        return std::clamp(bar, 0, BAR_START - 1);
    return BAR_START;
}

// Bar steps from the keeper's end to its side of 50: how many net player-seconds the other side needs to take the cart.
inline int FlipSteps(int bar, std::uint8_t byTeam)
{
    if (byTeam == TEAM_IDX_ALLIANCE)
        return bar >= BAR_START + 1 ? 0 : BAR_START + 1 - bar;     // up to 51
    return bar <= BAR_START - 1 ? 0 : bar - (BAR_START - 1);       // down to 49
}

// The seconds team `byTeam` needs to become the keeper with `ours` bodies against `theirs` (NEVER_S with no net edge).
inline float FlipSeconds(int bar, std::uint8_t byTeam, int ours, int theirs)
{
    int const steps = FlipSteps(bar, byTeam);
    if (!steps)
        return 0.0f;
    int const net = ours - theirs;
    return net > 0 ? static_cast<float>(steps) / static_cast<float>(net) : NEVER_S;
}

// How far the bar is from the keeper's loss for a team's own cart: steps the enemy needs from here, 0 for a cart we do not keep.
inline int MarginOf(int bar, std::uint8_t team)
{
    if (KeeperOfBar(bar) != team)
        return 0;
    return team == TEAM_IDX_ALLIANCE ? bar - BAR_START : BAR_START - bar;
}

// ---------------------------------------------------------------------------------------------------------------------
// The options per cart: decision point D18, a scored list. Facts are read once per refresh from the record.
// ---------------------------------------------------------------------------------------------------------------------
enum class OptionKind : std::uint8_t
{
    SKIP = 0,
    TAKE = 1,
    FLIP = 2,
    GUARD = 3
};

inline char const* OptionKindName(OptionKind kind)
{
    switch (kind)
    {
        case OptionKind::TAKE: return "take";
        case OptionKind::FLIP: return "flip";
        case OptionKind::GUARD: return "guard";
        case OptionKind::SKIP: break;
    }
    return "skip";
}

// A cart that docks in less than this is worth no fight: only the 150 stays to be kept.
constexpr float DOCK_SOON_S = 12.0f;
// The seconds of safety between the end of a flip and the dock.
constexpr float DOCK_MARGIN_S = 5.0f;
constexpr int PEEL_PERCENT = 60;

struct CartFacts
{
    std::uint8_t cart = CART_NONE;
    bool live = false;
    std::uint8_t keeper = TEAM_NONE;     // the aura: TEAM_IDX_*, TEAM_NONE
    int bar = BAR_START;                 // the modelled bar
    float remainingS = 0.0f;             // to the depot
    std::uint32_t friends = 0;           // our bodies in the circle
    std::uint32_t enemies = 0;           // theirs in the circle: seen, plus what call-outs placed there
    float etaS = NEVER_S;                // our nearest free bot's intercept seconds
    int arrivalBonus = 0;                // the opening's first-arrival points (OpeningBonus)
    int claimBonus = 0;                  // a human's "take <cart>" (good bots only, the AB 60)
};

struct CartOption
{
    std::uint8_t cart = CART_NONE;
    OptionKind kind = OptionKind::SKIP;
    int score = 0;
};

inline int CartWorth(float remainingS)
{
    return static_cast<int>(std::lround(remainingS)) + DOCK_POINTS;
}

inline CartOption ScoreCart(CartFacts const& f, std::uint8_t team)
{
    CartOption out;
    out.cart = f.cart;
    if (!f.live || f.remainingS < DOCK_SOON_S)
        return out;

    int const worth = CartWorth(f.remainingS);
    int const walk = f.etaS >= NEVER_S ? 0 : static_cast<int>(std::lround(f.etaS));
    bool const reachable = f.etaS < NEVER_S && f.etaS + DOCK_MARGIN_S < f.remainingS;

    if (f.keeper == team)
    {
        // A cart we keep: the share of its worth that the enemy's bodies put at risk, a quarter of it with nobody seen.
        int const risk = f.enemies ? std::min<int>(worth, static_cast<int>(worth * f.enemies / (f.friends + 1))) : worth / 4;
        out.kind = OptionKind::GUARD;
        out.score = std::max(0, risk) + f.arrivalBonus;
        return out;
    }

    if (!reachable)
        return out;

    if (f.keeper == TEAM_NONE)
    {
        out.kind = OptionKind::TAKE;
        out.score = worth - walk + f.arrivalBonus + f.claimBonus;
        return out;
    }

    // An enemy cart: we gain it and they lose it, so twice the worth, if the bar can be turned before the dock.
    float const flip = FlipSeconds(f.bar, team, static_cast<int>(f.friends) + 1, static_cast<int>(f.enemies));
    if (f.etaS + flip + DOCK_MARGIN_S >= f.remainingS)
        return out;
    out.kind = OptionKind::FLIP;
    out.score = 2 * worth - walk - static_cast<int>(std::lround(flip)) + f.arrivalBonus + f.claimBonus;
    return out;
}

// All three carts, best first (ties to the lower cart id); a SKIP scores 0 and comes last.
inline std::vector<CartOption> ScoreCarts(std::array<CartFacts, CART_COUNT> const& facts, std::uint8_t team)
{
    std::vector<CartOption> list;
    for (CartFacts const& f : facts)
        if (f.cart < CART_COUNT)
            list.push_back(ScoreCart(f, team));
    std::stable_sort(list.begin(), list.end(), [](CartOption const& a, CartOption const& b)
    {
        if (a.score != b.score)
            return a.score > b.score;
        return a.cart < b.cart;
    });
    return list;
}

// ---------------------------------------------------------------------------------------------------------------------
// The opening: both teams are about 110 to 120 yd from the pit and all carts roll at once; what differs is how soon each team's
// bots meet each cart on its rail from the start platform. The difference is the first-arrival bonus (2.5 points a second of lead,
// at most 10 s either way).
// ---------------------------------------------------------------------------------------------------------------------
constexpr float OPENING_BONUS_PER_S = 2.5f;
constexpr float OPENING_BONUS_CAP_S = 10.0f;

inline float OpeningInterceptSeconds(std::uint8_t team, std::uint8_t cart, bool opened)
{
    Route const route = BuildRoute(cart, opened);
    Intercept const hit = InterceptOf(route, 0.0f, TEAM_START[team].x, TEAM_START[team].y, false);
    return hit.possible ? hit.seconds : NEVER_S;
}

inline int OpeningBonus(std::uint8_t team, std::uint8_t cart)
{
    float const ours = OpeningInterceptSeconds(team, cart, false);
    float const theirs = OpeningInterceptSeconds(static_cast<std::uint8_t>(1 - team), cart, false);
    if (ours >= NEVER_S || theirs >= NEVER_S)
        return 0;
    float const lead = std::clamp(theirs - ours, -OPENING_BONUS_CAP_S, OPENING_BONUS_CAP_S);
    return static_cast<int>(std::lround(lead * OPENING_BONUS_PER_S));
}

// ---------------------------------------------------------------------------------------------------------------------
// Which cart is the main cart. The top scored cart, unless the cart the team played last is still scored and the new top is not
// better by CART_HYSTERESIS points (a team that changes its mind every refresh arrives nowhere). `previous` is CART_NONE at the start.
// ---------------------------------------------------------------------------------------------------------------------
constexpr int CART_HYSTERESIS = 40;

inline std::uint8_t HoldMain(std::vector<CartOption> const& options, std::uint8_t previous)
{
    if (options.empty() || options.front().score <= 0)
        return CART_NONE;
    if (previous < CART_COUNT)
        for (CartOption const& o : options)
            if (o.cart == previous && o.score > 0 && o.score + CART_HYSTERESIS > options.front().score)
                return previous;
    return options.front().cart;
}

// The list with `cart` first (the others keep their order).
inline std::vector<CartOption> WithMainFirst(std::vector<CartOption> options, std::uint8_t cart)
{
    std::stable_partition(options.begin(), options.end(), [cart](CartOption const& o) { return o.cart == cart; });
    return options;
}

// ---------------------------------------------------------------------------------------------------------------------
// Team intel (owner decision 2026-11-18): an enemy counts when one of our alive players stands within this many yards of it (a
// stand-in for sight, no line of sight test) or a call-out placed it there.
// ---------------------------------------------------------------------------------------------------------------------
constexpr float SEEN_RADIUS = 70.0f;

inline bool SeenFrom(float enemyX, float enemyY, float friendX, float friendY)
{
    return Dist2d(enemyX, enemyY, friendX, friendY) <= SEEN_RADIUS;
}

// A player within the circle of a cart: 3D, like the core's IsWithinDist.
inline bool InCircle(Pt const& cart, float x, float y, float z)
{
    float const dz = z - cart.z;
    float const d2 = Dist2d(x, y, cart.x, cart.y);
    return d2 * d2 + dz * dz <= CONTROL_RADIUS * CONTROL_RADIUS;
}

// ---------------------------------------------------------------------------------------------------------------------
// Counts: how many bots tag the off carts (the rest are the main group). Sheet section 4.
// ---------------------------------------------------------------------------------------------------------------------
constexpr std::uint32_t MAIN_MIN = 3;

inline std::uint32_t OffCount(std::uint32_t teamSize, BgLeader::Stance stance, bool hasHealer)
{
    if (teamSize < MAIN_MIN + 1 || !hasHealer)
        return 0;
    std::uint32_t off = 2;
    switch (stance)
    {
        case BgLeader::Stance::OFFENSE:
        case BgLeader::Stance::ALL_IN:
            off = 1;
            break;
        case BgLeader::Stance::DEFENSE:
        case BgLeader::Stance::TURTLE:
            off = teamSize >= 8 ? 3 : 2;
            break;
        default:
            break;
    }
    return std::min(off, teamSize - MAIN_MIN);
}

struct Wants
{
    std::array<std::uint32_t, CART_COUNT> count{ 0, 0, 0 };
    std::uint8_t mainCart = CART_NONE;
};

// The bodies each cart gets from `teamSize` alive bots. The top scored cart is the main cart; the next carts by score get one bot each
// while there are off bots to give and their score is positive; an off cart where the enemy outnumbers us by two or more and whose score is
// at least PEEL_PERCENT of the main cart's gets a second body from the main group while it keeps MAIN_MIN. No cart is scored: everybody
// goes to `fallbackCart` (the nearest live one, chosen by the caller).
inline Wants WantCounts(std::vector<CartOption> const& options, std::array<CartFacts, CART_COUNT> const& facts, std::uint32_t teamSize,
                        BgLeader::Stance stance, bool hasHealer, std::uint8_t fallbackCart)
{
    Wants w;
    if (!teamSize)
        return w;

    std::vector<CartOption> positive;
    for (CartOption const& o : options)
        if (o.score > 0 && o.kind != OptionKind::SKIP)
            positive.push_back(o);

    if (positive.empty())
    {
        w.mainCart = fallbackCart;
        if (fallbackCart < CART_COUNT)
            w.count[fallbackCart] = teamSize;
        return w;
    }

    w.mainCart = positive.front().cart;
    std::uint32_t offLeft = OffCount(teamSize, stance, hasHealer);
    std::uint32_t used = 0;
    for (std::size_t i = 1; i < positive.size() && offLeft > 0; ++i)
    {
        w.count[positive[i].cart] = 1;
        --offLeft;
        ++used;
    }

    // The peel: the enemy outnumbers the off bot and the cart is worth fighting for.
    std::uint32_t mainBodies = teamSize - used;
    for (std::size_t i = 1; i < positive.size(); ++i)
    {
        CartFacts const& f = facts[positive[i].cart];
        if (w.count[positive[i].cart] == 1 && f.enemies >= f.friends + 2 && positive[i].score * 100 >= positive.front().score * PEEL_PERCENT &&
            mainBodies > MAIN_MIN)
        {
            ++w.count[positive[i].cart];
            --mainBodies;
        }
    }
    w.count[w.mainCart] = mainBodies;
    return w;
}

// ---------------------------------------------------------------------------------------------------------------------
// Who goes where. Off carts first (the sturdiest and nearest), the rest to the main cart; a bot keeps its cart while the counts allow.
// ---------------------------------------------------------------------------------------------------------------------
struct Hand
{
    std::uint32_t guidLow = 0;
    Tier tier = Tier::GOOD;
    int offFit = 0;                    // NOT_ELIGIBLE: never an off bot
    int mainFit = 0;
    std::array<float, CART_COUNT> eta{ NEVER_S, NEVER_S, NEVER_S };
    std::uint8_t prevCart = CART_NONE;
};

// The durable mobile kits tag and hold an off cart; a healer is never one (sheet section 4).
inline int OffFitOf(std::uint8_t cls, bool healer, bool tank, bool feral)
{
    if (healer)
        return BgPlan::NOT_ELIGIBLE;
    if (tank)
        return 30;
    if (cls == BgPlan::CLS_DRUID)
        return feral ? 28 : 6;
    switch (cls)
    {
        case BgPlan::CLS_DEATH_KNIGHT: return 25;
        case BgPlan::CLS_WARRIOR: return 22;
        case BgPlan::CLS_PALADIN: return 20;
        case BgPlan::CLS_ROGUE: return 20;
        case BgPlan::CLS_MONK: return 18;
        case BgPlan::CLS_SHAMAN: return 10;
        case BgPlan::CLS_HUNTER: return 8;
        case BgPlan::CLS_MAGE: return 5;
        case BgPlan::CLS_WARLOCK: return 5;
        case BgPlan::CLS_PRIEST: return 4;
        default: break;
    }
    return 0;
}

inline int MainFitOf(bool healer)
{
    return healer ? 10 : 0;
}

constexpr int STICKY_POINTS = 14;
constexpr int ETA_POINTS_PER_S = 1;

// Returns the cart of every hand, in the order of `hands`. A hand with no cart (a fallback of no live cart) gets CART_NONE.
inline std::vector<std::uint8_t> Deal(std::vector<Hand> const& hands, Wants const& wants)
{
    std::vector<std::uint8_t> out(hands.size(), CART_NONE);
    if (wants.mainCart >= CART_COUNT)
        return out;

    std::vector<bool> taken(hands.size(), false);
    std::array<std::uint32_t, CART_COUNT> left = wants.count;

    // Off carts, the highest cart count first is not needed: every off cart wants one or two bots.
    for (std::uint8_t cart = 0; cart < CART_COUNT; ++cart)
    {
        if (cart == wants.mainCart)
            continue;
        while (left[cart] > 0)
        {
            std::size_t const pick = BgPlan::PickBest(hands, taken, [&](Hand const& h)
            {
                if (h.offFit <= BgPlan::NOT_ELIGIBLE || h.eta[cart] >= NEVER_S)
                    return BgPlan::NOT_ELIGIBLE;
                int const sticky = h.prevCart == cart ? STICKY_POINTS : 0;
                return h.offFit + BgPlan::TierFitCritical(h.tier) + sticky - static_cast<int>(h.eta[cart]) * ETA_POINTS_PER_S;
            });
            if (pick >= hands.size())
                break;
            out[pick] = cart;
            taken[pick] = true;
            --left[cart];
        }
    }

    // The main group: everyone else.
    for (std::size_t i = 0; i < hands.size(); ++i)
        if (!taken[i])
            out[i] = wants.mainCart;
    return out;
}

// ---------------------------------------------------------------------------------------------------------------------
// The track switches (decision point D19).
// ---------------------------------------------------------------------------------------------------------------------
// The depot a team calls its own: the ones within about 150 yd of its start platform (sheet section 2).
inline bool IsHomeDepot(std::uint8_t depot, std::uint8_t team)
{
    if (team == TEAM_IDX_ALLIANCE)
        return depot == DEPOT_DIAMOND;
    return depot == DEPOT_LAVA || depot == DEPOT_WATERFALL;
}

constexpr int HOME_DEPOT_POINTS = 60;
constexpr int SWITCH_MIN_GAIN = 40;

inline int DepotValueFor(std::uint8_t cart, bool opened, std::uint8_t team)
{
    std::uint8_t const depot = DepotOf(cart, opened);
    int value = CartWorth(RideSeconds(cart, opened));
    if (IsHomeDepot(depot, team))
        value += HOME_DEPOT_POINTS;
    else if (IsHomeDepot(depot, static_cast<std::uint8_t>(1 - team)))
        value -= HOME_DEPOT_POINTS;
    return value;
}

struct SwitchPreference
{
    bool opened = false;
    int gain = 0;     // points the preferred end is worth over the other
};

inline SwitchPreference PreferredSwitch(std::uint8_t track, std::uint8_t team)
{
    std::uint8_t const cart = CartOfTrack(track);
    int const opened = DepotValueFor(cart, true, team);
    int const closed = DepotValueFor(cart, false, team);
    SwitchPreference pref;
    pref.opened = opened > closed;
    pref.gain = std::abs(opened - closed);
    return pref;
}

// D19 option ids and what the bot does.
enum class SwitchAct : std::uint8_t
{
    CLICK_NOW = 0,      // go and click as soon as in reach
    WAIT_LAST = 1,      // stand at the switch and click in the last window before the junction
    LEAVE = 2,          // the duty is over: too late, a group is there, or nothing to change
    NONE = 3            // not a duty (not offered)
};

constexpr float LAST_WINDOW_S = 8.0f;          // an expert clicks inside this many seconds before the junction
constexpr float SWITCH_SLACK_S = 4.0f;         // time kept in hand for the walk and the cast
constexpr std::uint32_t SWITCH_GROUP = 2;      // a group of this many enemies at the switch ends the duty

struct SwitchFacts
{
    std::uint8_t track = TRACK_NONE;
    std::uint8_t team = TEAM_IDX_ALLIANCE;
    bool cartLive = false;
    bool beforeJunction = false;
    float secondsToJunction = 0.0f;
    bool opened = false;                 // the state now (the aura on the NPC)
    float walkS = 0.0f;                  // the bot's walk to the switch
    Tier tier = Tier::GOOD;
    std::uint32_t enemiesAtSwitch = 0;
};

inline bool SwitchWrong(SwitchFacts const& f)
{
    SwitchPreference const pref = PreferredSwitch(f.track, f.team);
    return pref.gain >= SWITCH_MIN_GAIN && pref.opened != f.opened;
}

// Whether the duty exists and which act the rule picks. NONE: nothing to do (the state is right, the cart is past the junction, a casual bot).
inline SwitchAct ChooseSwitch(SwitchFacts const& f)
{
    if (f.track >= TRACK_COUNT || !f.cartLive || !f.beforeJunction || f.tier == Tier::CASUAL || !SwitchWrong(f))
        return SwitchAct::NONE;
    if (f.walkS + SWITCH_CAST_S + SWITCH_SLACK_S > f.secondsToJunction)
        return SwitchAct::LEAVE;
    if (f.enemiesAtSwitch >= SWITCH_GROUP)
        return SwitchAct::LEAVE;
    if (f.tier == Tier::EXPERT && f.secondsToJunction > LAST_WINDOW_S + SWITCH_CAST_S)
        return SwitchAct::WAIT_LAST;
    return SwitchAct::CLICK_NOW;
}

// The expert at the switch clicks when the window opens; any other act clicks at once. The caller has checked the state is still wrong.
inline bool MayClickNow(SwitchAct act, float secondsToJunction)
{
    if (act == SwitchAct::CLICK_NOW)
        return true;
    return act == SwitchAct::WAIT_LAST && secondsToJunction <= LAST_WINDOW_S + SWITCH_CAST_S;
}

// During the cast: a click toggles, so a cast that the enemy has made useless (the state is the preferred one already) is cancelled.
inline bool CancelCast(SwitchFacts const& f)
{
    return !SwitchWrong(f);
}

struct SwitchCandidate
{
    std::uint32_t guid = 0;
    Tier tier = Tier::GOOD;
    float walkS = NEVER_S;
    bool offBot = false;            // an off bot or a switch rider: not the main group's
    bool onTheTrackCart = false;    // already rides the cart of this track
};

// The one bot that holds a switch for the team: a good or expert bot that can be there in time, the riders of the cart first, then the off
// bots, then the nearest; experts win a tie. 0 for none.
inline std::uint32_t PickSwitchHolder(std::vector<SwitchCandidate> const& list, float secondsToJunction)
{
    std::uint32_t best = 0;
    int bestScore = BgPlan::NOT_ELIGIBLE;
    for (SwitchCandidate const& c : list)
    {
        if (c.tier == Tier::CASUAL || c.walkS + SWITCH_CAST_S + SWITCH_SLACK_S > secondsToJunction)
            continue;
        int score = 100 - static_cast<int>(c.walkS) + (c.onTheTrackCart ? 40 : 0) + (c.offBot ? 20 : 0) + (c.tier == Tier::EXPERT ? 6 : 0);
        if (score > bestScore || (score == bestScore && BotSkillMath::Mix(c.guid) < BotSkillMath::Mix(best)))
        {
            bestScore = score;
            best = c.guid;
        }
    }
    return best;
}

// ---------------------------------------------------------------------------------------------------------------------
// The leader's stance from the race.
// ---------------------------------------------------------------------------------------------------------------------
constexpr int STANCE_ENTER_LEAD = 100;
constexpr int STANCE_LEAVE_LEAD = 70;
constexpr int STANCE_LATE_SCORE = 1000;
constexpr int STANCE_LATE_ENTER_LEAD = 150;
constexpr int STANCE_LATE_LEAVE_LEAD = 110;

struct RaceFacts
{
    int ourScore = 0;
    int theirScore = 0;
};

// ETA to 1600 at a rate of `pointsPerSecond` (at least 0.5), for the log and the decision layer.
inline int EtaSeconds(int score, int keptCarts)
{
    float const rate = std::max(0.5f, static_cast<float>(keptCarts));
    return static_cast<int>(std::lround(std::max(0, static_cast<int>(WIN_SCORE) - score) / rate));
}

// `rule` is the drawn plan; `previous` makes the thresholds a band (a stance is left at a smaller lead than it was entered at).
inline BgLeader::Stance ChooseStance(BgPlan::Plan rule, RaceFacts const& f, BgLeader::Stance previous)
{
    using BgLeader::Stance;
    int const lead = f.ourScore - f.theirScore;
    bool const late = std::max(f.ourScore, f.theirScore) >= STANCE_LATE_SCORE;

    bool const wasTurtle = previous == Stance::TURTLE;
    bool const wasAllIn = previous == Stance::ALL_IN;
    if (late && (lead >= (wasTurtle ? STANCE_LATE_LEAVE_LEAD : STANCE_LATE_ENTER_LEAD)))
        return Stance::TURTLE;
    if (late && (-lead >= (wasAllIn ? STANCE_LATE_LEAVE_LEAD : STANCE_LATE_ENTER_LEAD)))
        return Stance::ALL_IN;
    if (lead >= (previous == Stance::DEFENSE ? STANCE_LEAVE_LEAD : STANCE_ENTER_LEAD))
        return Stance::DEFENSE;
    if (-lead >= (previous == Stance::OFFENSE ? STANCE_LEAVE_LEAD : STANCE_ENTER_LEAD))
        return Stance::OFFENSE;
    return BgLeader::StanceOfPlan(rule);
}

// D1 for this map: the rule's stance 100, the other stances that make sense below it (a second option scores 45 or more only where the
// choice is real: level, or an early lead).
inline BgLeader::Offer StanceOffer(BgLeader::Stance rule, RaceFacts const& f)
{
    using BgLeader::Stance;
    BgLeader::Offer offer;
    offer.point = BgLeader::DecisionPoint::D1_STANCE;
    offer.options.push_back({ static_cast<std::uint8_t>(rule), 100 });
    auto const add = [&](Stance s, int score)
    {
        if (s != rule)
            offer.options.push_back({ static_cast<std::uint8_t>(s), score });
    };
    int const lead = f.ourScore - f.theirScore;
    if (rule == Stance::BALANCED || rule == Stance::OFFENSE || rule == Stance::DEFENSE)
    {
        if (std::abs(lead) < STANCE_ENTER_LEAD)
        {
            add(Stance::BALANCED, 60);
            add(Stance::OFFENSE, 50);
            add(Stance::DEFENSE, 50);
        }
        else
            add(Stance::BALANCED, 40);
    }
    else if (rule == Stance::TURTLE)
        add(Stance::DEFENSE, 45);
    else if (rule == Stance::ALL_IN)
        add(Stance::OFFENSE, 40);
    BgLeader::SortOptions(offer.options);
    return offer;
}

// ---------------------------------------------------------------------------------------------------------------------
// D18 and D19 as offers (the first option is the rule's answer).
// ---------------------------------------------------------------------------------------------------------------------
inline BgLeader::Offer CartOffer(std::vector<CartOption> const& options)
{
    BgLeader::Offer offer;
    offer.point = BgLeader::DecisionPoint::D18_SM_CART;
    for (CartOption const& o : options)
        if (o.score > 0 && o.kind != OptionKind::SKIP)
            offer.options.push_back({ o.cart, o.score });
    BgLeader::SortOptions(offer.options);
    return offer;
}

inline BgLeader::Offer SwitchOffer(SwitchAct rule)
{
    BgLeader::Offer offer;
    offer.point = BgLeader::DecisionPoint::D19_SM_SWITCH;
    auto const score = [&](SwitchAct act)
    {
        if (act == rule)
            return 100;
        return act == SwitchAct::LEAVE ? 40 : 55;
    };
    for (SwitchAct act : { SwitchAct::CLICK_NOW, SwitchAct::WAIT_LAST, SwitchAct::LEAVE })
        offer.options.push_back({ static_cast<std::uint8_t>(act), score(act) });
    BgLeader::SortOptions(offer.options);
    return offer;
}

// ---------------------------------------------------------------------------------------------------------------------
// The leash: a bot in a cart's circle fights what comes and does not leave the cart by more than LEASH_YARDS for a kill. An expert never
// chases, a good bot comes back after a few seconds, a casual bot follows about a third of the time (a human mistake on purpose).
// ---------------------------------------------------------------------------------------------------------------------
constexpr float LEASH_YARDS = 30.0f;
constexpr std::uint32_t LEASH_GOOD_AFTER_MS = 3000;
constexpr std::uint32_t LEASH_CASUAL_CHASE_PERCENT = 33;
constexpr std::uint32_t LEASH_ROLL_MIX = 2246822519u;

inline bool LeashReturns(Tier tier, float yardsFromCart, std::uint32_t outsideMs, std::uint32_t guidLow, std::uint32_t bucket)
{
    if (yardsFromCart <= LEASH_YARDS)
        return false;
    if (tier == Tier::EXPERT)
        return true;
    if (tier == Tier::GOOD)
        return outsideMs >= LEASH_GOOD_AFTER_MS;
    bool const chases = BotSkillMath::Mix(guidLow ^ (bucket * LEASH_ROLL_MIX)) % 100 < LEASH_CASUAL_CHASE_PERCENT;
    return !chases && outsideMs >= LEASH_GOOD_AFTER_MS;
}

// ---------------------------------------------------------------------------------------------------------------------
// The words of the call-outs (sheet section 8).
// ---------------------------------------------------------------------------------------------------------------------
inline char const* CartWord(std::uint8_t cart)
{
    return cart == CART_EAST ? "lava" : cart == CART_WATERFALL ? "mid" : cart == CART_NORTH ? "top" : "cart";
}

inline char const* SwitchWord(std::uint8_t track)
{
    return track == TRACK_EAST ? "east switch" : track == TRACK_NORTH ? "north switch" : "switch";
}

inline char const* DepotWord(std::uint8_t depot)
{
    return depot == DEPOT_WATERFALL ? "waterfall" : depot == DEPOT_LAVA ? "lava" : depot == DEPOT_DIAMOND ? "long" : "troll";
}

// The place ids of the reader and of the observations: the three carts, the Diamond depot and the two switches.
constexpr std::uint8_t PLACE_LONG = 3;
constexpr std::uint8_t PLACE_SWITCH_EAST = 4;
constexpr std::uint8_t PLACE_SWITCH_NORTH = 5;
}  // namespace SmLogic

#endif
