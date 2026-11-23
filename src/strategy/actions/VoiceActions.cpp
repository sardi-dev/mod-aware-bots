/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "VoiceActions.h"

#include "BotVoice.h"
#include "DungeonChat.h"
#include "Event.h"
#include "Playerbots.h"

bool VoiceWipeAction::Execute(Event /*event*/) { return BotVoice::SayAfterWipe(botAI); }

bool DungeonChatAction::Execute(Event /*event*/) { return DungeonChat::Deliver(botAI); }
