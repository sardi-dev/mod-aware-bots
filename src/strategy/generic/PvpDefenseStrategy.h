/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_PVPDEFENSESTRATEGY_H
#define _PLAYERBOT_PVPDEFENSESTRATEGY_H

#include "Strategy.h"

class PlayerbotAI;

// Rule R2 in a combat engine (PvpDefense.h): a defensive at the right moment and the way out of control, for a bot in a
// battleground or an arena. The triggers are silent everywhere else. AiPlayerbot.Pvp.Defensives.Enable in the configuration.
class PvpDefenseStrategy : public Strategy
{
public:
    PvpDefenseStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::string const getName() override { return "pvp defense"; }
};

#endif
