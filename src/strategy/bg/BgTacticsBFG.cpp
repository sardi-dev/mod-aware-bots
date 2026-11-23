/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Battle for Gilneas (map 761, BATTLEGROUND_BFG) bot tactics - issue #576, epic #580.
// AC has no spec for this map; this extends AC's Arathi Basin pattern (selectObjective AB case
// AC BattleGroundTactics.cpp:2312-2489, atFlag AB case :3561-3965, moveToStart AB branch :1719-1840)
// to BfG's three capturable nodes. BT-550 (docs/bg-sheets/battle-for-gilneas.md): the old random-role case is now the
// playbook's fallback: one near-node defender, the best node to click by swing value, a hold state at three nodes, no camp.

#include "BattleGroundTactics.h"

// DatabaseEnv.h must precede the battleground headers (see BattleGroundTactics.cpp).
#include "DatabaseEnv.h"

#include "AbPlan.h"
#include "BfgPlanLogic.h"
#include "BotSkill.h"
#include "Battleground.h"
#include "DBCStores.h"
#include "BattlegroundBFG.h"
#include "Creature.h"
#include "GameObject.h"
#include "Playerbots.h"
#include "PlayerbotSpec.h"
#include "PositionValue.h"
#include "ServerFacade.h"
#include "WorldSession.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <map>
#include <mutex>
#include <unordered_map>

