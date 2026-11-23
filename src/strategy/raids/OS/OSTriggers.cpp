/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "OSTriggers.h"

#include "PlayerbotSpec.h"
#include "Playerbots.h"
#include "RaidRoles.h"

namespace
{
    bool HasNearbyNpc(PlayerbotAI* botAI, uint32 entry)
    {
        AiObjectContext* context = botAI->GetAiObjectContext();
        GuidVector npcs = AI_VALUE(GuidVector, "nearest hostile npcs");
        for (BotGuid const& npc : npcs)
        {
            Unit* unit = botAI->GetUnit(npc);
            if (unit && unit->GetEntry() == entry)
                return true;
        }
        return false;
    }
}

// AC Ai/Raid/OS/OSTriggers.cpp:9. 5.4.8: AC PlayerbotAI::IsTank and the other role checks -> PlayerBotSpec
// (AI/PlayerbotSpec.h), IsAssistHealOfIndex -> RaidRoles (strategy/raids/RaidRoles.h).
bool SartharionTankTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "sartharion");
    if (!boss)
        return false;

    return PlayerBotSpec::IsTank(bot);
}

// AC Ai/Raid/OS/OSTriggers.cpp:17. 5.4.8: tanks dodge too. AC's tank spots sit between its waves, but the 5.4.8 script
// runs a wave straight down the main tank's spot (y 532.2, boss_sartharion.cpp FlameRight2Spawn).
bool FlameTsunamiTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "sartharion");
    if (!boss)
        return false;

    return HasNearbyNpc(botAI, NPC_FLAME_TSUNAMI);
}

// AC Ai/Raid/OS/OSTriggers.cpp:40
bool TwilightFissureTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "sartharion");
    if (!boss)
        return false;

    return HasNearbyNpc(botAI, NPC_TWILIGHT_FISSURE);
}

// AC Ai/Raid/OS/OSTriggers.cpp:61
bool SartharionDpsTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "sartharion");
    if (!boss)
        return false;

    return PlayerBotSpec::IsDps(bot);
}

// AC Ai/Raid/OS/OSTriggers.cpp:69
bool SartharionMeleePositioningTrigger::IsActive()
{
    if (!PlayerBotSpec::IsMelee(bot) || !PlayerBotSpec::IsDps(bot))
        return false;

    Unit* boss = AI_VALUE2(Unit*, "find target", "sartharion");
    if (!boss)
        return false;

    Unit* shadron = AI_VALUE2(Unit*, "find target", "shadron");
    Unit* tenebron = AI_VALUE2(Unit*, "find target", "tenebron");
    Unit* vesperon = AI_VALUE2(Unit*, "find target", "vesperon");

    return !(shadron || tenebron || vesperon);
}

// AC Ai/Raid/OS/OSTriggers.cpp:84. The main tank and the first healer stay with Sartharion. The 5.4.8 world database
// has no Twilight Portal spawn on map 615 (the script expects four, boss_sartharion.cpp OpenPortal), so this stays
// idle until the content gets them.
bool TwilightPortalEnterTrigger::IsActive()
{
    if (PlayerBotSpec::IsMainTank(bot) || RaidRoles::IsAssistHealOfIndex(bot, 0))
        return false;

    // Don't enter portal until drakes are dead
    if (bot->HasAura(SPELL_POWER_OF_SHADRON) || bot->HasAura(SPELL_POWER_OF_TENEBRON) ||
        bot->HasAura(SPELL_POWER_OF_VESPERON))
        return false;

    // 5.4.8: already through (AC's exit trigger handled it; see OSStrategy.cpp)
    if (bot->HasAura(SPELL_TWILIGHT_SHIFT))
        return false;

    Unit* boss = AI_VALUE2(Unit*, "find target", "sartharion");
    if (!boss)
        return false;

    return bot->FindNearestGameObject(GO_TWILIGHT_PORTAL, 100.0f) != nullptr;
}
