/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * Helpers for Travel/ and strategy/RPG/: what Legends of Azeroth's core offers as members (quest accept, transport
 * passengers, path length, reachability from the object's own position) and SkyFire does not. Each helper says which LoA
 * call it replaces.
 */

#ifndef MOD_AWARE_BOTS_COMPAT_BOTCOMPAT_T_H
#define MOD_AWARE_BOTS_COMPAT_BOTCOMPAT_T_H

#include "BotCompat_S.h"
#include "BotTerrain.h"
#include "GameObject.h"
#include "GameObjectAI.h"
#include "MoveSpline.h"
#include "MoveSplineInit.h"
#include "PathGenerator.h"
#include "Player.h"
#include "QuestDef.h"
#include "Transport.h"
#include "Unit.h"

#include <cmath>

// LoA Map::CanReachPositionAndGetValidCoords (Map.cpp), overload without a start point: the object's own position is
// the start. The start-point overload is in BotCompat_S.h.
inline bool BotCanReachPositionAndGetValidCoords(Map const* map, WorldObject const* source, float& destX, float& destY,
    float& destZ, bool failOnCollision = true, bool failOnSlopes = true)
{
    return BotCanReachPositionAndGetValidCoords(map, source, source->GetPositionX(), source->GetPositionY(),
        source->GetPositionZ(), destX, destY, destZ, failOnCollision, failOnSlopes);
}

// LoA Player::AddQuestAndCheckCompletion: add the quest, complete it when its objectives are already met, and tell the
// giver's script and AI. SkyFire's Player::AddQuest already completes the quest and runs the script hooks and a
// creature's AI hook; only a game object giver's AI hook is left to the caller (WorldSession::
// HandleQuestgiverAcceptQuestOpcode calls it).
inline void BotAddQuestAndCheckCompletion(Player* player, Quest const* quest, Object* questGiver)
{
    player->AddQuest(quest, questGiver);

    if (questGiver)
        if (GameObject* gameObject = questGiver->ToGameObject())
            if (gameObject->AI())
                gameObject->AI()->QuestAccept(player, quest);
}

// LoA Transport::AddPassenger(passenger, withAll). With withAll the passenger is first taken off the transport it is on,
// then boarded with the offset of where it stands (the position it has in the world right now), and units ignore
// pathfinding on the deck. SkyFire's AddPassenger only links the passenger, so the offset is set here on the public
// movement info.
inline void BotAddPassenger(Transport* transport, WorldObject* passenger, bool withAll = false)
{
    if (!withAll || !transport->IsInWorld())
    {
        transport->AddPassenger(passenger);
        return;
    }

    if (transport->GetPassengers().count(passenger))
        return;

    if (Transport* previous = passenger->GetTransport())
        previous->RemovePassenger(passenger);

    float x, y, z, o;
    passenger->GetPosition(x, y, z, o);

    transport->AddPassenger(passenger);
    transport->CalculatePassengerOffset(x, y, z, &o);
    passenger->m_movementInfo.transport.pos.Relocate(x, y, z, o);

    if (Unit* unit = passenger->ToUnit())
        unit->AddUnitState(UNIT_STATE_IGNORE_PATHFINDING);
}

// LoA Transport::RemovePassenger(passenger, withAll). LoA stops a rider whose spline is still bound to the transport
// before it detaches, so the next spline update does not put it at world coordinates (0, 0, 0). withAll = false only
// unlinked the passenger in LoA, and SkyFire's RemovePassenger always resets it, so callers pass true.
// PORT: SkyFire's MoveSpline has no IsEnteringTransport; a rider that is still stepping onto the deck is stopped too.
inline void BotRemovePassenger(Transport* transport, WorldObject* passenger, bool withAll = true)
{
    if (withAll && transport->GetPassengers().count(passenger))
        if (Unit* unit = passenger->ToUnit())
            if (unit->movespline->Initialized() && unit->movespline->onTransport)
                unit->StopMoving();

    transport->RemovePassenger(passenger);
}

