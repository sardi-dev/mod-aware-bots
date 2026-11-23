/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 *
 * The leader of a hand-formed dungeon party (BotLfm.h) leading like a player: stands where it recruits, then walks to the
 * dungeon's entrance (NewRpgBaseAction::MoveFarTo, as TravelToGroupAction), waits there for its members with a timeout
 * and walks in through the entrance trigger. Runs as the default action of the "lfm lead" strategy, which every random bot
 * carries; it is useful for a party leader only.
 */

#ifndef _PLAYERBOT_LFMLEADACTION_H
#define _PLAYERBOT_LFMLEADACTION_H

#include "NewRpgBaseAction.h"

class PlayerbotAI;

class LfmLeadAction : public NewRpgBaseAction
{
public:
    LfmLeadAction(PlayerbotAI* botAI) : NewRpgBaseAction(botAI, "lfm lead") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

#endif
