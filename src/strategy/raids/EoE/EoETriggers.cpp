/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "EoETriggers.h"

#include "Playerbots.h"

// AC Ai/Raid/EoE/EoETriggers.cpp:9. Phase 3 is the drake phase (the bot rides a Wyrmrest Skytalon), phases 1 and 2 are
// split at half health.
uint8 MalygosTrigger::getPhase(Player* bot, Unit* boss)
{
    uint8 phase = 0;
    if (bot->GetMapId() != EOE_MAP_ID)
        return phase;

    Unit* vehicle = bot->GetVehicleBase();
    if (vehicle && vehicle->GetEntry() == NPC_WYRMREST_SKYTALON)
        phase = 3;
    else if (boss && boss->HealthAbovePct(50))
        phase = 1;
    else if (boss)
        phase = 2;

    return phase;
}

// AC Ai/Raid/EoE/EoETriggers.cpp:32
bool MalygosTrigger::IsActive()
{
    return AI_VALUE2(Unit*, "find target", "malygos") != nullptr;
}

// AC Ai/Raid/EoE/EoETriggers.cpp:37
bool PowerSparkTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "malygos");
    if (!boss)
        return false;

    if (bot->getClass() != CLASS_DEATH_KNIGHT)
        return false;

    GuidVector targets = AI_VALUE(GuidVector, "possible targets no los");
    for (BotGuid const& target : targets)
    {
        Unit* unit = botAI->GetUnit(target);
        if (unit && unit->GetEntry() == NPC_POWER_SPARK)
            return true;
    }

    return false;
}
