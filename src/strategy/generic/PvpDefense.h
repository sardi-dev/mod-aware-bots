/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_PVPDEFENSE_H
#define _PLAYERBOT_PVPDEFENSE_H

// BT-522: the game side of rule R2 (Utils/PvpDefensivePolicy.h, docs/pvp/awareness-r1-r2.md): which defensive cooldown a
// bot presses against players, and how it gets out of control. One place reads the facts a player sees (its own health and
// how fast it fell, who hits it and what kind they are, the auras that hold it, its teammates' health, the enemy healer,
// the enemy's cooldowns it saw) and asks the pure policy; the 1v1 engine, the arena plan and the battleground and arena
// strategies use the answers, so a duel, a battleground and an arena play the same rules.

#include "PvpDefensivePolicy.h"

class Item;
class Player;
class PlayerbotAI;

namespace PvpDefenseGlue
{
// The bot is in a fight where these rules decide: a battleground, an arena, or a 1v1 (a started duel or one enemy player).
bool InPvp(PlayerbotAI* botAI, Player* bot);

struct DefensiveVerdict
{
    bool use = false;
    std::uint32_t spellId = 0;
    char const* why = "none";
};

// The defensive to press now. `due` is a caller that has its own "now" rule (the arena plan's score): only the choice of
// spell is asked then. Cheap when the bot is healthy and nobody hits it.
DefensiveVerdict Defensive(PlayerbotAI* botAI, Player* bot, bool due);

// Remembers a defensive that was cast (the one per go rule counts from here).
void NoteDefensiveCast(Player* bot);

struct BreakVerdict
{
    bool use = false;
    std::uint32_t spellId = 0;     // the spell to cast on itself (the item spell for the medallion)
    bool trinket = false;          // use the equipped medallion item
    Item* item = nullptr;
    char const* why = "none";
};

// How the bot gets out of control now: the cheapest break that removes the worst effect, the medallion by role.
// `needsToMove`: a root or a snare alone matters (a kited ranged bot, a chasing melee bot).
BreakVerdict Break(PlayerbotAI* botAI, Player* bot, bool needsToMove);

// An equipped PvP medallion that is ready, or nullptr.
Item* ReadyTrinket(Player* bot);

// The racial triggers (Every Man for Himself, Will of the Forsaken, Escape Artist) wait here: in a fight these rules govern the
// break is the engine's or the strategy's own action, so the racial trigger stands down. True outside such a fight.
bool RacialTriggerMayFire(PlayerbotAI* botAI, Player* bot);
}  // namespace PvpDefenseGlue

#endif
