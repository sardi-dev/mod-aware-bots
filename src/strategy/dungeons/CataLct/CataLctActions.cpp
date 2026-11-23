/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// MoP-only dungeon strategy: Lost City of the Tol'vir (epic #373, issue #666). See CataLctShared.h for sources and ids.

#include "CataLctActions.h"
#include "CataLctShared.h"
#include "EncounterHelpers.h"
#include "Map.h"
#include "Playerbots.h"
#include "Random.h"
#include <cmath>

using namespace EncounterHelpers;

bool LctAvoidLandMineAction::Execute(Event /*event*/)
{
    Position pos;
    float radius = 0.0f;
    if (!CataLct::LandMineDanger(botAI, bot, pos, radius))
        return false;

    // AC idiom: FoSActions.cpp:15-18, UBActions.cpp:61-69
    return FleePosition(pos, radius);
}

bool LctAvoidShockwaveAction::Execute(Event /*event*/)
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    if (!CataLct::ShockwaveDanger(botAI, bot, x, y, z))
        return false;

    if (IsWaitingForLastMove(MovementPriority::MOVEMENT_COMBAT))
        return false;

    return MoveTo(bot->GetMapId(), x, y, z, false, false, false, true, MovementPriority::MOVEMENT_COMBAT);
}

bool LctDropFromWallAction::Execute(Event /*event*/)
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    if (!CataLct::ThrownOnWall(botAI, bot, x, y, z))
        return false;

    if (IsWaitingForLastMove(MovementPriority::MOVEMENT_COMBAT))
        return false;

    return MoveTo(bot->GetMapId(), x, y, z, false, false, false, true, MovementPriority::MOVEMENT_COMBAT);
}

bool LctLeaveDustFlailAction::Execute(Event /*event*/)
{
    Unit* source = CataLct::DustFlailBoss(botAI, bot);
    if (!source)
        return false;

    if (IsWaitingForLastMove(MovementPriority::MOVEMENT_COMBAT))
        return false;

    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    if (!CataLct::LateralPoint(bot, source, x, y, z))
        return false;

    return MoveTo(bot->GetMapId(), x, y, z, false, false, false, true, MovementPriority::MOVEMENT_COMBAT);
}

bool LctKillCrocoliskAction::Execute(Event /*event*/)
{
    Unit* target = CataLct::CrocoliskFocus(botAI, bot);
    if (!target)
        return false;

    // AC FoSActions.cpp:24-52: one bot marks the priority target with the skull, then everyone attacks it
    if (IsMechanicTrackerBot(bot, CataLct::MAP_ID))
        MarkTargetWithSkull(bot, target);

    return Attack(target);
}

bool LctAvoidHeavensFuryAction::Execute(Event /*event*/)
{
    Position pos;
    float radius = 0.0f;
    if (!CataLct::HeavensFuryDanger(botAI, bot, pos, radius))
        return false;

    // AC idiom: FoSActions.cpp:15-18, UBActions.cpp:61-69
    return FleePosition(pos, radius);
}

bool LctKillSoulFragmentAction::Execute(Event /*event*/)
{
    Unit* target = CataLct::SoulFragmentFocus(botAI, bot);
    if (!target)
        return false;

    // AC FoSActions.cpp:24-52: one bot marks the priority target with the skull, then everyone attacks it
    if (IsMechanicTrackerBot(bot, CataLct::MAP_ID))
        MarkTargetWithSkull(bot, target);

    return Attack(target);
}

bool LctAvoidHallowedGroundAction::Execute(Event /*event*/)
{
    Position pos;
    float radius = 0.0f;
    if (!CataLct::HallowedGroundDanger(botAI, bot, pos, radius))
        return false;

    // AC idiom: FoSActions.cpp:15-18, UBActions.cpp:61-69
    return FleePosition(pos, radius);
}

bool LctKillHarbingerAction::Execute(Event /*event*/)
{
    Unit* target = CataLct::HarbingerFocus(botAI, bot);
    if (!target)
        return false;

    // AC FoSActions.cpp:24-52: one bot marks the priority target with the skull, then everyone attacks it
    if (IsMechanicTrackerBot(bot, CataLct::MAP_ID))
        MarkTargetWithSkull(bot, target);

    return Attack(target);
}

bool LctAvoidBlazeFireAction::Execute(Event /*event*/)
{
    Position pos;
    float radius = 0.0f;
    if (!CataLct::BlazeFireDanger(botAI, bot, pos, radius))
        return false;

    // AC idiom: FoSActions.cpp:15-18, UBActions.cpp:61-69
    return FleePosition(pos, radius);
}

bool LctAvoidSiamatStormAction::Execute(Event /*event*/)
{
    Position pos;
    float radius = 0.0f;
    if (!CataLct::SiamatStormDanger(botAI, bot, pos, radius))
        return false;

    // AC idiom: FoSActions.cpp:15-18, UBActions.cpp:61-69
    return FleePosition(pos, radius);
}

bool LctKillServantAction::Execute(Event /*event*/)
{
    Unit* target = CataLct::ServantFocus(botAI, bot);
    if (!target)
        return false;

    // AC FoSActions.cpp:24-52: one bot marks the priority target with the skull, then everyone attacks it
    if (IsMechanicTrackerBot(bot, CataLct::MAP_ID))
        MarkTargetWithSkull(bot, target);

    return Attack(target);
}

bool LctAvoidLightningChargeAction::Execute(Event /*event*/)
{
    Position pos;
    float radius = 0.0f;
    if (!CataLct::LightningChargeDanger(botAI, bot, pos, radius))
        return false;

    // AC idiom: FoSActions.cpp:15-18, UBActions.cpp:61-69
    return FleePosition(pos, radius);
}
