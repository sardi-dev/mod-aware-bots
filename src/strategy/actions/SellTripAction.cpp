/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "SellTripAction.h"

#include "BotSale.h"
#include "BotShopping.h"
#include "Creature.h"
#include "Event.h"
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

    uint64 const moneyBefore = bot->GetMoney();
    botAI->DoSpecificAction("sell", Event("sell trip", "policy"), true);
    bool const sold = bot->GetMoney() > moneyBefore;

    bool const bought = BotSale::BuySupplies(botAI, bot, vendor) > 0;
    // BT-219: and the tools its professions need, when this vendor sells them.
    bool const tooled = BotSale::BuyTools(bot, vendor) > 0;

    // Parity item 4: while at the vendor, a gear upgrade it sells and the bot can pay for is bought too.
    bool const geared = BotShopping::BuyUpgradesAt(botAI, bot, vendor);

    // A trip that did none of it is not repeated for a while (a vendor without food, nothing the bot may sell).
    BotSale::EndTrip(bot, sold || bought || tooled || geared);
    return true;
}
