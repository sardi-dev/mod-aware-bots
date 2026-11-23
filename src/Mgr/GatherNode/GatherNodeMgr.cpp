/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 *
 * Ported from the mod-playerbots module for AzerothCore (Mgr/GatherNode/GatherNodeMgr.cpp). See GatherNodeMgr.h.
 */

#include "GatherNodeMgr.h"

#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "GameObject.h"
#include "Log.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Random.h"
#include "SharedDefines.h"
#include "Timer.h"

#include <algorithm>
#include <cmath>

namespace
{
    // Square cells of this many yards: a zone is a few of them, a query reads the 3x3 around the bot.
    float const CELL_SIZE = 500.0f;
    // Cell coordinates are shifted by this so they stay positive in the key (a map is at most 34000 yards wide).
    int32 const CELL_OFFSET = 128;
    uint32 const NEAREST_PICK_COUNT = 4;

    // The base-rank mining picks, as the loot pipeline checks them (LootObject::IsLootPossible).
    uint32 const MINING_PICKS[] = { 756, 778, 1819, 1893, 1959, 2901, 9465, 20723, 40772, 40892, 40893 };

    bool HasMiningPick(Player* bot)
    {
        for (uint32 const pick : MINING_PICKS)
            if (bot->HasItemCount(pick, 1))
                return true;
        return false;
    }

    bool GetGatherSkillFromTemplate(GameObjectTemplate const* goInfo, uint32& skillId, uint32& reqSkillValue)
    {
        if (!goInfo || goInfo->type != GAMEOBJECT_TYPE_CHEST)
            return false;

        uint32 const lockId = goInfo->GetLockId();
        if (!lockId)
            return false;

        LockEntry const* lockInfo = sLockStore.LookupEntry(lockId);
        if (!lockInfo)
            return false;

        for (uint8 j = 0; j < 8; ++j)
        {
            if (lockInfo->Type[j] != LOCK_KEY_SKILL)
                continue;

            uint32 const skill = SkillByLockType(LockType(lockInfo->Index[j]));
            if (skill == SKILL_HERBALISM || skill == SKILL_MINING)
            {
                skillId = skill;
                reqSkillValue = std::max(1u, lockInfo->Skill[j]);
                return true;
            }
        }

        return false;
    }
}

int32 GatherNodeMgr::CellOf(float coordinate) { return int32(std::floor(coordinate / CELL_SIZE)); }

uint32 GatherNodeMgr::CellKey(int32 cx, int32 cy)
{
    return (uint32(cx + CELL_OFFSET) << 16) | uint32(cy + CELL_OFFSET);
}

void GatherNodeMgr::Load()
{
    uint32 const oldMSTime = getMSTime();
    _nodes.clear();
    _count = 0;

    // Loot ids with at least one freely lootable row. Quest-gated "gathering" chests carry a herbalism or mining lock but
    // all their loot rows are quest drops (a negative chance on this core), so a bot without the quest cannot harvest
    // them, and they never despawn: permanent bait. Filtering on the quest item list instead would be wrong, since
    // standard herbs carry a conditional quest item on top of their normal loot.
    std::unordered_set<uint32> openLootIds;
    if (QueryResult result = WorldDatabase.PQuery("SELECT DISTINCT entry FROM gameobject_loot_template WHERE ChanceOrQuestChance >= 0"))
    {
        do
        {
            openLootIds.insert((*result)[0].GetUInt32());
        } while (result->NextRow());
    }

    for (auto const& entry : sObjectMgr->GetAllGameObjectData())
    {
        uint32 const spawnId = entry.first;
        GameObjectData const& goData = entry.second;

        // Instances are excluded: the status targets open-world roaming.
        MapEntry const* mapEntry = sMapStore.LookupEntry(goData.mapid);
        if (!mapEntry || !mapEntry->IsContinent())
            continue;

        GameObjectTemplate const* goInfo = sObjectMgr->GetGameObjectTemplate(goData.id);
        uint32 skillId = 0;
        uint32 reqSkillValue = 0;
        if (!GetGatherSkillFromTemplate(goInfo, skillId, reqSkillValue))
            continue;

        if (!openLootIds.count(goInfo->GetLootId()))
            continue;

        GatherNodeSpawn node;
        node.spawnId = spawnId;
        node.entry = goData.id;
        node.pos = WorldPosition(goData.mapid, goData.posX, goData.posY, goData.posZ);
        node.skillId = skillId;
        node.reqSkillValue = reqSkillValue;
        _nodes[goData.mapid][CellKey(CellOf(goData.posX), CellOf(goData.posY))].push_back(std::move(node));
        ++_count;
    }

    SF_LOG_INFO("playerbots", ">> Loaded %u gather node spawns in %u ms", _count, GetMSTimeDiffToNow(oldMSTime));
}

GatherNodeMgr::Caps GatherNodeMgr::CapsOf(PlayerbotAI* botAI, Player* bot)
{
    Caps caps;
    if (botAI->HasSkill(SKILL_HERBALISM))
    {
        caps.herbalism = true;
        caps.herbalismValue = bot->GetSkillValue(SKILL_HERBALISM);
    }

    // No mining without a pick: the loot pipeline refuses the node and the bot would stand there until the timeout.
    if (botAI->HasSkill(SKILL_MINING) && HasMiningPick(bot))
    {
        caps.mining = true;
        caps.miningValue = bot->GetSkillValue(SKILL_MINING);
    }

    return caps;
}

