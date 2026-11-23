/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE). See PvpTeamActions.h for what this is.
 */

#include "PvpTeamActions.h"

#include "Event.h"
#include "Playerbots.h"
#include "PvpTeam.h"

bool PvpControlDueTrigger::IsActive()
{
    return PvpTeamGlue::ControlDue(botAI, bot);
}

bool PvpControlAction::Execute(Event /*event*/)
{
    return PvpTeamGlue::ControlCast(botAI, bot);
}

float PvpTeamMultiplier::GetValue(Action* action)
{
    return PvpTeamGlue::HoldsDamage(botAI, bot, action) ? 0.0f : 1.0f;
}
