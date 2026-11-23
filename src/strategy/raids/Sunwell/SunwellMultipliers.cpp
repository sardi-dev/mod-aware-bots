/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "SunwellMultipliers.h"

#include "MovementActions.h"
#include "Playerbots.h"
#include "PlayerbotSpec.h"
#include "RaidBotHelpers.h"
#include "ReachTargetActions.h"
#include "SunwellHelpers.h"

using namespace SunwellHelpers;

float SunwellHoldOutsideHazardMultiplier::GetValue(Action* action)
{
    if (!dynamic_cast<ReachTargetAction*>(action) && !dynamic_cast<CastReachTargetSpellAction*>(action))
        return 1.0f;

    if (botAI->GetState() != BOT_STATE_COMBAT || GetActiveEncounter(bot) == SunwellEncounter::None ||
        PlayerBotSpec::IsTank(bot) || !PlayerBotSpec::IsMelee(bot))
    {
        return 1.0f;
    }

    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target)
        return 1.0f;

    for (RaidGeometry::Circle const& hazard : GetHazards(bot))
    {
        float const distance = std::hypot(target->GetPositionX() - hazard.center.x,
            target->GetPositionY() - hazard.center.y);
        if (distance + RaidBotHelpers::MeleeRange(bot, target) < hazard.radius)
            return 0.0f;
    }

    return 1.0f;
}

float SunwellFelmystAirMultiplier::GetValue(Action* action)
{
    if (!dynamic_cast<ReachTargetAction*>(action) && !dynamic_cast<CastReachTargetSpellAction*>(action) &&
        !dynamic_cast<SetBehindTargetAction*>(action))
    {
        return 1.0f;
    }

    if (botAI->GetState() != BOT_STATE_COMBAT || GetFelmystPhase(bot) == SunwellLogic::FelmystPhase::Ground)
        return 1.0f;

    Unit* target = AI_VALUE(Unit*, "current target");
    return target && target->GetEntry() == NPC_FELMYST ? 0.0f : 1.0f;
}

float SunwellMeteorSoakMultiplier::GetValue(Action* action)
{
    if (!dynamic_cast<SetBehindTargetAction*>(action) || botAI->GetState() != BOT_STATE_COMBAT)
        return 1.0f;

    MeteorSlashPlan::Role const role = GetMeteorSlashPlan(bot).role;
    return role == MeteorSlashPlan::Role::Soak || role == MeteorSlashPlan::Role::TakeOver ? 0.0f : 1.0f;
}
