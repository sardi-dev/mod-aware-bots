/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// AC Ai/Raid/Mag/Util/MagHelpers.h (flattened into this folder). Checked against the core's boss_magtheridon.cpp and the
// client data on 2026-11-16. Ids, channeler spawns (90978 to 90982) and Manticron Cube spawns (43157 to 43161) match
// the world DB. What changed on 5.4.8:
// - Debris: the core has no Debris dynamic object (AC's 30630 is not in the client data). Its debris is an invisible
//   Burning Abyssal (17454) that carries the 5 s Debris warning aura 30632 and then casts the 8 yd Debris 30631, so the
//   warning auras are the debris positions. Blaze triggers are invisible abyssals too: both are not selectable, which
//   is how the real abyssals are told apart.
// - Timers: the core casts Blast Nova 60 s after Magtheridon is freed and every 60 s; Quake (a 7 s stun on him)
//   only holds a due Blast Nova until the stun ends, and the ceiling collapse delays nothing, so AC's fixed Quake and
//   collapse delays are dropped. The timer still restarts from every Blast Nova the tracker sees.
// - No spell or dynamic object script hooks: ShouldInterruptCast below is polled by PlayerbotAI (RaidInterrupts).
// - Spawns are found by table guid near their spawn point (the core gives instance spawns generated GUIDs).

#ifndef PLAYERBOTS_MAGHELPERS_H
#define PLAYERBOTS_MAGHELPERS_H

#include "Common.h"
#include "BotGuid.h"
#include "Position.h"
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Creature;
class GameObject;
class Map;
class Player;
class PlayerbotAI;
class Unit;

namespace MagHelpers
{

template <typename T, std::enable_if_t<std::is_enum_v<T>, int> = 0>
constexpr uint32 Id(T value)
{
    return static_cast<uint32>(value);
}

enum class MagSpells : uint32
{
    // Magtheridon
    SPELL_SHADOW_CAGE   = 30205,
    SPELL_SHADOW_GRASP  = 30410,
    SPELL_BLAST_NOVA    = 30616,
    SPELL_DEBRIS_WARNING = 30632,

    // Hunter: 34477 is on the hunter from the cast until the first attack, 35079 for 4 s after it.
    SPELL_MISDIRECTION_CAST = 34477,
    SPELL_MISDIRECTION  = 35079,
};

enum class MagNpcs : uint32
{
    NPC_BURNING_ABYSSAL = 17454,
};

enum class MagObjects : uint32
{
    GO_BLAZE            = 181832,
};

struct CubeInfo
{
    BotGuid guid;
    float x, y, z;
};

// Magtheridon's CombatReach is 12 yards.
inline constexpr uint32 MAG_MAP_ID                   = 544;
inline constexpr uint32 SOUTH_CHANNELER_DB_GUID      = 90978;
inline constexpr uint32 WEST_CHANNELER_DB_GUID       = 90979;
inline constexpr uint32 NORTHWEST_CHANNELER_DB_GUID  = 90980;
inline constexpr uint32 EAST_CHANNELER_DB_GUID       = 90982;
inline constexpr uint32 NORTHEAST_CHANNELER_DB_GUID  = 90981;

// Hold dps for this long after Magtheridon first becomes attackable.
inline constexpr float MAG_DPS_HOLD_MS = 8000; // 8s
// The core casts Blast Nova 60s after Magtheridon joins the fight (marked by the Shadow Cage aura
// coming off of him) and every 60s after that, held only while a Quake stuns him.
//
// This is the number of milliseconds since the previous Blast Nova that must pass before a cube
// clicker returns to go wait by a cube (or in the case of the first Blast Nova, since Magtheridon
// joined the fight).
inline constexpr uint32 BLAST_NOVA_INTERIM_MS = 48000; // 48s
// A queued action and the bot's next AI tick are both scheduled for the same moment, and either
// can be processed first. Pausing the AI for one extra world update (50ms) ensures the queued
// action occurs first.
inline constexpr uint32 ONE_WORLD_UPDATE_MS = 50;
inline constexpr float DEBRIS_HAZARD_RADIUS = 10.0f;
inline constexpr float CONFLAGRATION_HAZARD_RADIUS = 5.0f;
// At 30% HP, Magtheridon causes the ceiling to fall on players (knockdown); debris starts falling after it.
inline constexpr float CEILING_COLLAPSE_HP_PCT = 30.0f;

// The waiting position is SE-ish of Magtheridon.
inline Position const WAITING_FOR_MAGTHERIDON_POSITION = { -31.962f, -8.514f, -0.304f, 0.657f };
// The tank position is up against the Eastern wall.
inline Position const MAGTHERIDON_TANK_POSITION = { -6.147f, -37.812f, -0.411f, 0.0f };
inline Position const NW_CHANNELER_TANK_POSITION = { -11.764f, 30.818f, -0.411f, 0.0f };
inline Position const NE_CHANNELER_TANK_POSITION = { -12.490f, -26.211f, -0.411f, 0.0f };

extern std::unordered_map<uint32, uint32> magDpsWaitTimer;
extern std::unordered_map<uint32, uint32> blastNovaTimer;
extern std::unordered_map<uint32, bool> lastBlastNovaState;
extern std::unordered_set<uint32> ceilingCollapseApplied;
extern std::unordered_map<uint32, std::unordered_map<BotGuid, CubeInfo>> botToCubeAssignments;

extern std::vector<uint32> const MANTICRON_CUBE_DB_GUIDS;
// Get the positions of all Manticron Cubes by their database GUIDs.
std::vector<CubeInfo> GetAllCubeInfosByDbGuids(Map* map, std::vector<uint32> const& cubeDbGuids);
Creature* GetChanneler(Player* bot, uint32 dbGuid);
GuidVector FindBurningAbyssalGuids(Player* bot);
std::vector<Unit*> GetBurningAbyssals(PlayerbotAI* botAI);
bool IsMagtheridonActive(Unit* magtheridon);
bool IsCubeClicker(Player* bot);
// Debris begins falling only after the ceiling collapse at 30%.
bool IsBlastNovaCasting(Unit* magtheridon);
bool IsCeilingCollapsed(Player* bot);
std::vector<Position> FindDebrisPositions(Player* bot);
bool GetActiveDebrisPosition(PlayerbotAI* botAI, Position& debris);
bool IsPositionInActiveDebris(
    PlayerbotAI* botAI, float x, float y, float radius = DEBRIS_HAZARD_RADIUS);
std::vector<GameObject*> GetActiveConflagrations(PlayerbotAI* botAI);
bool IsPositionInConflagration(std::vector<GameObject*> const& blazes, float x, float y);
bool IsPositionInActiveConflagration(PlayerbotAI* botAI, float x, float y);
// AC drops a cast from its Blast Nova spell listener (cube clickers not yet holding a cube) and its debris dynamic
// object script (any bot within the hazard radius); this is the same test, polled while the bot casts.
bool ShouldInterruptCast(Player* bot);

}

#endif
