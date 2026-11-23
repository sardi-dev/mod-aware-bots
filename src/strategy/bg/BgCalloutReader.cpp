/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE). See BgCalloutReader.h for what this is.
 */

#include "BgCalloutReader.h"

// DatabaseEnv.h must precede the battleground headers (see BattleGroundTactics.cpp).
#include "DatabaseEnv.h"

#include "BgCalloutPolicy.h"
#include "Config.h"
#include "Group.h"
#include "Player.h"
#include "Playerbots.h"
#include "PvpConfig.h"
#include "PvpMatchState.h"
#include "Timer.h"
#include "WorldSession.h"

namespace
{
// AiPlayerbot.BattlegroundChat.Enable / .Replies: the same two settings the plans' own call-outs read.
bool BattlegroundChatOn()
{
    static bool const enabled = sConfigMgr->GetBoolDefault("AiPlayerbot.BattlegroundChat.Enable", true) &&
                                sConfigMgr->GetBoolDefault("AiPlayerbot.BattlegroundChat.Replies", true);
    return enabled;
}
}  // namespace

namespace BgCalloutReader
{
void OnTeamChat(Group* group, Player* sender, uint32 chatType, std::string const& message)
{
    if (!BattlegroundChatOn() || !PvpConfig::CalloutReaderEnabled() || !group || !sender || !group->isBGGroup())
        return;

    // The battleground's own chat: instance chat, which the client shows on the battleground tab.
    if (chatType != CHAT_MSG_INSTANCE && chatType != CHAT_MSG_INSTANCE_LEADER)
        return;

    // A human only, in the match of the group; the bots' own lines come from their plans, not from this hook.
    if (!sender->GetSession() || sender->GetSession()->IsBot() || !sender->InBattleground())
        return;

    BgCallout::MapKind const map = BgCallout::MapKindOf(sender->GetMapId());
    BgCallout::Report const report = BgCallout::Parse(message, map);
    if (!report.valid)
        return;

    // No state yet (no bot has run a refresh of this match): the line is dropped, like the plans' readers always did.
    PvpMatch::Handle const match = PvpMatch::Find(sender->GetBattlegroundId());
    if (!match)
        return;

    PvpObs::Message post;
    post.generation = match->Key().generation;
    post.side = sender->GetBGTeam() == ALLIANCE ? uint8(TEAM_ALLIANCE) : uint8(TEAM_HORDE);
    post.obs.kind = report.kind;
    post.obs.provenance = PvpObs::Provenance::HUMAN_CALL;
    post.obs.place = report.place;
    post.obs.count = report.count;
    post.obs.senderLow = sender->GetGUIDLow();
    post.obs.receivedMs = PvpObs::Stamp(getMSTime());
    match->Post(post);
}
}  // namespace BgCalloutReader
