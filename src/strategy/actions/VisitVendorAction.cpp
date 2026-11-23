/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "VisitVendorAction.h"

#include "BotShopping.h"
#include "ChatHelper.h"
#include "Creature.h"
#include "Event.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"

#include <mutex>
#include <sstream>
#include <unordered_map>

namespace
{
    // Bots update on the map threads, so the per-bot memory below is shared and locked.
    std::mutex sLock;

    // A visit that bought nothing; the bot tries again once its level or money grows.
    struct FailedVisit
    {
        uint8 level;
        uint64 money;
    };
    std::unordered_map<uint64, FailedVisit> sFailedVisits;

    constexpr float VENDOR_TALK_DISTANCE = 4.0f;
}

bool VisitVendorAction::isUseful()
{
    std::lock_guard<std::mutex> guard(sLock);
    auto const itr = sFailedVisits.find(bot->GetGUID());
    return itr == sFailedVisits.end() || bot->getLevel() > itr->second.level || bot->GetMoney() > itr->second.money;
}

bool VisitVendorAction::Execute(Event /*event*/)
{
    Creature* vendor = BotShopping::FindNearbyVendor(botAI, bot);
    if (!vendor)
        return false;

    if (bot->GetDistance(vendor) > VENDOR_TALK_DISTANCE)
        return MoveTo(vendor, VENDOR_TALK_DISTANCE - 1.0f);

    bot->SetFacingToObject(vendor);
    uint64 const moneyBefore = bot->GetMoney();
    uint32 const upgradesBefore = BotShopping::CountUpgradesAt(botAI, bot, vendor);

    // BuyAction's "vendor" mode buys what the bot can use within its budgets and equips the upgrades.
    botAI->DoSpecificAction("buy", Event("visit vendor", "vendor"), true);
    BotShopping::Forget(bot);

    // The item usage values may still be cached right after the purchase, so money spent counts as success too.
    uint32 const upgradesLeft = BotShopping::CountUpgradesAt(botAI, bot, vendor);
    bool const spent = bot->GetMoney() < moneyBefore;
    if (upgradesLeft >= upgradesBefore && !spent)
    {
        std::lock_guard<std::mutex> guard(sLock);
        sFailedVisits[bot->GetGUID()] = { bot->getLevel(), bot->GetMoney() };
        return false;
    }

    {
        std::lock_guard<std::mutex> guard(sLock);
        sFailedVisits.erase(bot->GetGUID());
    }

    std::ostringstream out;
    uint32 const bought = upgradesBefore > upgradesLeft ? upgradesBefore - upgradesLeft : 0;
    if (bought)
        out << "I bought " << bought << (bought == 1 ? " upgrade" : " upgrades") << " from " << vendor->GetName();
    else
        out << "I bought gear from " << vendor->GetName();
    if (spent)
        out << " for " << ChatHelper::formatMoney(uint32(moneyBefore - bot->GetMoney()));
    out << ".";
    if (!botAI->SayToParty(out.str()))   // a groupmate says it in party chat; alone, it whispers its master
        botAI->TellMaster(out);
    return true;
}

bool BuyMountAction::Execute(Event /*event*/)
{
    Creature* vendor = BotShopping::FindMountVendor(botAI, bot);
    if (!vendor)
        return false;

    if (bot->GetDistance(vendor) > VENDOR_TALK_DISTANCE)
        return MoveTo(vendor, VENDOR_TALK_DISTANCE - 1.0f);

    bot->SetFacingToObject(vendor);
    return BotShopping::BuyMountAt(bot, vendor);
}
