/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_ONEVSONESTRATEGY_H
#define _PLAYERBOT_ONEVSONESTRATEGY_H

#include "Strategy.h"

class PlayerbotAI;

// The 1v1 engine (OneVsOne.h) in a combat engine: ranged bots keep their distance from a melee enemy player, melee bots
// close the gap, by skill tier. Only active while the bot fights exactly one enemy player (a duel, an open-world fight, an
// isolated battleground skirmish); PvE targets and arenas never fire it. AiPlayerbot.OneVsOne.Enable in the configuration.
class OneVsOneStrategy : public Strategy
{
public:
    OneVsOneStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::string const getName() override { return "1v1"; }
};

#endif
