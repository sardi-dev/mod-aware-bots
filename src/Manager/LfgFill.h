/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 *
 * Dungeon Finder fill. A real player's finder queue needs four more players with the roles the group lacks. Left to
 * the old self-queue (LfgJoinAction, a random trigger per bot, role by spec) the pool fills with damage dealers while a
 * tank or a healer never comes, and a bot stays queued after the player left the queue. This pass runs on the world
 * thread every few seconds: for each real queued player or group it counts the roles already queued for the same
 * dungeons, picks free random bots of the same faction within the dungeon's level range and near the player's level that
 * have a role still missing, and queues them for exactly the dungeons that fit them. When a healer is missing and no
 * healer-spec bot fits, a hybrid bot (druid, paladin, priest, shaman, monk) inside the same level window switches to
 * its healing specialization and queues as the healer; it keeps that spec. Bots it queued leave the queue
 * when nobody wants their dungeons any more or after a timeout.
 *
 * It also starts a bot-only run now and then (AiPlayerbot.LfgBotRuns.*): five free random bots in a dungeon's band
 * queue for it together, so instances are used without a player. Capped, logged as [BOTPARTY], and wrapped up after a
 * time limit (the bots leave their group and the core teleports them out).
 *
 * All settings are AiPlayerbot.LfgFill.* and AiPlayerbot.LfgBotRuns.* (conf/playerbots.conf.dist), read every pass.
 */

#ifndef _PLAYERBOT_LFGFILL_H
#define _PLAYERBOT_LFGFILL_H

#include "Define.h"

#include <vector>

namespace LfgFill
{
    // Select and reserve a role against the latest world-thread census; -1 if unavailable.
    int PickSpecRole(uint32 guid, uint32 team, uint32 level, bool const available[3]);

    // AiPlayerbot.LfgFill.Enable, read by LfgJoinAction::isUseful (any thread).
    bool Enabled();

    // World thread, from RandomPlayerbotMgr::UpdateAIInternal; throttles itself.
    void Update();

    // Bots of a bot-only run (they are not an orphaned group when no player is in it).
    bool IsBotRun(uint32 botLowGuid);

    // AiPlayerbot.LfgBotRuns.MaxRunMinutes in seconds (any thread): the time limit a bot-led run gets, finder run or party.
    uint32 MaxRunSeconds();

    // The same for the dungeon on this map: the Black Morass and Old Hillsbrad get AiPlayerbot.LfgBotRuns.LongRunMinutes.
    uint32 MaxRunSecondsFor(uint32 mapId);

    // The run hit its time limit: these bots leave their group (LfgMannersAction).
    bool WrapUpRequested(uint32 botLowGuid);

    // A bot-led run in this dungeon cannot reach any boss left (LfgCrawlAction): the dungeon is not picked for a bot-only
    // run (finder or hand-formed party) for AiPlayerbot.LfgBotRuns.FailureCooldownHours, and the bots of a finder run
    // are asked to wrap up like at the time limit (any thread).
    void NoteRunFailed(uint32 dungeonId, std::vector<uint32> const& finderBotLowGuids);

    // The dungeon failed a bot-led run lately (any thread).
    bool DungeonCoolingDown(uint32 dungeonId);
}

#endif
