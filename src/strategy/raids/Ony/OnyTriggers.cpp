/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "OnyTriggers.h"
#include "Playerbots.h"
#include "PlayerbotSpec.h"

// AC Ai/Raid/Ony/OnyTriggers.cpp:15
OnyxiaDeepBreathTrigger::OnyxiaDeepBreathTrigger(PlayerbotAI* botAI) : Trigger(botAI, "ony deep breath warning") {}

bool OnyxiaDeepBreathTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "onyxia");
    if (!boss || !boss->HasUnitState(UNIT_STATE_CASTING))
        return false;

    // Check if Onyxia is casting
    Spell* currentSpell = boss->GetCurrentSpell(CURRENT_GENERIC_SPELL);

    if (!currentSpell || !currentSpell->m_spellInfo)
        return false;

    switch (currentSpell->m_spellInfo->Id)
    {
        case SPELL_ONYXIA_BREATH_NORTH_TO_SOUTH:
        case SPELL_ONYXIA_BREATH_SOUTH_TO_NORTH:
        case SPELL_ONYXIA_BREATH_EAST_TO_WEST:
        case SPELL_ONYXIA_BREATH_WEST_TO_EAST:
        case SPELL_ONYXIA_BREATH_SOUTHEAST_TO_NORTHWEST:
        case SPELL_ONYXIA_BREATH_NORTHWEST_TO_SOUTHEAST:
        case SPELL_ONYXIA_BREATH_SOUTHWEST_TO_NORTHEAST:
        case SPELL_ONYXIA_BREATH_NORTHEAST_TO_SOUTHWEST:
            return true;
        default:
            return false;
    }
}

// AC Ai/Raid/Ony/OnyTriggers.cpp:49
OnyxiaNearTailTrigger::OnyxiaNearTailTrigger(PlayerbotAI* botAI) : Trigger(botAI, "ony near tail") {}

// 5.4.8: AC botAI->IsTank(bot) -> static PlayerBotSpec::IsTank(bot) (AI/PlayerbotSpec.h:11); AC's boss->IsFlying()
// check is IsOnyxiaAirborne (OnyTriggers.h)
bool OnyxiaNearTailTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "onyxia");
    if (!boss || PlayerBotSpec::IsTank(bot))
        return false;

    // Skip if Onyxia is in air or transitioning
    if (!boss->IsInCombat() || IsOnyxiaAirborne(boss) || !boss->GetVictim())
        return false;

    return true;
}

// AC Ai/Raid/Ony/OnyTriggers.cpp:70
RaidOnyxiaFireballSplashTrigger::RaidOnyxiaFireballSplashTrigger(PlayerbotAI* botAI)
    : Trigger(botAI, "ony fireball splash incoming")
{
}

bool RaidOnyxiaFireballSplashTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "onyxia");
    if (!boss || !boss->HasUnitState(UNIT_STATE_CASTING))
        return false;

    // Check if Onyxia is casting Fireball
    Spell* currentSpell = boss->GetCurrentSpell(CURRENT_GENERIC_SPELL);
    if (!currentSpell || !currentSpell->m_spellInfo || currentSpell->m_spellInfo->Id != SPELL_ONYXIA_FIREBALL)
        return false;

    GuidVector nearbyUnits = AI_VALUE(GuidVector, "nearest friendly players");

    for (BotGuid guid : nearbyUnits)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || unit == bot || !unit->IsAlive())
            continue;

        if (bot->GetDistance(unit) < 8.0f)
            return true;
    }

    return false;
}

// AC Ai/Raid/Ony/OnyTriggers.cpp:91
RaidOnyxiaWhelpsSpawnTrigger::RaidOnyxiaWhelpsSpawnTrigger(PlayerbotAI* botAI) : Trigger(botAI, "ony whelps spawn") {}

// 5.4.8: AC botAI->IsHeal(bot) -> static PlayerBotSpec::IsHeal(bot); AC's boss->IsFlying() is IsOnyxiaAirborne
bool RaidOnyxiaWhelpsSpawnTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "onyxia");
    if (!boss)
        return false;

    return !PlayerBotSpec::IsHeal(bot) && IsOnyxiaAirborne(boss);  // DPS + Tanks only
}

// AC Ai/Raid/Ony/OnyTriggers.cpp:101
OnyxiaAvoidEggsTrigger::OnyxiaAvoidEggsTrigger(PlayerbotAI* botAI) : Trigger(botAI, "ony avoid eggs") {}

bool OnyxiaAvoidEggsTrigger::IsActive()
{
    if (bot->GetExactDist2d(-35.0f, -165.0f) <= 5.0f)
        return true;

    if (bot->GetExactDist2d(-35.0f, -260.0f) <= 5.0f)
        return true;

    return false;
}
