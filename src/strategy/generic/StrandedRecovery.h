/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_STRANDEDRECOVERY_H
#define _PLAYERBOT_STRANDEDRECOVERY_H

#include "NewRpgBaseAction.h"
#include "Trigger.h"

// A bot in a closed pocket of the navmesh (the deck of the flooded Thousand Needles barge) for two minutes first gets
// itself out like a player: it walks to the rail, jumps or swims to a spot of the open mesh, or side steps; then it casts
// its Hearthstone. The reads, the landings and the order of the steps are StrandedPolicy.h (owner rule 2026-11-21, seen
// by a player or not). AiPlayerbot.StrandedRecovery.Enable turns it off.
class StrandedTrigger : public Trigger
{
public:
    StrandedTrigger(PlayerbotAI* botAI) : Trigger(botAI, "stranded", 2000) {}

    bool IsActive() override;
};

class StrandedRecoveryAction : public NewRpgBaseAction
{
public:
    StrandedRecoveryAction(PlayerbotAI* botAI) : NewRpgBaseAction(botAI, "stranded recovery") {}

    bool Execute(Event event) override;
    bool isUseful() override;

private:
    bool GetOut();
    bool CastHearthstone();
};

#endif
