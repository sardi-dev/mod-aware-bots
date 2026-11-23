/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_VOICEACTIONS_H
#define _PLAYERBOT_VOICEACTIONS_H

#include "Action.h"

class PlayerbotAI;

// Says the wipe line once the bot is alive and out of combat after its whole group died (see BotVoice.h).
class VoiceWipeAction : public Action
{
public:
    VoiceWipeAction(PlayerbotAI* botAI) : Action(botAI, "voice wipe") {}

    bool Execute(Event event) override;
};

// Says the bot's due dungeon line and plays its emote, or drinks (Mgr/Text/DungeonChat.h).
class DungeonChatAction : public Action
{
public:
    DungeonChatAction(PlayerbotAI* botAI) : Action(botAI, "dungeon chat") {}

    bool Execute(Event event) override;
};

#endif
