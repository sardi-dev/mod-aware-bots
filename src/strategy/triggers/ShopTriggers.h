/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * The trigger for alt bots that buy their gear upgrades like a player (BotShopping.h).
 */

#ifndef MOD_AWARE_BOTS_SHOPTRIGGERS_H
#define MOD_AWARE_BOTS_SHOPTRIGGERS_H

#include "Trigger.h"

// A vendor within reach sells an upgrade for the bot's spec that it can pay for. Checked every tick, like "trainer
// nearby": the walk to the vendor only continues while this stays active.
class VendorUpgradeNearbyTrigger : public Trigger
{
public:
    VendorUpgradeNearbyTrigger(PlayerbotAI* botAI) : Trigger(botAI, "vendor upgrade nearby") {}

    bool IsActive() override;
};

// BT-218: a random bot that has trained riding and knows no mount sees a vendor selling one it can pay for.
class MountVendorNearbyTrigger : public Trigger
{
public:
    MountVendorNearbyTrigger(PlayerbotAI* botAI) : Trigger(botAI, "mount vendor nearby") {}

    bool IsActive() override;
};

#endif
