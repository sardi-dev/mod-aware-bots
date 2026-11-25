/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "SellTripAction.h"

#include "BotSale.h"
#include "BotShopping.h"
#include "Creature.h"
#include "Event.h"
#include "Item.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"

namespace
{
    constexpr float VENDOR_TALK_DISTANCE = 4.0f;
}

bool SellTripAction::Execute(Event /*event*/)
{
    Creature* vendor = BotSale::FindNearbyVendor(botAI, bot);
    if (!vendor)
        return false;

    BotSale::BeginTrip(bot);
    if (bot->GetDistance(vendor) > VENDOR_TALK_DISTANCE)
        return MoveTo(vendor, VENDOR_TALK_DISTANCE - 1.0f);

    bot->SetFacingToObject(vendor);

    std::vector<uint64> junkGuids;
    for (Item* item : BotSale::SelectItemsToSell(botAI, bot))
        if (BotSale::IsJunk(botAI, bot, item)) junkGuids.push_back(item->GetGUID());
    uint64 const moneyBefore = bot->GetMoney();
    botAI->DoSpecificAction("sell", Event("sell trip", "policy"), true);
    bool const sold = bot->GetMoney() > moneyBefore;

    bool soldJunk = false;
    for (uint64 guid : junkGuids)
        if (!bot->GetItemByGuid(guid)) soldJunk = true;
    bool const reserveSlot = soldJunk && BotSale::FreeBagSlots(bot) > 0;
    bool const bagged = (!reserveSlot || BotSale::FreeBagSlots(bot) > 1) && BotShopping::BuyBagAt(botAI, bot, vendor);
    bool const bought = BotSale::BuySupplies(botAI, bot, vendor, reserveSlot) > 0;
    bool const tooled = BotSale::BuyTools(bot, vendor, reserveSlot) > 0;

    // These actions may buy several items without exposing their individual storage decisions.
    bool const geared = !reserveSlot && BotShopping::BuyUpgradesAt(botAI, bot, vendor);
    bool const recipes = !reserveSlot && BotShopping::BuyRecipesAt(botAI, bot, vendor);

    // A trip that did none of it is not repeated for a while (a vendor without food, nothing the bot may sell).
    BotSale::EndTrip(bot, sold || bagged || bought || tooled || geared || recipes);
    return true;
}
