/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option) any later version.
 *
 * Ported from the mod-playerbots module for AzerothCore (Mgr/GatherNode/GatherNodeMgr.h), with these changes for this
 * core and server:
 *  - the index is bucketed by map and a square cell of CELL_SIZE yards instead of by zone, so building it needs no terrain
 *    data at startup and a query reads only the cells around the bot (a zone-sized neighbourhood);
 *  - a live node is looked up by the guid a continent spawn gets here (the spawn id is the low guid on a map without
 *    instances), not through a spawn id store;
 *  - quest-only gathering chests are told apart by the loot template's negative quest chance;
 *  - the mining pick is part of "usable".
 */

#ifndef PLAYERBOT_GATHERNODEMGR_H
#define PLAYERBOT_GATHERNODEMGR_H

#include "Define.h"
#include "WorldPosition.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>

class GameObject;
class Map;
class Player;
class PlayerbotAI;

struct GatherNodeSpawn
{
    uint32 spawnId{0};
    uint32 entry{0};
    WorldPosition pos{};
    uint32 skillId{0};  // SKILL_HERBALISM or SKILL_MINING
    uint32 reqSkillValue{0};
};

/// Index of gatherable gameobject spawns (herbs and veins) on continent maps, built once from the spawn data at world
/// start. Immutable afterwards, so it is read from any map thread without a lock.
class GatherNodeMgr
{
public:
    static GatherNodeMgr& instance()
    {
        static GatherNodeMgr instance;
        return instance;
    }

    void Load();

    // Cheap check for CheckRpgStatusAvailable: any node around the bot that it can harvest with its skills now?
    bool HasUsableNodes(Player* bot);

    // Random pick among the 4 nearest unvisited usable nodes around the bot (jitter keeps bots from herding on one route).
    GatherNodeSpawn const* GetNextNode(Player* bot, std::unordered_set<uint32> const& visited);

    // The live (spawned, ready) object of a node, nullptr when there is none or its grid is not loaded.
    static GameObject* FindLiveNode(Map* map, GatherNodeSpawn const& node);
    static GameObject* FindLiveNode(Map* map, uint32 entry, uint32 spawnId);

    // True when the spawn point is verifiably empty right now: grid loaded and no live object. An unloaded grid is
    // "unknown" (false). Never recorded anywhere: nodes respawn.
    static bool IsVerifiablyDown(Map* map, GatherNodeSpawn const& node);

    // Nearest unvisited usable node within `radius` that is verifiably up right now, or nullptr.
    GatherNodeSpawn const* GetNearestLiveNode(Player* bot, std::unordered_set<uint32> const& visited, float radius);

    uint32 NodeCount() const { return _count; }

private:
    // What the bot can harvest, worked out once per query: the skill checks and the pick search are not repeated per node.
    struct Caps
    {
        bool herbalism{false};
        uint32 herbalismValue{0};
        bool mining{false};
        uint32 miningValue{0};
    };
    static Caps CapsOf(PlayerbotAI* botAI, Player* bot);
    static bool IsUsable(Caps const& caps, GatherNodeSpawn const& node);

    using Cell = std::vector<GatherNodeSpawn>;
    using CellMap = std::unordered_map<uint32 /*cell key*/, Cell>;

    static uint32 CellKey(int32 cx, int32 cy);
    static int32 CellOf(float coordinate);

    // Calls fn(node) for every node in the cells within `reach` yards (rounded up to whole cells) of the position.
    template <typename Fn>
    void ForNodesAround(uint32 mapId, WorldPosition const& pos, float reach, Fn fn) const;

    std::unordered_map<uint32 /*mapId*/, CellMap> _nodes;
    uint32 _count{0};
};

#define sGatherNodeMgr GatherNodeMgr::instance()

#endif
