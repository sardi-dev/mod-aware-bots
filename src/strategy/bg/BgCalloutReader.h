/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_BGCALLOUTREADER_H
#define _PLAYERBOT_BGCALLOUTREADER_H

// BT-482: the human call-out reader of the battleground plans, the game-bound half of Utils/BgCalloutPolicy.h. It replaces the
// two separate chat readers of the Warsong Gulch and Arathi Basin plans: one hook, one parser, three maps (Warsong Gulch,
// Twin Peaks, Arathi Basin). A human's short line in the battleground chat becomes an immutable message in the bounded inbox
// of the match's shared state (PvpMatchState.h); the plans drain it on the map thread, weigh it against what they see, and
// treat it as information, never as a command (owner 2026-11-18, no command layer).

#include "Define.h"

#include <string>

class Group;
class Player;

namespace BgCalloutReader
{
// The group chat hook (session thread). Reads nothing but the sender, the group and the line, and writes nothing but one
// message into the match's inbox: the match state itself belongs to the map thread.
void OnTeamChat(Group* group, Player* sender, uint32 chatType, std::string const& message);
}

#endif
