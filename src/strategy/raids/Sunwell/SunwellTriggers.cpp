/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "SunwellTriggers.h"

#include "InterruptGate.h"
#include "Playerbots.h"
#include "PlayerbotSpec.h"
#include "SunwellHelpers.h"

using namespace SunwellHelpers;

bool SunwellStandingInHazardTrigger::IsActive()
{
    return bot->IsAlive() && IsInHazard(bot, GetHazards(bot));
}

bool SunwellInKalecgosConeTrigger::IsActive()
{
    return GetKalecgosConeThreat(bot) != nullptr;
}

bool SunwellInSpectralRealmTrigger::IsActive()
{
    if (GetActiveEncounter(bot) != SunwellEncounter::Kalecgos || PlayerBotSpec::IsHeal(bot) || !InSpectralRealm(bot))
        return false;

    Unit* sathrovarr = FindBoss(bot, NPC_SATHROVARR);
    return sathrovarr && AI_VALUE(Unit*, "current target") != sathrovarr;
}

bool SunwellPriorityAddTrigger::IsActive()
{
    if (PlayerBotSpec::IsTank(bot) || PlayerBotSpec::IsHeal(bot))
        return false;

    Unit* add = GetPriorityAdd(bot);
    return add && AI_VALUE(Unit*, "current target") != add;
}

bool SunwellInterruptTrigger::IsActive()
{
    Unit* caster = GetInterruptTarget(bot);
    return caster && MopDungeon::CanInterruptNow(bot, caster);
}

bool SunwellPyrogenicsTrigger::IsActive()
{
    return GetPyrogenicsTarget(bot) != nullptr;
}

bool SunwellTwinsTankSplitTrigger::IsActive()
{
    Unit* twin = GetAssignedTwin(bot);
    return twin && AI_VALUE(Unit*, "current target") != twin;
}

bool SunwellRangedSpreadTrigger::IsActive()
{
    if (!PlayerBotSpec::IsRanged(bot) || !bot->IsInCombat())
        return false;

    float const distance = GetRangedSpreadDistance(bot);
    return distance > 0.0f && !GetSpreadCircles(bot, distance - HAZARD_PADDING).empty();
}

bool SunwellMeteorSlashPositionTrigger::IsActive()
{
    MeteorSlashPlan const plan = GetMeteorSlashPlan(bot);
    return (plan.role == MeteorSlashPlan::Role::Soak || plan.role == MeteorSlashPlan::Role::Avoid) &&
        MeteorPlanNeedsMove(bot, plan);
}

bool SunwellBrutallusTakeOverTrigger::IsActive()
{
    MeteorSlashPlan const plan = GetMeteorSlashPlan(bot);
    return plan.role == MeteorSlashPlan::Role::TakeOver && plan.boss->GetVictim() != bot;
}

bool SunwellChasedTrigger::IsActive()
{
    Unit* chaser = bot->IsAlive() ? GetChaser(bot) : nullptr;
    return chaser && bot->GetExactDist2d(chaser) < KITE_UNTIL;
}

bool SunwellFelmystRegroupTrigger::IsActive()
{
    SunwellLogic::FelmystPhase const phase = GetFelmystPhase(bot);
    if (!bot->IsAlive() ||
        (phase != SunwellLogic::FelmystPhase::AirVapor && phase != SunwellLogic::FelmystPhase::AirMoving) ||
        GetChaser(bot))
    {
        return false;
    }

    RaidGeometry::Point rally{ 0.0f, 0.0f };
    return GetFelmystRally(bot, rally) &&
        SunwellLogic::NeedsRegroup(bot->GetExactDist2d(rally.x, rally.y), PlayerBotSpec::IsRangedDps(bot));
}
