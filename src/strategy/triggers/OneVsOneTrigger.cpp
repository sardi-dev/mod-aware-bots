/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "OneVsOneTrigger.h"

#include "OneVsOne.h"
#include "Playerbots.h"

bool DuelPendingTrigger::IsActive()
{
    return bot->duel && bot->duel->opponent && bot->duel->initiator && bot->duel->initiator != bot &&
           !bot->duel->startTimer && !bot->duel->startTime;
}

bool OneVsOneTrigger::IsActive()
{
    OneVsOneGlue::Decision decision;
    return OneVsOneGlue::Decide(botAI, bot, decision);
}
