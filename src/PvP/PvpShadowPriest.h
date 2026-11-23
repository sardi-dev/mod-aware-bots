/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_PVPSHADOWPRIEST_H
#define _PLAYERBOT_PVPSHADOWPRIEST_H

// BT-688 to BT-690: the game side of the Shadow Priest package (Utils/PvpShadowPriestPolicy.h): reads what a player sees (the group's
// health, who is stunned or rooted, the players hitting an ally, the immunity auras of the target, the priest's own silence and
// glyph), asks the policy and answers the triggers, the action and the multiplier of the Shadow strategy. Map thread only (bot AI).
// Everything answers only in a fight against players; PvE is untouched.

class Action;
class PlayerbotAI;
class Player;

namespace PvpShadowPriestGlue
{
enum class Rule
{
    LEAP,       // Leap of Faith on a stunned or trapped ally
    DISPEL      // Mass Dispel on a magic immunity of the current target
};

bool Due(PlayerbotAI* botAI, Player* bot, Rule rule);
bool Cast(PlayerbotAI* botAI, Player* bot, Rule rule);

// The multiplier: Vampiric Embrace waits for the burst or a hurt team against players.
bool Holds(PlayerbotAI* botAI, Player* bot, Action* action);
}  // namespace PvpShadowPriestGlue

#endif
