/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "VoATriggers.h"

#include "PlayerbotSpec.h"
#include "Playerbots.h"

// AC repeats this search in two triggers and an action (VoATriggers.cpp:36-50, 99-113, VoAActions.cpp:104-118)
Unit* VoAHelpers::FindOverchargedMinion(PlayerbotAI* botAI)
{
    AiObjectContext* context = botAI->GetAiObjectContext();
    GuidVector npcs = AI_VALUE(GuidVector, "nearest hostile npcs");
    for (BotGuid const& npc : npcs)
    {
        Unit* unit = botAI->GetUnit(npc);
        if (unit && unit->GetEntry() == NPC_TEMPEST_MINION && unit->HasAura(AURA_OVERCHARGE))
            return unit;
    }
    return nullptr;
}

// AC Ai/Raid/VoA/VoATriggers.cpp:12. 5.4.8: AC PlayerbotAI::IsTank -> PlayerBotSpec::IsTank (AI/PlayerbotSpec.h:11);
// AC reads the group without a null check.
bool EmalonMarkBossTrigger::IsActive()
{
    // Only tank bot can mark target
    if (!PlayerBotSpec::IsTank(bot))
        return false;

    Unit* boss = AI_VALUE2(Unit*, "find target", "emalon the storm watcher");
    if (!boss || !boss->IsAlive())
        return false;

    Group* group = bot->GetGroup();
    if (!group || group->GetTargetIcon(VoAHelpers::SKULL_ICON) == boss->GetGUID())
        return false;

    // An overcharged minion takes the skull instead
    return !VoAHelpers::FindOverchargedMinion(botAI);
}

// AC Ai/Raid/VoA/VoATriggers.cpp:60
bool EmalonLightingNovaTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "emalon the storm watcher");
    if (!boss || !boss->IsAlive())
        return false;

    // Tank dont need to move
    if (PlayerBotSpec::IsTank(bot))
        return false;

    bool const isCasting = boss->HasUnitState(UNIT_STATE_CASTING);
    bool const isLightingNova = boss->FindCurrentSpellBySpellId(SPELL_LIGHTNING_NOVA_10_MAN) ||
                                boss->FindCurrentSpellBySpellId(SPELL_LIGHTNING_NOVA_25_MAN);
    return isCasting && isLightingNova;
}

// AC Ai/Raid/VoA/VoATriggers.cpp:82
bool EmalonOverchargeTrigger::IsActive()
{
    // Only tank bot can mark target
    if (!PlayerBotSpec::IsTank(bot))
        return false;

    Unit* boss = AI_VALUE2(Unit*, "find target", "emalon the storm watcher");
    if (!boss || !boss->IsAlive())
        return false;

    Unit* overchargedMinion = VoAHelpers::FindOverchargedMinion(botAI);
    if (!overchargedMinion)
        return false;

    Group* group = bot->GetGroup();
    return group && group->GetTargetIcon(VoAHelpers::SKULL_ICON) != overchargedMinion->GetGUID();
}
