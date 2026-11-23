/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_PVPAFFLICTION_H
#define _PLAYERBOT_PVPAFFLICTION_H

// BT-663 to BT-668: the game side of the Affliction Warlock package (Utils/PvpAfflictionPolicy.h): reads what a player sees (the
// enemy players around the warlock, the lockout on its shadow spells, the Dark Regeneration aura, the bag), asks the policy and
// answers the triggers and actions of the Affliction strategy. Map thread only (bot AI). Everything answers only for a fight
// against players; PvE is untouched.

class PlayerbotAI;
class Player;
class Unit;

namespace PvpAfflictionGlue
{
enum class Rule
{
    HOWL,         // Howl of Terror on a melee player in reach
    FEL_FLAME,    // Fel Flame while the shadow school is locked out
    STONE         // the Healthstone beside a running Dark Regeneration
};

bool Due(PlayerbotAI* botAI, Player* bot, Rule rule);
bool HowlCast(PlayerbotAI* botAI, Player* bot);

// Haunt on this target waits for the window, a hurt target or a held one: true when the cast must be held.
bool HauntHeld(PlayerbotAI* botAI, Player* bot, Unit* target);
}  // namespace PvpAfflictionGlue

#endif
