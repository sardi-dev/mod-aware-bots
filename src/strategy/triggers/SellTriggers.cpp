/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "SellTriggers.h"

#include "BotSale.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"

bool VendorTripWantedTrigger::IsActive()
{
    // Not while the group's leader fights; IsEligible covers the rest (random bots only, alive, out of combat, in the
    // open world, not in a battleground or arena, not following a real player).
    Player* master = botAI->GetMaster();
    if (master && master != bot && master->IsInCombat())
        return false;

    return BotSale::TripWanted(botAI, bot) && BotSale::FindNearbyVendor(botAI, bot) != nullptr;
}
