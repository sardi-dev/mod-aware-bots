/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * Helpers for Scenario/, strategy/RPG/ and Travel/: what Legends of Azeroth's core offers as members (quest
 * objectives, quest items, movement updates, outdoor PvP and Alterac Valley state, path checks) and SkyFire does not
 * expose. Each helper says which LoA call it replaces.
 */

#ifndef MOD_AWARE_BOTS_COMPAT_BOTCOMPAT_S_H
#define MOD_AWARE_BOTS_COMPAT_BOTCOMPAT_S_H

#include "Battlefield.h"
#include "BattlegroundAV.h"
#include "BattlegroundMgr.h"
#include "BotTerrain.h"
#include "Creature.h"
#include "InstanceScript.h"
#include "ObjectMgr.h"
#include "OutdoorPvP.h"
#include "Player.h"
#include "QuestDef.h"
#include "ReputationMgr.h"
#include "Unit.h"
#include "WorldPacket.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

// LoA OutdoorPvP::GetCapturePoints, OPvPCapturePoint::GetSlider and GetMinValue, BattlegroundAV::GetTeamReinforcements,
// Battlefield::GetState and InstanceScript::GetEncounterCount, on the read-only getters of the core patches
// (patches/).
inline OutdoorPvP::OPvPCapturePointMap const& BotGetCapturePoints(OutdoorPvP const* pvp) { return pvp->GetCapturePoints(); }
inline float BotGetCapturePointSlider(OPvPCapturePoint const* point) { return point->GetSlider(); }
inline float BotGetCapturePointMinValue(OPvPCapturePoint const* point) { return point->GetMinValue(); }
inline int32 BotGetAvTeamReinforcements(BattlegroundAV const* av, uint8 teamIndex) { return av->GetTeamReinforcements(teamIndex); }
inline uint32 BotGetEncounterCount(InstanceScript const* instance) { return instance->GetEncounterCount(); }

// LoA Battlefield::GetState: 0 inactive, 1 warm-up (grouping has started), 2 in progress.
inline int8 BotGetBattlefieldState(Battlefield const* battlefield)
{
    if (battlefield->IsWarTime())
        return 2;
    return battlefield->GetTimer() <= battlefield->GetStartGroupingTimer() ? 1 : 0;
}

// LoA BattlegroundMgr::IsArenaType, which SkyFire keeps private (same list as BattlegroundMgr.cpp).
inline bool BotIsArenaType(BattlegroundTypeId bgTypeId)
{
    return bgTypeId == BattlegroundTypeId::BATTLEGROUND_AA
        || bgTypeId == BattlegroundTypeId::BATTLEGROUND_BE
        || bgTypeId == BattlegroundTypeId::BATTLEGROUND_NA
        || bgTypeId == BattlegroundTypeId::BATTLEGROUND_DS
        || bgTypeId == BattlegroundTypeId::BATTLEGROUND_RV
        || bgTypeId == BattlegroundTypeId::BATTLEGROUND_TV
        || bgTypeId == BattlegroundTypeId::BATTLEGROUND_RL;
}

// ---------------------------------------------------------------------------------------------------------------
// Quests
// ---------------------------------------------------------------------------------------------------------------

// LoA's Quest::Objectives (a vector of QuestObjective). SkyFire keeps a set of pointers ordered by address, so this
// copies them into a vector sorted by (Index, Id) and dereferences on iteration. Pointers stay valid: they point at the
// quest's own objectives. Use as `for (QuestObjective const& o : BotQuestObjectives(quest))`.
class BotQuestObjectives
{
public:
    class Iterator
    {
    public:
        explicit Iterator(std::vector<QuestObjective const*>::const_iterator it) : _it(it) { }
        QuestObjective const& operator*() const { return **_it; }
        Iterator& operator++() { ++_it; return *this; }
        bool operator!=(Iterator const& other) const { return _it != other._it; }

    private:
        std::vector<QuestObjective const*>::const_iterator _it;
    };

    explicit BotQuestObjectives(Quest const* quest)
    {
        if (!quest)
            return;
        _objectives.assign(quest->m_questObjectives.begin(), quest->m_questObjectives.end());
        std::sort(_objectives.begin(), _objectives.end(), [](QuestObjective const* a, QuestObjective const* b)
        {
            return a->Index != b->Index ? a->Index < b->Index : a->Id < b->Id;
        });
    }

    Iterator begin() const { return Iterator(_objectives.begin()); }
    Iterator end() const { return Iterator(_objectives.end()); }
    size_t size() const { return _objectives.size(); }
    bool empty() const { return _objectives.empty(); }
    QuestObjective const& operator[](size_t index) const { return *_objectives[index]; }

private:
    std::vector<QuestObjective const*> _objectives;
};

