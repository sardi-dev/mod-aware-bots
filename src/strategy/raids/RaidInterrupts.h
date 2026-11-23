/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 *
 * Raid strategies that must drop a cast the moment a boss ability is aimed at the bot (Hyjal: Anetheron's Inferno, Air
 * Burst, Doomfire). AzerothCore's mod-playerbots does it from spell-prepare and creature-update script hooks (its
 * RequestSpellInterrupt); the core of this fork has no such hooks, so PlayerbotAI asks here while a bot is casting.
 */

#ifndef PLAYERBOTS_RAIDINTERRUPTS_H
#define PLAYERBOTS_RAIDINTERRUPTS_H

class PlayerbotAI;

namespace RaidInterrupts
{
    // Whether the bot's cast in progress should be dropped now. Cheap for every bot outside a raid with such a strategy
    // (one map id compare).
    bool ShouldInterruptCast(PlayerbotAI* botAI);
}

#endif
