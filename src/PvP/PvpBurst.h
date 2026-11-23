/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_PVPBURST_H
#define _PLAYERBOT_PVPBURST_H

// BT-620: the game side of the burst window (Utils/PvpBurstPolicy.h). A class trigger that gates a big offensive cooldown on a
// boss, an elite or a pack asks here when its target is an enemy player: the glue reads what a player would see (immunity, a
// breakable crowd control, a held target, the medallion the bot saw used, the fight time) and the policy answers. The PvE gates of
// the triggers are untouched.
//
// Map thread only (bot AI). The only state is a small per-bot clock of how long the bot has fought its target, under a lock.

#include "Define.h"
#include "PvpBurstPolicy.h"

class Player;
class PlayerbotAI;
class Unit;

namespace PvpBurstGlue
{
// The window on `target` for this bot. Reason::NONE and closed when the target is not an attackable enemy player.
PvpBurst::Verdict Window(PlayerbotAI* botAI, Player* bot, Unit* target);

// The window on the bot's current target.
PvpBurst::Verdict WindowOnCurrentTarget(PlayerbotAI* botAI, Player* bot);

// Shorthands for the triggers: true when the target is an enemy player and the window is open.
bool Open(PlayerbotAI* botAI, Player* bot, Unit* target);
bool OpenOnCurrentTarget(PlayerbotAI* botAI, Player* bot);

// The bot's current target is an attackable enemy player (the PvP branch applies, so the PvE gate must not).
bool TargetIsEnemyPlayer(PlayerbotAI* botAI, Player* bot, Unit* target);

// Drops the bot's fight clock (logout).
void Forget(uint32 guidLow);
}  // namespace PvpBurstGlue

#endif
