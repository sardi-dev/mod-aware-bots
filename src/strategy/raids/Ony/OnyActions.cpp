/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "OnyActions.h"

#include <cmath>
#include <limits>

#include "Playerbots.h"

// AC Ai/Raid/Ony/OnyActions.cpp:14
bool RaidOnyxiaMoveToSideAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "onyxia");
    if (!boss)
        return false;

    float angleToBot = boss->GetAngle(bot);
    float bossFacing = boss->GetOrientation();
    float diff = fabs(angleToBot - bossFacing);
    if (diff > M_PI)
        diff = 2 * M_PI - diff;

    float distance = bot->GetDistance(boss);

    // Too close (30 yards) and either in front or behind
    if (distance <= 30.0f && (diff < M_PI / 4 || diff > 3 * M_PI / 4))
    {
        float offsetAngle = bossFacing + M_PI_2;  // 90 degrees to the right
        float offsetDist = 15.0f;

        float sideX = boss->GetPositionX() + offsetDist * cos(offsetAngle);
        float sideY = boss->GetPositionY() + offsetDist * sin(offsetAngle);

        return MoveTo(boss->GetMapId(), sideX, sideY, boss->GetPositionZ(), false, false, false, false,
                      MovementPriority::MOVEMENT_COMBAT);
    }

    return false;
}

// AC Ai/Raid/Ony/OnyActions.cpp:44
bool RaidOnyxiaSpreadOutAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "onyxia");

    if (!boss)
        return false;

    // Trigger may fire on one tick, but the action can execute on a later tick.
    // By that time the cast may have finished, so current spell can be null.
    Spell* currentSpell = boss->GetCurrentSpell(CURRENT_GENERIC_SPELL);
    if (!currentSpell || !currentSpell->m_spellInfo)
        return false;

    if (currentSpell->m_spellInfo->Id != SPELL_ONYXIA_FIREBALL)
        return false;

    Unit* unitTarget = currentSpell->m_targets.GetUnitTarget();
    Player* target = unitTarget ? unitTarget->ToPlayer() : nullptr;
    if (!target || target != bot)
        return false;

    return MoveFromGroup(9.0f);  // move 9 yards
}

// AC Ai/Raid/Ony/OnyActions.h RaidOnyxiaMoveToSafeZoneAction::GetSafeZonesForBreath
std::vector<OnyxiaSafeZone> RaidOnyxiaMoveToSafeZoneAction::GetSafeZonesForBreath(uint32 spellId)
{
    float constexpr SAFE_ZONE_RADIUS = 5.0f;

    switch (spellId)
    {
        case SPELL_ONYXIA_BREATH_NORTH_TO_SOUTH:
        case SPELL_ONYXIA_BREATH_SOUTH_TO_NORTH:
            return {
                OnyxiaSafeZone{{-10.0f, -180.0f, -87.0f, 0.0f}, SAFE_ZONE_RADIUS},
                OnyxiaSafeZone{{-20.0f, -250.0f, -88.0f, 0.0f}, SAFE_ZONE_RADIUS}
            };  // Bottom Safe Zone

        case SPELL_ONYXIA_BREATH_EAST_TO_WEST:
        case SPELL_ONYXIA_BREATH_WEST_TO_EAST:
            return {
                OnyxiaSafeZone{{20.0f, -210.0f, -85.5f, 0.0f}, SAFE_ZONE_RADIUS},
                OnyxiaSafeZone{{-75.0f, -210.0f, -83.4f, 0.0f}, SAFE_ZONE_RADIUS}
            };  // Left Safe Zone

        case SPELL_ONYXIA_BREATH_SOUTHEAST_TO_NORTHWEST:
        case SPELL_ONYXIA_BREATH_NORTHWEST_TO_SOUTHEAST:
            return {
                OnyxiaSafeZone{{-60.0f, -195.0f, -85.0f, 0.0f}, SAFE_ZONE_RADIUS},
                OnyxiaSafeZone{{10.0f, -240.0f, -85.9f, 0.0f}, SAFE_ZONE_RADIUS}
            };  // NW Safe Zone

        case SPELL_ONYXIA_BREATH_SOUTHWEST_TO_NORTHEAST:
        case SPELL_ONYXIA_BREATH_NORTHEAST_TO_SOUTHWEST:
            return {
                OnyxiaSafeZone{{7.0f, -185.0f, -86.2f, 0.0f}, SAFE_ZONE_RADIUS},
                OnyxiaSafeZone{{-60.0f, -240.0f, -85.2f, 0.0f}, SAFE_ZONE_RADIUS}
            };  // NE Safe Zone

        default:
            return {
                OnyxiaSafeZone{{-40.0f, -214.0f, -86.6f, 0.0f}, SAFE_ZONE_RADIUS}
            };  // Fallback center - shouldn't ever happen
    }
}

