/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_ARENAPLAN_H
#define _PLAYERBOT_ARENAPLAN_H

// BT-154: team play in a ladder arena match, play v1. A bot of a ladder team shares a plan with its partner: one focus
// target, picked by a deterministic score (ArenaPlanMath.h) at the tier's interval by whichever of the two ticks first,
// then read by both. On top of the shared focus a bot decides for itself when to use a big self defensive and when to
// walk out of the line of sight of the enemy casters. Every decision is a candidate with a score, and the best ones are
// logged at debug level ([ARENAPLAN]) so that a later selector can choose among them.
//
// Map thread only (bot AI). The plan of a team lives in a small table guarded by a lock; the two bots and the two enemies
// of a 2v2 arena are always on the same map, so the lock is never contended.

#include "Define.h"

class Player;
class Unit;

namespace ArenaPlan
{
// The bot is a member of a ladder team inside its match (the arena is in progress and the team has a tier).
bool Active(Player* bot);

// The shared focus of the bot's team, or nullptr when there is none (a healer has none: it heals).
Unit* FocusTarget(Player* bot);

// The self defensive the bot should cast now, 0 for none. Also notes the health sample for the prediction of a lethal burst.
uint32 DefensiveDue(Player* bot);
// Notes a defensive that was cast (the score counts a recent one against a second).
void NoteDefensiveCast(Player* bot);

// A point out of the line of sight of the enemy casters that the bot should walk to now. False for none.
bool CoverDue(Player* bot, float& x, float& y, float& z);
// The bot has set off for the spot: the next search may find another.
void NoteCoverTaken(Player* bot);
}  // namespace ArenaPlan

#endif
