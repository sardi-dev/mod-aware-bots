/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "OSMultipliers.h"

#include "ChooseTargetActions.h"
#include "EncounterHelpers.h"
#include "PlayerbotSpec.h"
#include "Playerbots.h"

// AC Ai/Raid/OS/OSMultipliers.cpp:16. 5.4.8: AC's taunt casts per class (CastTauntAction, CastDarkCommandAction,
// CastHandOfReckoningAction, CastGrowlAction) -> EncounterHelpers::IsTauntAction (Utils/EncounterHelpers.h), which
// names the same actions; AC's empty main tank TankFaceAction branch is dropped.
float SartharionMultiplier::GetValue(Action* action)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "sartharion");
    if (!boss)
        return 1.0f;

    Unit* target = action->GetTarget();

    if (PlayerBotSpec::IsDps(bot) && dynamic_cast<DpsAssistAction*>(action))
        return 0.0f;

    // The main tank keeps Sartharion; the drakes are the assist tank's
    if (PlayerBotSpec::IsMainTank(bot) && target && target != boss &&
        (dynamic_cast<TankAssistAction*>(action) || EncounterHelpers::IsTauntAction(bot, action)))
        return 0.0f;

    if (PlayerBotSpec::IsAssistTank(bot) && target && target == boss && EncounterHelpers::IsTauntAction(bot, action))
        return 0.0f;

    return 1.0f;
}