// AC Ai/Raid/Ony/OnyActions.cpp:72
bool RaidOnyxiaMoveToSafeZoneAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "onyxia");
    if (!boss)
        return false;

    Spell* currentSpell = boss->GetCurrentSpell(CURRENT_GENERIC_SPELL);
    if (!currentSpell || !currentSpell->m_spellInfo)
        return false;

    uint32 spellId = currentSpell->m_spellInfo->Id;

    std::vector<OnyxiaSafeZone> safeZones = GetSafeZonesForBreath(spellId);
    if (safeZones.empty())
        return false;

    // Find closest safe zone
    OnyxiaSafeZone* bestZone = nullptr;
    float bestDist = std::numeric_limits<float>::max();

    for (auto& zone : safeZones)
    {
        float dist = bot->GetExactDist2d(zone.pos.GetPositionX(), zone.pos.GetPositionY());
        if (dist < bestDist)
        {
            bestDist = dist;
            bestZone = &zone;
        }
    }

    if (!bestZone)
        return false;

    if (bot->IsWithinDist2d(bestZone->pos.GetPositionX(), bestZone->pos.GetPositionY(), bestZone->radius))
        return false;  // Already safe

    // Stop channeling spell first
    bot->AttackStop();
    bot->CastStop();

    return MoveTo(bot->GetMapId(), bestZone->pos.GetPositionX(), bestZone->pos.GetPositionY(),
                  bestZone->pos.GetPositionZ(), false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
}

// AC Ai/Raid/Ony/OnyActions.cpp:117
bool RaidOnyxiaKillWhelpsAction::Execute(Event /*event*/)
{
    Unit* currentTarget = AI_VALUE(Unit*, "current target");
    // If already attacking a whelp, don't swap targets
    if (currentTarget && currentTarget->GetEntry() == NPC_ONYXIAN_WHELP)
    {
        return false;
    }
    GuidVector targets = AI_VALUE(GuidVector, "possible targets");
    for (BotGuid guid : targets)
    {
        Creature* unit = botAI->GetCreature(guid);
        if (!unit || !unit->IsAlive() || !unit->IsInWorld())
            continue;

        if (unit->GetEntry() == NPC_ONYXIAN_WHELP)
            return Attack(unit);
    }
    return false;
}

// AC Ai/Raid/Ony/OnyActions.cpp:141
bool OnyxiaAvoidEggsAction::Execute(Event /*event*/)
{
    float x, y;

    // get safe zone slightly away from eggs (Can this be dynamic?)
    if (bot->GetExactDist2d(-36.0f, -164.0f) <= 5.0f)
    {
        x = -10.0f;
        y = -180.0f;
    }
    else if (bot->GetExactDist2d(-34.0f, -262.0f) <= 5.0f)
    {
        x = -16.0f;
        y = -250.0f;
    }
    else
    {
        return false;  // Not in danger zone
    }

    return MoveTo(bot->GetMapId(), x, y, bot->GetPositionZ(), false, false, false, false,
                  MovementPriority::MOVEMENT_COMBAT);
}
