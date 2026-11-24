/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 *
 * The soak side of the social layer (docs/social/social.md section 13): the OnlyBots scope test, the counters, the one log
 * line per social action and the summary line. The decisions and the line formats are in Utils/SocialSoakPolicy.h; this file
 * only holds the shared atomic counters and writes to the server log. Callable from the world, map and session threads.
 */

#ifndef _PLAYERBOT_SOCIALLOG_H
#define _PLAYERBOT_SOCIALLOG_H

#include "Define.h"
#include "SocialSoakPolicy.h"

#include <string>

class Player;

namespace SocialLog
{
    // True when AiPlayerbot.Social.OnlyBots is empty or names this bot. A null bot is not in scope.
    bool InScope(Player* bot);

    void Count(SocialSoak::Counter counter, uint64 amount = 1);

    // One action line: info level while AiPlayerbot.Social.SoakLog is on, debug level otherwise.
    void Action(std::string const& line);

    // An offer line: info level always (offers are rare and the owner asked to see them).
    void Offer(std::string const& line);

    // World thread, called every update of the offer pass: writes the summary line when it is due.
    void Update();

    // The server stops: the final summary line, when any social switch was on.
    void Shutdown();
}

#endif
