/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

// Two-flag battleground bot tactics - BT-153, docs/bg-sheets/warsong-gulch.md (BT-470: the maps are the plan's, see
// WsgPlan.h). The team plan, the roles and the option scores live in WsgPlan.cpp and Utils/WsgPlanLogic.h; this file is
// the part that needs the tactics class: it stores the objective the plan chose, reacts when the plan state changes, and
// escorts the carrier.

#include "BattleGroundTactics.h"

// DatabaseEnv.h must precede the battleground headers (see BattleGroundTactics.cpp).
#include "DatabaseEnv.h"

#include "Battleground.h"
#include "ObjectAccessor.h"
#include "MotionMaster.h"
#include "PositionValue.h"
#include "Playerbots.h"
#include "ServerFacade.h"
#include "WsgCarrier.h"
#include "WsgPlan.h"

namespace
{
// An escort that is closer than this to the carrier does not need a move command.
constexpr float ESCORT_FOLLOW_ARM = 33.0f;
// MoveNear to a carrier: stay this close once the escort moves (yards).
constexpr float ESCORT_CLOSE_UP = 5.0f;
}  // namespace

void BGTactics::wsgLeaveGraveyard(Battleground* bg, WsgPlan::Objective& objective)
{
    WsgPlan::Objective hop;
    if (!WsgPlan::GraveyardHop(bg, bot, objective, isCarryingObjective(bg), hop))
        return;

    // Only when MoveTo would refuse the objective from here and accept the exit (the navmesh decides, so a mesh that
    // is later fixed needs no change here). Twin Peaks (BT-473): the middle graveyards stand on islands with no walking
    // link to the mesh (module issue 747), so the landing point this hop names is refused from the island as well, the
    // test fails on purpose and the jump stays with tpJumpDown (called by selectObjectiveWp when the walk to the
    // objective fails). The hop data is the landing tpJumpDown uses, ready for the day the mesh is linked.
    if (PathIsWalkable(objective.x, objective.y, objective.z) || !PathIsWalkable(hop.x, hop.y, hop.z))
        return;

    objective.x = hop.x;
    objective.y = hop.y;
    objective.z = hop.z;
}

bool BGTactics::selectObjectiveWsg(Battleground* bg)
{
    if (!WsgPlan::Enabled(bg))
        return false;

    uint8 const strategyAlliance = GetBotStrategyForTeam(bg, TEAM_ALLIANCE);
    uint8 const strategyHorde = GetBotStrategyForTeam(bg, TEAM_HORDE);

    WsgPlan::Objective objective;
    if (!WsgPlan::Decide(bg, bot, strategyAlliance, strategyHorde, isCarryingObjective(bg),
                         objective))
        return false;

    wsgLeaveGraveyard(bg, objective);

    PositionMap& posMap = context->GetValue<PositionMap&>("position")->Get();
    PositionInfo pos = posMap["bg objective"];
    pos.Set(objective.x, objective.y, objective.z, bot->GetMapId());
    posMap["bg objective"] = pos;

    // Escorts walk with the carrier (a human carrier too), at the escort distance of their tier.
    if (objective.followGuid)
    {
        Player* carrier = ObjectAccessor::GetPlayer(*bot, objective.followGuid);
        float const stay = WsgPlan::EscortDistance(bg, bot);
        if (carrier && carrier != bot && stay > 0.0f &&
            sServerFacade->GetDistance2d(bot, carrier) < ESCORT_FOLLOW_ARM)
            Follow(carrier, stay);
    }

    return true;
}

