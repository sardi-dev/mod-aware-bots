/*
 * AC parity (#408): ported from AC mod-playerbots Ai/Class/Paladin/Strategy/PaladinPullStrategy.h.
 *
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option) any later version.
 */

#ifndef _PLAYERBOT_PALADINPULLSTRATEGY_H
#define _PLAYERBOT_PALADINPULLSTRATEGY_H

#include "PullStrategy.h"

#include <ctime>

class PaladinPullStrategy : public PullStrategy
{
public:
    PaladinPullStrategy(PlayerbotAI* botAI) : PullStrategy(botAI, "judgement", "seal of righteousness") {}

    std::string GetPullActionName() const override;
    std::string GetPreActionName() const override;

private:
    // BT-167: bounce check cache (see PaladinPullStrategy.cpp), the name getters are const.
    mutable uint64 bounceMark = 0;
    mutable time_t bounceCheckedAt = 0;
    mutable bool bounceRisk = false;
};

#endif
