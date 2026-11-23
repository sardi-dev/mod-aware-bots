/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_PVPMISTWEAVER_H
#define _PLAYERBOT_PVPMISTWEAVER_H

// BT-657 to BT-662: the game side of the Mistweaver package (Utils/PvpMistweaverPolicy.h): reads what a player sees (the health of
// the group, the players hitting an ally, the monk's own stacks and silence, the enemy burst auras, the bot's own kick book), asks
// the policy and answers the triggers and actions of the Mistweaver strategy. Map thread only (bot AI).

class PlayerbotAI;
class Player;

namespace PvpMistweaverGlue
{
enum class Rule
{
    SPHERE,     // Healing Sphere under the hurt ally
    COCOON,     // Life Cocoon on the ally the enemy team works on
    TEA,        // Thunder Focus Tea
    SURGE,      // the instant Surging Mist that the tea gave
    MANA_TEA,   // Mana Tea in a lockout or a quiet moment
    LUST        // Tiger's Lust on an impaired ally or self
};

bool Due(PlayerbotAI* botAI, Player* bot, Rule rule);
bool Cast(PlayerbotAI* botAI, Player* bot, Rule rule);
}  // namespace PvpMistweaverGlue

#endif
