/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_PVPTEAM_H
#define _PLAYERBOT_PVPTEAM_H

// BT-628 to BT-631: the game side of the shared team state (Utils/PvpTeamPolicy.h, docs/arena/team-play.md): the healer lock and the
// kill window, the control offers and reservations, the partners' burst, and the hold on breakable control. It works in an arena
// and in a battleground skirmish (the match state of the battleground holds one team table per side; the enemy healer and the
// partners are those within reach of the kill target, so a bot far from the fight takes no part).
//
// Map thread only (bot AI). What is public is read directly (auras, unit states, the group); what one bot saw of the enemy (the
// medallion, the diminishing returns, the kicks) comes from that bot's own awareness book and is never shared.

#include "Define.h"

class Action;
class Player;
class PlayerbotAI;
class Unit;

namespace PvpTeamGlue
{
// The longest time the unit cannot act freely from hard control or a silence (0: free), read off its auras.
uint32 LockedMs(Unit* unit);

// BT-651: the enemy healer within reach of `target` (a battleground has many enemies; the healer of this fight is the one near the
// target) and how long it is held. Public state only. Used by class rules that keep a control for the healer (Deep Freeze).
struct HealerFacts
{
    bool present = false;
    bool isTarget = false;     // the target is that healer
    uint32 lockedMs = 0;       // hard control or silence left on it
};
HealerFacts HealerNear(PlayerbotAI* botAI, Player* bot, Unit* target);

// BT-658: what a bot may read of the enemy side without a book: an enemy player shows a big offensive cooldown as an aura on itself
// (the list in PvpTeam.cpp), and a stopper (a kick) that the bot's own book does not show as used may stop a cast.
bool HasBurstAura(Unit* unit);
bool EnemyStopperNotSpent(PlayerbotAI* botAI, Player* bot);

// What the team state adds to a burst on `target`: the shared kill window and a partner's burst.
struct BurstFacts
{
    bool teamWindow = false;
    bool partnerBurst = false;
};
BurstFacts BurstFactsFor(PlayerbotAI* botAI, Player* bot, Player* target);

// The window of the bot's side is open on this enemy (the arena plan weighs it in the focus).
bool GoOpenOn(Player* bot, Unit* enemy);

// R4: this bot is the one to cast a control on the enemy healer (or the off damage dealer in an arena) now.
bool ControlDue(PlayerbotAI* botAI, Player* bot);
// The control cast. False when nothing is due or the cast failed.
bool ControlCast(PlayerbotAI* botAI, Player* bot);
// The control the bot would cast now is this spell (Presence of Mind goes to an instant Polymorph).
bool ControlPickIs(PlayerbotAI* botAI, Player* bot, uint32 spellId);

// R4: a damage cast at a target that a teammate holds in a breakable control would free it.
bool HoldsDamage(PlayerbotAI* botAI, Player* bot, Action* action);

void Forget(uint32 guidLow);
}  // namespace PvpTeamGlue

#endif
