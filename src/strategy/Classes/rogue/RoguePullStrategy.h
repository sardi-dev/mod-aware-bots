/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#ifndef _PLAYERBOT_ROGUEPULLSTRATEGY_H
#define _PLAYERBOT_ROGUEPULLSTRATEGY_H

#include "PullStrategy.h"

// A 5.4.8 rogue has no ranged weapon slot, so the generic "shoot" pull never runs. The ranged pull is the Shuriken
// Toss talent (114014); a rogue without it refuses the pull and fights in melee.
class RoguePullStrategy : public PullStrategy
{
public:
    RoguePullStrategy(PlayerbotAI* botAI) : PullStrategy(botAI, "shuriken toss") {}

    std::string GetPullActionName() const override;
};

#endif
