/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * "mark pull": the group's tank pulls the target its leader marked (design decision 0016). A non-combat strategy, since the
 * pull starts out of combat and the combat engine only runs during a fight.
 */

#ifndef MOD_AWARE_BOTS_MARKPULLSTRATEGY_H
#define MOD_AWARE_BOTS_MARKPULLSTRATEGY_H

#include "Strategy.h"

class MarkPullStrategy : public Strategy
{
public:
    MarkPullStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::string const getName() override { return "mark pull"; }
};

#endif
