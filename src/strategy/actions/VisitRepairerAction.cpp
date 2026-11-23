/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "VisitRepairerAction.h"

#include "BotRepair.h"
#include "BotSale.h"
#include "BotShopping.h"
#include "Creature.h"
#include "Event.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"

#include <mutex>
#include <unordered_map>

namespace
{
    // Bots update on the map threads, so the per-bot memory below is shared and locked.
    std::mutex sLock;

    // The money a bot had when a visit left its gear still worn (it could not pay for all of it); it tries again
    // once it has more.
    std::unordered_map<uint64, uint64> sShortOfMoney;

    constexpr float REPAIRER_TALK_DISTANCE = 4.0f;
}

bool VisitRepairerAction::isUseful()
{
    std::lock_guard<std::mutex> guard(sLock);
    auto const itr = sShortOfMoney.find(bot->GetGUID());
    return itr == sShortOfMoney.end() || bot->GetMoney() > itr->second;
}

bool VisitRepairerAction::Execute(Event /*event*/)
{
    Creature* repairer = BotRepair::FindNearbyRepairer(botAI, bot);
    if (!repairer)
        return false;

    if (bot->GetDistance(repairer) > REPAIRER_TALK_DISTANCE)
        return MoveTo(repairer, REPAIRER_TALK_DISTANCE - 1.0f);

    bot->SetFacingToObject(repairer);

    // A player sells first, so the money goes into the repair. BT-149: a random bot sells by the keep rules of
    // BotSale (grey items, gathered materials it has no profession for, gear it cannot use); an alt bot only the grey.
    bool const seller = BotSale::IsEligible(botAI, bot);
    if (repairer->IsVendor())
        botAI->DoSpecificAction("sell", Event("visit repairer", seller ? "policy" : "gray"), true);

    // RepairAllAction pays with the bot's money and the repairer's reputation discount; the core skips what the bot
    // cannot pay for.
    botAI->DoSpecificAction("repair", Event("visit repairer"), true);
    BotRepair::Forget(bot);

    // BT-149: what is left above the repair reserve buys food and drink, when this NPC sells them.
    if (seller && repairer->IsVendor())
    {
        BotSale::BuySupplies(botAI, bot, repairer);
        BotSale::BuyTools(bot, repairer);  // BT-219
        // Parity item 4: and a gear upgrade this vendor sells, out of the gear budget (repairs come first).
        BotShopping::BuyUpgradesAt(botAI, bot, repairer);
    }

    std::lock_guard<std::mutex> guard(sLock);
    if (BotRepair::NeedsRepair(bot))
    {
        sShortOfMoney[bot->GetGUID()] = bot->GetMoney();
        return false;
    }

    sShortOfMoney.erase(bot->GetGUID());
    return true;
}
