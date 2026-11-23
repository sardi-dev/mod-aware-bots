/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_PVPINTERRUPT_H
#define _PLAYERBOT_PVPINTERRUPT_H

// BT-521: the game side of rule R1 (Utils/PvpInterruptPolicy.h, docs/pvp/awareness-r1-r2.md). The class interrupt actions
// and triggers stay as they are; they ask this gate first when the caster is a player (or a player's pet or totem):
//   Wants()  is this a cast worth the bot's interrupt at all (crowd control on our side, a heal in our kill window, a large
//            damage cast on a hurt friend)? Independent of time, so a cached value may use it.
//   Allow()  is it time: late in the cast by tier, and after the interrupters of the group that rank before the bot?
// Everything else (monsters, a group in a dungeon) is not governed and answers yes, so the older paths are unchanged.

#include "PvpInterruptPolicy.h"

class PlayerbotAI;
class Player;
class Spell;
class Unit;

namespace PvpInterruptGlue
{
struct Evaluation
{
    bool governed = false;
    PvpInterrupt::Decision decision;
    std::uint32_t sinceFirstLookMs = 0;
    bool due = false;
};

// What the spell a unit is casting does (control, heal, damage, cast time): the policy's reading of a cast. Also used for the bot's
// own cast by the fake cast rule.
PvpInterrupt::CastShape ShapeOf(Spell* cast);

// The PvP rules govern a kick of this caster by this bot: the switch is on and the caster is a player or one of its pets
// or totems that the bot could attack.
bool Governs(Player* bot, Unit* caster);

// Reads the facts of the cast the caster is making now, notes the first look at it and asks the policy.
Evaluation Evaluate(Player* bot, Unit* caster);

// Not governed, or the policy wants this cast stopped (time is not asked).
bool Wants(Player* bot, Unit* caster);

// Not governed, or the policy wants this cast stopped and the moment has come.
bool Allow(Player* bot, Unit* caster);
}  // namespace PvpInterruptGlue

#endif
