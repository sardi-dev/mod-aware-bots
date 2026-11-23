/*
 * #897: rated battleground bot groups. See BgRatedBg.h. Pattern: BattleGroundJoinAction.cpp canLeadArenaParty /
 * gatherArenaParty / PlayerbotArena (AC BattleGroundJoinAction.cpp:64-194, :239-288; RandomPlayerbotMgr.cpp census).
 */

#include "BgRatedBg.h"
#include "BotScenario.h"

// Playerbots.h/Player.h first: Battleground.h -> BattlegroundQueue.h -> RatedPvp.h needs BotGuid and the DBC/Item types.
#include "Playerbots.h"
#include "Player.h"
#include "DatabaseEnv.h"  // must precede BattlegroundQueue.h (see BattleGroundJoinAction.cpp)

#include "Battleground.h"
#include "BattlegroundMgr.h"
#include "BattlegroundQueue.h"
#include "Config.h"
#include "DBCStores.h"
#include "Group.h"
#include "GroupMgr.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "RandomPlayerbotMgr.h"
#include "World.h"
#include "WorldSession.h"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <vector>

namespace BotScenario { bool BotScenarioHoldsArenaRoster(Player* bot); }  // BattleGroundJoinAction.cpp:50 (same guard)

namespace
{
struct RatedBgCensus
{
    std::atomic<uint32> botCount[2] = { 0, 0 };     // bots queued or inside, per team (0 = Alliance, 1 = Horde)
    std::atomic<uint32> playerCount[2] = { 0, 0 };  // real players queued or inside
    std::atomic<uint32> active{ 0 };                // a real player is waiting, or the seed asks for a match
    std::atomic<uint32> instanceCount{ 0 };         // rated BG instances with a bot inside
    std::atomic<uint32> minCount{ 0 };              // AiPlayerbot.RandomBotAutoJoinBGRatedBGCount
    std::vector<uint32> instances;                  // world thread only
    std::mutex bookLock;                            // quota check + booking is one step
};

RatedBgCensus& Census()
{
    static RatedBgCensus c;
    return c;
}

// BT-151: SkyFire declares no rated BG queue; NONE is an inert sentinel while support is disabled.
constexpr BattlegroundQueueTypeId RatedQueue = BATTLEGROUND_QUEUE_NONE;

uint32 TeamIdx(Player* p) { return p->GetTeamId() == TEAM_ALLIANCE ? 0 : 1; }

Battleground* RatedTemplate() { return sBattlegroundMgr->GetBattlegroundTemplate(BATTLEGROUND_RATED_10_VS_10); }
}  // namespace

// PORT: false until SkyFire can queue rated battlegrounds (see the queueing step below); otherwise bots would keep forming full
// groups that can never enter the queue.
constexpr bool RatedBgQueueSupported = false;

bool PlayerbotRatedBg::Enabled() { return RatedBgQueueSupported && Census().minCount.load() > 0; }

uint32 PlayerbotRatedBg::TeamSize()
{
    // PORT: SkyFire has no rated BG test mode (LoA's BattlegroundMgr::IsRatedBgTesting), so always the real size.
    Battleground* bg = RatedTemplate();
    return bg && bg->GetMaxPlayersPerTeam() ? bg->GetMaxPlayersPerTeam() : 10;
}

void PlayerbotRatedBg::ResetCensus()
{
    RatedBgCensus& c = Census();
    for (int i = 0; i < 2; ++i)
    {
        c.botCount[i] = 0;
        c.playerCount[i] = 0;
    }
    c.active = 0;
    c.instanceCount = 0;
    c.instances.clear();
}

void PlayerbotRatedBg::CountQueued(Player* player, bool isBot)
{
    RatedBgCensus& c = Census();
    uint32 const team = TeamIdx(player);
    Battleground* bg = player->GetBattleground();
    bool const inRatedMatch = player->InBattleground() && bg && bg->GetTypeID() == BATTLEGROUND_RATED_10_VS_10;

    if (isBot)
    {
        c.botCount[team]++;
        if (inRatedMatch)
        {
            uint32 const instanceId = bg->GetInstanceID();
            if (std::find(c.instances.begin(), c.instances.end(), instanceId) == c.instances.end())
                c.instances.push_back(instanceId);
            c.instanceCount = uint32(c.instances.size());
        }
        return;  // only a real player opens the queue for bots
    }

    c.playerCount[team]++;

    GroupQueueInfo ginfo;
    bool const inQueue =
        sBattlegroundMgr->GetBattlegroundQueue(RatedQueue).GetPlayerGroupInfoData(player->GetGUID(), &ginfo);
    bool const invited = inQueue && ginfo.IsInvitedToBGInstanceGUID != 0;
    if (!invited && !player->InBattleground())
        c.active = 1;
}

