/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 *
 * The guild life of the random bots. Random bots found guilds (up to AiPlayerbot.RandomBotGuildCount, the founder pays
 * the charter price), invite other random bots of their faction and level band (the invitee accepts or declines a little
 * later, like a player), promote an active member now and then, demote an inactive officer, kick a member that has been
 * offline for weeks, welcome and congratulate in guild chat (BotVoice), and a member leaves its guild once in a long while.
 * The guild's own rank rights apply: every action goes through the guild handlers with the acting bot's session, as a
 * player's click would.
 *
 * Players are never touched: a bot never kicks, demotes or promotes a human (or an alt bot, which a player owns), and it
 * invites a human only like a player would, with a whisper and the ordinary guild invite the human can decline, at a low
 * rate, near the bot, and only when AiPlayerbot.BotGuilds.InviteHumans is on (off by default).
 *
 * Everything runs on the world thread in one pass on a timer (AiPlayerbot.BotGuilds.IntervalSeconds, default 60), nothing
 * per bot tick. The guilds' member data comes from one query every RefreshMinutes. AzerothCore's guild strategy, actions and
 * triggers (a bot per tick) are replaced by this pass; its guild task system (rewards handed out) is not ported (design decision 0016).
 *
 * Settings are AiPlayerbot.BotGuilds.* (conf/playerbots.conf.dist) and the existing AiPlayerbot.RandomBotGuildCount.
 */

#ifndef _PLAYERBOT_BOTGUILDS_H
#define _PLAYERBOT_BOTGUILDS_H

#include "Define.h"

namespace BotGuilds
{
    // World thread, from RandomPlayerbotMgr::UpdateAIInternal; throttles itself.
    void Update();

    // This bot was invited into a guild by the guild pass, which accepts or declines for it a little later (any thread).
    // GuildAcceptAction leaves such an invite alone, so the guild is only changed on the world thread.
    bool OwnsInvite(uint32 botLowGuid);
}

#endif
