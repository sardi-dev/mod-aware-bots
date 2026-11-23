/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_PVPSUBTLETY_H
#define _PLAYERBOT_PVPSUBTLETY_H

// BT-691 to BT-693: the game side of the Subtlety Rogue package (Utils/PvpSubtletyPolicy.h): reads what a player sees (the stealth of
// the rogue, which enemies are fighting, the healer and caster specs of the enemy, the burst auras, the team go and the window, the
// bot's own book of diminishing returns), asks the policy and answers the triggers, the action and the multiplier of the Subtlety
// strategy. Map thread only (bot AI). Everything answers only against enemy players; PvE is untouched.

class Action;
class PlayerbotAI;
class Player;

namespace PvpSubtletyGlue
{
enum class Rule
{
    SAP,        // Sap on a second enemy in the opener
    TRICKS      // Tricks of the Trade on a damage partner at the go
};

bool Due(PlayerbotAI* botAI, Player* bot, Rule rule);
bool Cast(PlayerbotAI* botAI, Player* bot, Rule rule);

// The multiplier: Shadow Dance, Shadow Blades and Marked for Death wait for the burst window against a player.
bool Holds(PlayerbotAI* botAI, Player* bot, Action* action);
}  // namespace PvpSubtletyGlue

#endif
