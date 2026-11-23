/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_PVPUNITFACTS_H
#define _PLAYERBOT_PVPUNITFACTS_H

// BT-620: what a player sees on an enemy unit, read once and shared by the burst window (PvpBurst.cpp) and the team play
// (PvpTeam.cpp): is it immune, held by a crowd control that a hit breaks, stunned or rooted. Public state only (auras and unit
// states), the same things a player reads off a unit frame.

#include "Player.h"
#include "SpellAuraDefines.h"
#include "Unit.h"

namespace PvpUnitFacts
{
// Auras that make an enemy untouchable although the school immunity type does not show it: Deterrence deflects, Cloak of
// Shadows resists spells.
// BT-676: Touch of Karma (122470) joins them: a hit on the monk is absorbed and thrown back at the attacker, so the window closes
// and the team looks for another target (the course: swap off the monk while Karma is up).
constexpr uint32 EXTRA_IMMUNE_AURAS[] = { 19263, 31224, 122470 };

// A crowd control that a hit breaks (polymorph, sap, fear, sleep, banish, disorient, horror).
constexpr uint32 BREAKS_ON_DAMAGE_MECHANICS = (1u << MECHANIC_POLYMORPH) | (1u << MECHANIC_SAPPED) | (1u << MECHANIC_DISORIENTED) |
    (1u << MECHANIC_FEAR) | (1u << MECHANIC_SLEEP) | (1u << MECHANIC_BANISH) | (1u << MECHANIC_HORROR);

inline bool IsImmune(Unit* unit)
{
    if (unit->HasAuraType(SPELL_AURA_SCHOOL_IMMUNITY) || unit->HasAuraType(SPELL_AURA_DAMAGE_IMMUNITY))
        return true;
    for (uint32 spell : EXTRA_IMMUNE_AURAS)
        if (unit->HasAura(spell))
            return true;
    return false;
}

inline bool HasBreakableCc(Unit* unit)
{
    return unit->HasAuraWithMechanic(BREAKS_ON_DAMAGE_MECHANICS);
}

// Stunned or rooted: the target stands still and a burst lands in full. A breakable crowd control does not count (the caller
// asks HasBreakableCc first: a sheep is a reason to hold the burst, not to spend it).
inline bool IsHeldStill(Unit* unit)
{
    return unit->HasUnitState(UNIT_STATE_STUNNED | UNIT_STATE_ROOT);
}
}  // namespace PvpUnitFacts

#endif
