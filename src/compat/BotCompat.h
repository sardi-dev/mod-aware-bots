/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * Included first in every file of the module (module.cmake). It gives the playerbots code, written against a core
 * with TrinityCore's newer APIs, the names it expects on SkyFire: the GUID types, the playerbots database, and the
 * unscoped names of enums that SkyFire made enum classes.
 */

#ifndef MOD_AWARE_BOTS_COMPAT_BOTCOMPAT_H
#define MOD_AWARE_BOTS_COMPAT_BOTCOMPAT_H

#ifdef __cplusplus

#include "BotChatConstants.h"
#include "BotCharSections.h"
#include "BotGuid.h"
#include "BotTerrain.h"
#include "BotWorld.h"
#include "BotCompat_A.h"
#include "BotCompat_E.h"
#include "BotCompat_C.h"
#include "BotCompat_G.h"
#include "BotCompat_S.h"
#include "BotCompat_T.h"
#include "Corpse.h"
#include "GameObject.h"
#include "Group.h"
#include "LootMgr.h"
#include "ItemPrototype.h"
#include "Player.h"
#include "QuestDef.h"
#include "DBCEnums.h"
#include "TemporarySummon.h"
#include "PlayerbotsDatabase.h"
#include "SharedDefines.h"
#include "Unit.h"
#include "World.h"

using enum BattlegroundTypeId;
using enum CombatRating;
using enum CorpseType;
using enum GOState;
using enum GroupMemberFlags;
using enum Language;
using enum LootType;
using enum SpellCastResult;
using enum TempSummonType;
using enum TypeID;
using enum WorldBoolConfigs;
using enum WorldIntConfigs;

typedef PreparedStatement PlayerbotsPreparedStatement;
// TrinityCore names a prepared statement per database; SkyFire has one PreparedStatement type.
typedef PreparedStatement CharacterDatabasePreparedStatement;
typedef PreparedStatement LoginDatabasePreparedStatement;

// Names Legends of Azeroth's core has and SkyFire spells differently, with the same values.
constexpr DifficultyID DUNGEON_DIFFICULTY_NORMAL = DIFFICULTY_NORMAL;
constexpr DifficultyID DUNGEON_DIFFICULTY_HEROIC = DIFFICULTY_HEROIC;
constexpr DifficultyID DUNGEON_DIFFICULTY_CHALLENGE = DIFFICULTY_CHALLENGE;
constexpr uint8 SUMMON_SLOT_TOTEM_FIRE = SUMMON_SLOT_TOTEM;
constexpr uint8 SUMMON_SLOT_TOTEM_EARTH = SUMMON_SLOT_TOTEM + 1;
constexpr uint8 SUMMON_SLOT_TOTEM_WATER = SUMMON_SLOT_TOTEM + 2;
constexpr uint8 SUMMON_SLOT_TOTEM_AIR = SUMMON_SLOT_TOTEM + 3;
#ifndef SUMMON_SLOT_TOTEM_EXTRA  // newer cores define it for Totemic Persistence
constexpr uint8 SUMMON_SLOT_TOTEM_EXTRA = 7;
#endif
constexpr uint8 EQUIPMENT_SLOT_RANGED = 17;  // removed from the client in 5.x; LoA keeps the old index for checks
constexpr uint8 QUEST_OBJECTIVE_MONSTER = QUEST_OBJECTIVE_TYPE_NPC;
constexpr uint8 QUEST_OBJECTIVE_ITEM = QUEST_OBJECTIVE_TYPE_ITEM;

// Item binding names as Legends of Azeroth spells them; SkyFire's ItemBondingType has the same values.
constexpr ItemBondingType BIND_NONE = NO_BIND;
constexpr ItemBondingType BIND_ON_ACQUIRE = BIND_WHEN_PICKED_UP;
constexpr ItemBondingType BIND_ON_EQUIP = BIND_WHEN_EQUIPED;
constexpr ItemBondingType BIND_ON_USE = BIND_WHEN_USE;
constexpr ItemBondingType BIND_QUEST = BIND_QUEST_ITEM;

typedef SQLTransaction PlayerbotsDatabaseTransaction;

// TrinityCore's SpellInfo::HasAttribute(SpellAttrN): SkyFire keeps each attribute group in its own field.
inline bool BotHasAttribute(SpellInfo const* info, SpellAttr0 a) { return info->Attributes & a; }
inline bool BotHasAttribute(SpellInfo const* info, SpellAttr1 a) { return info->AttributesEx & a; }
inline bool BotHasAttribute(SpellInfo const* info, SpellAttr2 a) { return info->AttributesEx2 & a; }
inline bool BotHasAttribute(SpellInfo const* info, SpellAttr3 a) { return info->AttributesEx3 & a; }
inline bool BotHasAttribute(SpellInfo const* info, SpellAttr4 a) { return info->AttributesEx4 & a; }
inline bool BotHasAttribute(SpellInfo const* info, SpellAttr5 a) { return info->AttributesEx5 & a; }
inline bool BotHasAttribute(SpellInfo const* info, SpellAttr6 a) { return info->AttributesEx6 & a; }
inline bool BotHasAttribute(SpellInfo const* info, SpellAttr7 a) { return info->AttributesEx7 & a; }
inline bool BotHasAttribute(SpellInfo const* info, SpellAttr8 a) { return info->AttributesEx8 & a; }
inline bool BotHasAttribute(SpellInfo const* info, SpellAttr9 a) { return info->AttributesEx9 & a; }

// The petition type of a guild charter; SkyFire keeps it file-local (Handlers/PetitionsHandler.cpp).
constexpr uint32 BOT_GUILD_CHARTER_TYPE = 4;

// TrinityCore's VehicleFlags value for cannons and other vehicles that must not move (Vehicle.dbc m_flags).
constexpr uint32 VEHICLE_FLAG_FIXED_POSITION = 0x00200000;

// Specialisations per class plus the pet specialisations, as Legends of Azeroth defines it.
#define MAX_SPECIALIZATIONS 5

// TrinityCore's Player::GetSpecialization(); SkyFire stores the same specialisation id per talent group.
inline Specializations BotGetSpecialization(Player const* player)
{
    return Specializations(player->GetTalentSpecialization(player->GetActiveSpec()));
}

#endif

#endif
