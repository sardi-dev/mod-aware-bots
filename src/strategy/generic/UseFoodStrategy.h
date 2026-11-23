#ifndef _PLAYERBOT_USEFOODSTRATEGY_H
#define _PLAYERBOT_USEFOODSTRATEGY_H

#include "Strategy.h"

class PlayerbotAI;

// AC parity (Ai/Base/Strategy/UseFoodStrategy.{h,cpp}): eat/drink when low, out of combat. AC's "food cheat" branch
// (refill at medium health or high mana without items) is left out (design decision 0016).
class UseFoodStrategy : public Strategy
{
public:
    UseFoodStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::string const getName() override { return "food"; }
};

#endif
