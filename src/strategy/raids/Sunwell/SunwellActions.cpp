/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "SunwellActions.h"

#include "EncounterHelpers.h"
#include "Playerbots.h"
#include "PlayerbotSpec.h"
#include "RaidBotHelpers.h"
#include "SunwellHelpers.h"

using namespace SunwellHelpers;

namespace
{
// The dedicated interrupts of the 5.4.8 classes, by name (as the dungeon strategies cast them).
char const* const INTERRUPT_SPELLS[] = {
    "kick", "pummel", "wind shear", "mind freeze", "counterspell", "spell lock", "spear hand strike", "rebuke",
    "skull bash", "counter shot", "silencing shot", "silence", "strangulate", "arcane torrent"
};

char const* const PYROGENICS_REMOVERS[] = { "spellsteal", "purge", "dispel magic", "tranquilizing shot" };

// The taunt of each tanking class in 5.4.8.
char const* ClassTaunt(uint8 classId)
{
    switch (classId)
    {
        case CLASS_WARRIOR:
            return "taunt";
        case CLASS_PALADIN:
            return "hand of reckoning";
        case CLASS_DEATH_KNIGHT:
            return "dark command";
        case CLASS_DRUID:
            return "growl";
        case CLASS_MONK:
            return "provoke";
        default:
            return nullptr;
    }
}
}

bool SunwellMoveOutOfCirclesAction::MoveOutOf(std::vector<RaidGeometry::Circle> const& circles, float maxDistance,
    MovementPriority priority)
{
    constexpr float searchStep = 2.0f;
    RaidGeometry::Point safe{ 0.0f, 0.0f };
    if (!RaidGeometry::NearestPointOutside({ bot->GetPositionX(), bot->GetPositionY() }, circles, searchStep,
            maxDistance, safe))
    {
        return false;
    }

    // Check the ground on the way; when the step cannot be checked the path finder still takes the bot there.
    float stepX = safe.x;
    float stepY = safe.y;
    float stepZ = bot->GetPositionZ();
    float const distance = bot->GetExactDist2d(safe.x, safe.y);
    if (!EncounterHelpers::CanTakeStepTowards(bot, safe.x, safe.y, distance, stepX, stepY, stepZ))
    {
        stepX = safe.x;
        stepY = safe.y;
        stepZ = bot->GetPositionZ();
    }

    return MoveTo(bot->GetMapId(), stepX, stepY, stepZ, false, false, false, false, priority, true);
}

bool SunwellMoveOutOfCirclesAction::MoveToPoint(RaidGeometry::Point point, MovementPriority priority)
{
    float stepX = point.x;
    float stepY = point.y;
    float stepZ = bot->GetPositionZ();
    float const distance = bot->GetExactDist2d(point.x, point.y);
    if (distance < 0.5f)
        return false;

    if (!EncounterHelpers::CanTakeStepTowards(bot, point.x, point.y, distance, stepX, stepY, stepZ))
    {
        stepX = point.x;
        stepY = point.y;
        stepZ = bot->GetPositionZ();
    }

    return MoveTo(bot->GetMapId(), stepX, stepY, stepZ, false, false, false, false, priority, true);
}

bool SunwellMeteorSlashPositionAction::Execute(Event /*event*/)
{
    MeteorSlashPlan const plan = GetMeteorSlashPlan(bot);
    if (!MeteorPlanNeedsMove(bot, plan))
        return false;

    RaidGeometry::Point target = plan.spot;
    if (plan.role == MeteorSlashPlan::Role::Avoid)
    {
        target = SunwellLogic::MeteorAvoidSpot({ plan.boss->GetPositionX(), plan.boss->GetPositionY() },
            plan.boss->GetOrientation(), { bot->GetPositionX(), bot->GetPositionY() }, PlayerBotSpec::IsMelee(bot),
            RaidBotHelpers::MeleeRange(bot, plan.boss) * 0.7f);
    }
    else if (plan.role != MeteorSlashPlan::Role::Soak)
        return false;

    return MoveToPoint(target, MovementPriority::MOVEMENT_COMBAT);
}

bool SunwellBrutallusTakeOverAction::Execute(Event /*event*/)
{
    MeteorSlashPlan const plan = GetMeteorSlashPlan(bot);
    if (plan.role != MeteorSlashPlan::Role::TakeOver)
        return false;

    if (MeteorPlanNeedsMove(bot, plan))
        return MoveToPoint(plan.spot, MovementPriority::MOVEMENT_COMBAT);

    if (plan.boss->GetVictim() == bot)
        return false;

    char const* taunt = ClassTaunt(bot->getClass());
    // With the taunt on cooldown the tank's own rotation keeps building threat from the front.
    return taunt && botAI->CanCastSpell(taunt, plan.boss) && botAI->CastSpell(taunt, plan.boss);
}

