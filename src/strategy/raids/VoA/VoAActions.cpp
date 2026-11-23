/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "VoAActions.h"

#include "EncounterHelpers.h"
#include "PlayerbotSpec.h"
#include "Playerbots.h"
#include "RaidRoles.h"
#include "VoATriggers.h"

// AC Ai/Raid/VoA/VoAActions.cpp:31-79 and 120-167 (the same block in both mark actions). 5.4.8: AC
// PlayerbotAI::IsAssistTankOfIndex -> RaidRoles (strategy/raids/RaidRoles.h); the icon is set through
// EncounterHelpers::MarkTargetWithSkull (Utils/EncounterHelpers.h), which takes 5.4.8's extra Group::SetTargetIcon
// argument.
bool VoAHelpers::MarkWithSkullAsTank(Player* bot, Unit* mainTankUnit, Unit* target)
{
    if (!target)
        return false;

    Player* mainTank = mainTankUnit ? mainTankUnit->ToPlayer() : nullptr;
    if (mainTank && !GET_PLAYERBOT_AI(mainTank))  // Main tank is a real player
    {
        // The first of up to three bot assist tanks marks
        for (uint8 i = 0; i < 3; ++i)
        {
            if (RaidRoles::IsAssistTankOfIndex(bot, i))
                return EncounterHelpers::MarkTargetWithSkull(bot, target);
        }
        return false;
    }

    if (PlayerBotSpec::IsMainTank(bot))
        return EncounterHelpers::MarkTargetWithSkull(bot, target);

    return false;
}

// AC Ai/Raid/VoA/VoAActions.cpp:17
bool EmalonMarkBossAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "emalon the storm watcher");
    if (!boss || !boss->IsAlive())
        return false;

    return VoAHelpers::MarkWithSkullAsTank(bot, AI_VALUE(Unit*, "main tank"), boss);
}

bool EmalonMarkBossAction::isUseful()
{
    EmalonMarkBossTrigger emalonMarkBossTrigger(botAI);
    return emalonMarkBossTrigger.IsActive();
}

// AC Ai/Raid/VoA/VoAActions.cpp:88. 20 yards plus 5 for safety in 10 player mode; the 25 player nova has no maximum
// range, but 25 yards should be ok.
bool EmalonLightingNovaAction::Execute(Event /*event*/)
{
    float constexpr radius = 25.0f;

    Unit* boss = AI_VALUE2(Unit*, "find target", "emalon the storm watcher");
    if (!boss)
        return false;

    float const currentDistance = bot->GetDistance2d(boss);
    if (currentDistance < radius)
        return MoveAway(boss, radius - currentDistance);

    return false;
}

bool EmalonLightingNovaAction::isUseful()
{
    EmalonLightingNovaTrigger emalonLightingNovaTrigger(botAI);
    return emalonLightingNovaTrigger.IsActive();
}

// AC Ai/Raid/VoA/VoAActions.cpp:101
bool EmalonOverchargeAction::Execute(Event /*event*/)
{
    Unit* minion = VoAHelpers::FindOverchargedMinion(botAI);
    if (!minion)
        return false;

    return VoAHelpers::MarkWithSkullAsTank(bot, AI_VALUE(Unit*, "main tank"), minion);
}

bool EmalonOverchargeAction::isUseful()
{
    EmalonOverchargeTrigger emalonOverchargeTrigger(botAI);
    return emalonOverchargeTrigger.IsActive();
}
