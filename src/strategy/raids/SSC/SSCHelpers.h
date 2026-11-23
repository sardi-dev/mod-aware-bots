/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// AC Ai/Raid/SSC/Util/SSCHelpers.h (flattened into this folder). Checked against the core's SerpentShrine scripts and
// the client data on 2026-11-16; creature and object entries, the generator spawns (47482 to 47485) and the boss
// positions match the world DB. What changed on 5.4.8 / this core:
// - Hydross: no Corruption aura; nature phase is his corrupted display, and he switches 18 yd from his home point.
// - Lurker: Spout is a 20 s rotate movement with no wind-up and no spin auras, so the spin is read from his facing;
//   submerged is his Submerge aura.
// - Leotheras: Banish 37833; no Metamorphosis aura, the demon phase is his demon display; the Inner Demon keeps its
//   victim as AI data.
// - Karathress: Cyclone aura 29538 (from the Cyclone add).
// - Vashj: Tainted Elementals do not attack, so they are found in the grid; holding a Core is a bag check (no
//   Paralyze); a used generator is read from the instance data; loot and item use follow the 5.4.8 handlers.
// - Spells: Faerie Fire 770, Vigilance 114030, Searing Pain and the old traps and aspects replaced or dropped.
// - Cheats removed: the free Fear Ward aura on Strider tanks (tanks use their own fear breaker) and the teleport
//   down to the floor (the bot falls instead).
// - No spell listener hooks: ShouldInterruptCast below is polled by PlayerbotAI (RaidInterrupts).

#ifndef PLAYERBOTS_SSCHELPERS_H
#define PLAYERBOTS_SSCHELPERS_H

#include "Common.h"
#include "ObjectDefines.h"
#include "BotGuid.h"
#include "Position.h"
#include <array>
#include <initializer_list>
#include <limits>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

class Creature;
class GameObject;
class Player;
class PlayerbotAI;
class Unit;

namespace SscHelpers
{

// SkyFire's Position distance and angle helpers take a pointer; AC's take a reference. Call sites wrap their one
// argument in PosPtr so both an object and a position work unchanged.
inline Position const* PosPtr(Position const& position) { return &position; }
inline Position const* PosPtr(Position const* position) { return position; }

template <typename T, std::enable_if_t<std::is_enum_v<T>, int> = 0>
constexpr uint32 Id(T value)
{
    return static_cast<uint32>(value);
}

enum class SscSpells : uint32
{
    // Trash
    SPELL_TOXIC_POOL             = 38718,

    // Hydross the Unstable <Duke of Currents>
    SPELL_MARK_OF_HYDROSS_10     = 38215,
    SPELL_MARK_OF_HYDROSS_25     = 38216,
    SPELL_MARK_OF_HYDROSS_50     = 38217,
    SPELL_MARK_OF_HYDROSS_100    = 38218,
    SPELL_MARK_OF_HYDROSS_250    = 38231,
    SPELL_MARK_OF_HYDROSS_500    = 40584,
    SPELL_MARK_OF_CORRUPTION_10  = 38219,
    SPELL_MARK_OF_CORRUPTION_25  = 38220,
    SPELL_MARK_OF_CORRUPTION_50  = 38221,
    SPELL_MARK_OF_CORRUPTION_100 = 38222,
    SPELL_MARK_OF_CORRUPTION_250 = 38230,
    SPELL_MARK_OF_CORRUPTION_500 = 40583,

    // The Lurker Below
    SPELL_LURKER_SUBMERGE        = 37550,

    // Leotheras the Blind
    // The core's Spellbinders hold Leotheras with Banish 37833 (AC: 37546).
    SPELL_LEOTHERAS_BANISHED     = 37833,
    SPELL_LEOTHERAS_WHIRLWIND    = 37640,
    SPELL_CHAOS_BLAST            = 37675,
    SPELL_INSIDIOUS_WHISPER      = 37676,

    // Fathom-Lord Karathress
    // The core's Caribdis summons a Cyclone (22104) whose aura 29538 tosses players within 5 yd for 5 s (AC: 38517).
    SPELL_CYCLONE                = 29538,

    // Lady Vashj <Coilfang Matron>
    SPELL_FEAR_WARD              =  6346,
    SPELL_BERSERKER_RAGE         = 18499,
    SPELL_LICHBORNE              = 49039,
    SPELL_MAGIC_BARRIER          = 38112,
    SPELL_STATIC_CHARGE          = 38280,
    SPELL_ENTANGLE               = 38316,
    SPELL_TOXIC_SPORES           = 38575,

    // Druid
    // 5.4.8 has one Faerie Fire for every form (AC: Faerie Fire (Feral) 16857).
    SPELL_FAERIE_FIRE            =   770,
    SPELL_TREE_OF_LIFE           = 33891,
    SPELL_DRUID_BERSERK          = 50334,

    // Hunter
    SPELL_MISDIRECTION_CAST      = 34477,
    SPELL_MISDIRECTION           = 35079, // the aura on the hunter

    // Paladin
    SPELL_DIVINE_SHIELD          =   642,
    SPELL_AVENGING_WRATH         = 31884,

    // Priest
    SPELL_DISPERSION             = 47585,

    // Rogue
    SPELL_CLOAK_OF_SHADOWS       = 31224,

    // Shaman
    SPELL_GROUNDING_TOTEM_EFFECT =  8178,

    // Warrior
    // 5.4.8 Vigilance (AC: 50720)
    SPELL_VIGILANCE              = 114030,
};

enum class SscNpcs : uint32
{
    // Trash
    NPC_WATER_ELEMENTAL_TOTEM    = 22236,

    // The Lurker Below
    NPC_THE_LURKER_BELOW         = 21217,
    NPC_COILFANG_GUARDIAN        = 21873,

    // Leotheras the Blind
    NPC_LEOTHERAS_THE_BLIND      = 21215,
    NPC_INNER_DEMON              = 21857,
    NPC_SHADOW_OF_LEOTHERAS      = 21875,

    // Morogrim Tidewalker
    NPC_TIDEWALKER_LURKER        = 21920,

    // Fathom-Lord Karathress
    NPC_FATHOM_LORD_KARATHRESS   = 21214,
    NPC_FATHOM_GUARD_CARIBDIS    = 21964,
    NPC_FATHOM_GUARD_TIDALVESS   = 21965,
    NPC_FATHOM_GUARD_SHARKKIS    = 21966,
    NPC_SPITFIRE_TOTEM           = 22091,
    NPC_FATHOM_LURKER            = 22119,
    NPC_FATHOM_SPOREBAT          = 22120,

