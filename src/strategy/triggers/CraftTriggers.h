/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * The trigger for alt bots that craft their gear upgrades like a player (BotCrafting.h).
 */

#ifndef MOD_AWARE_BOTS_CRAFTTRIGGERS_H
#define MOD_AWARE_BOTS_CRAFTTRIGGERS_H

#include "Trigger.h"

// A known recipe makes a gear upgrade for the bot's spec, and the bot has what it needs to make it here.
class CraftableUpgradeTrigger : public Trigger
{
public:
    CraftableUpgradeTrigger(PlayerbotAI* botAI) : Trigger(botAI, "craftable upgrade") {}

    bool IsActive() override;
};

#endif
