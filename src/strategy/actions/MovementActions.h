#pragma once

#include "Action.h"
#include "PlayerbotAIConfig.h"
#include <cmath>
#include <ctime>
#include <utility>

class DynamicObject;
class PathGenerator;
class Player;
class PlayerbotAI;
class Unit;
class WorldObject;
class Position;
class SpellInfo;

#define ANGLE_45_DEG (static_cast<float>(M_PI) / 4.f)
#define ANGLE_90_DEG M_PI_2
#define ANGLE_120_DEG (2.f * static_cast<float>(M_PI) / 3.f)

enum class MovementPriority
{
    MOVEMENT_IDLE,
    MOVEMENT_NORMAL,
    MOVEMENT_COMBAT,
    MOVEMENT_FORCED
};

class MovementAction : public Action
{
public:
    MovementAction(PlayerbotAI* botAI, std::string const name);

    bool IsMovingAllowed();
    bool IsMovingAllowed(WorldObject* target);

    // -- combat movement
    bool ChaseTo(WorldObject* obj, float distance = 0.0f, float angle = 0.0f);
    bool ReachCombatTo(Unit* target, float distance = 0.0f);
    // BT-181: why the last ReachCombatTo returned false ("" after a move), and the path type when the path was refused.
    char const* ReachFailure() const { return reachFailure; }
    uint32 ReachFailurePathType() const { return reachFailurePathType; }

protected:
    void UpdateMovementState();
    void ClearIdleState();

    // -- Generic movement
    void WaitForReach(float distance);
    float MoveDelay(float distance, bool backwards = false);
    bool MoveTo(uint32 mapId, float x, float y, float z, bool idle = false, bool react = false, bool normal_only = false, bool exact_waypoint = false, MovementPriority priority = MovementPriority::MOVEMENT_NORMAL, bool lessDelay = false, bool backwards = false);
    bool MoveTo(WorldObject* target, float distance = 0.0f, MovementPriority priority = MovementPriority::MOVEMENT_NORMAL);
    /// Jump to a point instead of pathing to it - used by the battleground tactics to drop off
    /// ledges (the Warsong tunnel roofs, the Eye of the Storm towers) that pathing will not take.
    bool JumpTo(uint32 mapId, float x, float y, float z,
                MovementPriority priority = MovementPriority::MOVEMENT_NORMAL);
    bool MoveNear(uint32 mapId, float x, float y, float z, float distance = sPlayerbotAIConfig->contactDistance, MovementPriority priority = MovementPriority::MOVEMENT_NORMAL);
    bool MoveNear(WorldObject* target, float distance = sPlayerbotAIConfig->contactDistance, MovementPriority priority = MovementPriority::MOVEMENT_NORMAL);
    bool MoveAway(Unit* target, float distance = sPlayerbotAIConfig->fleeDistance, bool backwards = false);
    bool MoveFromGroup(float distance);
    bool MoveInside(uint32 mapId, float x, float y, float z, float distance = sPlayerbotAIConfig->followDistance, MovementPriority priority = MovementPriority::MOVEMENT_NORMAL);
    bool Move(float angle, float distance);
    bool Flee(Unit* target);
    bool FleePosition(Position pos, float radius, uint32 minInterval = 1000);
    bool CheckLastFlee(float curAngle, std::list<FleeInfo>& infoList);
    Position BestPositionForMeleeToFlee(Position pos, float radius);
    Position BestPositionForRangedToFlee(Position pos, float radius);

    bool IsWaitingForLastMove(MovementPriority priority);
    bool IsMovingAllowed(uint32 mapId, float x, float y, float z);
    bool IsDuplicateMove(uint32 mapId, float x, float y, float z);

    // -- Follow movement
    bool Follow(Unit* target, float distance = sPlayerbotAIConfig->followDistance);
    bool Follow(Unit* target, float distance, float angle);
    float GetFollowAngle();

protected:
    struct CheckAngle
    {
        float angle;
        bool strict;
    };

    // BT-386: the battleground tactics ask it before they choose between two objectives.
    // #747: true when the core will walk the bot to (x,y,z) along the navmesh; false when MovePoint would end in
    // the straight 2-point spline of MoveSplineInit::MoveTo (NOPATH / SHORT, or a shortcut that leaves the surface).
    bool PathIsWalkable(float x, float y, float z);

private:
    char const* reachFailure = "";
    uint32 reachFailurePathType = 0;
    // BT-182: when this action last told the leader "All paths not in LOS".
    time_t losErrorAt = 0;

    const Movement::PointsArray SearchForBestPath(float x, float y, float z, float& modified_z, int maxSearchCount = 5, bool normal_only = false, float step = 8.0f);
    static bool PathTypeIsWalkable(PathGenerator const& gen, Unit const* unit);
    // #747 (map 726): lift path points that run along a river bed to just under the liquid surface; true if any moved.
    bool RaisePathToWaterSurface(Movement::PointsArray& path, float destZ) const;
    // #747 (map 726): the bed under a point in liquid (where the navmesh polygons are), else z; inLiquid says which.
    float BedUnderLiquid(float x, float y, float z, bool* inLiquid = nullptr) const;
};

