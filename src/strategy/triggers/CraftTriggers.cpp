/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "CraftTriggers.h"

#include "BotCrafting.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"

bool CraftableUpgradeTrigger::IsActive()
{
    // A player crafts between fights; so does an alt, and never while its master fights.
    if (!BotCrafting::Enabled() || !bot->IsAlive() || bot->IsInCombat() || bot->isMoving())
        return false;

    Player* master = botAI->GetMaster();
    if (master && master != bot && master->IsInCombat())
        return false;

    return BotCrafting::FindCraftableUpgrade(botAI, bot) != 0;
}
