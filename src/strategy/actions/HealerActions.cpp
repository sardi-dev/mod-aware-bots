/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "HealerActions.h"

#include "MarkTriggers.h"
#include "MarkPullPolicy.h"
#include "ReachPolicy.h"
#include "Log.h"
#include <ctime>

#include "Event.h"
#include "MovePathMath.h"
#include "PathGenerator.h"
#include "Playerbots.h"

namespace
{
// How close the healer walks to a tank it cannot see, in yards: near enough to have line of sight around a corner.
constexpr float TANK_OUT_OF_SIGHT_APPROACH_DISTANCE = 8.0f;
// How close it walks to a tank it sees but stands too far from: a few yards inside its heal range, never into melee.
constexpr float TANK_IN_SIGHT_RANGE_MARGIN = 8.0f;
}  // namespace

bool HealerReachTankAction::Execute(Event /*event*/)
{
    Unit* tank = AI_VALUE(Unit*, "healer tank to reach");
    if (!tank)
        return false;

    bool const inSight = bot->IsWithinLOSInMap(tank);
    float const healReach =
        std::max(sPlayerbotAIConfig->healDistance - TANK_IN_SIGHT_RANGE_MARGIN, TANK_OUT_OF_SIGHT_APPROACH_DISTANCE);
    // In a fight the walk to a tank out of sight ends at the first point of the route where the healer sees him within
    // heal reach, not 8 yd from him: that walk took her round the corner into the pack the tank holds.
    if (ReachPolicy::UseHealingPath(inSight, bot->IsInCombat(), tank->IsInCombat()))
    {
        PathGenerator path(bot);
        path.CalculatePath(tank->GetPositionX(), tank->GetPositionY(), tank->GetPositionZ(), false);
        if (path.GetPathType() & (PATHFIND_NORMAL | PATHFIND_INCOMPLETE))
        {
            Movement::PointsArray const& points = path.GetPath();
            std::size_t const stop = MovePathMath::FirstPointWhere(points, [&](G3D::Vector3 const& p)
            {
                return tank->GetExactDist(p.x, p.y, p.z) <= healReach && tank->IsWithinLOS(p.x, p.y, p.z);
            });
            if (stop < points.size())
                return MoveTo(bot->GetMapId(), points[stop].x, points[stop].y, points[stop].z, false, false, false,
                              false, MovementPriority::MOVEMENT_COMBAT);
        }
    }
    return MoveTo(tank, inSight ? healReach : ReachPolicy::HiddenApproachDistance());
}

bool HealerReachTankAction::isUseful()
{
    if (botAI->HasStrategy("stay", botAI->GetState()) || bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL) != nullptr)
        return false;

    return AI_VALUE(Unit*, "healer tank to reach") != nullptr;
}

Value<Unit*>* CastHealerProactiveHotAction::GetTargetValue()
{
    return context->GetValue<Unit*>("healer proactive target");
}

bool MarkHealerMoveIntoSightAction::Execute(Event /*event*/)
{
    Unit* mark = nullptr;
    Player* tank = MarkHealerOutOfSightTrigger::BlockingTank(botAI, &mark);
    auto& state = botAI->markHealerSight;
    long const now = long(time(nullptr));
    if (!tank || !mark || state.tankGuid != tank->GetGUID() || state.markGuid != mark->GetGUID() ||
        !MarkPullPolicy::HealerWalksIntoSight(state.blockedSince, now, bot->GetDistance(tank), bot->IsWithinLOSInMap(tank)))
        return false;
    if (!state.walkSince)
    {
        state.walkSince = now;
        SF_LOG_INFO("playerbots", "[MARKPULL] %s moves into sight of %s at %.1f yd (mark %u)",
                    bot->GetName().c_str(), tank->GetName().c_str(), bot->GetDistance(tank), mark->GetEntry());
    }
    // The tank's own position is in its sight; MoveTo follows the navmesh around the obstacle.
    return MoveTo(tank->GetMapId(), tank->GetPositionX(), tank->GetPositionY(), tank->GetPositionZ(), false, false,
                  false, false, MovementPriority::MOVEMENT_COMBAT);
}
