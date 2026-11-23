/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * The action for bots that repair their gear like a player (BotRepair.h).
 */

#ifndef MOD_AWARE_BOTS_VISITREPAIRERACTION_H
#define MOD_AWARE_BOTS_VISITREPAIRERACTION_H

#include "MovementActions.h"

// Walks to the nearby repair NPC, sells the junk if it is also a vendor, and pays for the repair.
class VisitRepairerAction : public MovementAction
{
public:
    VisitRepairerAction(PlayerbotAI* botAI) : MovementAction(botAI, "visit repairer") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

#endif
