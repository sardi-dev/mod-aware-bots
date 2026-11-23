/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * The action for alt bots that buy their gear upgrades like a player (BotShopping.h).
 */

#ifndef MOD_AWARE_BOTS_VISITVENDORACTION_H
#define MOD_AWARE_BOTS_VISITVENDORACTION_H

#include "MovementActions.h"

// Walks to the nearby vendor that sells an upgrade, buys what is useful, equips the upgrades and tells its group.
class VisitVendorAction : public MovementAction
{
public:
    VisitVendorAction(PlayerbotAI* botAI) : MovementAction(botAI, "visit vendor") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

// BT-218: walks to the vendor that sells a mount the bot can pay for, buys the cheapest and learns it.
class BuyMountAction : public MovementAction
{
public:
    BuyMountAction(PlayerbotAI* botAI) : MovementAction(botAI, "buy mount") {}

    bool Execute(Event event) override;
};

#endif
