/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * "repair": a bot repairs its worn gear at a repair NPC and pays for it, like a player (BotRepair.h).
 */

#ifndef MOD_AWARE_BOTS_REPAIRSTRATEGY_H
#define MOD_AWARE_BOTS_REPAIRSTRATEGY_H

#include "Strategy.h"

class RepairStrategy : public Strategy
{
public:
    RepairStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::string const getName() override { return "repair"; }
};

#endif
