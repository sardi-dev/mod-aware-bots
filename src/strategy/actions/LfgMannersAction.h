/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 *
 * Finder manners: how a random bot that the Dungeon Finder put in a group behaves like the strangers a player meets
 * there. In the dungeon it greets the group (a bot at a time, spaced), once asks whether anyone needs the quests, says
 * it is ready, and after the final boss (the group's finder state turns to "finished", which the core sets when the last
 * encounter dies) it thanks, waits for the loot and the group's open loot rolls (at most 60 s, BT-184) and leaves with a
 * goodbye. It never leaves in the middle of a run.
 * A group left with no player in it, and a bot-only run that hit its time limit (LfgFill.h), leave quietly; a bot held in
 * a combat state does so too, from its combat engine ("lfg combat leave"), once its own fight is over (BT-206).
 *
 * All lines go through the bot voice (Mgr/Text/BotVoice.h) under the names lfg_hello, lfg_quest, lfg_ready, lfg_thanks and
 * lfg_bye (a text table row per name or per personality, an in-code default each), with a cooldown per bot and per group. Settings are
 * AiPlayerbot.LfgManners.* (conf/playerbots.conf.dist).
 */

#ifndef _PLAYERBOT_LFGMANNERSACTION_H
#define _PLAYERBOT_LFGMANNERSACTION_H

#include "Action.h"

class Player;
class PlayerbotAI;

namespace LfgManners
{
    // The trigger's question: is a line or the leaving due for this bot now.
    bool Due(PlayerbotAI* botAI, Player* bot);
}

// "lfg manners": says the next line that is due, or leaves the group after the dungeon.
class LfgMannersAction : public Action
{
public:
    LfgMannersAction(PlayerbotAI* botAI) : Action(botAI, "lfg manners") {}

    bool Execute(Event event) override;
};

#endif