namespace
{
// Node state, the core's own numbering: 0 neutral, 1 and 2 contested by the Alliance and the Horde, 3 and 4 occupied by them
// (BattlegroundBFG::GetNodeStatus, the read-only hook of the core patches; it is updated at the click, with no
// 2 s blind window like the banner objects).
constexpr uint8 BFG_STATE_UNKNOWN = 255;  // not map 761

// The key spell of the Lighthouse banner lock (Lock.dbc 1956).
constexpr uint32 BFG_LIGHTHOUSE_CAPTURE_SPELL = 98322;

// Entries a bot may cast SPELL_CAPTURE_BANNER on: the three neutral node banners and the
// contested/occupied variants (BattlegroundBFG.h:48-52, 84-88).
std::vector<uint32> const vFlagsBFG = {
    GILNEAS_BG_OBJECTID_NODE_BANNER_0, GILNEAS_BG_OBJECTID_NODE_BANNER_1, GILNEAS_BG_OBJECTID_NODE_BANNER_2,
    GILNEAS_BG_OBJECTID_BANNER_A,      GILNEAS_BG_OBJECTID_BANNER_CONT_A, GILNEAS_BG_OBJECTID_BANNER_H,
    GILNEAS_BG_OBJECTID_BANNER_CONT_H,
};

// Walkable standing height above the banner object's z: the banner sits in a navmesh hole and the
// surrounding mesh surface is 0.65-0.9 yd above GILNEAS_BG_NodePositions z (mmtile_probe scan, notes file).
float const BFG_NODE_Z_OFFSET = 0.7f;

// The spell that opens a node banner. Waterworks and Mines banners are locked to the Opening cast (lock type 17, spell 21651),
// but the Lighthouse banner (208779) is locked to the spell 98322 "Capturing" (lock 1956, type spell key), so a bot casting
// Opening there never flipped the node: live check 2026-11-19, an Alliance rogue cast it 542 times in six minutes and the
// Lighthouse stayed neutral. The key spell of the banner's own lock decides, Opening is the fallback.
uint32 BannerCaptureSpell(GameObject const* banner)
{
    GameObjectTemplate const* info = banner ? banner->GetGOInfo() : nullptr;
    LockEntry const* lock = info ? sLockStore.LookupEntry(info->GetLockId()) : nullptr;
    if (lock)
        for (uint8 i = 0; i < MAX_LOCK_CASE; ++i)
            if (lock->Type[i] == LOCK_KEY_SPELL && lock->Index[i] && sSpellMgr->GetSpellInfo(lock->Index[i]))
                return lock->Index[i];
    return SPELL_CAPTURE_BANNER;
}

// A capture cast of any banner: Opening or the Lighthouse's key spell.
bool IsBannerCaptureSpell(SpellInfo const* spell)
{
    return spell && (spell->Id == SPELL_CAPTURE_BANNER || spell->Id == BFG_LIGHTHOUSE_CAPTURE_SPELL);
}

// Waiting spots in the start rooms: GILNEAS_BG_SpiritGuidePos[3] (Alliance) / [4] (Horde), verified ON NAVMESH.
Position const BFG_WAITING_POS_ALLIANCE = {908.274f, 1338.60f, 27.6449f, 5.95f};
Position const BFG_WAITING_POS_HORDE = {1401.38f, 977.125f, 7.44215f, 3.04f};

uint8 BfgNodeState(Battleground* bg, uint8 node)
{
    if (!bg || bg->GetMapId() != BfgLogic::MAP_ID || node >= GILNEAS_BG_DYNAMIC_NODES_COUNT)
        return BFG_STATE_UNKNOWN;
    return static_cast<BattlegroundBFG*>(bg)->GetNodeStatus(node);
}

// Mirror of the click check in BattlegroundBFG::EventPlayerClickedOnFlag (BattlegroundBFG.cpp:386-387):
// a banner may be used when the node is neutral or teamIndex == m_nodes % 2. That means the owner
// of a contested node cannot click its own contested banner again, only the opponent can.
bool BfgCanClick(uint8 state, TeamId team)
{
    if (state == BFG_STATE_UNKNOWN)
        return false;
    return state == GILNEAS_BG_NODE_TYPE_NEUTRAL || uint8(team) == (state % 2);
}

// A point 2 to 6 yd around a node's banner on the navmesh surface (the banner sits in a mesh hole): a random one to click it
// from, a fixed one per bot (`rank`, the bot's place among those that stand there) for a post, so that a bot at its post does
// not wander when the objective is chosen again.
void BfgPointNearNode(uint8 node, float minR, float maxR, float& x, float& y, float& z)
{
    float const angle = frand(0.0f, 2.0f * 3.14159265f);
    float const r = frand(minR, maxR);
    x = BfgLogic::NODE_POS[node].x + r * std::cos(angle);
    y = BfgLogic::NODE_POS[node].y + r * std::sin(angle);
    z = BfgLogic::NODE_POS[node].z + BFG_NODE_Z_OFFSET;
}

constexpr float BFG_POST_RING_MIN = 3.0f;
constexpr float BFG_POST_RING_MAX = 8.0f;
constexpr float BFG_STAGE_RING_MIN = 12.0f;
constexpr float BFG_STAGE_RING_MAX = 20.0f;

void BfgPostPoint(uint8 node, uint32 guidLow, uint32 rank, bool staged, float& x, float& y, float& z)
{
    float const angle = 2.0f * 3.14159265f * float(BotSkillMath::Mix(guidLow) % 360) / 360.0f + 0.7f * float(node) + 0.9f * float(rank);
    float const t = float(BotSkillMath::Mix(guidLow + 17u) % 100) / 100.0f;
    float const r = staged ? BFG_STAGE_RING_MIN + (BFG_STAGE_RING_MAX - BFG_STAGE_RING_MIN) * t :
                             BFG_POST_RING_MIN + (BFG_POST_RING_MAX - BFG_POST_RING_MIN) * t;
    x = BfgLogic::NODE_POS[node].x + r * std::cos(angle);
    y = BfgLogic::NODE_POS[node].y + r * std::sin(angle);
    z = BfgLogic::NODE_POS[node].z + BFG_NODE_Z_OFFSET;
}

// ---- The team roster of the old case (BT-550) -----------------------------------------------------------------------
// The near-node defender and the rank of a bot among the others are computed from the bots of the team: one pass over the
// battleground's players at most every 2 s per match (the objective is chosen when a bot arrives or the node situation changes,
// not every tick), shared by the bots of the match behind one mutex. The node plan (BT-5xx) has its own record and replaces this.
struct BfgTeamRoster
{
    std::vector<BfgLogic::Member> bots;
    uint32 size = 0;    // everybody on the team, humans included
};

struct BfgRoster
{
    uint32 refreshedMs = 0;
    BfgTeamRoster team[2];
};

constexpr uint32 BFG_ROSTER_REFRESH_MS = 2000;
constexpr uint32 BFG_ROSTER_MAX_AGE_MS = 10 * 60 * 1000;
std::mutex s_bfgRosterLock;
std::map<uint32, BfgRoster> s_bfgRoster;

// A bot that holds a post (a defender, a guard of the hold state or of a capture): the node. The friend-casting rule leaves it be.
std::mutex s_bfgPostLock;
std::unordered_map<uint32, uint8> s_bfgPost;

void BfgReadRoster(Battleground* bg, BfgRoster& roster, uint32 nowMs)
{
    roster.refreshedMs = nowMs ? nowMs : 1;
    for (BfgTeamRoster& team : roster.team)
    {
        team.bots.clear();
        team.size = 0;
    }
    for (auto const& ref : bg->GetBgMap()->GetPlayers())
    {
        Player* p = ref.GetSource();
        if (!p || !p->IsInWorld() || p->GetBattlegroundId() != bg->GetInstanceID() || p->IsGameMaster())
            continue;

        BfgTeamRoster& team = roster.team[p->GetBGTeam() == ALLIANCE ? uint32(TEAM_ALLIANCE) : uint32(TEAM_HORDE)];
        ++team.size;
        if (!p->GetSession() || !p->GetSession()->IsBot())
            continue;   // a human takes a slot and is never commanded

        BfgLogic::Member m;
        m.guidLow = p->GetGUIDLow();
        m.cls = p->getClass();
        m.level = p->getLevel();
        m.healer = PlayerBotSpec::IsHeal(p, true);
        m.tank = PlayerBotSpec::IsTank(p, true);
        m.feral = m.cls == BgPlan::CLS_DRUID && BotGetSpecialization(p) == SPEC_DRUID_FERAL;
        m.tier = BotSkill::TierOf(p);
        team.bots.push_back(m);
    }
}

// The roster of the match (refreshed when due or when the bot is not in it yet), copied for the bot's own team.
BfgTeamRoster BfgTeamOf(Battleground* bg, Player* bot)
{
    uint32 const nowMs = getMSTime();
    uint32 const t = bot->GetBGTeam() == ALLIANCE ? uint32(TEAM_ALLIANCE) : uint32(TEAM_HORDE);
    std::lock_guard<std::mutex> guard(s_bfgRosterLock);

    for (auto itr = s_bfgRoster.begin(); itr != s_bfgRoster.end();)
        itr = (nowMs - itr->second.refreshedMs > BFG_ROSTER_MAX_AGE_MS) ? s_bfgRoster.erase(itr) : std::next(itr);

    BfgRoster& roster = s_bfgRoster[bg->GetInstanceID()];
    auto const has = [&]() {
        for (BfgLogic::Member const& m : roster.team[t].bots)
            if (m.guidLow == bot->GetGUIDLow())
                return true;
        return false;
    };
    if (!roster.refreshedMs || nowMs - roster.refreshedMs >= BFG_ROSTER_REFRESH_MS || !has())
        BfgReadRoster(bg, roster, nowMs);
    return roster.team[t];
}

// ---- Graveyard islands (#576) ---------------------------------------------------------------------------------
// Offline Detour check (dtsim.py-style union-find over the 15 map-761 tiles of testserver data-mop-extract mmaps):
// the Horde/Alliance start rooms, all three nodes, the Waterworks graveyard and both base graveyards are in ONE
// connected component (14647 polygons), but the Lighthouse graveyard (WorldSafeLocs 1736, 1034.8 1335.6 z 12) and the
// Mine graveyard (1735, 1252.2 836.5 z 27.8) each sit on a separate navmesh ISLAND (30 / 34 polygons, 684 / 1194 sq yd)
// with no path off it. GetClosestGraveYard hands a dead team the graveyard of every node it owns, so the first bot that
// died after a capture respawned on an island and stood at its rim for the rest of the match: wave 33 (t575 b33-bfg) has
// Rogerus, Inianuel, Giseliane, Rineva, Elinayla around (1258,845) / (1242,832) for 130+ s each, and Zorian, Erinie around
// (1037,1325) / (1009,1337), all "move to objective:FAILED". The rim is only 1-3 yd above the main mesh. Same cure as
// Silvershard Mines (BgTacticsSM.cpp SM_LEDGES) and AC's wsJumpDown/eyJumpDown: walk to a take-off point on the island,
// JumpTo() a landing point on the main mesh. Take-off/landing from the same offline check (nearest island/main vertex
// pairs; the take-off resolves to the island component and the landing to the main one).
struct BfgIslandPoly
{
    uint8 n;
    float v[18];  // x, y, z per vertex, up to 6 vertices (navmesh polygon, game coordinates)
};

BfgIslandPoly const BFG_ISLAND_LIGHTHOUSE_GY[] = {
    {4, {1009.1,1337.1,3.7,1009.1,1333.9,4.3,1012.5,1333.9,8.8,1012.8,1337.1,8.8}},
    {5, {1012.5,1333.9,8.8,1016.5,1329.6,9.9,1020.8,1333.9,10.9,1017.1,1341.3,12.0,1012.8,1337.1,8.8}},
    {5, {1024.0,1344.0,12.3,1017.1,1344.0,14.1,1017.1,1341.3,12.0,1020.8,1333.9,10.9,1024.0,1333.9,11.2}},
    {3, {1017.1,1346.1,15.7,1017.1,1344.0,14.1,1024.0,1344.0,12.3}},
    {4, {1024.0,1349.6,16.0,1013.6,1349.6,18.1,1017.1,1346.1,15.7,1024.0,1344.0,12.3}},
    {3, {1035.2,1342.9,11.7,1034.4,1344.0,12.0,1033.1,1344.0,12.0}},
    {3, {1043.2,1327.2,6.7,1045.3,1326.1,4.8,1045.3,1329.3,9.1}},
    {6, {1035.2,1342.9,11.7,1033.1,1344.0,12.0,1030.4,1343.2,12.0,1025.1,1333.9,10.9,1029.1,1329.6,10.4,1033.6,1329.6,10.1}},
    {3, {1037.3,1325.6,4.0,1041.9,1325.6,4.0,1043.2,1327.2,6.7}},
    {5, {1030.4,1343.2,12.0,1029.9,1344.0,12.0,1024.0,1344.0,12.3,1024.0,1333.9,11.2,1025.1,1333.9,10.9}},
    {3, {1045.3,1344.0,11.5,1037.9,1344.0,11.7,1037.1,1342.9,11.7}},
    {6, {1037.3,1325.6,4.0,1043.2,1327.2,6.7,1045.3,1329.3,9.1,1045.3,1344.0,11.5,1037.1,1342.9,11.7,1033.6,1329.6,10.1}},
    {3, {1037.1,1342.9,11.7,1035.2,1342.9,11.7,1033.6,1329.6,10.1}},
    {4, {1033.6,1345.9,12.0,1033.1,1344.0,12.0,1034.4,1344.0,12.0,1034.4,1345.6,12.0}},
    {3, {1024.0,1344.0,12.3,1029.9,1344.0,12.0,1029.3,1346.7,12.3}},
    {5, {1037.3,1346.1,12.0,1037.1,1345.1,13.6,1037.9,1344.0,11.7,1038.7,1345.3,12.0,1039.2,1348.5,12.0}},
    {3, {1033.6,1345.9,12.0,1034.4,1345.6,12.0,1036.3,1346.4,12.0}},
    {5, {1033.9,1349.3,12.3,1024.0,1349.6,16.0,1024.0,1344.0,12.3,1029.3,1346.7,12.3,1031.2,1347.7,12.3}},
    {3, {1031.2,1347.7,12.3,1032.5,1347.7,12.3,1033.9,1349.3,12.3}},
    {4, {1039.2,1348.5,12.0,1040.5,1348.8,12.0,1040.5,1353.6,12.3,1037.9,1353.6,12.3}},
    {3, {1036.3,1346.4,12.0,1037.3,1346.1,12.0,1039.2,1348.5,12.0}},
    {6, {1033.9,1349.3,12.3,1032.5,1347.7,12.3,1033.6,1345.9,12.0,1036.3,1346.4,12.0,1039.2,1348.5,12.0,1037.9,1353.6,12.3}},
    {3, {1042.7,1348.0,11.7,1042.4,1345.3,11.7,1045.3,1345.6,11.5}},
    {3, {1040.5,1353.6,12.3,1040.5,1348.8,12.0,1042.7,1348.0,11.7}},
    {4, {1042.7,1348.0,11.7,1045.3,1345.6,11.5,1045.3,1353.6,11.5,1040.5,1353.6,12.3}},
    {4, {1045.3,1344.0,11.5,1045.3,1345.6,11.5,1042.4,1345.3,11.7,1041.3,1344.5,11.7}},
    {3, {1041.3,1344.5,11.7,1038.7,1345.3,12.0,1037.9,1344.0,11.7}},
    {3, {1045.3,1344.0,11.5,1041.3,1344.5,11.7,1037.9,1344.0,11.7}},
    {4, {1045.3,1344.0,11.5,1045.3,1329.3,9.1,1053.6,1337.3,9.6,1046.9,1344.0,11.5}},
    {3, {1046.9,1344.0,11.5,1045.3,1345.6,11.5,1045.3,1344.0,11.5}},
};

BfgIslandPoly const BFG_ISLAND_MINE_GY[] = {
    {3, {1242.1,810.7,28.0,1242.1,805.3,28.0,1245.6,808.8,28.0}},
    {4, {1242.1,810.7,28.0,1245.6,808.8,28.0,1246.4,808.8,28.0,1258.7,810.7,28.5}},
    {3, {1253.9,804.5,31.5,1258.7,804.5,28.8,1258.7,810.7,28.5}},
    {4, {1246.4,808.8,28.0,1252.0,802.7,31.5,1253.9,804.5,31.5,1258.7,810.7,28.5}},
    {4, {1238.1,812.5,32.3,1239.5,810.9,32.8,1241.9,812.5,28.3,1238.1,820.8,28.5}},
    {6, {1255.7,825.1,28.0,1251.5,826.9,28.0,1248.5,824.5,28.0,1241.9,812.5,28.3,1242.1,810.7,28.0,1258.7,810.7,28.5}},
    {4, {1242.1,824.5,28.0,1245.6,825.9,28.0,1246.7,828.8,28.0,1242.1,832.0,28.0}},
    {3, {1255.7,825.1,28.0,1253.3,826.7,28.0,1251.5,826.9,28.0}},
    {4, {1254.7,828.0,28.0,1254.1,828.3,28.0,1253.3,826.7,28.0,1255.7,825.1,28.0}},
    {4, {1251.5,826.9,28.0,1250.7,829.3,28.0,1249.9,828.0,28.0,1248.5,824.5,28.0}},
    {4, {1253.9,829.6,28.0,1254.1,828.3,28.0,1254.7,828.0,28.0,1257.3,829.3,28.0}},
    {3, {1258.1,829.3,28.0,1258.7,828.3,28.0,1258.7,832.0,28.0}},
    {5, {1248.5,824.5,28.0,1245.6,825.9,28.0,1242.1,824.5,28.0,1238.1,820.8,28.5,1241.9,812.5,28.3}},
    {3, {1258.7,810.7,28.5,1258.7,825.6,28.0,1255.7,825.1,28.0}},
    {3, {1250.7,829.3,28.0,1246.7,828.8,28.0,1249.9,828.0,28.0}},
    {4, {1257.3,829.3,28.0,1258.1,829.3,28.0,1258.7,832.0,28.0,1253.9,829.6,28.0}},
    {4, {1250.7,829.3,28.0,1252.3,829.3,29.6,1253.9,829.6,28.0,1258.7,832.0,28.0}},
    {4, {1242.1,832.0,28.0,1246.7,828.8,28.0,1250.7,829.3,28.0,1258.7,832.0,28.0}},
    {3, {1257.9,841.9,28.5,1257.9,844.8,23.7,1254.7,841.6,28.5}},
    {4, {1258.7,841.6,28.5,1257.9,841.9,28.5,1254.7,841.6,28.5,1258.7,832.0,28.0}},
    {5, {1253.6,841.6,28.3,1247.7,847.5,26.1,1242.1,841.6,28.0,1242.1,832.0,28.0,1258.7,832.0,28.0}},
    {3, {1258.7,832.0,28.0,1254.7,841.6,28.5,1253.6,841.6,28.3}},
    {4, {1266.4,808.8,28.8,1270.9,808.8,28.5,1272.5,810.7,28.0,1258.7,810.7,28.5}},
    {4, {1258.7,810.7,28.5,1258.7,804.5,28.8,1262.4,804.5,29.6,1266.4,808.8,28.8}},
    {4, {1275.2,826.1,28.5,1276.3,826.7,30.1,1276.3,827.5,29.9,1274.7,828.8,27.7}},
    {5, {1274.7,829.6,28.3,1276.5,831.2,32.3,1275.7,832.0,32.0,1258.7,832.0,28.0,1258.7,828.3,28.0}},
    {6, {1272.5,810.7,28.0,1274.7,812.8,28.3,1280.0,819.5,29.9,1274.7,824.5,28.5,1258.7,825.6,28.0,1258.7,810.7,28.5}},
    {5, {1274.7,824.5,28.5,1275.2,826.1,28.5,1274.7,828.8,27.7,1258.7,828.3,28.0,1258.7,825.6,28.0}},
    {4, {1274.7,812.8,28.3,1278.7,812.8,28.5,1280.0,814.4,28.5,1280.0,819.5,29.9}},
    {3, {1274.7,828.8,27.7,1274.7,829.6,28.3,1258.7,828.3,28.0}},
    {3, {1275.7,832.0,32.0,1270.9,837.1,34.4,1268.3,835.2,29.6}},
    {3, {1258.7,832.0,28.0,1275.7,832.0,32.0,1268.3,835.2,29.6}},
    {4, {1260.5,843.2,27.7,1258.7,841.6,28.5,1258.7,832.0,28.0,1268.3,835.2,29.6}},
    {3, {1280.0,819.5,29.9,1280.0,814.4,28.5,1282.7,816.8,29.3}},
};

struct BfgIsland
{
    char const* name;
    BfgIslandPoly const* polys;
    uint32 count;
    float minX, maxX, minY, maxY;
    float takeoff[3];
    float landing[3];
};

BfgIsland const BFG_ISLANDS[2] = {
    {"lighthouse-gy", BFG_ISLAND_LIGHTHOUSE_GY, uint32(sizeof(BFG_ISLAND_LIGHTHOUSE_GY) / sizeof(BfgIslandPoly)),
     1008.0f, 1055.0f, 1324.0f, 1355.0f, {1041.9f, 1325.6f, 4.0f}, {1039.3f, 1323.9f, 2.5f}},
    {"mine-gy", BFG_ISLAND_MINE_GY, uint32(sizeof(BFG_ISLAND_MINE_GY) / sizeof(BfgIslandPoly)),
     1237.0f, 1284.0f, 801.0f, 849.0f, {1257.9f, 844.8f, 23.7f}, {1255.7f, 849.2f, 20.7f}},
};
float const BFG_TAKEOFF_RADIUS = 2.5f;
float const BFG_ISLAND_Z_TOLERANCE = 4.0f;

bool BfgPointInPoly(float x, float y, BfgIslandPoly const& p)
{
    bool inside = false;
    for (uint32 i = 0, j = p.n - 1u; i < p.n; j = i++)
    {
        float const xi = p.v[i * 3], yi = p.v[i * 3 + 1], xj = p.v[j * 3], yj = p.v[j * 3 + 1];
        if (((yi > y) != (yj > y)) && (x < (xj - xi) * (y - yi) / (yj - yi) + xi))
            inside = !inside;
    }
    return inside;
}

// The island the bot is standing on (inside one of its polygons, within a few yards of that polygon's height), or null.
BfgIsland const* BfgIslandOf(Player* bot)
{
    float const x = bot->GetPositionX(), y = bot->GetPositionY(), z = bot->GetPositionZ();
    for (BfgIsland const& isl : BFG_ISLANDS)
    {
        if (x < isl.minX || x > isl.maxX || y < isl.minY || y > isl.maxY)
            continue;
        for (uint32 i = 0; i < isl.count; ++i)
        {
            BfgIslandPoly const& p = isl.polys[i];
            if (!BfgPointInPoly(x, y, p))
                continue;
            float avgZ = 0.0f;
            for (uint32 k = 0; k < p.n; ++k)
                avgZ += p.v[k * 3 + 2];
            avgZ /= p.n;
            if (std::fabs(z - avgZ) <= BFG_ISLAND_Z_TOLERANCE)
                return &isl;
        }
    }
    return nullptr;
}

// ---- Match evidence (#576) ------------------------------------------------------------------------------------
// "[BGBFG] node-change" on every observed node state change and one "[BGBFG] state" line per instance every 30 s,
// written by whichever bot of the instance runs first (the core banner objects are the truth, the bot only reports).
// Process-wide with a mutex, not thread_local: an instance's map update can run on any MapUpdate.Threads worker.
char const* const BFG_NODE_NAMES[3] = {"lighthouse", "waterworks", "mine"};
char const* const BFG_STATE_NAMES[5] = {"neutral", "A-contested", "H-contested", "A-occupied", "H-occupied"};

struct BfgWatch
{
    uint32 lastLogMs = 0;
    uint8 state[3] = {BFG_STATE_UNKNOWN, BFG_STATE_UNKNOWN, BFG_STATE_UNKNOWN};
    uint32 changes = 0;
};
std::mutex s_bfgWatchLock;
std::map<uint32, BfgWatch> s_bfgWatch;

char const* BfgStateName(uint8 s)
{
    return s < 5 ? BFG_STATE_NAMES[s] : "swap";
}

void BfgObserve(Battleground* bg)
{
    if (bg->GetStatus() != STATUS_IN_PROGRESS)
        return;

    uint8 now[3];
    for (uint8 i = 0; i < 3; ++i)
        now[i] = BfgNodeState(bg, i);

    std::lock_guard<std::mutex> guard(s_bfgWatchLock);
    BfgWatch& w = s_bfgWatch[bg->GetInstanceID()];
    for (uint8 i = 0; i < 3; ++i)
    {
        if (now[i] == BFG_STATE_UNKNOWN || now[i] == w.state[i])
            continue;
        if (w.state[i] != BFG_STATE_UNKNOWN)
        {
            ++w.changes;
            SF_LOG_DEBUG("playerbots", "[BGBFG] node-change inst=%u node=%s %s->%s score=%u-%u", bg->GetInstanceID(),
                         BFG_NODE_NAMES[i], BfgStateName(w.state[i]), BfgStateName(now[i]),
                         bg->GetTeamScore(TEAM_ALLIANCE), bg->GetTeamScore(TEAM_HORDE));
        }
        w.state[i] = now[i];
    }

    uint32 const nowMs = getMSTime();
    if (w.lastLogMs && getMSTimeDiff(w.lastLogMs, nowMs) < 30000)
        return;
    w.lastLogMs = nowMs;
    SF_LOG_DEBUG("playerbots", "[BGBFG] state inst=%u score=%u-%u changes=%u lighthouse=%s waterworks=%s mine=%s",
                 bg->GetInstanceID(), bg->GetTeamScore(TEAM_ALLIANCE), bg->GetTeamScore(TEAM_HORDE), w.changes,
                 BfgStateName(now[0]), BfgStateName(now[1]), BfgStateName(now[2]));
}
}  // namespace

