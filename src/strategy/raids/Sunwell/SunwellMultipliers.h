/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef PLAYERBOTS_SUNWELLMULTIPLIERS_H
#define PLAYERBOTS_SUNWELLMULTIPLIERS_H

#include "Multiplier.h"

// A melee damage dealer whose target stands in a hazard (M'uru inside his own Darkness, a Shield Orb in Armageddon)
// waits outside instead of walking back in after every step out.
class SunwellHoldOutsideHazardMultiplier : public Multiplier
{
public:
    SunwellHoldOutsideHazardMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "sunwell hold outside hazard") {}
    float GetValue(Action* action) override;
};

// Felmyst in the air: nobody walks or turns towards her (melee would stand under her, in the lane she breathes down).
class SunwellFelmystAirMultiplier : public Multiplier
{
public:
    SunwellFelmystAirMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "sunwell felmyst air") {}
    float GetValue(Action* action) override;
};

// Brutallus: a soaker does not walk behind him (its class "set behind" habit) while its team soaks Meteor Slash.
class SunwellMeteorSoakMultiplier : public Multiplier
{
public:
    SunwellMeteorSoakMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "sunwell meteor soak") {}
    float GetValue(Action* action) override;
};

#endif
