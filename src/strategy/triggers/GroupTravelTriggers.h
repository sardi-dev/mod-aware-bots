/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_GROUPTRAVELTRIGGERS_H
#define _PLAYERBOT_GROUPTRAVELTRIGGERS_H

#include "Trigger.h"

class PlayerbotAI;

// A random bot in a player's group that is far from it, or already on its way (see TravelToGroupAction.h).
class FarFromGroupLeaderTrigger : public Trigger
{
public:
    FarFromGroupLeaderTrigger(PlayerbotAI* botAI) : Trigger(botAI, "far from group leader") {}

    bool IsActive() override;
};

#endif
