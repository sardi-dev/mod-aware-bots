/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "BreakAction.h"

#include "BotBreak.h"
#include "Event.h"
#include "Playerbots.h"

bool BreakAction::isUseful() { return BotBreak::Wants(bot->GetGUIDLow()); }

bool BreakAction::Execute(Event /*event*/)
{
    BotBreak::Tick(botAI);
    // The tick keeps a bot on break still; an offer it turned down or a break that ended leaves the tick to others.
    return BotBreak::IsOnBreak(bot->GetGUIDLow());
}
