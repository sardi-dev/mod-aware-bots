/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef PLAYERBOTS_TOCMULTIPLIERS_H
#define PLAYERBOTS_TOCMULTIPLIERS_H

#include "Multiplier.h"

// Keeps the generic target actions from undoing the strategy's choices: damage dealers do not fall back to the tank's
// target, the main tank neither assists nor taunts onto what the assist tank holds, and the assist tank does not
// taunt the main tank's unit.
class ToCMultiplier : public Multiplier
{
public:
    ToCMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "toc") {}

    float GetValue(Action* action) override;
};

#endif
