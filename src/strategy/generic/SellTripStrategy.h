/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * "sell trip": a random bot sells what it does not need and buys food and drink at a vendor (BotSale.h, BT-149).
 */

#ifndef MOD_AWARE_BOTS_SELLTRIPSTRATEGY_H
#define MOD_AWARE_BOTS_SELLTRIPSTRATEGY_H

#include "Strategy.h"

class SellTripStrategy : public Strategy
{
public:
    SellTripStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::string const getName() override { return "sell trip"; }
};

#endif
