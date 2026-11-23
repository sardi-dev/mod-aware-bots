/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef MOD_AWARE_BOTS_SELLTRIGGERS_H
#define MOD_AWARE_BOTS_SELLTRIGGERS_H

#include "Trigger.h"

class PlayerbotAI;

// A random bot's bags are almost full with things to sell, or its food or drink ran out, and a vendor is in sight
// (BotSale.h). Checked every tick like "repair needed nearby", since the walk only continues while it stays active; the
// costly item checks inside run at most every few seconds per bot.
class VendorTripWantedTrigger : public Trigger
{
public:
    VendorTripWantedTrigger(PlayerbotAI* botAI) : Trigger(botAI, "vendor trip wanted") {}

    bool IsActive() override;
};

#endif
