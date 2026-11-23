/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_PVPFROSTMAGE_H
#define _PLAYERBOT_PVPFROSTMAGE_H

// BT-650 to BT-655: the game side of the Frost Mage package (Utils/PvpFrostMagePolicy.h): reads what a player sees (the bar of the
// mage, the pet, the target's auras, the burst window, the healer near the target), asks the policy and answers the triggers and
// actions of the Frost strategy. Map thread only (bot AI). Nothing here is cast against a target that is not an enemy player.

class PlayerbotAI;
class Player;
class Unit;

namespace PvpFrostMageGlue
{
// Alter Time is due now: the burst window of the current target is open and the bar is loaded.
bool AlterTimeDue(PlayerbotAI* botAI, Player* bot);

// Frozen Orb on this target waits for the window (or the sheep on it): true when the cast must be held.
bool OrbHeld(PlayerbotAI* botAI, Player* bot, Unit* target);

// Deep Freeze on this target is kept for the enemy healer: true when the cast must be held.
bool DeepFreezeKept(PlayerbotAI* botAI, Player* bot, Unit* target);

// The Water Elemental's Freeze goes on the current target now (before Deep Freeze, or to bank a Fingers of Frost charge).
bool PetFreezeDue(PlayerbotAI* botAI, Player* bot);
bool PetFreezeCast(PlayerbotAI* botAI, Player* bot);

// Incanter's Ward goes up now (before the burst, or when a player hits the mage).
bool WardDue(PlayerbotAI* botAI, Player* bot);

// The target is rooted by the mage's own elemental or a Frost Nova: Deep Freeze has its frozen target.
bool TargetFrozenByPet(Unit* target);
}  // namespace PvpFrostMageGlue

#endif
