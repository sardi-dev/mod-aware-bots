/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef MOD_AWARE_BOTS_REPAIRTRIGGERS_H
#define MOD_AWARE_BOTS_REPAIRTRIGGERS_H

#include "Trigger.h"

class PlayerbotAI;

// The bot's equipped gear is worn and a repair NPC is in sight (BotRepair.h).
class RepairNeededNearbyTrigger : public Trigger
{
public:
    RepairNeededNearbyTrigger(PlayerbotAI* botAI) : Trigger(botAI, "repair needed nearby") {}

    bool IsActive() override;
};

#endif
