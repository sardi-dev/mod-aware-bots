/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 *
 * Breaks, like a player's. AFK on a random bot means a real break: the CPU-saving AFK flag of idle bots is gone (the CPU
 * saving itself stays, see PlayerbotAI::UpdateAI).
 *
 *  - A solo bot sometimes sits down for 2 to 10 minutes at an inn, in a city or at a quiet spot ("brb, food"), flagged
 *    AFK with an auto-reply, and gets up afterwards.
 *  - A bot in a group with a player takes a break only rarely, never while anyone is fighting or anything hostile is
 *    near, announces it in party chat, and comes back when its time is up, when a fight starts or when the player walks
 *    off.
 *  - Now and then a bot logs off for a while and comes back later. It uses the manager's own events: the "add" event
 *    (how long the bot stays in the world) and the "logout" event (how long it stays out), see
 *    RandomPlayerbotMgr::ScheduleLogoff.
 *
 * Decisions are made on the world thread, one cheap pass every 5 seconds (Update). A bot that is offered a break
 * validates it on its own AI tick (BreakAction), starts it and holds still; the end of a break only touches flags, so
 * the world thread can end an overdue one when the bot is on idle ticks. Every line goes through BotVoice.
 *
 * Config: AiPlayerbot.BotBreaks.* (see conf/playerbots.conf.dist). Log lines: [BREAK].
 */

#ifndef _PLAYERBOT_BOTBREAK_H
#define _PLAYERBOT_BOTBREAK_H

#include "Define.h"

class Player;
class PlayerbotAI;

namespace BotBreak
{
    // AiPlayerbot.BotBreaks.Enable
    bool Enabled();

    // World thread, from RandomPlayerbotMgr::UpdateAIInternal; throttles itself.
    void Update();

    // The bot has a break on offer or is on one (any thread, one atomic read while nobody does).
    bool Wants(uint32 botLowGuid);

    // The bot is on a break now: its AFK flag is real and stays.
    bool IsOnBreak(uint32 botLowGuid);

    // The bot's own AI tick (BreakAction): validates an offer, starts the break, keeps the bot still, ends it.
    void Tick(PlayerbotAI* botAI);
}

#endif