void PlayerbotRatedBg::SeedAutoJoin()
{
    RatedBgCensus& c = Census();
    int32 const minCount = std::max(0, sConfigMgr->GetIntDefault("AiPlayerbot.RandomBotAutoJoinBGRatedBGCount", 0));
    c.minCount = uint32(minCount);

    // AC updateRatedArenaInstanceCount pattern (PlayerbotArena::SeedAutoJoin)
    if (minCount > 0 && c.active == 0 && c.instanceCount < uint32(minCount) && c.instances.size() < size_t(minCount))
        c.active = 1;
}

void PlayerbotRatedBg::LogCensus()
{
    RatedBgCensus& c = Census();
    if (!c.minCount && !c.active && !c.instanceCount && !c.botCount[0] && !c.botCount[1] && !c.playerCount[0] &&
        !c.playerCount[1])
        return;

    SF_LOG_DEBUG("playerbots",
                 "[RBG] census: seed=%u active=%u instances=%u team=%u Alliance bots=%u players=%u Horde bots=%u players=%u",
                 c.minCount.load(), c.active.load(), c.instanceCount.load(), TeamSize(), c.botCount[0].load(),
                 c.playerCount[0].load(), c.botCount[1].load(), c.playerCount[1].load());
}

bool PlayerbotRatedBg::CanLead(Player* bot)
{
    if (!Enabled() || !sRandomPlayerbotMgr->IsRandomBot(bot))
        return false;

    // level-max leaders only (the rated BG is a max-level queue)
    if (bot->getLevel() < sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL))
        return false;

    Battleground* tmpl = RatedTemplate();
    if (!tmpl || !GetBattlegroundBracketByLevel(tmpl->GetMapId(), bot->getLevel()))
        return false;

    // what Group::CanJoinBattlegroundQueue refuses for the leader (Group.cpp:2553-2564)
    if (!bot->CanJoinToBattleground(tmpl) || bot->isUsingLfg() || !bot->HasFreeBattlegroundQueueId())
        return false;

    RatedBgCensus& c = Census();
    uint32 const team = TeamIdx(bot);
    uint32 const size = TeamSize();
    std::lock_guard<std::mutex> guard(c.bookLock);
    if (c.botCount[team] + c.playerCount[team] >= size * (c.active + c.instanceCount))
        return false;

    c.botCount[team] += size;  // the whole group is booked at once, like AC :258-265
    SF_LOG_DEBUG("playerbots", "[RBG] Bot %u <%s>: leads a rated BG group of %u (team %u bots now %u)",
                 bot->GetGUIDLow(), bot->GetName().c_str(), size, team, c.botCount[team].load());
    return true;
}

