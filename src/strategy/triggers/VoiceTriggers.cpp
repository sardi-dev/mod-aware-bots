/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "VoiceTriggers.h"

#include "BotVoice.h"
#include "DungeonChat.h"
#include "Playerbots.h"

bool VoiceWipeTrigger::IsActive() { return bot->IsAlive() && BotVoice::HasPendingWipe(bot); }

bool DungeonChatTrigger::IsActive() { return DungeonChat::Due(botAI, bot); }
