/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * Role sheet tank.md gap 8, the part BT-292 left open: the pack-start cooldowns (Demoralizing Shout, Avenging Wrath,
 * Dancing Rune Weapon, Berserk) fire on the opening of a pull of three or more mobs, before the damage builds up, where
 * the danger reading (TankDangerPolicy) only reacts once health is already falling. Free of game types for tests/.
 */

#ifndef _PLAYERBOT_TANKPACKSTARTPOLICY_H
#define _PLAYERBOT_TANKPACKSTARTPOLICY_H

#include <cstdint>

namespace TankPackStartPolicy
{
// Three mobs on the tank make a pack (the same count as TankDangerPolicy::MANY_ATTACKERS).
constexpr unsigned PACK_ATTACKERS = 3;
// The opening of a pull: the time a tank needs to gather a pack and land its first area threat.
constexpr std::uint32_t OPENING_MS = 8000;

constexpr bool PackStart(unsigned attackersOnTank, std::uint32_t msInCombat)
{
    return attackersOnTank >= PACK_ATTACKERS && msInCombat <= OPENING_MS;
}
}  // namespace TankPackStartPolicy

#endif
