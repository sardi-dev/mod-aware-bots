/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_PVPARMS_H
#define _PLAYERBOT_PVPARMS_H

// BT-673 to BT-676: the game side of the Arms Warrior package (Utils/PvpArmsPolicy.h): reads what a player sees (the rage, the
// burst window, the target's auras and range, what the warrior cast last, the bot's own book of diminishing returns), asks the
// policy and answers the triggers, the action and the multiplier of the Arms strategy. Map thread only (bot AI). Everything
// answers only for an enemy player target; PvE is untouched.

class Action;
class PlayerbotAI;
class Player;

namespace PvpArmsGlue
{
// Recklessness, Skull Banner and Avatar against a player wait for the go (the burst window and the pooled rage): true when the
// cast must be held.
bool GoHeld(PlayerbotAI* botAI, Player* bot, Action* action);

// The stun chain: Storm Bolt or Shockwave on the current target now.
bool StunDue(PlayerbotAI* botAI, Player* bot);
bool StunCast(PlayerbotAI* botAI, Player* bot);

// Disarm on the current target now (after a stun, or against a physical burst).
bool DisarmDue(PlayerbotAI* botAI, Player* bot);

// Drops the bot's window clock (logout).
void Forget(uint32 guidLow);
}  // namespace PvpArmsGlue

#endif