bool SunwellKiteChaserAction::Execute(Event /*event*/)
{
    Unit* chaser = GetChaser(bot);
    if (!chaser)
        return false;

    RaidGeometry::Point const step = SunwellLogic::KitePoint({ bot->GetPositionX(), bot->GetPositionY() },
        { chaser->GetPositionX(), chaser->GetPositionY() }, GetRaidCenter(bot), KITE_STEP);
    return MoveToPoint(step, MovementPriority::MOVEMENT_FORCED);
}

bool SunwellFelmystRegroupAction::Execute(Event /*event*/)
{
    RaidGeometry::Point rally{ 0.0f, 0.0f };
    if (!GetFelmystRally(bot, rally))
        return false;

    // Ranged damage dealers keep 15 yd from the middle, on their own side of it.
    RaidGeometry::Point target = rally;
    if (PlayerBotSpec::IsRangedDps(bot))
        target = RaidGeometry::AwayFrom(rally, { bot->GetPositionX(), bot->GetPositionY() }, 15.0f);

    std::vector<RaidGeometry::Circle> const hazards = GetHazards(bot);
    if (RaidGeometry::InAnyCircle(target, hazards))
        return false;

    return MoveToPoint(target, MovementPriority::MOVEMENT_COMBAT);
}

bool SunwellLeaveHazardAction::Execute(Event /*event*/)
{
    constexpr float maxEscapeDistance = 30.0f;
    std::vector<RaidGeometry::Circle> const hazards = GetHazards(bot);
    if (!IsInHazard(bot, hazards))
        return false;

    return MoveOutOf(hazards, maxEscapeDistance, MovementPriority::MOVEMENT_FORCED);
}

bool SunwellSpreadRangedAction::Execute(Event /*event*/)
{
    constexpr float maxSpreadStep = 12.0f;
    float const distance = GetRangedSpreadDistance(bot);
    if (distance <= 0.0f)
        return false;

    // Do not spread into a hazard.
    std::vector<RaidGeometry::Circle> circles = GetSpreadCircles(bot, distance);
    std::vector<RaidGeometry::Circle> const hazards = GetHazards(bot);
    circles.insert(circles.end(), hazards.begin(), hazards.end());
    return MoveOutOf(circles, maxSpreadStep, MovementPriority::MOVEMENT_COMBAT);
}

bool SunwellLeaveKalecgosConeAction::Execute(Event /*event*/)
{
    Unit* kalecgos = GetKalecgosConeThreat(bot);
    if (!kalecgos)
        return false;

    // Melee take his flank within reach; ranged keep their distance, clamped inside the breath range.
    float distance = bot->GetExactDist2d(kalecgos);
    if (PlayerBotSpec::IsMelee(bot))
        distance = RaidBotHelpers::MeleeRange(bot, kalecgos) * 0.8f;
    else
        distance = std::clamp(distance, 10.0f, KALECGOS_CONE_RANGE);

    RaidGeometry::Point const side = RaidGeometry::FlankPoint({ kalecgos->GetPositionX(), kalecgos->GetPositionY() },
        kalecgos->GetOrientation(), RaidGeometry::PI / 2.0f, distance, { bot->GetPositionX(), bot->GetPositionY() });

    return MoveTo(bot->GetMapId(), side.x, side.y, bot->GetPositionZ(), false, false, false, false,
        MovementPriority::MOVEMENT_COMBAT, true);
}

bool SunwellAttackSathrovarrAction::Execute(Event /*event*/)
{
    Unit* sathrovarr = FindBoss(bot, NPC_SATHROVARR);
    return sathrovarr && Attack(sathrovarr);
}

bool SunwellAttackPriorityAddAction::Execute(Event /*event*/)
{
    Unit* add = GetPriorityAdd(bot);
    return add && Attack(add);
}

bool SunwellTwinsTankSplitAction::Execute(Event /*event*/)
{
    Unit* twin = GetAssignedTwin(bot);
    return twin && Attack(twin);
}

bool SunwellInterruptAction::Execute(Event /*event*/)
{
    Unit* caster = GetInterruptTarget(bot);
    if (!caster || !gate.Allow(bot, caster))
        return false;

    for (char const* spell : INTERRUPT_SPELLS)
    {
        if (botAI->CanCastSpell(spell, caster) && botAI->CastSpell(spell, caster))
            return true;
    }

    return false;
}

bool SunwellRemovePyrogenicsAction::Execute(Event /*event*/)
{
    Unit* alythess = GetPyrogenicsTarget(bot);
    if (!alythess)
        return false;

    for (char const* spell : PYROGENICS_REMOVERS)
    {
        if (botAI->CanCastSpell(spell, alythess) && botAI->CastSpell(spell, alythess))
            return true;
    }

    return false;
}
