/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef PLAYERBOTS_SUNWELLSTRATEGY_H
#define PLAYERBOTS_SUNWELLSTRATEGY_H

#include "Strategy.h"

class RaidSunwellStrategy : public Strategy
{
public:
    RaidSunwellStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}
    std::string const getName() override { return "sunwell"; }
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    void InitMultipliers(std::vector<Multiplier*>& multipliers) override;
    void AppendTargetExclusions(GuidSet& exclusions, TargetValueExclusionType type) override;
    bool HasTargetExclusions() const override { return true; }
};

#endif