bool GatherNodeMgr::IsUsable(Caps const& caps, GatherNodeSpawn const& node)
{
    if (node.skillId == SKILL_HERBALISM)
        return caps.herbalism && caps.herbalismValue >= node.reqSkillValue;

    return caps.mining && caps.miningValue >= node.reqSkillValue;
}

template <typename Fn>
void GatherNodeMgr::ForNodesAround(uint32 mapId, WorldPosition const& pos, float reach, Fn fn) const
{
    auto const mapItr = _nodes.find(mapId);
    if (mapItr == _nodes.end())
        return;

    int32 const span = int32(std::ceil(reach / CELL_SIZE));
    int32 const cx = CellOf(pos.GetPositionX());
    int32 const cy = CellOf(pos.GetPositionY());
    for (int32 dx = -span; dx <= span; ++dx)
        for (int32 dy = -span; dy <= span; ++dy)
        {
            auto const cellItr = mapItr->second.find(CellKey(cx + dx, cy + dy));
            if (cellItr == mapItr->second.end())
                continue;

            for (GatherNodeSpawn const& node : cellItr->second)
                fn(node);
        }
}

bool GatherNodeMgr::HasUsableNodes(Player* bot)
{
    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    if (!botAI)
        return false;

    Caps const caps = CapsOf(botAI, bot);
    if (!caps.herbalism && !caps.mining)
        return false;

    bool found = false;
    ForNodesAround(bot->GetMapId(), WorldPosition(bot), CELL_SIZE, [&](GatherNodeSpawn const& node)
    {
        if (!found && IsUsable(caps, node))
            found = true;
    });
    return found;
}

GatherNodeSpawn const* GatherNodeMgr::GetNextNode(Player* bot, std::unordered_set<uint32> const& visited)
{
    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    if (!botAI)
        return nullptr;

    Caps const caps = CapsOf(botAI, bot);
    if (!caps.herbalism && !caps.mining)
        return nullptr;

    Map* map = bot->GetMap();
    WorldPosition const botPos(bot);
    std::vector<std::pair<float, GatherNodeSpawn const*>> candidates;
    ForNodesAround(bot->GetMapId(), botPos, CELL_SIZE, [&](GatherNodeSpawn const& node)
    {
        if (visited.count(node.spawnId) || !IsUsable(caps, node))
            return;

        // Not to a spawn point already seen to be empty. Evaluated fresh at every pick, so respawns return to the pool.
        if (IsVerifiablyDown(map, node))
            return;

        float const dx = node.pos.GetPositionX() - botPos.GetPositionX();
        float const dy = node.pos.GetPositionY() - botPos.GetPositionY();
        candidates.push_back({ dx * dx + dy * dy, &node });
    });

    if (candidates.empty())
        return nullptr;

    uint32 const nearestCount = std::min<uint32>(NEAREST_PICK_COUNT, uint32(candidates.size()));
    std::partial_sort(candidates.begin(), candidates.begin() + nearestCount, candidates.end(),
                      [](auto const& a, auto const& b) { return a.first < b.first; });

    return candidates[urand(0, nearestCount - 1)].second;
}

GameObject* GatherNodeMgr::FindLiveNode(Map* map, uint32 entry, uint32 spawnId)
{
    GameObject* go = map->GetGameObject(BotGuid::Create<BotHighGuid::GameObject>(entry, spawnId));
    return go && go->isSpawned() && go->GetGoState() == GO_STATE_READY ? go : nullptr;
}

GameObject* GatherNodeMgr::FindLiveNode(Map* map, GatherNodeSpawn const& node)
{
    return FindLiveNode(map, node.entry, node.spawnId);
}

bool GatherNodeMgr::IsVerifiablyDown(Map* map, GatherNodeSpawn const& node)
{
    // An unloaded grid means "unknown", not "not up".
    if (!map->IsGridLoaded(node.pos.GetPositionX(), node.pos.GetPositionY()))
        return false;

    return !FindLiveNode(map, node);
}

GatherNodeSpawn const* GatherNodeMgr::GetNearestLiveNode(Player* bot, std::unordered_set<uint32> const& visited, float radius)
{
    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    if (!botAI)
        return nullptr;

    Caps const caps = CapsOf(botAI, bot);
    if (!caps.herbalism && !caps.mining)
        return nullptr;

    Map* map = bot->GetMap();
    WorldPosition const botPos(bot);
    GatherNodeSpawn const* nearest = nullptr;
    float nearestDistSq = radius * radius;
    ForNodesAround(bot->GetMapId(), botPos, radius, [&](GatherNodeSpawn const& node)
    {
        float const dx = node.pos.GetPositionX() - botPos.GetPositionX();
        float const dy = node.pos.GetPositionY() - botPos.GetPositionY();
        float const distSq = dx * dx + dy * dy;
        if (distSq > nearestDistSq || visited.count(node.spawnId) || !IsUsable(caps, node))
            return;

        if (!map->IsGridLoaded(node.pos.GetPositionX(), node.pos.GetPositionY()) || !FindLiveNode(map, node))
            return;

        nearest = &node;
        nearestDistSq = distSq;
    });
    return nearest;
}
