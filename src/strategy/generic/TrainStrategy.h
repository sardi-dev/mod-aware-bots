/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * "train": an alt bot learns its class spells at a trainer like a player (BotTraining.h).
 */

#ifndef MOD_AWARE_BOTS_TRAINSTRATEGY_H
#define MOD_AWARE_BOTS_TRAINSTRATEGY_H

#include "Strategy.h"

class TrainStrategy : public Strategy
{
public:
    TrainStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::string const getName() override { return "train"; }
};

#endif