    // Lady Vashj <Coilfang Matron>
    NPC_ENCHANTED_ELEMENTAL      = 21958,
    NPC_TAINTED_ELEMENTAL        = 22009,
    NPC_COILFANG_ELITE           = 22055,
    NPC_COILFANG_STRIDER         = 22056,
    NPC_TOXIC_SPOREBAT           = 22140,

    // Pets that PetAI keeps at spell range while they have mana to cast
    NPC_IMP                      =   416,
    NPC_WATER_ELEMENTAL          =   510,
    NPC_WATER_ELEMENTAL_PERM     = 37994,
};

enum class SscItems : uint32
{
    // Lady Vashj <Coilfang Matron>
    ITEM_TAINTED_CORE            = 31088,
};

// General

inline constexpr uint32 SSC_MAP_ID = 548;
inline constexpr uint32 HAZARD_CACHE_INTERVAL_MS = 200;
inline constexpr float PATH_STEP_DISTANCE = 3.5f;
inline constexpr float PATH_BACKWARD_STEP_DISTANCE = 2.25f;

bool ClearSscTargetIcon(Player* bot, uint8 iconId, std::initializer_list<uint32> entries);
bool MisdirectTargetToTank(PlayerbotAI* botAI, Unit* target, Player* tank);
bool CastTankTaunt(PlayerbotAI* botAI, Unit* target);
bool FindHazardEscapeStep(
    Player* bot, Position const& hazard, float moveDist, float& stepX, float& stepY, float& stepZ);
bool IsDryGround(Player* bot, float x, float y);
bool GetPathStepTowardPoint(
    Player* bot, Position const& destination, float stopDistance, float stepDistance,
    float& stepX, float& stepY);
bool GetPathStepTowardUnit(
    Player* bot, Unit* target, float stopDistance, float& stepX, float& stepY);
bool GetRangedArcAngle(Player* bot, float arcCenter, float arcSpan, float& angle);
std::vector<Unit*> GetOtherLivingGroupMembers(Player* bot);

// Trash

// 25y radius + ~2y player CombatReach
inline constexpr float TOXIC_POOL_HAZARD_RADIUS = 27.0f;
inline constexpr float TOXIC_POOL_HOLDING_RADIUS = TOXIC_POOL_HAZARD_RADIUS + 5.0f;
inline constexpr float TOXIC_POOL_SEARCH_RADIUS = TOXIC_POOL_HOLDING_RADIUS + 2.0f;

bool GetToxicPoolPosition(PlayerbotAI* botAI, Position& toxicPool);
bool IsNearToxicPool(PlayerbotAI* botAI, float radius);

inline constexpr float WATER_ELEMENTAL_TOTEM_SEARCH_DISTANCE = 20.0f;
inline constexpr uint32 WATER_ELEMENTAL_TOTEM_CACHE_INTERVAL_MS = 1000;

BotGuid FindWaterElementalTotemGuid(Player* bot);
Creature* GetWaterElementalTotem(PlayerbotAI* botAI);
bool IsSkullOnWaterElementalTotem(PlayerbotAI* botAI);

// Hydross the Unstable <Duke of Currents>

enum class HydrossDpsHoldWindow : uint8
{
    None,
    BeforePhaseChange,
    AfterPhaseChange,
};

// Ranged spread this far apart in frost phase to mitigate Water Tomb.
inline constexpr float HYDROSS_FROST_RANGED_SPREAD_DISTANCE = 5.0f;
// The core switches Hydross to nature when he is more than 18 yd (centre to centre, 2D) from his
// home point and back to frost inside it (boss_hydross_the_unstable.cpp SWITCH_RADIUS, HYDROSS_X/Y).
inline constexpr float HYDROSS_CLEANSING_FIELD_RADIUS = 18.0f;
// boss_hydross_the_unstable.cpp MODEL_CORRUPT
inline constexpr uint32 HYDROSS_CORRUPT_DISPLAY_ID = 20609;
// The incoming phase's tank waits this far short of the field's edge, on its own side.
inline constexpr float HYDROSS_HANDOFF_SHORT_DISTANCE = 8.0f;

inline Position const HYDROSS_FROST_TANK_POSITION = { -235.653f, -354.823f, -0.828f, 0.0f };
inline Position const HYDROSS_NATURE_TANK_POSITION = { -224.721f, -324.755f, -3.682f, 0.0f };
inline Position const HYDROSS_CLEANSING_FIELD_CENTER = { -239.439f, -363.481f, -0.745f, 0.0f };

// The main tank holds Hydross in frost phase, the first assist tank in nature phase. Every other
// tank is an add tank and picks up the Elementals that spawn upon phase changes.
bool IsHydrossFrostTank(Player* bot);
bool IsHydrossNatureTank(Player* bot);
bool IsHydrossPhaseTank(Player* bot);
bool IsHydrossAddTank(Player* bot);
bool IsHydrossInFrostPhase(Unit* hydross);
bool IsHydrossInNaturePhase(Unit* hydross);
HydrossDpsHoldWindow GetHydrossDpsHoldWindow(Unit* hydross);
Position GetHydrossHandoffPosition(bool frostTank);
bool CanHydrossMarkBeMaxed(Unit* hydross);
bool HasMarkOfHydrossAt100Percent(Player* player);
bool HasNoMarkOfHydross(Player* bot);
bool HasMarkOfCorruptionAt100Percent(Player* player);
bool HasNoMarkOfCorruption(Player* bot);

// The Lurker Below

inline constexpr float LURKER_WHIRL_RADIUS = 25.0f;
inline constexpr float LURKER_RANGED_SAFE_DISTANCE = LURKER_WHIRL_RADIUS + 2.0f;
// Melee returning to Lurker from an islet stop this far from Lurker, on the walkway.
inline constexpr float LURKER_WALKWAY_RADIUS = 21.0f;
// A melee bot farther than this from Lurker is out on an islet. The Ambushers' islets are
// 45-55 yd from Lurker, and the Guardians spawn on land 25-30 yd from Lurker.
inline constexpr float LURKER_ISLET_DISTANCE = 40.0f;

// Spout: each bot runs on its own radius 20-22y from Lurker (main tank 20y), close to Lurker but
// mostly on dry land, and spread so it looks less artificial. Bots in the 120 degree cone behind Lurker
// are safe and wait out the wind-up until the spin direction is known.
inline constexpr float LURKER_SPOUT_RUN_RADIUS_MIN = 20.0f;
inline constexpr float LURKER_SPOUT_RUN_RADIUS_MAX = 22.0f;
inline constexpr float LURKER_SPOUT_RUN_ARC_HALF_WIDTH = static_cast<float>(M_PI) / 3.0f;
inline constexpr float LURKER_SPOUT_RUN_STEP = 3.5f;
inline constexpr float LURKER_SPOUT_RUN_RADIAL_DEADZONE = 2.0f;
// A bot may run this far past directly behind Lurker, in the spin direction, before it stops.
// This is to prevent the very intelligent bots from lapping Lurker and getting blasted.
inline constexpr float LURKER_SPOUT_RUN_OVERTAKE_MARGIN = static_cast<float>(M_PI) / 6.0f;

inline constexpr size_t LURKER_GUARDIAN_TANK_COUNT = 3;
using LurkerGuardianTankAssignments = std::array<BotGuid, LURKER_GUARDIAN_TANK_COUNT>;
inline constexpr uint32 LURKER_GUARDIAN_CACHE_INTERVAL_MS = 200;
inline constexpr uint32 LURKER_GUARDIAN_TANK_CACHE_INTERVAL_MS = 1000;
inline constexpr float LURKER_GUARDIAN_SEARCH_RADIUS = 100.0f;

// In front of a pillar to limit the distance that the main tank gets knocked back by Whirl.
inline Position const LURKER_MAIN_TANK_POSITION = { 23.706f, -406.038f, -19.686f, 0.0f };

// Lurker is passive during Spout: the core turns him once in 20 s with a rotate movement, either
// direction, with no wind-up (AC: a 3 s wind-up and a 16 s spin aura).
bool IsLurkerSpouting(Unit* lurker);
bool IsLurkerSubmerged(Unit* lurker);
bool IsLurkerSurfacedAndCalm(Unit* lurker);
int8 GetLurkerSpoutSpin(Unit* lurker);
bool DoesPathRoundLurker(Player* bot, Unit* lurker, float x, float y, float z, int8 direction);
float GetArrivingPathLength(Player* bot, float x, float y, float z, float tolerance);
GuidVector FindLurkerGuardianGuids(Player* bot);
std::vector<Unit*> GetLurkerGuardians(PlayerbotAI* botAI);
GuidVector FindLurkerGuardianTankGuids(Player* bot);
int8 GetLurkerGuardianTankIndex(PlayerbotAI* botAI);
bool ShouldGoToLurkerWalkway(Player* bot, Unit* lurker, Unit* target);

// Leotheras the Blind

inline constexpr float LEOTHERAS_SEARCH_DISTANCE = 100.0f;
inline constexpr uint32 LEOTHERAS_CACHE_INTERVAL_MS = 200;
inline constexpr uint32 LEOTHERAS_HUMANOID_DPS_WAIT_MS = 3 * IN_MILLISECONDS;
inline constexpr uint32 LEOTHERAS_DEMON_DPS_WAIT_MS = 10 * IN_MILLISECONDS;
inline constexpr uint32 LEOTHERAS_FINAL_DPS_WAIT_MS = 5 * IN_MILLISECONDS;
inline constexpr uint32 LEOTHERAS_WHIRLWIND_DPS_WAIT_MS = 3 * IN_MILLISECONDS;
// Ranged stay 15y away during Humanoid phase until Whirlwind begins, at which point they move to
// 25y away. Whirlwind has a 10y radius only, but Leo moves very fast when Whirlwinding.
inline constexpr float LEOTHERAS_RANGED_SAFE_DISTANCE = 15.0f;
inline constexpr float LEOTHERAS_WHIRLWIND_SAFE_DISTANCE = 25.0f;
inline constexpr float LEOTHERAS_RANGED_SPREAD_DISTANCE = 4.0f;
// Chaos Blast deals splash damage within 8y of the target.
inline constexpr float LEOTHERAS_CHAOS_BLAST_SAFE_DISTANCE = 10.0f;
inline constexpr float LEOTHERAS_SHADOW_SEPARATION_DISTANCE = 20.0f;

// boss_leotheras_the_blind.cpp MODEL_DEMON
inline constexpr uint32 LEOTHERAS_DEMON_DISPLAY_ID = 20125;
// The core's Inner Demon is summoned by Leotheras and keeps its victim as AI data (INNER_DEMON_VICTIM).
inline constexpr int32 INNER_DEMON_VICTIM_DATA = 1;

// The world DB spawn of Leotheras on this core (AC: 376.5, -438.6).
inline Position const LEOTHERAS_SPAWN_POSITION = { 370.1f, -430.5f, 29.5f, 0.0f };

BotGuid FindLeotherasGuid(Player* bot);
BotGuid FindShadowOfLeotherasGuid(Player* bot);
Creature* GetLeotheras(PlayerbotAI* botAI);
bool IsSpellbinderPhase(Unit* leotheras);
Creature* GetActiveLeotherasHumanoid(PlayerbotAI* botAI);
bool IsLeotherasHumanoidPhase(PlayerbotAI* botAI);
Creature* GetLeotherasDemon(PlayerbotAI* botAI);
bool IsLeotherasDemonPhase(PlayerbotAI* botAI);
Creature* GetShadowOfLeotheras(PlayerbotAI* botAI);
bool IsLeotherasFinalPhase(PlayerbotAI* botAI);
Creature* GetLeotherasDemonOrShadow(PlayerbotAI* botAI);
// (1) First priority is an assistant Warlock (real player or bot).
// (2) If there is no assistant Warlock, then look for any Warlock bot.
Player* GetLeotherasWarlockTank(Player* bot);
bool IsLeotherasWarlockTank(Player* bot);
bool IsLeotherasChannelingWhirlwind(Unit* leotheras);
Creature* GetLeotherasHumanoidToAvoid(PlayerbotAI* botAI);
Unit* GetDemonTargetToAvoid(Player* bot, Unit* demon);
Unit* GetChaosBlastTargetToAvoid(PlayerbotAI* botAI);
Unit* GetShadowTargetToSeparateFrom(PlayerbotAI* botAI);
bool IsLeotherasDpsHoldActive(PlayerbotAI* botAI, Unit* leotheras);
bool HasTooManyChaosBlastStacks(Player* bot);
bool HasInnerDemon(Player* bot);
Creature* GetPersonalInnerDemon(PlayerbotAI* botAI);

// Fathom-Lord Karathress

struct KarathressCouncilAssignment
{
    char const* name;
    int8 assistTankIndex; // -1 for the main tank
};

inline constexpr std::array KARATHRESS_COUNCIL = {
    KarathressCouncilAssignment{ "fathom-lord karathress", -1 },
    KarathressCouncilAssignment{ "fathom-guard caribdis", 0 },
    KarathressCouncilAssignment{ "fathom-guard sharkkis", 1 },
    KarathressCouncilAssignment{ "fathom-guard tidalvess", 2 },
};

// Karathress gains Blessing of the Tides if he hits 75% HP with any Fathom-Guard still alive, so if
// ranged fail to kill Caribdis before he gets to this percent health, melee needs to stop dps.
inline constexpr float KARATHRESS_BLESSING_HOLD_HEALTH_PCT = 85.0f;
// The widest tank AoE is Death and Decay at 10 yd.
inline constexpr float KARATHRESS_AOE_THREAT_CLEARANCE = 15.0f;
inline constexpr uint32 KARATHRESS_DPS_WAIT_MS = 12 * IN_MILLISECONDS;

inline constexpr float SPITFIRE_TOTEM_SEARCH_DISTANCE = 75.0f;
// Ranged attack Spitfire Totems only when this close. This will exclude some ranged bots on
// Caribdis, which is the point, as ranged needs to maintain their spread due to Cyclones.
inline constexpr float SPITFIRE_TOTEM_RANGED_ATTACK_DISTANCE = 30.0f;
inline constexpr uint32 SPITFIRE_TOTEM_CACHE_INTERVAL_MS = 200;

inline constexpr float CARIBDIS_HEALER_DISTANCE = 32.0f;
inline constexpr float CARIBDIS_HEALER_MAX_DISTANCE = 35.0f;
// Tidal Surge's range is 10 yards.
inline constexpr float CARIBDIS_TIDAL_SURGE_SAFE_DISTANCE = 12.0f;
// Bots walk to Caribdis until she's in sight and then stop at no closer than this distance away.
inline constexpr float CARIBDIS_APPROACH_STOP_DISTANCE = 5.0f;
// A Cyclone spawns on a random player within 45y of Caribdis and picks up all players within 4y.
inline constexpr float CARIBDIS_CYCLONE_SUMMON_RANGE = 45.0f;
inline constexpr float CARIBDIS_RANGED_SPREAD_DISTANCE = 4.0f;
// A bot more than 1y above the ground is treated as being in a Cyclone. One toss lifts players
// about 1.5y, and a bot standing on the ground is well under 1 yd from the navmesh.
inline constexpr float CARIBDIS_CYCLONE_DROP_HEIGHT = 1.0f;

inline Position const KARATHRESS_TANK_POSITION = { 474.403f, -531.118f, -7.548f, 0.0f };
inline Position const CARIBDIS_TANK_POSITION = { 464.462f, -475.820f, -13.158f, 0.0f };
inline Position const SHARKKIS_TANK_POSITION = { 508.057f, -541.109f, -10.133f, 0.0f };
inline Position const TIDALVESS_TANK_POSITION = { 521.833f, -503.329f, -13.158f, 0.0f };

BotGuid FindSpitfireTotemGuid(Player* bot);
Creature* GetSpitfireTotem(PlayerbotAI* botAI);
bool ShouldAttackSpitfireTotem(Player* bot, Unit* totem);
// Sharkkis's tank picks up his pets too.
Unit* GetSharkkisTankTarget(PlayerbotAI* botAI);
Unit* GetSharkkisPet(Player* bot);
bool IsHoldingAnotherTanksCouncilMember(PlayerbotAI* botAI, Unit* ownTarget);
bool IsAnotherCouncilMemberWithin(PlayerbotAI* botAI, float range);

// Morogrim Tidewalker

inline constexpr float TIDEWALKER_PHASE_2_HEALTH_PCT = 25.0f;
// The move to the corner starts a little early so it is done before the first Globules arrive.
inline constexpr float TIDEWALKER_PHASE_2_MOVE_HEALTH_PCT = TIDEWALKER_PHASE_2_HEALTH_PCT + 2.0f;
// Non-tanks farther than this range in phase 1, such as one sent out by Watery Grave, are brought
// back. Every grave is inside this range, so a healer going to a grave victim isn't pulled back.
inline constexpr float TIDEWALKER_MAX_DISTANCE_FROM_BOSS = 45.0f;
inline constexpr float TIDEWALKER_RANGED_BEHIND_DISTANCE = 5.0f;
// Hunters can't shoot inside Tidewalker's melee range (about 8.8 yd center to center), so they get
// a farther stack distance than everybody else.
inline constexpr float TIDEWALKER_HUNTER_BEHIND_DISTANCE = 13.0f;
inline constexpr float TIDEWALKER_RANGED_STACK_RADIUS = 3.0f;
inline constexpr float TIDEWALKER_MURLOC_MAX_TARGET_DISTANCE = 50.0f;

inline Position const TIDEWALKER_PHASE_1_TANK_POSITION = { 410.925f, -741.916f, -7.146f, 0.0f };
inline Position const TIDEWALKER_PHASE_2_TANK_POSITION = { 446.571f, -767.155f, -7.144f, 0.0f };

// The stack point is directly across Tidewalker from his tank to keep it stable.
Position GetTidewalkerStackPoint(Player const& bot, Unit const& tidewalker);

// Lady Vashj <Coilfang Matron>

// Vashj: General

// The dais is a regular dodecagon, so the apothem for each side is identical.
inline constexpr float VASHJ_DAIS_APOTHEM = 57.05f;
inline constexpr float VASHJ_STAIR_BASE_APOTHEM = 90.19f;
// This is the closest that Vashj's tank will come to the North rock (to keep Vashj from evading).
inline constexpr float VASHJ_NORTH_ROCK_CLEARANCE = 5.0f;
// The minimum distance from the North rock for bots other than Vashj's tank.
inline constexpr float VASHJ_STANDING_ROCK_CLEARANCE = 2.0f;
// Vashj follows her tank, so keeping the tank on the dais keeps her on it, but this is some slack.
inline constexpr float VASHJ_DAIS_MARGIN = 1.0f;
// Bots this far above ground in P3 fall back down (they can airwalk after chasing Sporebats).
inline constexpr float VASHJ_ABOVE_GROUND_HEIGHT = 1.5f;

inline Position const VASHJ_PLATFORM_CENTER_POSITION = { 29.634f, -923.541f, 42.902f, 0.0f };
// The large rock that cuts into the North edge of the dais, measured from the stair base, up
// across the dais, and back down.
inline std::array const VASHJ_NORTH_ROCK = {
    Position{ 119.256f, -910.155f, 22.314f },
    Position{  85.970f, -893.277f, 38.525f },
    Position{  73.946f, -897.039f, 41.173f },
    Position{  68.584f, -917.259f, 41.333f },
    Position{  77.624f, -925.960f, 41.165f },
    Position{ 120.362f, -931.205f, 22.520f },
};

int8 GetLadyVashjPhase(Unit* vashj);
bool IsOnVashjDais(float x, float y, float margin, float rockClearance);

// Vashj: Static Charge, Entangle and Shock Blast

// Static Charge's range is 10y from the target's center.
inline constexpr float VASHJ_STATIC_CHARGE_SAFE_DISTANCE = 11.0f;
// Vashj's Entangle has a 15y range, so ranged bots stay at least this far back from her.
inline constexpr float VASHJ_PHASE_3_RANGED_DISTANCE = 15.0f;
inline constexpr float VASHJ_PHASE_3_RANGED_SPREAD_DISTANCE = 4.0f;

bool HasVashjStaticCharge(Player* player);
bool IsVashjPhase3RangedTooClose(Player* bot, Unit* vashj);
bool ShouldAvoidVashjStaticCharge(Player* bot, Unit* vashj);
bool IsInVashjStaticChargeReach(Player* bot, Unit* vashj);
Player* GetVashjHandOfFreedomTarget(PlayerbotAI* botAI, Unit* vashj);
Player* GetVashjGroundingShaman(Player* bot);
// Vashj's Shock Blast (38509) stuns her tank for 5s. It can be absorbed by Grounding Totem Effect,
// but it is party only, so the totem must be placed by a Shaman in the main tank's subgroup.
Player* FindVashjGroundingShaman(Player* bot);

// Vashj: Toxic Spores

// Toxic Spore pools have a range of 5y + the target's combat reach.
inline constexpr float TOXIC_SPORES_HIT_RADIUS = 6.5f;
inline constexpr float TOXIC_SPORES_AVOID_RADIUS = 7.5f;
// Vashj's tank has a greater avoidance radius to keep melee dps on the far side of Vashj clear
// as well. She is ~3.5y from the tank and melee ~3.75y past her (7.25y total + 0.25y slack).
inline constexpr float TOXIC_SPORES_TANK_AVOID_RADIUS = TOXIC_SPORES_AVOID_RADIUS + 7.5f;
inline constexpr float TOXIC_SPORES_SEARCH_RADIUS = 50.0f;
// Melee dps that are this close to a toxic pool have their standard movement overriden by the
// melee spore action to keep them from running through pools to reach Vashj.
inline constexpr float TOXIC_SPORES_MELEE_CONTROL_RADIUS = 10.0f;

std::vector<Position> const& GetToxicSporePositions(PlayerbotAI* botAI);
bool FindVashjDaisStepAwayFromPositions(
    Player* bot, std::vector<Position> const& positions, Unit* facing, float rockClearance,
    float& stepX, float& stepY, float& stepZ, bool& backwards,
    std::vector<Position> const* spores = nullptr, float sporeRadius = TOXIC_SPORES_AVOID_RADIUS);
bool FindVashjDaisStepAwayFromUnits(
    Player* bot, std::vector<Unit*> const& units, Unit* facing, float rockClearance,
    float& stepX, float& stepY, float& stepZ, bool& backwards,
    std::vector<Position> const* spores = nullptr, float sporeRadius = TOXIC_SPORES_AVOID_RADIUS);
bool FindVashjTankBreakoutSpot(
    Player* bot, std::vector<Position> const& spores, Position& spot);
bool IsVashjRingMelee(Player* bot, Unit* vashj);
bool IsNearToxicSpores(PlayerbotAI* botAI, float radius);
bool IsInMeleeRangeClearOfSpores(
    Player* bot, Unit* target, std::vector<Position> const& spores, float radius);
// If a bot has Divine Shield or Dispersion, it may walk through pools (but not stop in them).
bool CanWalkThroughToxicSpores(Player* bot);
bool GetMeleeRingStepClearOfSpores(
    Player* bot, Unit* target, std::vector<Position> const& spores, float radius, float& stepX,
    float& stepY, float& stepZ);
bool GetStepOutOfNearestSpore(
    Player* bot, std::vector<Position> const& spores, float radius, float& stepX, float& stepY,
    float& stepZ);
bool GetVashjReachBlockedBySpores(PlayerbotAI* botAI, Unit*& target, float& range);
bool GetStepToCastRangeAroundSpores(
    Player* bot, Unit* target, float castRange, std::vector<Position> const& spores, float& stepX,
    float& stepY, float& stepZ);

// Vashj: Phase 2 Ranged Stations

inline constexpr size_t VASHJ_STATION_RANGED_SLOTS = 3;
inline constexpr int8 VASHJ_STATION_HEALER_SLOT = 3;
inline constexpr float VASHJ_STATION_ARRIVAL_DISTANCE = 2.0f;

struct VashjStation
{
    std::array<Position, VASHJ_STATION_RANGED_SLOTS> ranged;
    Position healer;
};

// A bot's station and its slot there: 0-2 for ranged dps, VASHJ_STATION_HEALER_SLOT for the healer.
struct VashjStationSlot
{
    int8 station = -1;
    int8 slot = -1;
};

inline std::array const VASHJ_STATIONS = {
    // Slots at -156, -178, and -134 degrees
    VashjStation{
        {
            Position{ -17.87f, -944.69f, 41.30f },
            Position{ -22.33f, -925.36f, 41.30f },
            Position{  -6.49f, -960.95f, 41.30f },
        },
        Position{ -6.91f, -939.81f, 41.65f },
    },
    // Slots at 116, 102, and 130 degrees
    VashjStation{
        {
            Position{   6.84f, -876.80f, 41.30f },
            Position{  18.82f, -872.68f, 41.30f },
            Position{  -3.79f, -883.71f, 41.30f },
        },
        Position{ 12.10f, -887.59f, 41.65f },
    },
    // Slots at -68, -80, and -56 degrees
    VashjStation{
        {
            Position{  49.11f, -971.75f, 41.30f },
            Position{  38.66f, -974.75f, 41.30f },
            Position{  58.71f, -966.65f, 41.30f },
        },
        Position{ 44.62f, -960.63f, 41.65f },
    },
    // Slots at 43, 37, and 49 degrees
    VashjStation{
        {
            Position{  67.66f, -888.08f, 41.30f },
            Position{  71.16f, -892.25f, 41.30f },
            Position{  63.75f, -884.30f, 41.30f },
        },
        Position{ 60.35f, -894.90f, 41.65f },
    },
};

inline constexpr size_t VASHJ_STATION_COUNT = std::tuple_size_v<decltype(VASHJ_STATIONS)>;
// The Northeast station is filled first because it has the farthest potential run to a Tainted
// Elemental (the spawn point just east of the North rock), so it is the first to get a 3rd dps.
inline constexpr std::array VASHJ_STATION_FILL_ORDER = {
    int8{ 2 }, int8{ 0 }, int8{ 1 }, int8{ 3 },
};
static_assert(VASHJ_STATION_FILL_ORDER.size() == VASHJ_STATION_COUNT);
using VashjStationHolders =
    std::array<std::array<BotGuid, VASHJ_STATION_RANGED_SLOTS + 1>, VASHJ_STATION_COUNT>;

std::vector<VashjStationSlot> GetVashjStationFillOrder();
bool IsVashjGeneratorUsed(GameObject* generator);
bool IsLiveVashjStationHolder(Player* bot, BotGuid guid);
bool HasVashjStationVacancy(Player* bot);
VashjStationSlot GetVashjStationSlot(Player* bot);
Position const* GetVashjStationPositionToReturnTo(Player* bot, Unit* currentTarget);
// One subtlety: the Tainted Elemental spawn spot just east of the North rock is closest to the NW
// station in a straight line, but NW is blocked by the rock, so the NE station gets it instead.
int8 GetNearestVashjStation(Unit* unit);

// Vashj: Adds and Target Priority

struct VashjAddGuids
{
    GuidVector enchanted;
    GuidVector elites;
    GuidVector striders;
    GuidVector sporebats;
};

enum class VashjTarget : uint8
{
    TaintedElemental,
    CoilfangStrider,
    CoilfangElite,
    EnchantedElemental,
    ToxicSporebat,
    LadyVashj,
};

struct VashjTargetTier
{
    VashjTarget target;
    float maxDistanceFromVashj = std::numeric_limits<float>::max();
};

struct VashjTargetFacts
{
    Unit* vashj = nullptr;
    Unit* tainted = nullptr;
    int8 phase = -1;
    // Measured from the bot, and in phase 2 also from the platform centre, so bots don't chase
    // adds down the stairs.
    float maxPursueRange = 0.0f;
    float maxSearchRange = 0.0f;
    float spellRange = 0.0f;
    // Phase 2 ranged dps at stations shoot only what's in range of their slot, except the station
    // sent after a Tainted Elemental.
    bool holdsStationSlot = false;
    // Phase 2: everyone but tanks leaves an Elite or Strider alone until a tank has it, so nobody
    // pulls one onto a station.
    bool waitForTank = false;
    // Tanks: one per Elite or Strider, so the others stay free for the next ones. A new one goes
    // to the nearest free tank.
    bool oneTankEach = false;
    // The living Striders. Melee dps attack targets only outside their Panic radius.
    std::vector<Unit*> panicStriders;
};

inline constexpr uint32 VASHJ_ADDS_CACHE_INTERVAL_MS = 200;
// Panic (38258) fears every player within 11y of a Strider, center to center.
inline constexpr float VASHJ_STRIDER_PANIC_RADIUS = 11.0f;
inline constexpr float VASHJ_STRIDER_STEP_IN_DISTANCE = 50.0f;
inline constexpr float VASHJ_ENCHANTED_NEAR_HER_DISTANCE = 20.0f;
inline constexpr float VASHJ_IDLE_TANK_DISTANCE = 10.0f;
inline constexpr float VASHJ_ADD_TANK_ARRIVAL_DISTANCE = 3.0f;
inline constexpr float VASHJ_PHASE_3_STRIDER_DISTANCE_FROM_VASHJ = 28.0f;

// Target tiers by phase and role, best first (GetVashjTargetTiers)
inline std::vector<VashjTargetTier> const VASHJ_PHASE_2_STATION_RANGED_TIERS = {
    VashjTargetTier{ VashjTarget::CoilfangStrider },
    VashjTargetTier{ VashjTarget::EnchantedElemental },
    VashjTargetTier{ VashjTarget::CoilfangElite },
};
inline std::vector<VashjTargetTier> const VASHJ_PHASE_2_TAINTED_KILLER_TIERS = {
    VashjTargetTier{ VashjTarget::TaintedElemental },
    VashjTargetTier{ VashjTarget::CoilfangStrider },
    VashjTargetTier{ VashjTarget::EnchantedElemental },
    VashjTargetTier{ VashjTarget::CoilfangElite },
};
inline std::vector<VashjTargetTier> const VASHJ_PHASE_2_MELEE_TIERS = {
    VashjTargetTier{ VashjTarget::EnchantedElemental, VASHJ_ENCHANTED_NEAR_HER_DISTANCE },
    VashjTargetTier{ VashjTarget::CoilfangElite },
};
inline std::vector<VashjTargetTier> const VASHJ_PHASE_2_TANK_TIERS = {
    VashjTargetTier{ VashjTarget::CoilfangStrider },
    VashjTargetTier{ VashjTarget::CoilfangElite },
    VashjTargetTier{ VashjTarget::EnchantedElemental, VASHJ_ENCHANTED_NEAR_HER_DISTANCE },
};
inline std::vector<VashjTargetTier> const VASHJ_PHASE_2_HEALER_TIERS = {
    VashjTargetTier{ VashjTarget::EnchantedElemental },
    VashjTargetTier{ VashjTarget::CoilfangElite },
    VashjTargetTier{ VashjTarget::CoilfangStrider },
};
inline std::vector<VashjTargetTier> const VASHJ_PHASE_3_MAIN_TANK_TIERS = {
    VashjTargetTier{ VashjTarget::LadyVashj },
};
inline std::vector<VashjTargetTier> const VASHJ_PHASE_3_TANK_TIERS = {
    VashjTargetTier{ VashjTarget::CoilfangStrider },
    VashjTargetTier{ VashjTarget::CoilfangElite },
    VashjTargetTier{ VashjTarget::EnchantedElemental },
    VashjTargetTier{ VashjTarget::LadyVashj },
};
inline std::vector<VashjTargetTier> const VASHJ_PHASE_3_HUNTER_TIERS = {
    VashjTargetTier{ VashjTarget::ToxicSporebat },
    VashjTargetTier{ VashjTarget::EnchantedElemental },
    VashjTargetTier{ VashjTarget::CoilfangStrider },
    VashjTargetTier{ VashjTarget::CoilfangElite },
    VashjTargetTier{ VashjTarget::LadyVashj },
};
inline std::vector<VashjTargetTier> const VASHJ_PHASE_3_RANGED_TIERS = {
    VashjTargetTier{ VashjTarget::EnchantedElemental },
    VashjTargetTier{ VashjTarget::CoilfangStrider },
    VashjTargetTier{ VashjTarget::CoilfangElite },
    VashjTargetTier{ VashjTarget::LadyVashj },
};
inline std::vector<VashjTargetTier> const VASHJ_PHASE_3_MELEE_TIERS = {
    VashjTargetTier{ VashjTarget::EnchantedElemental, VASHJ_ENCHANTED_NEAR_HER_DISTANCE },
    VashjTargetTier{ VashjTarget::CoilfangElite },
    VashjTargetTier{ VashjTarget::LadyVashj },
};

// Each position is between two ranged stations, 16y+ from every station slot and healer post and
// 18y+ from each generator.
inline std::array const VASHJ_STRIDER_HOLD_POSITIONS = {
    Position{ -6.0f, -913.5f, 41.9f },
    Position{  9.5f, -963.5f, 41.5f },
    Position{ 33.5f, -889.5f, 41.9f },
};
// Each position is in range of all three ranged dps slots of one ranged station and 18y+ from
// every Strider holding position.
inline std::array const VASHJ_ELITE_TANK_POSITIONS = {
    Position{ 57.0f, -913.0f, 42.0f },
    Position{  5.5f, -934.0f, 42.1f },
};

VashjAddGuids FindVashjAddGuids(PlayerbotAI* botAI);
std::vector<VashjTargetTier> const& GetVashjTargetTiers(Player* bot, int8 phase, bool killsTainted);
bool IsVashjAddHeldByTank(Unit* unit);
Player* GetVashjAddOwningTank(Player* bot, Unit* add);
bool IsNearestFreeVashjTank(Player* bot, Unit* add, Unit* vashj, int8 phase);
bool GetStepToBringTankedUnitTo(
    Player* bot, Unit* add, Position const& spot, float arrivalDistance, float& stepX,
    float& stepY, bool& backwards);
Position const& GetVashjStriderHoldPosition(Unit const& strider);
Position const& GetVashjEliteTankPosition(Unit const& elite);
bool ShouldPositionVashjStrider(Player* bot, Unit* strider, Unit* vashj, int8 phase);
bool IsTankedStriderInStepInReach(Player* bot, Unit* unit);
// Pets skip targets they're useless on: Vashj while she's immune in Phase 2, Striders (Panic fears
// any pet in melee range), and Sporebats (out of reach).
Unit* GetVashjPetTarget(PlayerbotAI* botAI, Creature* pet, Unit* vashj);

// Vashj: Tainted Elemental

struct TaintedCoreLooter
{
    BotGuid tainted;
    BotGuid looter;
    int8 station = -1;
};

inline constexpr float VASHJ_CORE_LOOT_RANGE = INTERACTION_DISTANCE - 2.0f;

Player* FindTaintedCoreLooter(Player* bot, Unit* tainted, int8 station);
Creature* GetAssignedTaintedElemental(Player* bot);
int8 GetTaintedCoreLootSlot(Creature* tainted);
bool IsTaintedCoreStillToLoot(Creature* tainted);
Creature* GetTaintedElementalToKill(Player* bot);
// The core's Tainted Elemental does not attack, so nobody is on its threat list and "find target" cannot see it until
// it is hit; it is searched in the grid instead.
Creature* FindTaintedElemental(Player* bot);
bool IsDesignatedCoreLooter(Player* bot);
bool HasTaintedCore(Player* player);

// Vashj: Core Passing Chain

struct VashjCoreCatcher
{
    Position spot;
    BotGuid bot = BotGuid::Empty;
    // The first catcher is released when the plan is made, the second when the first starts moving,
    // each later one when the one before reaches its spot, delayed by readyDelay for realism.
    bool released = false;
    uint32 releaseTime = 0;
    uint32 readyDelay = 0;
    bool prepositions = false;
    bool arrived = false;
};

// The route one core takes to a generator, per instance. The mechanic tracker bot plans it when
// the looter is picked. If the next throw can't be made, the holder replans from where it stands.
struct VashjCorePassingChain
{
    BotGuid tainted;
    BotGuid generator;
    // Who throws to the first catcher: the looter, or the core holder if the chain was replanned.
    BotGuid originBot;
    std::vector<VashjCoreCatcher> catchers;
    // A catcher whose throws failed is left out of every later replan (the most likely reason for
    // repeated failure is a full inventory).
    BotGuid excluded;
    int8 reached = -1;
    bool failed = false;
    uint8 replans = 0;
    // The last throw, kept through a replan so the next throw still waits for it.
    BotGuid throwTarget;
    uint32 throwTime = 0;
    uint8 failedThrows = 0;
    // The catcher the holder is waiting on, when that wait started, and when the catcher started
    // standing on its spot but out of throw range.
    BotGuid waitTarget;
    uint32 waitStart = 0;
    uint32 blockedStart = 0;
};

// Throw Key's actual range is 40y + both combat reaches (so 43y minimum). To allow a bit of slack
// and realism, positions in the passing chain are planned only 40y apart, center to center.
inline constexpr float VASHJ_CORE_THROW_PLAN_DISTANCE = 40.0f;
// The looter stands up to about 6y from the corpse, and the first throw is planned from the corpse.
inline constexpr float VASHJ_CORE_LOOTER_OFFSET = 6.0f;
// Line of sight is checked from each player's collision height, 1.21 (Gnome) to 2.64 (female
// Tauren). Plans assume everybody is a Gnome.
inline constexpr float VASHJ_CORE_PLAN_EYE_HEIGHT = 1.2f;
// The core's Elites and Striders attack a random raid member the moment they spawn, so keeping away from their lanes
// buys nothing (AC keeps 22 yd); chain spots only stay off the spawn points themselves.
inline constexpr float VASHJ_CORE_SPOT_SPAWN_CLEARANCE = 8.0f;
// The generators have display id 7265 at size 2.1. The widest point of the generator's base,
// measured from its bounding box, is about 3.2y. I don't know the exact collision radius.
inline constexpr float VASHJ_CORE_SPOT_GENERATOR_CLEARANCE = 5.0f;
// Five players should be able to get from every Tainted Elemental spawn position to every
// generator while following the passing rules (e.g., collision, distance from add lanes).
inline constexpr size_t VASHJ_CORE_MAX_CATCHERS = 5;
// A chain member needs to be within this distance of its calculated spot to be considered in
// position. The use spot has less tolerance to ensure the bot is truly within use distance.
inline constexpr float VASHJ_CORE_SPOT_ARRIVAL_DISTANCE = 1.0f;
inline constexpr float VASHJ_CORE_USE_SPOT_ARRIVAL_DISTANCE = 0.5f;
// boss_lady_vashj SHIED_GENERATOR_CHANNEL: summoned on each generator in phase 2, its beam holds Vashj's barrier.
inline constexpr uint32 VASHJ_SHIELD_GENERATOR_CHANNEL = 19870;
inline constexpr std::array VASHJ_SHIELD_GENERATOR_SPAWN_IDS = {
    uint32{ 47482 }, // NW
    uint32{ 47483 }, // NE
    uint32{ 47484 }, // SE
    uint32{ 47485 }, // SW
};

// Where the core summons Elites and Striders (boss_lady_vashj.cpp CoilfangElitePos and CoilfangStriderPos; the
// Elite spot in the middle under Vashj is left out).
inline std::array const VASHJ_ADD_SPAWN_POSITIONS = {
    Position{  31.183f, -953.503f, 41.5f },
    Position{  58.895f, -923.124f, 41.5f },
    Position{  66.427f, -948.779f, 41.3f },
    Position{   7.514f, -959.538f, 41.3f },
    Position{ -12.843f, -907.798f, 41.2f },
};
// The small rock that cuts into the Southwestern stairs, measured from stair base to the highest
// point of the rock and back down the other side.
inline std::array const VASHJ_SOUTH_WEST_ROCK = {
    Position{ -16.473f, -843.635f, 22.78f },
    Position{ -10.493f, -849.837f, 27.23f },
    Position{ -10.034f, -860.397f, 32.37f },
    Position{ -14.241f, -865.373f, 32.69f },
    Position{ -46.103f, -872.655f, 22.53f },
};
// The world DB spawns of the four generators (x and y; z is AC's model centre height for the sight checks).
inline std::array const VASHJ_SHIELD_GENERATOR_POSITIONS = {
    Position{ 49.626f, -902.181f, 44.0f },
    Position{ 49.313f, -943.398f, 44.0f },
    Position{ 10.386f, -944.036f, 44.0f },
    Position{ 10.988f, -901.616f, 44.0f },
};

void PlanVashjCorePassingChain(Player* bot, Unit* tainted, Player* looter);
bool ReplanVashjCorePassingChain(Player* holder, VashjCorePassingChain& chain, BotGuid excluded);
bool ReassignVashjCoreCatcher(Player* bot, VashjCorePassingChain& chain, size_t index);
void ReleaseVashjCoreCatcher(Player* bot, VashjCorePassingChain& chain, size_t index);
VashjCorePassingChain* GetVashjCorePassingChain(Player* bot);
int8 GetVashjCoreCatcherIndex(VashjCorePassingChain const& chain, Player* bot);
bool IsVashjCoreCatcherActive(Player* bot, VashjCorePassingChain const& chain, int8 index);
float GetVashjCoreSpotArrivalDistance(VashjCorePassingChain const& chain, int8 index);

// Shared encounter state
struct SscInstanceState
{
    std::optional<uint32> hydrossFrostPhaseStartTime;
    std::optional<uint32> hydrossNaturePhaseStartTime;
    std::optional<uint32> hydrossFrostMarkMaxedTime;
    std::optional<uint32> hydrossNatureMarkMaxedTime;
    std::optional<LurkerGuardianTankAssignments> lurkerGuardianTankAssignments;
    std::optional<uint32> leotherasHumanoidPhaseStartTime;
    std::optional<uint32> leotherasWhirlwindEndTime;
    std::optional<uint32> leotherasDemonPhaseStartTime;
    std::optional<uint32> leotherasFinalPhaseStartTime;
    std::optional<uint32> karathressDpsWaitTimer;
    std::optional<BotGuid> vashjGroundingShaman;
    std::optional<VashjStationHolders> vashjStationHolders;
    std::optional<TaintedCoreLooter> vashjTaintedCoreLooter;
    std::optional<VashjCorePassingChain> vashjCorePassingChain;
};

// AC drops a cast from its Whirlwind and Toxic Spores spell listeners (SSCScripts.cpp); SkyFire has no such hook, so
// PlayerbotAI polls this while the bot casts (RaidInterrupts): Leotheras whirlwinding within 25 yd of a bot that is
// not a tank and holds no Inner Demon, or a Toxic Spore pool under the bot.
bool ShouldInterruptCast(Player* bot);

SscInstanceState& SscState(uint32 instanceId);
bool SscResetInstance(uint32 instanceId);

template <typename T, typename U>
bool EmplaceIfUnset(std::optional<T>& field, U&& value)
{
    if (field)
        return false;

    field.emplace(std::forward<U>(value));
    return true;
}

template <typename T>
bool ResetIfSet(std::optional<T>& field)
{
    bool const wasSet = field.has_value();
    field.reset();
    return wasSet;
}

}

#endif
