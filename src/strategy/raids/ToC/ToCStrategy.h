/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef PLAYERBOTS_TOCSTRATEGY_H
#define PLAYERBOTS_TOCSTRATEGY_H

#include "Strategy.h"

// Trial of the Crusader (map 649), strategy "totc". New for this module: mod-playerbots has none for this raid.
class RaidToCStrategy : public Strategy
{
public:
    RaidToCStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}
    std::string const getName() override { return "totc"; }
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    void InitMultipliers(std::vector<Multiplier*>& multipliers) override;
};

#endif
