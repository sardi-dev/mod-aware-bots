/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_VOICETRIGGERS_H
#define _PLAYERBOT_VOICETRIGGERS_H

#include "Trigger.h"

class PlayerbotAI;

// The bot noted that its whole group died and is alive and out of combat again. Checked every 5 seconds; the check is
// one atomic read while no bot has a note.
class VoiceWipeTrigger : public Trigger
{
public:
    VoiceWipeTrigger(PlayerbotAI* botAI) : Trigger(botAI, "voice wipe", 5) {}

    bool IsActive() override;
};

// A dungeon chat line is due for this bot, or the group's state moved (Mgr/Text/DungeonChat.h). Checked every 1.5 seconds; out of
// a dungeon, or without a player master, the check ends at the first test.
class DungeonChatTrigger : public Trigger
{
public:
    DungeonChatTrigger(PlayerbotAI* botAI) : Trigger(botAI, "dungeon chat", 1500) {}

    bool IsActive() override;
};

#endif
