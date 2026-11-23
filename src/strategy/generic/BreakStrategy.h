/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_BREAKSTRATEGY_H
#define _PLAYERBOT_BREAKSTRATEGY_H

#include "Strategy.h"

class PlayerbotAI;

// Carries the "break" default action (BreakAction.h).
class BreakStrategy : public Strategy
{
public:
    BreakStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    std::string const getName() override { return "break"; }
    uint32 GetType() const override { return STRATEGY_TYPE_NONCOMBAT; }
    NextAction** getDefaultActions() override;
};

#endif
