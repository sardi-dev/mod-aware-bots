/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_SUMMONTRIGGERS_H
#define _PLAYERBOT_SUMMONTRIGGERS_H

#include "Trigger.h"

class PlayerbotAI;

// The bot has a summon request waiting for its answer. One atomic read while no bot has one.
class SummonPendingTrigger : public Trigger
{
public:
    SummonPendingTrigger(PlayerbotAI* botAI) : Trigger(botAI, "summon pending") {}

    bool IsActive() override;
};

// A random bot in a group with a player, with a bot group mate far away, checked every 10 seconds.
class GroupMateFarTrigger : public Trigger
{
public:
    GroupMateFarTrigger(PlayerbotAI* botAI) : Trigger(botAI, "group mate far", 10) {}

    bool IsActive() override;
};

#endif
