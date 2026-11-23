/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "ShopTriggers.h"

#include "BotShopping.h"
#include "Map.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"

bool VendorUpgradeNearbyTrigger::IsActive()
{
    // A player shops between fights and outside dungeons; so does an alt, and never while its master fights.
    if (!BotShopping::Enabled() || !bot->IsAlive() || bot->IsInCombat() || bot->GetMap()->Instanceable())
        return false;

    Player* master = botAI->GetMaster();
    if (master && master != bot && master->IsInCombat())
        return false;

    return BotShopping::FindNearbyVendor(botAI, bot) != nullptr;
}

bool MountVendorNearbyTrigger::IsActive()
{
    if (!bot->IsAlive() || bot->IsInCombat() || bot->GetMap()->Instanceable())
        return false;

    return BotShopping::FindMountVendor(botAI, bot) != nullptr;
}