bool BGTactics::moveToStartBFG(Battleground* bg)
{
    if (!bg)
        return false;

    // AC moveToStart AB branch (AC BattleGroundTactics.cpp:1760-1767): spread around the waiting spot.
    if (BgTeamIdOf(bot) == TEAM_HORDE)
        return MoveTo(bg->GetMapId(), BFG_WAITING_POS_HORDE.GetPositionX() + frand(-4.0f, 4.0f),
                      BFG_WAITING_POS_HORDE.GetPositionY() + frand(-4.0f, 4.0f), BFG_WAITING_POS_HORDE.GetPositionZ());

    return MoveTo(bg->GetMapId(), BFG_WAITING_POS_ALLIANCE.GetPositionX() + frand(-4.0f, 4.0f),
                  BFG_WAITING_POS_ALLIANCE.GetPositionY() + frand(-4.0f, 4.0f),
                  BFG_WAITING_POS_ALLIANCE.GetPositionZ());
}

// The objective of the old case (BT-550, sheet section 13 G1): the same answer for every bot from the node states and the
// roster, with no random role. The rules are in BfgLogic::Choose (pure, tested): the near-node defender holds its node, a node we
// are capturing counts as ours, the best node to click by swing value (the third node first), and with nothing to click the hold
// state at the posts. Nothing walks to the enemy start room and nothing chases an enemy across the map.
bool BGTactics::selectObjectiveBFG(Battleground* bg)
{
    if (!bg || bg->GetMapId() != BfgLogic::MAP_ID)
        return false;

    PositionMap& posMap = context->GetValue<PositionMap&>("position")->Get();
    PositionInfo pos = posMap["bg objective"];

    // BT-552: the node plan (AbPlan.cpp over the Gilneas row of BgNodeMap.h) plays the playbook: the opening groups, the regimes,
    // the hold state, the retreat, the call-outs. When it cannot say (no record yet, the bot has no slot), the old case below runs.
    if (AbPlan::Enabled(bg))
    {
        AbPlan::Objective objective;
        if (AbPlan::Decide(bg, bot, GetBotStrategyForTeam(bg, TEAM_ALLIANCE), GetBotStrategyForTeam(bg, TEAM_HORDE), objective))
        {
            {
                std::lock_guard<std::mutex> guard(s_bfgPostLock);
                s_bfgPost.erase(bot->GetGUIDLow());
            }
            pos.Set(objective.x, objective.y, objective.z, bot->GetMapId());
            posMap["bg objective"] = pos;
            return true;
        }
    }

    TeamId const team = BgTeamIdOf(bot);
    uint8 const teamIdx = team == TEAM_ALLIANCE ? BfgLogic::Ab::TEAM_IDX_ALLIANCE : BfgLogic::Ab::TEAM_IDX_HORDE;

    BfgLogic::Input in;
    in.team = teamIdx;
    for (uint8 i = 0; i < BfgLogic::NODE_COUNT; ++i)
        in.state[i] = BfgNodeState(bg, i);
    in.x = bot->GetPositionX();
    in.y = bot->GetPositionY();
    in.level = bot->getLevel();

    // The defenders of the team and this bot's place among the others, from the roster (a human is never commanded).
    BfgTeamRoster const roster = BfgTeamOf(bg, bot);
    BgPlan::Plan const plan = BgPlan::PlanFromStrategy(GetBotStrategyForTeam(bg, team));
    std::vector<uint32> const defenders = BfgLogic::PickDefenders(roster.bots, BfgLogic::DefenderCount(plan, roster.size));
    in.defender = std::find(defenders.begin(), defenders.end(), bot->GetGUIDLow()) != defenders.end();

    std::vector<uint32> others;
    for (BfgLogic::Member const& m : roster.bots)
        if (std::find(defenders.begin(), defenders.end(), m.guidLow) == defenders.end())
            others.push_back(m.guidLow);
    std::sort(others.begin(), others.end(), [](uint32 a, uint32 b)
    {
        uint32 const ha = BotSkillMath::Mix(a);
        uint32 const hb = BotSkillMath::Mix(b);
        return ha != hb ? ha < hb : a < b;
    });
    in.others = uint32(others.size());
    auto const rank = std::find(others.begin(), others.end(), bot->GetGUIDLow());
    in.rank = rank == others.end() ? 0u : uint32(rank - others.begin());

    BfgLogic::Choice const choice = BfgLogic::Choose(in);
    if (choice.what == BfgLogic::What::NOTHING || choice.node >= BfgLogic::NODE_COUNT)
        return false;

    float rx, ry, rz;
    if (choice.what == BfgLogic::What::POST)
        BfgPostPoint(choice.node, bot->GetGUIDLow(), in.defender ? 0u : in.rank, choice.staged, rx, ry, rz);
    else
        BfgPointNearNode(choice.node, 2.0f, 6.0f, rx, ry, rz);

    {
        std::lock_guard<std::mutex> guard(s_bfgPostLock);
        if (choice.what == BfgLogic::What::POST)
            s_bfgPost[bot->GetGUIDLow()] = choice.node;
        else
            s_bfgPost.erase(bot->GetGUIDLow());
    }

    pos.Set(rx, ry, rz, bot->GetMapId());
    posMap["bg objective"] = pos;
    return true;
}

