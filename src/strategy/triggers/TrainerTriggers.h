/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * Triggers for alt bots that learn at a class trainer like a player (BotTraining.h).
 */

#ifndef MOD_AWARE_BOTS_TRAINERTRIGGERS_H
#define MOD_AWARE_BOTS_TRAINERTRIGGERS_H

#include "Trigger.h"

// A class trainer of the bot's class is within reach and teaches it a spell it can learn now. Checked every tick: the
// walk to the trainer only continues while this stays active, since following the master is the default action.
class TrainerNearbyTrigger : public Trigger
{
public:
    TrainerNearbyTrigger(PlayerbotAI* botAI) : Trigger(botAI, "trainer nearby") {}

    bool IsActive() override;
};

// The bot has class spells to learn and no trainer in reach.
class SpellsToTrainTrigger : public Trigger
{
public:
    SpellsToTrainTrigger(PlayerbotAI* botAI) : Trigger(botAI, "spells to train", 60) {}

    bool IsActive() override;
};

#endif