class MoveRandomAction : public MovementAction
{
public:
    MoveRandomAction(PlayerbotAI* botAI) : MovementAction(botAI, "move random") {}

    bool Execute(Event event) override;
    bool isUseful() override;
    bool isPossible() override;
};

class CombatFormationMoveAction : public MovementAction
{
public:
    CombatFormationMoveAction(PlayerbotAI* botAI, std::string name = "combat formation move", int moveInterval = 1000)
        : MovementAction(botAI, name), moveInterval(moveInterval)
    {
    }

    bool isUseful() override;
    bool Execute(Event event) override;

protected:
    Position AverageGroupPos(float dis = sPlayerbotAIConfig->sightDistance, bool ranged = false, bool self = false);
    Player* NearestGroupMember(float dis = sPlayerbotAIConfig->sightDistance);
    float AverageGroupAngle(Unit* from, bool ranged = false, bool self = false);
    Position GetNearestPosition(const std::vector<Position>& positions);
    int lastMoveTimer = 0;
    int moveInterval;
};

class TankFaceAction : public CombatFormationMoveAction
{
public:
    TankFaceAction(PlayerbotAI* botAI) : CombatFormationMoveAction(botAI, "tank face") {}

    bool Execute(Event event) override;
};

/// Ported from AC mod-playerbots MovementActions.h:105 / .cpp:1832-2054 (#149). AC's isUseful()/
/// Execute() cover three cases: an aura backed by a damaging DynamicObject (e.g. Rain of Fire), a
/// trap GameObject with a damage spell (e.g. Lava Bomb), and a trigger NPC with a periodic damage
/// aura (e.g. Lesser Shadow Fissure). The trigger NPC case came first (#149); BT-294 adds the
/// dynamic-object case, read from the bot's own auras at the moment of use (no cached Aura*, which
/// AC had to guard against a changed owner, and no search). The trap case needs a game-object
/// search every second and stays out.
class AvoidAoeAction : public MovementAction
{
public:
    AvoidAoeAction(PlayerbotAI* botAI, int moveInterval = 1000)
        : MovementAction(botAI, "avoid aoe"), moveInterval(moveInterval)
    {
    }

    bool isUseful() override;
    bool Execute(Event event) override;

protected:
    bool AvoidUnitWithDamageAura();
    // BT-294: the bot stands in a harmful area spell (an enemy's Consecration, Rain of Fire, a boss's ground effect).
    bool AvoidAuraWithDynamicObj();
    // The dynamic object of the first harmful area aura on the bot within the avoid radius, with its spell; nullptr if none.
    DynamicObject* HarmfulAreaUnderBot(SpellInfo const** spell);
    time_t lastTellTimer = 0;
    int lastMoveTimer = 0;
    int moveInterval;
};

/// Ported from AC mod-playerbots MovementActions.h/.cpp (#149). Uses the already-present
/// GetNearestPosition/CheckLastFlee (CombatFormationMoveAction/MovementAction) and the "recently flee
/// info" value.
class SetBehindTargetAction : public CombatFormationMoveAction
{
public:
    SetBehindTargetAction(PlayerbotAI* botAI) : CombatFormationMoveAction(botAI, "set behind") {}

    bool Execute(Event event) override;
};

class SetFacingTargetAction : public Action
{
public:
    SetFacingTargetAction(PlayerbotAI* botAI) : Action(botAI, "set facing") {}

    bool Execute(Event event) override;
    bool isUseful() override;
    bool isPossible() override;
};

class MoveFromGroupAction : public MovementAction
{
public:
    MoveFromGroupAction(PlayerbotAI* botAI, std::string const name = "move from group") : MovementAction(botAI, name) {}

    bool Execute(Event event) override;
};

// A ranged bot backs off from an enemy player that closes in on it (PvpKiteTrigger): it runs away from the player, facing
// away, and turns back to cast once there is room.
class PvpKiteAction : public MovementAction
{
public:
    PvpKiteAction(PlayerbotAI* botAI) : MovementAction(botAI, "pvp kite") {}

    bool Execute(Event event) override;
};

class RunAwayAction : public MovementAction
{
public:
    RunAwayAction(PlayerbotAI* botAI) : MovementAction(botAI, "runaway") {}

    bool Execute(Event event) override;
};

class FleeAction : public MovementAction
{
public:
    FleeAction(PlayerbotAI* botAI, float distance = sPlayerbotAIConfig->spellDistance)
        : MovementAction(botAI, "flee"), distance(distance)
    {
    }

    bool Execute(Event event) override;
    bool isUseful() override;

private:
    float distance;
};

