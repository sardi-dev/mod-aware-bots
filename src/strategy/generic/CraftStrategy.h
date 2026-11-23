/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * "craft": an alt bot crafts its gear upgrades with its own professions and reagents (BotCrafting.h).
 */

#ifndef MOD_AWARE_BOTS_CRAFTSTRATEGY_H
#define MOD_AWARE_BOTS_CRAFTSTRATEGY_H

#include "Strategy.h"

class CraftStrategy : public Strategy
{
public:
    CraftStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::string const getName() override { return "craft"; }
};

#endif
