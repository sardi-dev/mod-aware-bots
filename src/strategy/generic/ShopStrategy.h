/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * "shop": an alt bot buys its gear upgrades at a vendor like a player (BotShopping.h).
 */

#ifndef MOD_AWARE_BOTS_SHOPSTRATEGY_H
#define MOD_AWARE_BOTS_SHOPSTRATEGY_H

#include "Strategy.h"

class ShopStrategy : public Strategy
{
public:
    ShopStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::string const getName() override { return "shop"; }
};

#endif
