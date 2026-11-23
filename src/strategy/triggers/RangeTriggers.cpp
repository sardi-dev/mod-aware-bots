/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#include "RangeTriggers.h"

#include "MoveSplineInit.h"
#include "Config.h"
#include "PlayerbotAIConfig.h"
#include "ReachTargetActions.h"
#include "PlayerbotSpec.h"
#include "Playerbots.h"
#include "ServerFacade.h"
#include "GameObject.h"
#include "Map.h"

#include <cmath>

static float GetSpeedInMotion(Unit* target)
{
    uint32 flags = target->GetUnitMovementFlags();
    bool backwards = flags & MOVEMENTFLAG_BACKWARD;
    UnitMoveType type = flags & MOVEMENTFLAG_FLYING ? (backwards ? MOVE_FLIGHT_BACK : MOVE_FLIGHT) :
        flags & MOVEMENTFLAG_SWIMMING ? (backwards ? MOVE_SWIM_BACK : MOVE_SWIM) :
        flags & MOVEMENTFLAG_WALKING ? MOVE_WALK : (backwards ? MOVE_RUN_BACK : MOVE_RUN);
    return target->GetSpeed(type);
}

namespace
{
    struct PvpKiteSettings
    {
        bool enabled;
        float triggerDistance;  // yards: an enemy player this close (to the bot's edge) makes it move
    };

    // BT-145: a kite is worth it against an enemy at least 15 percent slower than the bot.
    constexpr float KITE_SPEED_ADVANTAGE = 0.85f;
    // BT-145: yards from the duel flag a kite may end at (the out of bounds warning clears within 40).
    constexpr float DUEL_SAFE_RADIUS = 40.0f;

    // The same setting PvpKiteAction moves by.
    float KiteDistance()
    {
        static float const distance = sConfigMgr->GetFloatDefault("AiPlayerbot.PvpKite.Distance", 14.0f);
        return distance;
    }

    PvpKiteSettings const& KiteSettings()
    {
        static PvpKiteSettings const settings = []
        {
            PvpKiteSettings s;
            s.enabled = sConfigMgr->GetBoolDefault("AiPlayerbot.PvpKite.Enable", true);
            s.triggerDistance = sConfigMgr->GetFloatDefault("AiPlayerbot.PvpKite.TriggerDistance", 8.0f);
            return s;
        }();
        return settings;
    }
}

bool PvpKiteTrigger::IsActive()
{
    PvpKiteSettings const& settings = KiteSettings();
    if (!settings.enabled)
        return false;

    // Battlegrounds and arenas keep their own tactics (the movement is the tactics' business there).
    if (bot->InBattleground() || bot->InArena() || bot->IsMounted() || bot->IsInFlight())
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || target->GetTypeId() != TypeID::TYPEID_PLAYER || !bot->IsValidAttackTarget(target))
        return false;

    // A healer in a group stays by its group; a tank and a melee bot fight where they stand.
    if (PlayerBotSpec::IsTank(bot, true) || !PlayerBotSpec::IsRanged(bot, true) ||
        (bot->GetGroup() && PlayerBotSpec::IsHeal(bot, true)))
        return false;

    // Standing still only while casting; no moving when rooted or held.
    if (bot->IsNonMeleeSpellCasted(false) || bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL) || !bot->CanFreeMove())
        return false;

    // Nothing to back off from when the enemy is a ranged player too.
    Player* enemy = target->ToPlayer();
    if (PlayerBotSpec::IsRanged(enemy, true) || bot->GetExactDist(target) > settings.triggerDistance)
        return false;

    // BT-145: running only pays when the enemy cannot keep up. An enemy that is rooted or stunned but still within
    // melee reach is stepped away from; a snared or slower one is kited. Anything else is fought where the bot stands:
    // a low level mage with only Frost Nova used to run until it lost the duel by leaving its area (owner, 2026-11-11).
    bool const held = !target->CanFreeMove() || target->HasUnitState(UNIT_STATE_STUNNED);
    if (held && !bot->IsWithinMeleeRange(target))
        return false;

    // The effective run speed already carries every snare and speed bonus, so it decides alone.
    bool const slowed = target->GetSpeed(MOVE_RUN) < bot->GetSpeed(MOVE_RUN) * KITE_SPEED_ADVANTAGE;
    if (!held && !slowed)
        return false;

    // In a duel the kite must not end outside the duel area: the core warns at 50 yards from the flag and clears the
    // warning only within 40 (Player::CheckDuelDistance). The end point is taken a full kite distance from the bot,
    // straight away from the enemy, which is never short of where MoveAway goes; without a flag there is no kite.
    if (bot->duel)
    {
        GameObject* flag = bot->GetMap()->GetGameObject(bot->GetUInt64Value(PLAYER_FIELD_DUEL_ARBITER));
        if (!flag)
            return false;

        float const angle = target->GetAngle(bot);
        float const x = bot->GetPositionX() + std::cos(angle) * KiteDistance();
        float const y = bot->GetPositionY() + std::sin(angle) * KiteDistance();
        if (flag->GetExactDist2d(x, y) > DUEL_SAFE_RADIUS)
            return false;
    }
    return true;
}

bool EnemyTooCloseForSpellTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    return target && (target->GetVictim() != bot || target->isFrozen() || !target->CanFreeMove()) &&
        target->GetObjectSize() <= 10.0f && target->IsWithinCombatRange(bot, MIN_MELEE_REACH);
}

bool EnemyTooCloseForAutoShotTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, "current target");

    return target && (target->GetVictim() != bot || target->isFrozen() || !target->CanFreeMove()) &&
        bot->IsWithinMeleeRange(target);
}

bool EnemyTooCloseForShootTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    // target->IsWithinCombatRange()

    return target && (target->GetVictim() != bot || target->isFrozen() || !target->CanFreeMove()) &&
        target->IsWithinCombatRange(bot, MIN_MELEE_REACH);
}

bool EnemyTooCloseForMeleeTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    if (target && (target->GetTypeId() == TypeID::TYPEID_PLAYER))
        return false;

    return target && AI_VALUE2(bool, "inside target", "current target");
}

bool EnemyIsCloseTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    return target && sServerFacade->IsDistanceLessOrEqualThan(AI_VALUE2(float, "distance", "current target"),
        sPlayerbotAIConfig->tooCloseDistance);
}

bool EnemyWithinMeleeTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    return target && bot->IsWithinMeleeRange(target);
}

bool OutOfRangeTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, GetTargetName());
    // increase contact distance to prevent calculation error
    float dis = distance + CONTACT_DISTANCE;
    return target &&
        !bot->IsWithinCombatRange(
            target,
            dis);  // sServerFacade->IsDistanceGreaterThan(AI_VALUE2(float, "distance", GetTargetName()), distance);
}

EnemyOutOfSpellRangeTrigger::EnemyOutOfSpellRangeTrigger(PlayerbotAI* botAI)
    : OutOfRangeTrigger(botAI, "enemy out of spell range", botAI->GetRange("spell"))
{
}

bool PartyMemberToHealOutOfSpellRangeTrigger::IsActive()
{
    Unit* visible = AI_VALUE(Unit*, GetTargetName());
    Unit* target = ReachPartyMemberToHealAction::SelectHealReachTarget(botAI, visible);
    if (!target)
        return false;
    if (target != visible)
        return true;

    return sServerFacade->GetDistance2d(bot, target) > (distance + sPlayerbotAIConfig->contactDistance) ||
        !bot->IsWithinLOSInMap(target);
}

PartyMemberToHealOutOfSpellRangeTrigger::PartyMemberToHealOutOfSpellRangeTrigger(PlayerbotAI* botAI)
    : OutOfRangeTrigger(botAI, "party member to heal out of spell range", botAI->GetRange("heal") + 1.0f)
{
}

bool FarFromMasterTrigger::IsActive()
{
    return sServerFacade->IsDistanceGreaterThan(AI_VALUE2(float, "distance", "master target"), distance);
}
