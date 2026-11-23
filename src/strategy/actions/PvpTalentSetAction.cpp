/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "PvpTalentSetAction.h"

#include "BotPvpTalents.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"

bool PvpTalentSetAction::Execute(Event /*event*/)
{
    return BotPvpTalents::Apply(botAI, bot);
}
