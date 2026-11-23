/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * Terrain, position and grid-search helpers that Legends of Azeroth's core provides as members (WorldObject,
 * Unit, Map) and SkyFire does not. The module calls BotX(object, ...) where the LoA code called object->X(...).
 * Each helper follows LoA's implementation (references/loa-548-playerbots, file and function named per helper) on
 * SkyFire's APIs.
 */

#ifndef MOD_AWARE_BOTS_COMPAT_BOTTERRAIN_H
#define MOD_AWARE_BOTS_COMPAT_BOTTERRAIN_H

#include "DetourNavMeshQuery.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "IVMapManager.h"
#include "Map.h"
#include "MMapFactory.h"
#include "Object.h"
#include "Player.h"
#include "G3D/Vector3.h"
#include "Unit.h"
#include "VMapFactory.h"

#include <cmath>
#include <list>

constexpr float BOT_Z_OFFSET_FIND_HEIGHT = 0.5f;

// LoA WorldObject::GetMapHeight (Object.cpp)
inline float BotGetMapHeight(WorldObject const* object, float x, float y, float z, bool vmap = true, float distanceToSearch = DEFAULT_HEIGHT_SEARCH)
{
    if (z != MAX_HEIGHT)
        z += BOT_Z_OFFSET_FIND_HEIGHT;

    return object->GetMap()->GetHeight(object->GetPhaseMask(), x, y, z, vmap, distanceToSearch);
}

// LoA WorldObject::GetMapWaterOrGroundLevel (Object.cpp); SkyFire's map query has no collision height parameter.
inline float BotGetMapWaterOrGroundLevel(WorldObject const* object, float x, float y, float z, float* ground = nullptr)
{
    Unit const* unit = object->ToUnit();
    bool const swim = unit ? !unit->HasAuraType(SPELL_AURA_WATER_WALK) : false;
    return object->GetMap()->GetWaterOrGroundLevel(x, y, z, ground, swim);
}

// LoA Unit::GetHoverHeight (Unit.h)
inline float BotGetHoverHeight(Unit const* unit)
{
    return unit->HasUnitMovementFlag(MOVEMENTFLAG_HOVER) ? unit->GetFloatValue(UNIT_FIELD_HOVER_HEIGHT) : 0.0f;
}

// LoA Unit::isSwimming (Unit.h)
inline bool BotIsSwimming(Unit const* unit)
{
    return unit->HasUnitMovementFlag(MOVEMENTFLAG_SWIMMING);
}

// LoA WorldObject::GetWorldLocation: the object's own position with its map.
inline WorldLocation BotGetWorldLocation(WorldObject const* object)
{
    return WorldLocation(object->GetMapId(), object->GetPositionX(), object->GetPositionY(), object->GetPositionZ(), object->GetOrientation());
}

// LoA WorldObject::GetCreatureListWithEntryInGridAppend (Object.cpp): adds the found creatures to the list.
inline void BotGetCreatureListWithEntryInGridAppend(WorldObject const* object, std::list<Creature*>& creatures, uint32 entry, float range)
{
    std::list<Creature*> found;
    object->GetCreatureListWithEntryInGrid(found, entry, range);
    creatures.splice(creatures.end(), found);
}

// The navigation-mesh raycast LoA's PathGenerator runs with SetUseRaycast(true) from an explicit start point: moves
// dest back to the first wall of the mesh between start and dest. Returns false when the mesh has no polygon near
// the start (no mmaps, flying, water), which the caller treats as "not on a path".
inline bool BotNavMeshRaycast(WorldObject const* source, float startX, float startY, float startZ, float& destX, float& destY, float& destZ)
{
    MMAP::MMapManager* mmap = MMAP::MMapFactory::createOrGetMMapManager();
    dtNavMeshQuery const* query = mmap->GetNavMeshQuery(source->GetMapId(), source->GetInstanceId(), source->GetTerrainSwaps());
    if (!query)
        return false;

    dtQueryFilter filter;
    filter.setIncludeFlags(NAV_GROUND | NAV_WATER | NAV_MAGMA | NAV_SLIME);  // players, as PathGenerator::CreateFilter
    filter.setExcludeFlags(0);

    // Recast works in (y, z, x).
    float const start[3] = { startY, startZ, startX };
    float const end[3] = { destY, destZ, destX };
    float const extents[3] = { 3.0f, 5.0f, 3.0f };
    float closest[3];
    dtPolyRef startRef = 0;
    if (dtStatusFailed(query->findNearestPoly(start, extents, &filter, &startRef, closest)) || !startRef)
        return false;

    float hitT = 0.0f;
    float hitNormal[3];
    dtPolyRef visited[64];
    int visitedCount = 0;
    if (dtStatusFailed(query->raycast(startRef, closest, end, &filter, &hitT, hitNormal, visited, &visitedCount, 64)))
        return false;

    if (hitT < 1.0f)
    {
        destX = closest[2] + (end[2] - closest[2]) * hitT;
        destY = closest[0] + (end[0] - closest[0]) * hitT;
        destZ = closest[1] + (end[1] - closest[1]) * hitT;
    }

    return true;
}