// AC mod-playerbots MovementActions.h:97-103
class FleeWithPetAction : public MovementAction
{
public:
    FleeWithPetAction(PlayerbotAI* botAI) : MovementAction(botAI, "flee with pet") {}

    bool Execute(Event event) override;
};

// AC mod-playerbots MovementActions.h:153-173.
// 90 degree minimum angle prevents any frontal cleaves/breaths and avoids parry-hasting the boss.
// 120 degree maximum angle leaves a 120 degree symmetrical cone at the tail end which is usually enough to avoid
// tail swipes. Some dragons or mobs may have different danger zone angles, override if needed.
class RearFlankAction : public MovementAction
{
public:
    RearFlankAction(PlayerbotAI* botAI, float distance = 0.0f, float minAngle = ANGLE_90_DEG,
                    float maxAngle = ANGLE_120_DEG)
        : MovementAction(botAI, "rear flank")
    {
        this->distance = distance;
        this->minAngle = minAngle;
        this->maxAngle = maxAngle;
    }

    bool Execute(Event event) override;
    bool isUseful() override;

protected:
    float distance, minAngle, maxAngle;
};

// AC mod-playerbots MovementActions.h:175-183
class DisperseSetAction : public Action
{
public:
    DisperseSetAction(PlayerbotAI* botAI, std::string const name = "disperse set") : Action(botAI, name) {}

    bool Execute(Event event) override;
    float DEFAULT_DISPERSE_DISTANCE_RANGED = 5.0f;
    float DEFAULT_DISPERSE_DISTANCE_MELEE = 2.0f;
};

// AC mod-playerbots MovementActions.h:201-208
class MoveOutOfEnemyContactAction : public MovementAction
{
public:
    MoveOutOfEnemyContactAction(PlayerbotAI* botAI) : MovementAction(botAI, "move out of enemy contact") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

// AC mod-playerbots MovementActions.h:228-235
class MoveOutOfCollisionAction : public MovementAction
{
public:
    MoveOutOfCollisionAction(PlayerbotAI* botAI) : MovementAction(botAI, "move out of collision") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

// AC mod-playerbots MovementActions.h:246-259
class MoveInsideAction : public MovementAction
{
public:
    MoveInsideAction(PlayerbotAI* ai, float x, float y, float distance = 5.0f) : MovementAction(ai, "move inside")
    {
        this->x = x;
        this->y = y;
        this->distance = distance;
    }

    bool Execute(Event event) override;

protected:
    float x, y, distance;
};

// AC mod-playerbots MovementActions.h:261-290.
// FindNearestWaypoint() is declared in AC too but has no body in MovementActions.cpp - it is
// defined in AC NaxxActions_Shared.cpp:9, i.e. outside this file. Kept as a declaration only; it is
// never called from here, so there is no unresolved symbol until a Naxx port defines it.
class RotateAroundTheCenterPointAction : public MovementAction
{
public:
    RotateAroundTheCenterPointAction(PlayerbotAI* ai, std::string name, float center_x, float center_y,
                                     float radius = 40.0f, uint32 intervals = 16, bool clockwise = true,
                                     float start_angle = 0)
        : MovementAction(ai, name)
    {
        this->center_x = center_x;
        this->center_y = center_y;
        this->radius = radius;
        this->intervals = intervals;
        this->clockwise = clockwise;
        this->call_counters = 0;
        for (uint32 i = 0; i < intervals; i++)
        {
            float angle = start_angle + 2 * M_PI * i / intervals;
            waypoints.push_back(std::make_pair(center_x + cos(angle) * radius, center_y + sin(angle) * radius));
        }
    }

    bool Execute(Event event) override;

protected:
    virtual uint32 GetCurrWaypoint() { return 0; }
    uint32 FindNearestWaypoint();
    float center_x, center_y, radius;
    uint32 intervals, call_counters;
    bool clockwise;
    std::vector<std::pair<float, float>> waypoints;
};

// AC mod-playerbots MovementActions.h:300-315
class MoveAwayFromCreatureAction : public MovementAction
{
public:
    MoveAwayFromCreatureAction(PlayerbotAI* botAI, std::string name, uint32 creatureId, float range, bool alive = true)
        : MovementAction(botAI, name), creatureId(creatureId), range(range), alive(alive)
    {
    }

    bool Execute(Event event) override;
    bool isPossible() override;

private:
    uint32 creatureId;
    float range;
    bool alive;
};

// AC mod-playerbots MovementActions.h:317-331
class MoveAwayFromPlayerWithDebuffAction : public MovementAction
{
public:
    MoveAwayFromPlayerWithDebuffAction(PlayerbotAI* botAI, std::string name, uint32 spellId, float range)
        : MovementAction(botAI, name), spellId(spellId), range(range)
    {
    }

    bool Execute(Event event) override;
    bool isPossible() override;

private:
    uint32 spellId;
    float range;
};
