/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "EoEMultipliers.h"

#include "ChooseTargetActions.h"
#include "EoEActions.h"
#include "EoETriggers.h"
#include "FollowActions.h"
#include "MovementActions.h"
#include "PlayerbotSpec.h"
#include "Playerbots.h"

// AC Ai/Raid/EoE/EoEMultipliers.cpp:16. 5.4.8: AC PlayerbotAI role checks -> PlayerBotSpec (AI/PlayerbotSpec.h).
float MalygosMultiplier::GetValue(Action* action)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "malygos");

    uint8 const phase = MalygosTrigger::getPhase(bot, boss);
    if (phase == 0)
        return 1.0f;

    if (phase == 1)
    {
        if (dynamic_cast<FollowAction*>(action))
            return 0.0f;

        if (PlayerBotSpec::IsDps(bot) && dynamic_cast<DpsAssistAction*>(action))
            return 0.0f;

        if (PlayerBotSpec::IsRangedDps(bot) && dynamic_cast<DropTargetAction*>(action))
            return 0.0f;

        if (!PlayerBotSpec::IsMainTank(bot) && dynamic_cast<TankAssistAction*>(action))
            return 0.0f;
    }
    else if (phase == 2)
    {
        if (PlayerBotSpec::IsDps(bot) && dynamic_cast<DpsAssistAction*>(action))
            return 0.0f;

        if (dynamic_cast<FleeAction*>(action))
            return 0.0f;

        if (dynamic_cast<TankAssistAction*>(action))
        {
            Unit* target = action->GetTarget();
            if (target && target->GetEntry() == NPC_SCION_OF_ETERNITY)
                return 0.0f;
        }
    }
    else if (phase == 3)
    {
        // Suppresses FollowAction as well as some attack-based movements
        if (dynamic_cast<MovementAction*>(action) && !dynamic_cast<EoEFlyDrakeAction*>(action))
            return 0.0f;
    }

    return 1.0f;
}