void BGTactics::wsgTick(Battleground* bg)
{
    if (!WsgPlan::Enabled(bg) || !bot->IsAlive())
        return;

    // A call-out that is due (the bot's own delay passed, the team's limiter allows it).
    WsgPlan::DeliverCalls(botAI);

    WsgCarrier::DropTarget(botAI, bot);

    // A carrier uses its defensive, speed and freedom spells while it runs (owner rule 2026-11-17).
    if (isCarryingObjective(bg))
        WsgCarrier::UseHelpers(botAI, bot, bg);

    uint8 const strategyAlliance = GetBotStrategyForTeam(bg, TEAM_ALLIANCE);
    uint8 const strategyHorde = GetBotStrategyForTeam(bg, TEAM_HORDE);

    // Both are asked every time: the graveyard check also keeps the bot's dwell clock.
    bool const leaveGraveyard = WsgPlan::MustLeaveGraveyard(bg, bot);
    bool const planChanged = WsgPlan::PlanChanged(bg, bot, strategyAlliance, strategyHorde);
    if (leaveGraveyard || planChanged)
    {
        resetObjective();
        return;
    }

    // Hunt, escort and cover objectives follow a moving target: keep the stored position current.
    WsgPlan::Objective moving;
    if (WsgPlan::RefreshDynamic(bg, bot, moving))
    {
        wsgLeaveGraveyard(bg, moving);
        PositionMap& posMap = context->GetValue<PositionMap&>("position")->Get();
        PositionInfo pos = posMap["bg objective"];
        if (pos.isSet())
        {
            pos.Set(moving.x, moving.y, moving.z, bot->GetMapId());
            posMap["bg objective"] = pos;
        }
    }
}

bool BGTactics::protectFcWsg(Battleground* bg)
{
    Unit* teamFC = AI_VALUE(Unit*, "team flag carrier");
    if (!teamFC || teamFC == bot)
        return false;

    // Covering MID fighters can peel without becoming followers.
    float const stay = WsgPlan::EscortDistance(bg, bot);
    if (stay <= 0.0f)
    {
        if (Unit* attacker = WsgPlan::PeelTarget(bg, bot, teamFC))
            if (AI_VALUE(Unit*, "current target") != attacker && EngageThreat(bot, attacker))
                return true;
        return false;
    }

    // Like the strike run, replace a chase but yield during a point move for instant casts.
    bool const closeInCombat = WsgPlan::EscortCloseDue(bg, bot);
    if (closeInCombat && (bot->isMoving() || !bot->IsStopped()) &&
        bot->GetMotionMaster()->GetCurrentMovementGeneratorType() != CHASE_MOTION_TYPE)
        return false;
    if (closeInCombat && !WsgPlan::IsHealerFollower(bg, bot))
        return MoveNear(bot->GetMapId(), teamFC->GetPositionX(), teamFC->GetPositionY(), teamFC->GetPositionZ(),
                        ESCORT_CLOSE_UP, MovementPriority::MOVEMENT_COMBAT);

    // Owner rule 2026-11-17: the healers follow the carrier. A healer that heals stands where it is while its target runs
    // out of reach (the first live match: no healer was seen with a carrier), so it closes up in a fight as well, with a
    // leash and a closing distance so it does not stop and go at the edge. Its heals come from the usual heal target
    // value, which already puts the carrier first.
    if (WsgPlan::IsHealerFollower(bg, bot))
    {
        if (bot->HasUnitState(UNIT_STATE_ROOT | UNIT_STATE_CONTROLLED))
            return false;
        float const dist = bot->GetDistance(teamFC);
        if (WsgLogic::HealerStopsToHeal(dist, teamFC->GetHealthPct()))
        {
            wsgHealerClosing = false;
            return false;
        }

        wsgHealerClosing = WsgLogic::HealerShouldClose(dist, wsgHealerClosing);
        if (!wsgHealerClosing)
            return false;

        return MoveNear(bot->GetMapId(), teamFC->GetPositionX(), teamFC->GetPositionY(), teamFC->GetPositionZ(),
                        ESCORT_CLOSE_UP, bot->IsInCombat() ? MovementPriority::MOVEMENT_COMBAT : MovementPriority::MOVEMENT_NORMAL);
    }

    // Good rule: peel the player that is hitting the carrier. EngageThreat selects it and switches to combat.
    if (Unit* attacker = WsgPlan::PeelTarget(bg, bot, teamFC))
    {
        if (AI_VALUE(Unit*, "current target") == attacker)
            return false;
        if (EngageThreat(bot, attacker))
            return true;
    }

    // Out of combat and too far: close up to the carrier.
    if (!bot->IsInCombat() && !bot->IsWithinDistInMap(teamFC, stay))
        return MoveNear(bot->GetMapId(), teamFC->GetPositionX(), teamFC->GetPositionY(), teamFC->GetPositionZ(),
                        ESCORT_CLOSE_UP, MovementPriority::MOVEMENT_NORMAL);

    return false;
}
