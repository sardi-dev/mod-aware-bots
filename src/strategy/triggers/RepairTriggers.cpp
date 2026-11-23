/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "RepairTriggers.h"

#include "BotRepair.h"
#include "Map.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"

bool RepairNeededNearbyTrigger::IsActive()
{
    // A player repairs between fights and outside dungeons, and not while the group's leader fights.
    if (!bot->IsAlive() || bot->IsInCombat() || bot->GetMap()->Instanceable())
        return false;

    Player* master = botAI->GetMaster();
    if (master && master != bot && master->IsInCombat())
        return false;

    // The durability check is cheap; the NPC search runs only when the gear is worn.
    return BotRepair::NeedsRepair(bot) && BotRepair::FindNearbyRepairer(botAI, bot) != nullptr;
}
