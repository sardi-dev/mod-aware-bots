/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_TANKSTANDOFF_H
#define _PLAYERBOT_TANKSTANDOFF_H

#include <string>

class PlayerbotAI;

// BT-194 (TankStandoffPolicy.h): the [TANK] line for a tank whose target loses no health.
namespace TankStandoff
{
    // Reads the target of a tank in combat (at most once a second). True when a [TANK] line is due now.
    bool Observe(PlayerbotAI* botAI);
    // Writes the line; `decision` is what the engine did on this tick.
    void Report(PlayerbotAI* botAI, std::string const& decision);
}

#endif
