/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * The action of the "pvp talents" strategy (Factory/BotPvpTalents.h, BT-320).
 */

#ifndef MOD_AWARE_BOTS_PVPTALENTSETACTION_H
#define MOD_AWARE_BOTS_PVPTALENTSETACTION_H

#include "Action.h"

// Starts the Activate Spec cast to the PvP or PvE talent group, or puts the PvP set into the secondary group.
class PvpTalentSetAction : public Action
{
public:
    PvpTalentSetAction(PlayerbotAI* botAI) : Action(botAI, "pvp talent set") {}

    bool Execute(Event event) override;
};

#endif