// BT-158: navmesh paths omit dynamic game object collision, including closed doors.
constexpr float BOT_DOOR_RAY_HEIGHT = 1.5f;

// BT-166: the dynamic tree holds every game object model (cannons, levers, chests), not only doors, and a bot that
// took a Deadmines cannon for a closed door stood still for two minutes (live 2026-11-13). A hit only blocks the
// path when a closed door game object stands near the hit point. Returns that door's entry, or 0.
uint32 BotClosedDoorEntryNear(Player* bot, G3D::Vector3 const& point);

template<class Points>
inline bool BotPathBlockedByClosedDoor(Player* bot, Points const& path, size_t& segment, G3D::Vector3& hit,
    uint32* doorEntry = nullptr)
{
    for (size_t i = 1; i < path.size(); ++i)
    {
        auto const& a = path[i - 1];
        auto const& b = path[i];
        float x, y, z;
        if (bot->GetMap()->getObjectHitPos(bot->GetPhaseMask(), a.x, a.y, a.z + BOT_DOOR_RAY_HEIGHT,
            b.x, b.y, b.z + BOT_DOOR_RAY_HEIGHT, x, y, z, 0.0f))
        {
            G3D::Vector3 const point(x, y, z - BOT_DOOR_RAY_HEIGHT);
            uint32 const entry = BotClosedDoorEntryNear(bot, point);
            if (!entry)
                continue;
            segment = i - 1;
            hit = point;
            if (doorEntry)
                *doorEntry = entry;
            return true;
        }
    }
    return false;
}

// LoA Map::CheckCollisionAndGetValidCoords (Map.cpp): from start towards dest, stop at the first collision with the
// navigation mesh, static map geometry or a game object, then put dest on the ground. Returns false when the point
// is invalid, or when failOnCollision is set and something was hit.
inline bool BotCheckCollisionAndGetValidCoords(Map const* map, WorldObject const* source, float startX, float startY, float startZ,
    float& destX, float& destY, float& destZ, bool failOnCollision = true)
{
    if (!Skyfire::IsValidMapCoord(startX, startY, startZ) || !Skyfire::IsValidMapCoord(destX, destY, destZ))
        return false;

    Unit const* unit = source->ToUnit();
    bool const isWaterNext = map->IsInWater(destX, destY, destZ);
    bool const onMesh = BotNavMeshRaycast(source, startX, startY, startZ, destX, destY, destZ);
    bool const notOnGround = !onMesh || isWaterNext || (unit && unit->IsFlying());

    bool collided = false;
    float const halfHeight = 1.0f;  // SkyFire has no collision height per model; a man-sized half height

    // Not on the ground: check static geometry with the vmaps.
    if (notOnGround)
    {
        if (VMAP::VMapFactory::createOrGetVMapManager()->getObjectHitPos(source->GetMapId(),
            startX, startY, startZ + halfHeight, destX, destY, destZ + halfHeight, destX, destY, destZ, -CONTACT_DISTANCE))
            collided = true;
        destZ -= halfHeight;
    }

    // Dynamic collision: game objects such as doors.
    if (const_cast<Map*>(map)->getObjectHitPos(source->GetPhaseMask(),
        startX, startY, startZ + halfHeight, destX, destY, destZ + halfHeight, destX, destY, destZ, -CONTACT_DISTANCE))
        collided = true;
    destZ -= halfHeight;

    // Put the point on the ground; with no ground under it, fall back to the terrain height, as LoA does.
    float const groundZ = map->GetHeight(source->GetPhaseMask(), destX, destY, destZ + halfHeight, true, DEFAULT_HEIGHT_SEARCH);
    if (groundZ <= INVALID_HEIGHT)
    {
        if (unit && !unit->CanFly())
        {
            float const gridHeight = map->GetHeight(source->GetPhaseMask(), destX, destY, MAX_HEIGHT, false, DEFAULT_HEIGHT_SEARCH);
            if (gridHeight <= INVALID_HEIGHT)
                return false;
            destZ = gridHeight + BotGetHoverHeight(unit);
        }
    }
    else if (!unit || !unit->CanFly() || destZ < groundZ)
        destZ = groundZ;

    return !failOnCollision || !collided;
}

#endif
