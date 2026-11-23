/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#ifndef _PLAYERBOT_GENERICDKSTRATEGY_H
#define _PLAYERBOT_GENERICDKSTRATEGY_H

#include "MeleeCombatStrategy.h"

class PlayerbotAI;

class GenericDKStrategy : public MeleeCombatStrategy
{
public:
    GenericDKStrategy(PlayerbotAI* botAI);

    std::string const getName() override { return "DK"; }
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;

protected:
    // Class sheet, blood death knight (BT-134): the tank strategy sets its own Icebound Fortitude and Rune Tap thresholds
    // (BloodDKStrategy.cpp), so it asks the shared "dk health 40" edge to stay out. Frost and Unholy keep that edge.
    virtual bool HasTankDefensiveThresholds() const { return false; }
};

#endif
