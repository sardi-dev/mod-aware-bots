/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 *
 * Dungeon chat: the small talk of a random bot in a dungeon group with a player. The bots notice the moments of a run
 * (the start, a boss pull and kill, a death, a long fight, a healer running low on mana, a loot roll, the end of the
 * run, quiet walks between packs) and answer simple party chat of the players ("ready?", "wait", "thanks", "lol",
 * "good job"). Everything here is deterministic: the lines come from the bot text table (rows "dgn_<event>" with a
 * variant per personality "dgn_<event>_<personality>"), with a few lines of each in the code as a fallback.
 *
 * Rate limits, so a group never floods the chat: one decision per moment for the whole group (the first bot that notices
 * a moment rolls for it, and at most one bot speaks), a gap between any two lines of a group, a longer gap per bot, and
 * a roll per moment. The lines go through the bot voice (BotVoice.h), which also keeps its own cooldowns.
 *
 * Config: AiPlayerbot.DungeonChat.* (see conf/playerbots.conf.dist). Nothing here runs outside a dungeon or without a
 * player in the group; the trigger polls every 1.5 seconds and the group state is re-read once a second at most.
 */

#ifndef _PLAYERBOT_DUNGEONCHAT_H
#define _PLAYERBOT_DUNGEONCHAT_H

#include "Define.h"

#include <string>

class Creature;
class Group;
class Player;
class PlayerbotAI;
struct ItemTemplate;

namespace DungeonChat
{
    // AiPlayerbot.DungeonChat.Enable.
    bool Enabled();

    // The trigger's question: does this bot have a line to say or a thing to do now. Also advances the group's state, so
    // it is the only call a bot needs to make on its own tick.
    bool Due(PlayerbotAI* botAI, Player* bot);

    // The action: says the bot's due line (and plays its emote), or does the planned drink. False when nothing was due.
    bool Deliver(PlayerbotAI* botAI);

    // A party or raid message of a player in a dungeon group: one bot may answer a simple phrase after a short pause.
    // True when the phrase is one the rules answer (planned, or held back by their roll or reply gap): the AI layer then
    // leaves the line alone.
    bool OnGroupChat(Group* group, Player* sender, uint32 chatType, std::string const& message);

    // A bot saw a dungeon boss die (the experience packet). True when the dungeon chat took the moment, so the caller
    // skips its own boss line; the moment is claimed once per boss for the whole group.
    bool OnBossKilled(PlayerbotAI* botAI, Creature* boss);

    // A bot voted need or greed on a group roll (vote is a RollType value).
    void OnLootRoll(PlayerbotAI* botAI, ItemTemplate const* item, uint32 vote);
}

#endif
