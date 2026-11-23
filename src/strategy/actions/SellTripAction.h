/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * The action of a random bot's vendor trip (BotSale.h, BT-149).
 */

#ifndef MOD_AWARE_BOTS_SELLTRIPACTION_H
#define MOD_AWARE_BOTS_SELLTRIPACTION_H

#include "MovementActions.h"

// Walks to the nearby vendor, sells what the bot does not need and buys food and drink with its own money.
class SellTripAction : public MovementAction
{
public:
    SellTripAction(PlayerbotAI* botAI) : MovementAction(botAI, "sell trip") {}

    bool Execute(Event event) override;
};

#endif
