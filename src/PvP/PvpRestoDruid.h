/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_PVPRESTODRUID_H
#define _PLAYERBOT_PVPRESTODRUID_H

// BT-668 to BT-672: the game side of the Restoration Druid package (Utils/PvpRestoDruidPolicy.h): reads what a player sees (the group's
// health, the players hitting an ally, the burst auras of the enemy, the druid's own cooldowns and silence, the bot's own book of
// diminishing returns and kicks), asks the policy and answers the triggers and actions of the Restoration strategy. Map thread
// only (bot AI). Everything answers only in a fight against players; PvE is untouched.

class PlayerbotAI;
class Player;

namespace PvpRestoDruidGlue
{
enum class Rule
{
    WARD,        // Cenarion Ward on the ally the enemy team works on, or in a silence
    IRONBARK,    // Ironbark early on that ally when an enemy burst is up
    SWIFTNESS,   // Nature's Swiftness before the instant Cyclone
    CYCLONE,     // Cyclone on a bursting enemy damage dealer
    DISPLACER,   // Displacer Beast away from a melee player that has connected
    TYPHOON      // Typhoon as the peel
};

bool Due(PlayerbotAI* botAI, Player* bot, Rule rule);
bool Cast(PlayerbotAI* botAI, Player* bot, Rule rule);
}  // namespace PvpRestoDruidGlue

#endif
