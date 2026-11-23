/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_DPSSHEETSTRATEGY_H
#define _PLAYERBOT_DPSSHEETSTRATEGY_H

#include "Multiplier.h"
#include "Strategy.h"

class PlayerbotAI;

// Damage class sheets: a bot plays the priority lines its skill tier allows (Utils/DpsSheetPolicy.h).
class DpsSheetMultiplier : public Multiplier
{
public:
    explicit DpsSheetMultiplier(PlayerbotAI* ai) : Multiplier(ai, "dps sheet") {}
    float GetValue(Action* action) override;
};

class DpsSheetStrategy : public Strategy
{
public:
    explicit DpsSheetStrategy(PlayerbotAI* ai) : Strategy(ai) {}
    void InitMultipliers(std::vector<Multiplier*>& multipliers) override;
    std::string const getName() override { return "dps sheet"; }
};

#endif
