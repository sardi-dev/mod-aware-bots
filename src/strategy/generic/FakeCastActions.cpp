/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE). See FakeCastActions.h for what this is.
 */

#include "FakeCastActions.h"

#include "Event.h"
#include "FakeCast.h"
#include "Playerbots.h"

bool FakeCastDueTrigger::IsActive()
{
    return FakeCastGlue::CancelDue(botAI, bot);
}

bool FakeCastAction::Execute(Event /*event*/)
{
    return FakeCastGlue::Cancel(botAI, bot);
}
