/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_WARLOCKDOTREFRESH_H
#define _PLAYERBOT_WARLOCKDOTREFRESH_H

#include "BotSkill.h"
#include "DpsRotationMath.h"
#include "Player.h"

// The refresh window of a warlock's own damage over time spell, in milliseconds: 0 (recast only when the aura is gone)
// for every tier but expert. Read once when the trigger or action is built; a bot's tier never changes.
inline uint32 WarlockDotRefreshMs(Player* bot)
{
    return DpsRotationMath::Warlock::DotRefreshMs(bot && BotSkill::Uses(bot, BotSkillMath::Tier::EXPERT));
}

#endif
