/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 *
 * Bots that form a dungeon group by hand, like players. Now and then a free random bot picks a dungeon in its level
 * band and becomes a leader: it posts short "LFM <dungeon>, need <role>" lines in the LookingForGroup channel (and in
 * General while it stands in a capital) a few times over several minutes, free bots of its faction and level band
 * answer with a whisper and are invited, and a real player who whispers "inv" (or says it in the channel after a post)
 * is invited when the faction and the level band fit; the player can decline the invite. The full group (or one of at
 * least BotLfm.MinMembers after the recruiting time) meets at the dungeon's entrance: the leader walks there
 * (LfmLeadAction, the route code of TravelToGroupAction), waits for stragglers with a timeout and walks in through the
 * entrance trigger. Its bot members follow it through TravelToGroupAction. Inside, the leader and its group are left to
 * whatever leads a dungeon (BotLfm::IsLeader stays true while the party is inside).
 *
 * The bookkeeping runs on the world thread (Update, from RandomPlayerbotMgr::UpdateAIInternal, throttled to one pass
 * every 5 seconds); the leader's own movement runs on its AI tick. Every line goes through BotVoice.
 *
 * Config: AiPlayerbot.BotLfm.* (see conf/playerbots.conf.dist). Log lines: [BOTLFM].
 */

#ifndef _PLAYERBOT_BOTLFM_H
#define _PLAYERBOT_BOTLFM_H

#include "Define.h"

#include <string>

class Player;

namespace BotLfm
{
    enum class Phase : uint8
    {
        RECRUIT,  // posting and inviting
        TRAVEL,   // walking to the entrance
        WAIT,     // at the entrance, waiting for stragglers
        INSIDE    // walked in; left to the dungeon logic
    };

    struct PartyInfo
    {
        uint32 dungeonId = 0;
        uint32 mapId = 0;
        Phase phase = Phase::RECRUIT;
        uint32 phaseSinceMs = 0;
        uint32 waitPosts = 0;
        uint32 lastWaitPostMs = 0;
    };

    // AiPlayerbot.BotLfm.Enable
    bool Enabled();

    // AiPlayerbot.BotLfm.StragglerSeconds: how long the leader waits at the entrance for its members.
    uint32 StragglerSeconds();

    // World thread, from RandomPlayerbotMgr::UpdateAIInternal; throttles itself.
    void Update();

    // This bot leads a party right now (any thread, one atomic read while no party exists).
    bool IsLeader(uint32 botLowGuid);

    // This bot was invited into, or is in, a party led by a bot: it follows that bot even when a player joins.
    bool IsMember(uint32 botLowGuid);

    // This bot leads, or is a member of, a party that has walked into its dungeon (any thread; one atomic read while no
    // party exists). The dungeon logic (LfgCrawlAction, LfgMannersAction) acts for such a bot when no player is in its group.
    bool InsideParty(Player* bot);

    // The leader's own view of its party, and the changes its AI tick reports back.
    bool GetParty(uint32 leaderLowGuid, PartyInfo& info);
    void SetPhase(uint32 leaderLowGuid, Phase phase);
    void NotePostedWait(uint32 leaderLowGuid);

    // A whisper to a bot, from a player. True when it was a request to join a party the bot leads (then it is handled).
    bool OnWhisper(Player* bot, Player* from, std::string const& message);

    // A message in a chat channel. A short "inv" after a leader's post is a request to join that party.
    void OnChannelChat(Player* from, std::string const& message);
}

#endif
