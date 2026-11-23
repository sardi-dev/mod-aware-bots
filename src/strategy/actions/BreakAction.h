/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 *
 * The bot's own side of a break (BotBreak.h): validates the offer, starts the break, keeps the bot still and ends it. The
 * default action of the "break" strategy that every random bot carries; it is useful only while the bot has a break.
 */

#ifndef _PLAYERBOT_BREAKACTION_H
#define _PLAYERBOT_BREAKACTION_H

#include "Action.h"

class PlayerbotAI;

class BreakAction : public Action
{
public:
    BreakAction(PlayerbotAI* botAI) : Action(botAI, "break") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

#endif
