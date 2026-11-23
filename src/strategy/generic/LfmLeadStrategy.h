/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_LFMLEADSTRATEGY_H
#define _PLAYERBOT_LFMLEADSTRATEGY_H

#include "Strategy.h"

class PlayerbotAI;

// Carries the "lfm lead" default action (LfmLeadAction.h).
class LfmLeadStrategy : public Strategy
{
public:
    LfmLeadStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    std::string const getName() override { return "lfm lead"; }
    uint32 GetType() const override { return STRATEGY_TYPE_NONCOMBAT; }
    NextAction** getDefaultActions() override;
};

#endif