// LoA Player::IsQuestObjectiveComplete, over the objective types SkyFire's Player::CanCompleteQuest checks.
inline bool BotIsQuestObjectiveComplete(Player* player, Quest const* /*quest*/, QuestObjective const& objective)
{
    switch (objective.Type)
    {
        case QUEST_OBJECTIVE_TYPE_SPELL:
            return player->HasSpell(objective.ObjectId);
        case QUEST_OBJECTIVE_TYPE_FACTION_REP:
        case QUEST_OBJECTIVE_TYPE_FACTION_REP2:
            return player->GetReputationMgr().GetReputation(objective.ObjectId) >= objective.Amount;
        case QUEST_OBJECTIVE_TYPE_MONEY:
            return player->HasEnoughMoney(uint64(objective.Amount));
        case QUEST_OBJECTIVE_TYPE_CURRENCY:
            return player->HasCurrency(objective.ObjectId, objective.Amount);
        case QUEST_OBJECTIVE_TYPE_DUMMY:
        case QUEST_OBJECTIVE_TYPE_PET_BATTLE_TAMER:
        case QUEST_OBJECTIVE_TYPE_PET_BATTLE_ELITE:
        case QUEST_OBJECTIVE_TYPE_PET_BATTLE_PVP:
            // Event objectives often carry Amount 0 in the DB but still need the event (as CanCompleteQuest does).
            return player->GetQuestObjectiveCounter(objective.Id) >= uint32(objective.Amount > 0 ? objective.Amount : 1);
        default:
            return player->GetQuestObjectiveCounter(objective.Id) >= uint32(objective.Amount);
    }
}

// LoA Player::DestroyQuestItems: the objective items and the items handed out by the quest, on abandon or fail
// (SkyFire's Player::FailQuest does the same inline).
inline void BotDestroyQuestItems(Player* player, uint32 questId)
{
    Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
    if (!quest)
        return;

    for (QuestObjective const& objective : BotQuestObjectives(quest))
        if (objective.Type == QUEST_OBJECTIVE_TYPE_ITEM)
            player->DestroyItemCount(objective.ObjectId, objective.Amount, true, true);

    for (uint8 i = 0; i < QUEST_SOURCE_ITEM_IDS_COUNT; ++i)
        if (quest->RequiredSourceItemId[i] > 0 && quest->RequiredSourceItemCount[i] > 0)
            player->DestroyItemCount(quest->RequiredSourceItemId[i], quest->RequiredSourceItemCount[i], true, true);
}

// ---------------------------------------------------------------------------------------------------------------
// Spawns
// ---------------------------------------------------------------------------------------------------------------

// LoA's CreatureData::spawnId and GameObjectData::spawnId. SkyFire's data rows do not carry their key (the spawn
// guid), so this finds the row in the spawn store by its address (the store's rows never move once loaded). Returns 0
// for a row that is not in the store.
inline uint32 BotCreatureSpawnId(CreatureData const& data)
{
    static std::unordered_map<CreatureData const*, uint32> const index = []
    {
        std::unordered_map<CreatureData const*, uint32> rows;
        for (auto const& [spawnId, row] : sObjectMgr->GetAllCreatureData())
            rows.emplace(&row, spawnId);
        return rows;
    }();

    auto itr = index.find(&data);
    return itr != index.end() ? itr->second : 0;
}

inline uint32 BotGameObjectSpawnId(GameObjectData const& data)
{
    static std::unordered_map<GameObjectData const*, uint32> const index = []
    {
        std::unordered_map<GameObjectData const*, uint32> rows;
        for (auto const& [spawnId, row] : sObjectMgr->GetAllGameObjectData())
            rows.emplace(&row, spawnId);
        return rows;
    }();

    auto itr = index.find(&data);
    return itr != index.end() ? itr->second : 0;
}

// ---------------------------------------------------------------------------------------------------------------
// Movement
// ---------------------------------------------------------------------------------------------------------------

// Collision height of a man-sized unit; SkyFire has no per-model collision height (see BotTerrain.h).
constexpr float BOT_COLLISION_HEIGHT = 2.0f;

// LoA getSlopeAngleAbs (PathGenerator.cpp)
inline float BotSlopeAngleAbs(float startX, float startY, float startZ, float destX, float destY, float destZ)
{
    float const floorDist = std::sqrt(std::pow(startY - destY, 2.0f) + std::pow(startX - destX, 2.0f));
    return std::abs(std::atan(std::abs(destZ - startZ) / std::abs(floorDist)));
}

// LoA PathGenerator::IsWalkableClimb: the climbable height shrinks with the slope in degrees.
inline bool BotIsWalkableClimb(float x, float y, float z, float destX, float destY, float destZ, float sourceHeight)
{
    float const slopeDegrees = BotSlopeAngleAbs(x, y, z, destX, destY, destZ) * 180.0f / float(M_PI);
    float const climbableHeight = sourceHeight - (sourceHeight * (slopeDegrees / 100));
    return std::abs(destZ - z) <= climbableHeight;
}

// LoA Map::CanReachPositionAndGetValidCoords (Map.cpp), start-point overload: the collision check of
// BotCheckCollisionAndGetValidCoords, then the walkable checks (water, creatures that cannot walk, slope).
inline bool BotCanReachPositionAndGetValidCoords(Map const* map, WorldObject const* source, float startX, float startY,
    float startZ, float& destX, float& destY, float& destZ, bool failOnCollision = true, bool failOnSlopes = true)
{
    if (!BotCheckCollisionAndGetValidCoords(map, source, startX, startY, startZ, destX, destY, destZ, failOnCollision))
        return false;

    Unit const* unit = source->ToUnit();
    if (!unit)
        return true;

    bool const isWaterNext = map->IsInWater(destX, destY, destZ);
    Creature const* creature = unit->ToCreature();
    bool const cannotEnterWater = isWaterNext && (creature && !creature->CanSwim());
    bool const cannotWalkOrFly = !isWaterNext && !source->ToPlayer() && !unit->CanFly() && (creature && !creature->CanWalk());
    return !(cannotEnterWater || cannotWalkOrFly ||
        (failOnSlopes && !BotIsWalkableClimb(startX, startY, startZ, destX, destY, destZ, BOT_COLLISION_HEIGHT)));
}

#endif