bool BGTactics::atObjectiveBFG(Battleground* bg)
{
    if (!bg)
        return false;

    BfgObserve(bg);

    // Graveyard islands (#576): get down to the main mesh first - see BFG_ISLANDS. Runs after every resurrection too,
    // since the Lighthouse and Mine graveyards are the islands.
    if (bg->GetStatus() == STATUS_IN_PROGRESS && bot->IsAlive() && !bot->GetVehicle())
    {
        // Mid-jump (MotionMaster::MoveJump runs an EffectMovementGenerator): the arc passes through the island.
        if (bot->GetMotionMaster()->GetCurrentMovementGeneratorType() == EFFECT_MOTION_TYPE)
            return true;

        if (BfgIsland const* island = BfgIslandOf(bot))
        {
            if (bot->GetExactDist2d(island->takeoff[0], island->takeoff[1]) < BFG_TAKEOFF_RADIUS)
            {
                if (JumpTo(bg->GetMapId(), island->landing[0], island->landing[1], island->landing[2]))
                    SF_LOG_DEBUG("playerbots", "[BGBFG] jump-down bot=%s island=%s from=%.1f,%.1f,%.1f to=%.1f,%.1f,%.1f",
                                 bot->GetName().c_str(), island->name, bot->GetPositionX(), bot->GetPositionY(),
                                 bot->GetPositionZ(), island->landing[0], island->landing[1], island->landing[2]);
                return true;
            }

            MoveTo(bg->GetMapId(), island->takeoff[0], island->takeoff[1], island->takeoff[2]);
            return true;
        }
    }

    TeamId const team = BgTeamIdOf(bot);

    GuidVector closeObjects = *context->GetValue<GuidVector>("closest game objects");
    GuidVector closePlayers = *context->GetValue<GuidVector>("closest friendly players");
    float const flagRange = INTERACTION_DISTANCE;

    // Keep the bot still, dismounted and channelling while its own capture cast runs (atFlag).
    auto keepStationaryWhileCapturing = [&](CurrentSpellTypes spellType)
    {
        Spell* currentSpell = bot->GetCurrentSpell(spellType);
        if (!currentSpell || !IsBannerCaptureSpell(currentSpell->m_spellInfo))
            return false;

        GameObject* targetFlag = currentSpell->m_targets.GetGOTarget();
        if (!targetFlag || !targetFlag->isSpawned() || targetFlag->GetGoState() != GO_STATE_READY)
        {
            bot->CastStop();
            resetObjective();
            return false;
        }

        if (bot->IsMounted())
            bot->RemoveAurasByType(SPELL_AURA_MOUNTED);
        if (bot->IsInDisallowedMountForm())
            bot->RemoveAurasByType(SPELL_AURA_MOD_SHAPESHIFT);
        if (bot->isMoving())
            bot->StopMoving();
        return true;
    };

    if (keepStationaryWhileCapturing(CURRENT_CHANNELED_SPELL) || keepStationaryWhileCapturing(CURRENT_GENERIC_SPELL))
        return true;

    if (closeObjects.empty())
        return false;

    // Find a banner in reach that this team may actually click (see BfgCanClick).
    GameObject* targetFlag = nullptr;
    for (BotGuid const guid : closeObjects)
    {
        GameObject* go = botAI->GetGameObject(guid);
        if (!go)
            continue;

        if (std::find(vFlagsBFG.begin(), vFlagsBFG.end(), go->GetEntry()) == vFlagsBFG.end())
            continue;

        if (!go->isSpawned() || go->GetGoState() != GO_STATE_READY)
            continue;

        if (!bot->CanUseBattlegroundObject(go))
            continue;

        if (bot->GetDistance(go) > flagRange)
            continue;

        // Which node is this banner on? (all banner variants of a node share its position)
        int32 node = -1;
        for (uint8 i = 0; i < GILNEAS_BG_DYNAMIC_NODES_COUNT; ++i)
        {
            float const dx = go->GetPositionX() - GILNEAS_BG_NodePositions[i][0];
            float const dy = go->GetPositionY() - GILNEAS_BG_NodePositions[i][1];
            if (dx * dx + dy * dy < 25.0f)
            {
                node = i;
                break;
            }
        }
        if (node < 0)
            continue;

        if (!BfgCanClick(BfgNodeState(bg, uint8(node)), team))
            continue;

        // The plan says no: a raider with two defenders awake near the banner, an expert at a node the flip limiter holds back.
        if (!AbPlan::MayCapture(bg, bot, uint8(node)))
            continue;

        targetFlag = go;
        break;
    }

    if (!targetFlag)
        return false;

    // Engage-before-capture (AC atFlag AB behaviour): an enemy near the banner is fought first.
    if (Unit* enemyPlayer = AI_VALUE(Unit*, "enemy player target"))
    {
        if (enemyPlayer->IsAlive() && enemyPlayer->GetDistance(targetFlag) < flagRange * 2.0f)
        {
            context->GetValue<Unit*>("current target")->Set(enemyPlayer);
            return false;
        }
    }

    // A friend is already capturing: do not stack. A bot with a post (the defender, the capping group, a guard of the hold
    // state) stays where it is and only steps aside; any other picks another node (AC atFlag AB behaviour).
    for (BotGuid const guid : closePlayers)
    {
        Unit* pFriend = botAI->GetUnit(guid);
        if (!pFriend || pFriend->GetGUID() == bot->GetGUID())
            continue;

        Spell* friendSpell = pFriend->GetCurrentSpell(CURRENT_GENERIC_SPELL);
        if (!friendSpell)
            friendSpell = pFriend->GetCurrentSpell(CURRENT_CHANNELED_SPELL);

        if (friendSpell && IsBannerCaptureSpell(friendSpell->m_spellInfo))
        {
            if (bot->GetDistance2d(pFriend) < 3.0f)
            {
                float const angle = bot->GetAngle(pFriend);
                MoveTo(bot->GetMapId(), bot->GetPositionX() + 5.0f * std::cos(angle),
                       bot->GetPositionY() + 5.0f * std::sin(angle), bot->GetPositionZ());
            }

            bool holdsPost = AbPlan::StaysWhileFriendCasts(bg, bot);
            if (!holdsPost)
            {
                std::lock_guard<std::mutex> guard(s_bfgPostLock);
                holdsPost = s_bfgPost.count(bot->GetGUIDLow()) != 0;
            }
            if (holdsPost)
                return false;

            resetObjective();
            if (selectObjectiveBFG(bg))
                moveToObjective(true);
            return true;
        }
    }

    // Prevent capturing from inside the banner model (AC atFlag AB case).
    float const dist = bot->GetDistance(targetFlag);
    if (dist == 0.0f)
    {
        float const moveDist = bot->GetObjectSize() + targetFlag->GetObjectSize() + 0.1f;
        return MoveTo(bot->GetMapId(), targetFlag->GetPositionX() + (urand(0, 1) ? -moveDist : moveDist),
                      targetFlag->GetPositionY() + (urand(0, 1) ? -moveDist : moveDist), targetFlag->GetPositionZ());
    }

    // #754 (beyond AC): fight whoever is hitting the bot before starting the capture cast (BGTactics::CaptureThreat)
    if (Unit* threat = CaptureThreat(bot))
    {
        // #754 (owner 2026-10-08) keep the current target while it is alive and attacking the bot, else the threat;
        // engage it (#756: setting the value alone left a bot hit by a pet/guardian idle at the flag)
        Unit* const cur = AI_VALUE(Unit*, "current target");
        Unit* const fight = (cur && cur->IsAlive() && cur->GetVictim() == bot) ? cur : threat;
        if (!EngageThreat(bot, fight))
            context->GetValue<Unit*>("current target")->Set(fight);
        return false;
    }

    if (bot->IsMounted())
        bot->RemoveAurasByType(SPELL_AURA_MOUNTED);
    if (bot->IsInDisallowedMountForm())
        bot->RemoveAurasByType(SPELL_AURA_MOD_SHAPESHIFT);

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(BannerCaptureSpell(targetFlag));
    if (!spellInfo)
        return false;

    SF_LOG_DEBUG("playerbots", "[BGBFG] %s captures banner %u", bot->GetName().c_str(), targetFlag->GetEntry());

    Spell* spell = new Spell(bot, spellInfo, TRIGGERED_NONE);
    spell->m_targets.SetGOTarget(targetFlag);
    spell->prepare(&spell->m_targets);

    botAI->WaitForSpellCast(spell);

    resetObjective();
    return true;
}
