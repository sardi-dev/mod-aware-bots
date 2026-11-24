/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 *
 * Stale bot groups. A group of random bots with no player in it, that is no finder, battleground or raid group and no party
 * the bots are gathering (BotLfm), has nobody to lead it once its leader is gone: a leader may teleport to a random spot, a
 * member may not, so the members stay behind and walk after a leader thousands of yards away for hours. Every member whose
 * leader has been offline, on another map or farther than StaleGroups.ApartYards for StaleGroups.ApartMinutes leaves the
 * group (the group ends when one member is left).
 *
 * World thread, from RandomPlayerbotMgr::UpdateAIInternal; throttled to one pass every 30 seconds.
 * Config: AiPlayerbot.StaleGroups.* (see conf/playerbots.conf.dist). Log lines: [STALEGROUP].
 */

#ifndef _PLAYERBOT_BOTGROUPSWEEP_H
#define _PLAYERBOT_BOTGROUPSWEEP_H

namespace BotGroupSweep
{
    void Update();
}

#endif