// LoA PathGenerator::getPathLength (PathGenerator.h): the length of the path points, from the start position.
inline float BotPathLength(PathGenerator const& path)
{
    Movement::PointsArray const& points = path.GetPath();
    if (points.empty())
        return 0.0f;

    G3D::Vector3 const& start = path.GetStartPosition();
    float length = std::sqrt((points[0].x - start.x) * (points[0].x - start.x) +
                             (points[0].y - start.y) * (points[0].y - start.y) +
                             (points[0].z - start.z) * (points[0].z - start.z));
    for (size_t i = 1; i < points.size(); ++i)
        length += (points[i] - points[i - 1]).length();
    return length;
}

// LoA PathGenerator::ShortenPathUntilDist, for its callers here, which only use the end point of the shortened path:
// walking back from the end, the first point at least `dist` from the target that the unit can still see from the
// point before it. Works on a copy, since SkyFire's path is read-only.
inline G3D::Vector3 BotShortenedPathEnd(Unit const* source, PathGenerator const& path, G3D::Vector3 const& target, float dist)
{
    Movement::PointsArray const& points = path.GetPath();
    if (points.empty())
        return target;
    if (path.GetPathType() == PATHFIND_BLANK || points.size() < 2)
        return points.back();

    float const distSq = dist * dist;
    if ((points[0] - target).squaredLength() < distSq || (points.back() - target).squaredLength() >= distSq)
        return points.back();

    float const eye = BOT_COLLISION_HEIGHT;
    for (size_t i = points.size() - 1; i > 0; --i)
    {
        G3D::Vector3 const& before = points[i - 1];
        if ((before - target).squaredLength() >= distSq)
        {
            // the end point lies on the segment before -> points[i], at `dist` from the target
            G3D::Vector3 const& after = points[i];
            G3D::Vector3 const dir = after - before;
            float const a = dir.squaredLength();
            float const b = 2.0f * dir.dot(before - target);
            float const c = (before - target).squaredLength() - distSq;
            float const disc = b * b - 4.0f * a * c;
            if (a <= 0.0f || disc < 0.0f)
                return before;
            float const t = std::min(1.0f, std::max(0.0f, (-b - std::sqrt(disc)) / (2.0f * a)));
            return before + dir * t;
        }

        // keep the last point that still sees the target's direction from the point before it
        // PORT: SkyFire has no GetHitSpherePointFor; the line of sight starts at the unit's eye height above the point.
        if (!source->GetMap()->isInLineOfSight(points[i].x, points[i].y, points[i].z + eye,
                before.x, before.y, before.z + eye, source->GetPhaseMask(), VMAP::ModelIgnoreFlags::Nothing))
            return points[i];
    }
    return points[1];
}

// LoA MotionMaster::MoveSmoothPath (MotionMaster.cpp): one spline through the points.
// PORT: SkyFire's MotionMaster cannot push LoA's EffectMovementGenerator from outside, so the spline runs on its own;
// the unit's movement generator is left idle while it moves.
inline void BotMoveSmoothPath(Unit* unit, Position const* pathPoints, size_t pathSize, bool walk, bool fly)
{
    Movement::MoveSplineInit init(unit);
    if (fly)
    {
        init.SetFly();
        init.SetUncompressed();
        init.SetSmooth();
    }

    Movement::PointsArray path;
    path.reserve(pathSize);
    for (size_t i = 0; i < pathSize; ++i)
        path.emplace_back(pathPoints[i].GetPositionX(), pathPoints[i].GetPositionY(), pathPoints[i].GetPositionZ());

    init.MovebyPath(path);
    init.SetWalk(walk);
    init.Launch();
}

// LoA Map::GetGridHeight: the raw terrain height at a point, without vmaps.
inline float BotGridHeight(Map const* map, float x, float y)
{
    return map->GetHeight(x, y, MAX_HEIGHT, false);
}

#endif
