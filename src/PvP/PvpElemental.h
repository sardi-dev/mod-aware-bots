/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_PVPELEMENTAL_H
#define _PLAYERBOT_PVPELEMENTAL_H

// BT-677 to BT-681: the game side of the Elemental Shaman package (Utils/PvpElementalPolicy.h): reads what a player sees (the casts
// of the enemy players and who they are aimed at, the cooldowns of the shaman, the burst window, the bot's own kick book), asks
// the policy and answers the triggers, the actions and the multiplier of the caster strategy. Map thread only (bot AI). Everything
// answers only against enemy players; PvE is untouched.

class Action;
class PlayerbotAI;
class Player;

namespace PvpElementalGlue
{
enum class Rule
{
    SWIFTNESS,   // Ancestral Swiftness before the Hex on the healer
    GROUNDING,   // Grounding Totem on an incoming control cast
    CALL,        // Call of the Elements to bring the Grounding back
    GRACE        // Spiritwalker's Grace while running
};

bool Due(PlayerbotAI* botAI, Player* bot, Rule rule);
bool Cast(PlayerbotAI* botAI, Player* bot, Rule rule);

// The multiplier: the burst (Ascendance, Fire Elemental, Elemental Mastery) waits for the window and Ancestral Swiftness for the Hex.
bool Holds(PlayerbotAI* botAI, Player* bot, Action* action);

// Drops the bot's cast tracks (logout).
void Forget(unsigned int guidLow);
}  // namespace PvpElementalGlue

#endif
