/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_PVPHOLYPALADIN_H
#define _PLAYERBOT_PVPHOLYPALADIN_H

// BT-683 to BT-687: the game side of the Holy Paladin package (Utils/PvpHolyPaladinPolicy.h): reads what a player sees (the group's
// health, the players hitting an ally, the control casts on their way to an ally, the burst auras, the paladin's own silence and
// kick book), asks the policy and answers the triggers and actions of the Holy strategy. Map thread only (bot AI). Everything
// answers only in a fight against players; PvE is untouched.

class PlayerbotAI;
class Player;

namespace PvpHolyPaladinGlue
{
enum class Rule
{
    DENOUNCE,     // Denounce in the downtime
    SACRIFICE,    // Hand of Sacrifice before a control or in an emergency
    PROTECTION,   // Hand of Protection on a stunned or burst ally
    AURA,         // Devotion Aura in a silence
    BEACON        // Beacon of Light on the focused ally
};

bool Due(PlayerbotAI* botAI, Player* bot, Rule rule);
bool Cast(PlayerbotAI* botAI, Player* bot, Rule rule);
}  // namespace PvpHolyPaladinGlue

#endif
