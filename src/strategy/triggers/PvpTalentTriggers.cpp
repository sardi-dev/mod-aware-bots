/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "PvpTalentTriggers.h"

#include "BotPvpTalents.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"

bool PvpTalentSetDueTrigger::IsActive()
{
    return BotPvpTalents::Enabled() && BotPvpTalents::Evaluate(bot) != PvpTalentSetPolicy::Step::NONE;
}
