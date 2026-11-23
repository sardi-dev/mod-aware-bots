/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "ToCMultipliers.h"

#include "ChooseTargetActions.h"
#include "EncounterHelpers.h"
#include "PlayerbotSpec.h"
#include "Playerbots.h"
#include "ToCTriggers.h"

float ToCMultiplier::GetValue(Action* action)
{
    // The scan only runs for the three kinds of action this multiplier judges
    bool const dpsAssist = dynamic_cast<DpsAssistAction*>(action) != nullptr;
    bool const tankAssist = dynamic_cast<TankAssistAction*>(action) != nullptr;
    bool const taunt = EncounterHelpers::IsTauntAction(bot, action);
    if (!dpsAssist && !tankAssist && !taunt)
        return 1.0f;

    ToC::Scene const& scene = ToC::Scan(botAI);

    if (dpsAssist)
        return (PlayerBotSpec::IsDps(bot) && ToC::PriorityTarget(scene, bot, nullptr)) ? 0.0f : 1.0f;

    // The champions have no tank (they pick their targets by their own threat rule), so the tanks stay generic there
    Unit* mainTarget = ToC::MainTankUnit(scene);
    Unit* target = action->GetTarget();
    if (!mainTarget || !target)
        return 1.0f;

    if (PlayerBotSpec::IsMainTank(bot) && target != mainTarget && (tankAssist || taunt))
        return 0.0f;

    if (PlayerBotSpec::IsAssistTank(bot) && target == mainTarget && taunt)
        return 0.0f;

    return 1.0f;
}