bool PlayerbotRatedBg::GatherAndQueue(Player* bot)
{
    uint32 const teamSize = TeamSize();
    Battleground* tmpl = RatedTemplate();
    if (!tmpl)
        return false;

    PvPDifficultyEntry const* leaderBracket = GetBattlegroundBracketByLevel(tmpl->GetMapId(), bot->getLevel());
    if (!leaderBracket)
        return false;

    uint32 const maxLevel = sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL);

    // gatherArenaParty's member walk (AC :70-120), plus level == max, same faction and the core's per-member refusals
    GuidVector members;
    for (auto const& itr : sRandomPlayerbotMgr->GetAllBots())
    {
        if (members.size() + 1 >= teamSize)
            break;

        Player* member = itr.second;
        if (!member || member == bot || !member->IsInWorld())
            continue;

        PlayerbotAI* memberBotAI = GET_PLAYERBOT_AI(member);
        if (!memberBotAI)
            continue;

        if (member->GetGroup() && memberBotAI->HasActivePlayerMaster())
            continue;

        if (!sPlayerbotAIConfig->IsInRandomAccountList(member->GetSession()->GetAccountId()))
            continue;

        if (member->IsInCombat() || !member->IsAlive() || member->InBattleground() || member->InBattlegroundQueue())
            continue;

        if (member->getLevel() < maxLevel || member->GetTeam() != bot->GetTeam())
            continue;

        if (GetBattlegroundBracketByLevel(tmpl->GetMapId(), member->getLevel()) != leaderBracket)
            continue;

        if (!member->CanJoinToBattleground(tmpl) || member->isUsingLfg() || !member->HasFreeBattlegroundQueueId())
            continue;

        if (BotScenario::BgJoinBlocked(member))
            continue;

        members.push_back(member->GetGUID());
    }

    if (members.size() + 1 < teamSize)
    {
        SF_LOG_DEBUG("playerbots", "[RBG] Bot %u <%s>: not enough free bots for a rated BG group (%u of %u)",
                     bot->GetGUIDLow(), bot->GetName().c_str(), uint32(members.size() + 1), teamSize);
        return false;
    }

    for (BotGuid const& guid : members)
    {
        Player* member = ObjectAccessor::FindPlayer(guid);
        if (!member)
            continue;

        if (member->GetGroup())
            member->GetGroup()->RemoveMember(member->GetGUID());

        if (PlayerbotAI* memberBotAI = GET_PLAYERBOT_AI(member))
            memberBotAI->Reset();
    }

    if (bot->GetGroup())
        bot->GetGroup()->Disband(true);

    Group* group = new Group();
    if (!group->Create(bot))
    {
        delete group;
        return false;
    }
    sGroupMgr->AddGroup(group);
    if (teamSize > MAXGROUPSIZE)
        group->ConvertToRaid();

    for (BotGuid const& guid : members)
    {
        if (group->GetMembersCount() >= teamSize)
            break;

        Player* member = ObjectAccessor::FindPlayer(guid);
        if (!member || !group->AddMember(member))
            continue;

        if (PlayerbotAI* memberBotAI = GET_PLAYERBOT_AI(member))
            memberBotAI->Reset();
        member->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_TELEPORTED);
        member->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_CHANGE_MAP);
        member->TeleportTo(bot->GetMapId(), bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(), 0);
    }

    if (group->GetMembersCount() < teamSize)
    {
        SF_LOG_DEBUG("playerbots", "[RBG] Bot %u <%s>: rated BG group not full (%u of %u), disbanded",
                     bot->GetGUIDLow(), bot->GetName().c_str(), group->GetMembersCount(), teamSize);
        group->Disband();
        return false;
    }

    // PORT: SkyFire cannot queue rated battlegrounds yet (no CMSG_BATTLEMASTER_JOIN_RATED handler), so the group
    // never enters the queue and the check below reports it.

    bool const queued = bot->InBattlegroundQueueForBattlegroundQueueType(RatedQueue);
    SF_LOG_DEBUG("playerbots", "[RBG] Bot %u %s:%u <%s> queued Rated BG (party %u) ok=%u", bot->GetGUIDLow(),
                 bot->GetTeamId() == TEAM_ALLIANCE ? "A" : "H", bot->getLevel(), bot->GetName().c_str(),
                 group->GetMembersCount(), queued ? 1 : 0);
    if (!queued)
        group->Disband();
    return queued;
}

bool PlayerbotRatedBg::PlayerWaiting()
{
    RatedBgCensus& c = Census();
    return c.playerCount[0] + c.playerCount[1] > 0;
}

bool PlayerbotRatedBg::LeaveQueueIfWaitedTooLong(Player* bot, GroupQueueInfo const& ginfo)
{
    constexpr uint32 MaxWaitMs = 300000;
    if (getMSTimeDiff(ginfo.JoinTime, getMSTime()) <= MaxWaitMs || PlayerWaiting() ||
        BotScenario::BotScenarioHoldsArenaRoster(bot))
        return false;

    SF_LOG_DEBUG("playerbots", "[RBG] Bot %u <%s> waited too long and leaves the rated BG queue",
                 bot->GetGUIDLow(), bot->GetName().c_str());
    bot->RemoveBattlegroundQueueId(RatedQueue);
    sBattlegroundMgr->GetBattlegroundQueue(RatedQueue).RemovePlayer(bot->GetGUID(), true);
    LeaveGroupAfterMatch(bot);
    return true;
}

void PlayerbotRatedBg::LeaveGroupAfterMatch(Player* bot)
{
    Group* group = bot->GetGroup();
    if (!group || bot->InBattleground())
        return;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        PlayerbotAI* memberBotAI = member ? GET_PLAYERBOT_AI(member) : nullptr;
        if (!memberBotAI || !sRandomPlayerbotMgr->IsRandomBot(member) || memberBotAI->HasActivePlayerMaster())
            return;  // a real player's group stays
    }

    SF_LOG_DEBUG("playerbots", "[RBG] Bot %u <%s>: leaves the rated BG group after the match",
                 bot->GetGUIDLow(), bot->GetName().c_str());
    group->RemoveMember(bot->GetGUID());
}
