/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

// BT-154: the arena ladder of the random bots (design decision 0027). See ArenaLadder.h for the threading rules.
//
// The flow of one match:
//   1. The scheduler picks two available teams (ArenaMath::PickPair) in a pass that the owner's clock, the world load and
//      a roll allow, builds a party of each team's two bots and queues both for the rated 2v2 arena (the core pairs the
//      only two rated groups of the bracket, since every rating it knows is 1).
//   2. The bots accept the invitation with the module's usual battleground status action and enter the arena.
//   3. The poll (every world tick while a match is live) sees the arena, remembers which side each team fights on, cuts a
//      bot-only match as a draw after 12 minutes, and books the result when the core has set the winner. A member that
//      leaves before the end voids the match.
//   4. The booking changes both ratings (ArenaMath), streaks and records, writes them through to the database on the
//      asynchronous queue, and lets the winner (or the team that broke a streak) tell the General channel.

#include "DatabaseEnv.h"

#include "ArenaChat.h"
#include "ArenaComp.h"
#include "ArenaLadder.h"
#include "ArenaNames.h"
#include "ArenaState.h"
#include "ArenaText.h"
#include "ArenaTeam.h"
#include "Battleground.h"
#include "BattleGroundJoinAction.h"
#include "BattlegroundMgr.h"
#include "BattlegroundQueue.h"
#include "BgRatedBg.h"
#include "BotScenario.h"
#include "Config.h"
#include "DBCStores.h"
#include "Group.h"
#include "GroupMgr.h"
#include "LFGMgr.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotSpec.h"
#include "Playerbots.h"
#include "Random.h"
#include "RandomPlayerbotMgr.h"
#include "Timer.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <optional>
#include <set>
#include <sstream>

namespace ArenaLadder
{
namespace
{
std::atomic<bool> gEnabled{ false };

// The core's value for "the arena ended without a winner" (Battleground::EndBattleground sets 3 for a draw; 2 means that
// the arena has not ended).
constexpr uint8 CORE_WINNER_DRAW = 3;

constexpr uint32 SCHEDULER_BASE_MS = 60000;
constexpr uint32 SCHEDULER_JITTER_MS = 10000;  // the pass runs every 60 s, plus or minus this
constexpr uint32 MS_PER_SECOND = 1000;
constexpr uint32 SECONDS_PER_DAY = 86400;
constexpr uint32 ENTRY_GRACE_SECONDS = 90;   // from the first bot in the arena until all four are inside
constexpr uint32 BACKOFF_AFTER_QUEUE_FAIL_SECONDS = 600;
constexpr uint32 ROLL_RANGE = 10000;
constexpr double UPDATE_SMOOTHING = 0.01;     // per world tick: an average over about five seconds
constexpr uint32 UPDATE_SAMPLE_CAP_MS = 1000;  // the first diff after the start-up is the start-up time, not a tick
constexpr int64 RIVAL_WINDOW_SECONDS = int64(ArenaChatMath::RIVAL_WINDOW_DAYS) * SECONDS_PER_DAY;

int64 UtcNow()
{
    return int64(time(nullptr));
}

BotGuid GuidOf(uint32 low)
{
    return BotGuid::Create<BotHighGuid::Player>(low);
}

Player* PlayerOf(uint32 low)
{
    Player* player = ObjectAccessor::FindPlayer(GuidOf(low));
    return player && player->IsInWorld() ? player : nullptr;
}

// A tier from a guid alone, the way BotSkill derives it (the same shares from the same settings).
BotSkillMath::Tier NaturalTier(uint32 guidLow)
{
    if (!sConfigMgr->GetBoolDefault("AiPlayerbot.SkillTiers.Enable", true))
        return BotSkillMath::Tier::EXPERT;

    BotSkillMath::Shares shares;
    shares.expertPercent = std::min(100u, uint32(std::max(0, sConfigMgr->GetIntDefault("AiPlayerbot.SkillTiers.ExpertPercent", 10))));
    shares.goodPercent = std::min(100u - shares.expertPercent,
        uint32(std::max(0, sConfigMgr->GetIntDefault("AiPlayerbot.SkillTiers.GoodPercent", 60))));
    return BotSkillMath::TierOf(guidLow, shares);
}

// The two bots of a team at the moment: nullptr for an offline member.
struct Roster
{
    Player* bot[2] = { nullptr, nullptr };
    bool complete = false;
};

Roster RosterOf(Team const& team)
{
    Roster roster;
    if (team.members.size() != 2)
        return roster;
    roster.bot[0] = PlayerOf(team.members[0].guid);
    roster.bot[1] = PlayerOf(team.members[1].guid);
    roster.complete = roster.bot[0] && roster.bot[1];
    return roster;
}

// A random bot that can be put into a party and an arena queue right now. The checks are the ones the core makes for a
// party member (Group::CanJoinBattlegroundQueue) plus the module's own: no group, not in a dungeon, a fight, the finder or
// a harness scenario.
bool IsFreeBot(Player* bot)
{
    if (!bot || !bot->IsInWorld() || !sRandomPlayerbotMgr->IsRandomBot(bot))
        return false;

    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    if (!botAI || botAI->HasActivePlayerMaster())
        return false;

    if (bot->GetGroup() || bot->GetGroupInvite() || !bot->IsAlive() || bot->IsInCombat() || bot->IsInFlight() ||
        bot->IsBeingTeleported() || bot->InBattleground() || bot->InBattlegroundQueue() || bot->InArena())
        return false;

    Map* map = bot->GetMap();
    if (!map || map->Instanceable())
        return false;

    if (bot->isUsingLfg() || sLFGMgr->GetState(bot->GetGUID()) != lfg::LFG_STATE_NONE)
        return false;

    if (bot->HasAura(lfg::LFG_SPELL_DUNGEON_DESERTER) || bot->HasAura(lfg::LFG_SPELL_DUNGEON_COOLDOWN))
        return false;

    if (BotScenario::BgJoinBlocked(bot))
        return false;

    Battleground* arenaTemplate = sBattlegroundMgr->GetBattlegroundTemplate(BATTLEGROUND_AA);
    return arenaTemplate && bot->CanJoinToBattleground(arenaTemplate) && bot->HasFreeBattlegroundQueueId();
}

Member MemberOf(Player* bot)
{
    Member member;
    member.guid = bot->GetGUIDLow();
    member.classId = bot->getClass();
    member.specId = uint16(BotGetSpecialization(bot));
    member.role = PlayerBotSpec::IsHeal(bot, true) ? 1 : 0;
    return member;
}

std::string MemberNameOf(uint32 guid)
{
    if (CharacterNameData const* data = sWorld->GetCharacterNameData(guid))
        return data->m_name;
    return std::string("?");
}

TeamSummary SummaryOf(State const& st, Team const& team, uint32 rank)
{
    TeamSummary out;
    out.id = team.id;
    out.name = team.name;
    out.bracket = team.bracket;
    out.faction = team.faction;
    out.band = team.band;
    out.tier = TierOfNumber(team.tier);
    out.rating = uint32(std::max(0, team.rating));
    out.peakRating = uint32(std::max(0, team.peak));
    out.games = team.games;
    out.wins = team.wins;
    out.losses = team.losses;
    out.draws = team.draws;
    out.streak = team.streak;
    out.bestStreak = team.bestStreak;
    out.status = team.status;
    out.rank = rank;
    out.lastPlayed = team.lastPlayed;
    out.lastResults = team.recentResults;
    for (Member const& m : team.members)
    {
        MemberSummary member;
        member.guid = m.guid;
        member.name = MemberNameOf(m.guid);
        member.classId = m.classId;
        member.specId = m.specId;
        member.healer = m.role != 0;
        out.members.push_back(member);
    }
    for (Live const& live : st.live)
        if (live.teamA == team.id || live.teamB == team.id)
            out.playingNow = true;

    // The opponent met most often in the newest matches, with the record against it.
    std::map<uint32, std::array<uint32, 3>> against;  // opponent -> wins, losses, draws
    for (MatchRow const& row : st.history)
    {
        if (row.teamA != team.id && row.teamB != team.id)
            continue;
        if (row.winner == uint8(ArenaMath::Winner::VOIDED))
            continue;
        bool const isA = row.teamA == team.id;
        uint32 const opponent = isA ? row.teamB : row.teamA;
        auto& record = against[opponent];
        if (row.winner == uint8(ArenaMath::Winner::DRAW))
            ++record[2];
        else if ((row.winner == uint8(ArenaMath::Winner::TEAM_A)) == isA)
            ++record[0];
        else
            ++record[1];
    }
    uint32 bestMeetings = 0;
    for (auto const& entry : against)
    {
        uint32 const meetings = entry.second[0] + entry.second[1] + entry.second[2];
        auto const opponent = st.teams.find(entry.first);
        if (meetings > bestMeetings && opponent != st.teams.end())
        {
            bestMeetings = meetings;
            out.rival = opponent->second.name + ", " + std::to_string(entry.second[0]) + "-" + std::to_string(entry.second[1]) +
                " us";
        }
    }
    return out;
}

// The rank of a team among the active teams of its bracket by rating (1 based), 0 for a retired team.
uint32 RankOfLocked(State const& st, uint32 teamId)
{
    auto const found = st.teams.find(teamId);
    if (found == st.teams.end() || found->second.status != STATUS_ACTIVE)
        return 0;

    uint32 rank = 1;
    for (auto const& entry : st.teams)
    {
        Team const& other = entry.second;
        if (other.status != STATUS_ACTIVE || other.bracket != found->second.bracket || other.id == teamId)
            continue;
        if (other.rating > found->second.rating || (other.rating == found->second.rating && other.id < teamId))
            ++rank;
    }
    return rank;
}

bool IsBusyLocked(State const& st, uint32 teamId)
{
    for (Live const& live : st.live)
        if (live.teamA == teamId || live.teamB == teamId)
            return true;
    return false;
}

bool NameTakenLocked(State const& st, std::string const& name)
{
    std::string const key = ArenaNames::UniqueKey(name);
    for (auto const& entry : st.teams)
        if (ArenaNames::UniqueKey(entry.second.name) == key)
            return true;
    return false;
}

void MarkDirty(State& st)
{
    st.snapshotDirty = true;
}

// ---- teams ------------------------------------------------------------------------------------------------------

struct PoolBot
{
    Player* bot;
    ArenaComp::Candidate candidate;
    uint8 faction;
    uint8 band;
};

std::vector<PoolBot> CollectPool(Config const& cfg, std::set<uint32> const& inTeams)
{
    std::vector<PoolBot> pool;
    for (auto const& entry : sRandomPlayerbotMgr->GetAllBots())
    {
        Player* bot = entry.second;
        if (!bot || !bot->IsInWorld() || !sRandomPlayerbotMgr->IsRandomBot(bot))
            continue;

        PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
        if (!botAI || botAI->HasActivePlayerMaster())
            continue;

        uint32 const level = bot->getLevel();
        if (level < cfg.minLevel || inTeams.count(bot->GetGUIDLow()))
            continue;

        PoolBot item;
        item.bot = bot;
        item.candidate.guid = bot->GetGUIDLow();
        item.candidate.specId = uint16(BotGetSpecialization(bot));
        item.candidate.level = uint8(level);
        item.candidate.tier = NaturalTier(bot->GetGUIDLow());
        item.faction = bot->GetTeamId() == TEAM_ALLIANCE ? 0 : 1;
        item.band = uint8(ArenaMath::BandOfLevel(level));
        pool.push_back(item);
    }
    return pool;
}

std::set<uint32> MembersInActiveTeamsLocked(State const& st)
{
    std::set<uint32> guids;
    for (auto const& entry : st.teams)
        if (entry.second.status == STATUS_ACTIVE)
            for (Member const& m : entry.second.members)
                guids.insert(m.guid);
    return guids;
}

void RetireLocked(State& st, Team& team, char const* why)
{
    team.status = STATUS_RETIRED;
    team.retired = UtcNow();
    Store::SaveTeam(team);
    MarkDirty(st);
    SF_LOG_INFO("playerbots", "[ARENALADDER] retired team %u <%s>: %s", team.id, team.name.c_str(), why);
}

// What a member's character is now: its level and whether it still exists. Online: the live values.
struct MemberFacts
{
    bool exists = false;
    bool online = false;
    uint32 level = 0;
    Player* bot = nullptr;
};

MemberFacts FactsOf(uint32 guid)
{
    MemberFacts facts;
    if (Player* bot = PlayerOf(guid))
    {
        facts.exists = facts.online = true;
        facts.level = bot->getLevel();
        facts.bot = bot;
        return facts;
    }
    if (CharacterNameData const* data = sWorld->GetCharacterNameData(guid))
    {
        facts.exists = true;
        facts.level = data->m_level;
    }
    return facts;
}

// A team that lost a member (the character was deleted or re-created): the other member may find a new partner, once per
// replacePartnerEveryDays, and keeps name and history; the rating moves a quarter of the way back to the tier base.
bool TryReplacePartnerLocked(State& st, Team& team, size_t lostIndex, std::vector<PoolBot> const& pool, int64 now)
{
    Config const& cfg = st.cfg;
    if (team.lastPartnerReplace && now - team.lastPartnerReplace < int64(cfg.replacePartnerEveryDays) * SECONDS_PER_DAY)
        return false;

    Member const keeper = team.members[1 - lostIndex];
    MemberFacts const keeperFacts = FactsOf(keeper.guid);
    if (!keeperFacts.online || keeperFacts.level < cfg.minLevel)
        return false;

    uint32 const keeperBand = ArenaMath::BandOfLevel(keeperFacts.level);
    ArenaComp::Candidate const keeperAsCandidate = { keeper.guid, keeper.specId, uint8(keeperFacts.level), NaturalTier(keeper.guid) };
    ArenaComp::Candidate const* best = nullptr;
    int32 bestScore = cfg.minPairScore - 1;
    for (PoolBot const& item : pool)
    {
        if (item.faction != team.faction || item.band != keeperBand)
            continue;
        int32 const score = ArenaComp::PairScore({ keeper.specId, uint8(keeperFacts.level) }, { item.candidate.specId, item.candidate.level });
        if (score > bestScore || (score == bestScore && best && item.candidate.guid < best->guid))
        {
            best = &item.candidate;
            bestScore = score;
        }
    }
    if (!best)
        return false;

    Player* partner = PlayerOf(best->guid);
    if (!partner)
        return false;

    team.members.clear();
    team.members.push_back(keeper);
    team.members.push_back(MemberOf(partner));
    std::sort(team.members.begin(), team.members.end(), [](Member const& a, Member const& b) { return a.guid < b.guid; });
    BotSkillMath::Tier const tier = ArenaMath::WeakestTier(keeperAsCandidate.tier, best->tier);
    team.tier = uint8(tier);
    team.rating = int32(ArenaMath::RatingAfterPartnerReplaced(uint32(std::max(0, team.rating)), ArenaMath::TierSeed(tier)));
    team.lastPartnerReplace = now;
    team.failedChecks = 0;
    team.band = uint8(keeperBand);
    Store::SaveTeam(team);
    MarkDirty(st);
    SF_LOG_INFO("playerbots", "[ARENALADDER] team %u <%s> found a new partner (%s), rating now %d", team.id, team.name.c_str(),
        partner->GetName().c_str(), team.rating);
    return true;
}

// The periodic check of every active team: levels (a team moves up a band with its members), specialisations (a
// re-randomised bot may break a composition), deleted characters.
void ValidateTeamsLocked(State& st, std::vector<PoolBot> const& pool, int64 now)
{
    Config const& cfg = st.cfg;
    for (auto& entry : st.teams)
    {
        Team& team = entry.second;
        if (team.status != STATUS_ACTIVE || team.members.size() != 2 || IsBusyLocked(st, team.id))
            continue;

        MemberFacts const first = FactsOf(team.members[0].guid);
        MemberFacts const second = FactsOf(team.members[1].guid);
        if (!first.exists || !second.exists)
        {
            if (!TryReplacePartnerLocked(st, team, first.exists ? 1 : 0, pool, now))
                RetireLocked(st, team, "a member's character is gone and no new partner was found");
            continue;
        }

        ArenaMath::ValidationResult const result = ArenaMath::ValidateTeam(team.band, first.level, second.level,
            cfg.minLevel, team.failedChecks, cfg.retireAfterChecks);
        team.failedChecks = result.failedChecks;
        if (result.action == ArenaMath::Validation::RETIRE)
        {
            RetireLocked(st, team, "its members have been out of step or below the minimum level for too long");
            continue;
        }
        if (result.action == ArenaMath::Validation::MOVE_BAND)
        {
            SF_LOG_INFO("playerbots", "[ARENALADDER] team %u <%s> moves up to band %u (level %u to %u)", team.id,
                team.name.c_str(), result.band, ArenaMath::BandMinLevel(result.band), ArenaMath::BandMaxLevel(result.band));
            team.band = uint8(result.band);
            Store::SaveTeam(team);
            MarkDirty(st);
        }

        // A re-randomised bot may have a new specialisation: refresh it, and rest the team while the pair no longer scores.
        bool changed = false;
        for (size_t i = 0; i < 2; ++i)
        {
            MemberFacts const& facts = i == 0 ? first : second;
            if (!facts.online)
                continue;
            Member fresh = MemberOf(facts.bot);
            if (fresh.specId != team.members[i].specId || fresh.classId != team.members[i].classId)
            {
                team.members[i] = fresh;
                changed = true;
            }
        }
        if (changed)
        {
            Store::SaveTeam(team);
            MarkDirty(st);
        }
        int32 const score = ArenaComp::PairScore({ team.members[0].specId, uint8(first.level) }, { team.members[1].specId, uint8(second.level) });
        bool const dormant = score < cfg.minPairScore - 10;
        if (dormant != team.dormant)
        {
            team.dormant = dormant;
            SF_LOG_INFO("playerbots", "[ARENALADDER] team %u <%s> %s (pair score %d)", team.id, team.name.c_str(),
                dormant ? "rests: its composition no longer scores" : "is back on the ladder", score);
        }
    }
}

// One new team per (faction, band) per run, from the online bots that are in no team.
std::vector<Team> FormTeamsLocked(State& st, std::vector<PoolBot> const& pool, int64 now)
{
    Config const& cfg = st.cfg;
    std::vector<Team> founded;

    std::set<std::pair<uint8, uint8>> groups;
    for (PoolBot const& item : pool)
        groups.insert({ item.faction, item.band });

    for (auto const& group : groups)
    {
        uint8 const faction = group.first;
        uint8 const band = group.second;
        if (!ArenaMath::BandAllowed(band, cfg.minLevel))
            continue;

        uint32 have[3] = { 0, 0, 0 };
        uint32 active = 0;
        for (auto const& entry : st.teams)
        {
            Team const& t = entry.second;
            if (t.status == STATUS_ACTIVE && t.bracket == BRACKET_2V2 && t.faction == faction && t.band == band)
            {
                ++active;
                ++have[std::min<uint8>(t.tier, 2)];
            }
        }
        if (active >= cfg.teamsPerFactionAndBand)
            continue;

        std::vector<ArenaComp::Candidate> candidates;
        for (PoolBot const& item : pool)
            if (item.faction == faction && item.band == band)
                candidates.push_back(item.candidate);

        ArenaComp::TierMix mix;
        mix.target[0] = cfg.mixExpert;
        mix.target[1] = cfg.mixGood;
        mix.target[2] = cfg.mixCasual;
        std::optional<ArenaComp::FormedPair> const pair = ArenaComp::FormNextTeam(candidates, cfg.minPairScore, mix, have);
        if (!pair)
            continue;

        Player* first = PlayerOf(pair->first);
        Player* second = PlayerOf(pair->second);
        if (!first || !second)
            continue;

        uint32 const counter = ++st.teamCounter;
        std::string const name = ArenaNames::GenerateUnique(faction, counter, true,
            [&st](std::string const& candidate) { return NameTakenLocked(st, candidate); });
        if (name.empty())
        {
            SF_LOG_WARN("playerbots", "[ARENALADDER] no free team name for faction %u (counter %u)", uint32(faction), counter);
            continue;
        }

        Team team;
        team.id = st.nextTeamId++;
        team.name = name;
        team.bracket = BRACKET_2V2;
        team.faction = faction;
        team.band = band;
        team.tier = uint8(pair->tier);
        team.rating = ArenaMath::SeedRating(pair->tier, team.id);
        team.peak = team.rating;
        team.created = now;
        team.status = STATUS_ACTIVE;
        team.members.push_back(MemberOf(first));
        team.members.push_back(MemberOf(second));
        std::sort(team.members.begin(), team.members.end(), [](Member const& a, Member const& b) { return a.guid < b.guid; });

        st.teams[team.id] = team;
        Store::SaveTeam(team);
        MarkDirty(st);
        founded.push_back(team);
        SF_LOG_INFO("playerbots", "[ARENALADDER] formed team %u <%s> (%s, band %u, %s, rating %d): %s (%s) + %s (%s), pair score %d",
            team.id, team.name.c_str(), faction == 0 ? "Alliance" : "Horde", uint32(band), ArenaText::TierName(pair->tier),
            team.rating, first->GetName().c_str(), ArenaText::SpecAndClass(team.members[0].classId, team.members[0].specId).c_str(),
            second->GetName().c_str(), ArenaText::SpecAndClass(team.members[1].classId, team.members[1].specId).c_str(),
            pair->score);
    }
    return founded;
}

// ---- matches ----------------------------------------------------------------------------------------------------

void SetTiersLocked(State& st, Live const& live, bool on)
{
    std::unique_lock<std::shared_mutex> guard(st.tierLock);
    for (uint32 teamId : { live.teamA, live.teamB })
    {
        auto const team = st.teams.find(teamId);
        if (team == st.teams.end())
            continue;
        for (Member const& m : team->second.members)
        {
            if (on)
                st.tierByGuid[m.guid] = team->second.tier;
            else
                st.tierByGuid.erase(m.guid);
        }
    }
}

// A party of the team's two bots, queued for the rated 2v2 arena. The captain is the first member (the lower guid).
bool QueueTeam(Team const& team)
{
    Roster const roster = RosterOf(team);
    if (!roster.complete || !IsFreeBot(roster.bot[0]) || !IsFreeBot(roster.bot[1]))
        return false;

    Player* captain = roster.bot[0];
    Group* group = new Group();
    if (!group->Create(captain))
    {
        delete group;
        return false;
    }
    sGroupMgr->AddGroup(group);

    if (!group->AddMember(roster.bot[1]))
    {
        group->Disband(true);
        return false;
    }

    // The same reset the legacy arena party gets: a bot that joins a party re-reads its master and strategies.
    for (Player* bot : roster.bot)
        if (PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot))
            botAI->Reset();

    uint8 const slot = ArenaTeam::GetSlotByType(BRACKET_2V2);
    if (slot >= MAX_ARENA_SLOT)
    {
        group->Disband(true);
        return false;
    }

    // The packet is a single byte, the arena slot (WorldSession::HandleBattlemasterJoinArena); the bot sessions never
    // drain packets, so the handler is called directly, as the legacy arena queueing does.
    WorldPacket packet(CMSG_BATTLEMASTER_JOIN_ARENA, 1);
    packet << uint8(slot);
    captain->GetSession()->HandleBattlemasterJoinArena(packet);

    if (!captain->InBattlegroundQueueForBattlegroundQueueType(BATTLEGROUND_QUEUE_2v2))
    {
        group->Disband(true);
        return false;
    }
    return true;
}

// Takes the party apart again: the two bots leave their arena party (the module's own random-bot rule).
void ReleaseParty(Team const& team)
{
    Roster const roster = RosterOf(team);
    for (Player* bot : roster.bot)
        if (bot && !bot->InBattleground())
            PlayerbotRatedBg::LeaveGroupAfterMatch(bot);
}

void DropQueue(Team const& team)
{
    Roster const roster = RosterOf(team);
    for (Player* bot : roster.bot)
    {
        if (!bot)
            continue;
        if (bot->InBattlegroundQueueForBattlegroundQueueType(BATTLEGROUND_QUEUE_2v2))
        {
            bot->RemoveBattlegroundQueueId(BATTLEGROUND_QUEUE_2v2);
            sBattlegroundMgr->GetBattlegroundQueue(BATTLEGROUND_QUEUE_2v2).RemovePlayer(bot->GetGUID(), true);
        }
    }
}

void EndLiveLocked(State& st, size_t index)
{
    Live const live = st.live[index];
    SetTiersLocked(st, live, false);
    for (uint32 teamId : { live.teamA, live.teamB })
    {
        auto const team = st.teams.find(teamId);
        if (team != st.teams.end())
            ReleaseParty(team->second);
    }
    st.live.erase(st.live.begin() + index);
    MarkDirty(st);
}

bool HumanWaiting(uint32 band)
{
    return PlayerbotArena::Census(uint32(BATTLEGROUND_QUEUE_2v2), band).ratedArenaPlayerCount > 0;
}

// One scheduler pass. `force` (the console command) skips the clock, the roll and the world load, never the safety checks.
void RunSchedulerLocked(State& st, int64 now, bool force)
{
    Config const& cfg = st.cfg;

    ArenaMath::LocalTime const local = OwnerNow(cfg, now);
    uint32 const weight = ArenaMath::ActivityWeightPercent(local);
    if (!force)
    {
        if (weight == 0)
            return;
        if (!ArenaMath::WorldLoadAllows(uint32(st.avgUpdateMs), cfg.maxUpdateMs))
            return;
        double const roll = double(urand(0, ROLL_RANGE - 1)) / double(ROLL_RANGE);
        if (!ArenaMath::ShouldStart(weight, cfg.peakMatchesPerHour, roll))
            return;
    }

    if (st.live.size() >= cfg.maxConcurrentBotMatches)
        return;

    std::vector<ArenaMath::TeamView> views;
    for (auto const& entry : st.teams)
    {
        Team const& team = entry.second;
        if (team.status != STATUS_ACTIVE || team.dormant || team.bracket != BRACKET_2V2 || now < team.backoffUntil ||
            IsBusyLocked(st, team.id) || HumanWaiting(team.band))
            continue;

        Roster const roster = RosterOf(team);
        if (!roster.complete || !IsFreeBot(roster.bot[0]) || !IsFreeBot(roster.bot[1]))
            continue;

        ArenaMath::TeamView view;
        view.id = team.id;
        view.rating = team.rating;
        view.bracket = team.bracket;
        view.band = team.band;
        view.faction = team.faction;
        int64 const since = team.lastPlayed ? team.lastPlayed : team.created;
        view.idleSeconds = uint32(std::max<int64>(0, now - since));
        view.failedPasses = team.failedPasses;
        view.recentOpponents.assign(team.recentOpponents.begin(), team.recentOpponents.end());
        views.push_back(std::move(view));
    }

    if (views.size() < 2)
        return;

    ArenaMath::PairChoice const choice = ArenaMath::PickPair(views);
    for (uint32 id : choice.unmatched)
        ++st.teams[id].failedPasses;
    if (!choice.found)
        return;

    Team& a = st.teams[choice.teamA];
    Team& b = st.teams[choice.teamB];
    a.failedPasses = b.failedPasses = 0;

    // Both parties are queued in the same tick, so the core pairs them with each other (they are the only rated groups
    // of the bracket while no human waits).
    bool const queuedA = QueueTeam(a);
    bool const queuedB = queuedA && QueueTeam(b);
    if (!queuedA || !queuedB)
    {
        Team& failed = queuedA ? b : a;
        failed.backoffUntil = now + BACKOFF_AFTER_QUEUE_FAIL_SECONDS;
        DropQueue(a);
        DropQueue(b);
        ReleaseParty(a);
        ReleaseParty(b);
        SF_LOG_WARN("playerbots", "[ARENALADDER] could not queue team %u <%s> for the match against %u <%s>; it rests for %u s",
            failed.id, failed.name.c_str(), (queuedA ? a : b).id, (queuedA ? a : b).name.c_str(), BACKOFF_AFTER_QUEUE_FAIL_SECONDS);
        return;
    }

    Live live;
    live.teamA = a.id;
    live.teamB = b.id;
    live.state = LiveState::QUEUED;
    live.orderedAt = now;
    st.live.push_back(live);
    SetTiersLocked(st, live, true);
    MarkDirty(st);
    SF_LOG_INFO("playerbots", "[ARENALADDER] match queued: %u <%s> (%d, %s) against %u <%s> (%d, %s)%s", a.id, a.name.c_str(),
        a.rating, ArenaText::TierName(TierOfNumber(a.tier)), b.id, b.name.c_str(), b.rating,
        ArenaText::TierName(TierOfNumber(b.tier)), force ? " [forced]" : "");
}

// The result of a match, booked to both teams. `kind` is the match table's winner column.
struct Booked
{
    bool any = false;
    Team winner;
    Team loser;
    ArenaChatMath::ResultFacts winnerFacts;
    ArenaChatMath::ResultFacts loserFacts;
    bool draw = false;
};

Booked BookLocked(State& st, Live const& live, ArenaMath::Winner kind, uint32 seconds, int64 now)
{
    Booked booked;
    auto itA = st.teams.find(live.teamA);
    auto itB = st.teams.find(live.teamB);
    if (itA == st.teams.end() || itB == st.teams.end())
        return booked;

    Team& a = itA->second;
    Team& b = itB->second;

    MatchRow row;
    row.id = st.nextMatchId++;
    row.bracket = BRACKET_2V2;
    row.teamA = a.id;
    row.teamB = b.id;
    row.winner = uint8(kind);
    row.ratingA = row.ratingANew = uint32(std::max(0, a.rating));
    row.ratingB = row.ratingBNew = uint32(std::max(0, b.rating));
    row.mapId = uint16(live.mapId);
    row.seconds = uint16(std::min<uint32>(seconds, 65535));
    row.ended = now;

    if (kind == ArenaMath::Winner::VOIDED)
    {
        st.history.push_back(row);
        while (st.history.size() > HISTORY_IN_RAM)
            st.history.pop_front();
        Store::SaveMatch(row, nullptr, nullptr);
        MarkDirty(st);
        SF_LOG_INFO("playerbots", "[ARENALADDER] match %u voided: %u <%s> against %u <%s> (no rating change)", row.id, a.id,
            a.name.c_str(), b.id, b.name.c_str());
        return booked;
    }

    ArenaMath::Outcome const outA = kind == ArenaMath::Winner::TEAM_A ? ArenaMath::Outcome::WIN
        : kind == ArenaMath::Winner::TEAM_B ? ArenaMath::Outcome::LOSS : ArenaMath::Outcome::DRAW;
    ArenaMath::Outcome const outB = ArenaMath::Flip(outA);

    uint32 const rankBeforeA = RankOfLocked(st, a.id);
    uint32 const rankBeforeB = RankOfLocked(st, b.id);
    int32 const streakBeforeA = a.streak;
    int32 const streakBeforeB = b.streak;

    int32 const changeA = ArenaMath::RatingChange(a.rating, b.rating, outA, a.games);
    int32 const changeB = ArenaMath::RatingChange(b.rating, a.rating, outB, b.games);

    auto apply = [&](Team& team, Team const& opponent, ArenaMath::Outcome outcome, int32 change)
    {
        team.rating = int32(ArenaMath::Apply(uint32(std::max(0, team.rating)), change));
        team.peak = std::max(team.peak, team.rating);
        ++team.games;
        if (outcome == ArenaMath::Outcome::WIN)
            ++team.wins;
        else if (outcome == ArenaMath::Outcome::LOSS)
            ++team.losses;
        else
            ++team.draws;
        team.streak = ArenaMath::NextStreak(team.streak, outcome);
        if (team.streak > 0)
            team.bestStreak = std::max(team.bestStreak, uint32(team.streak));
        team.lastPlayed = now;
        team.recentOpponents.push_back(opponent.id);
        if (team.recentOpponents.size() > RECENT_OPPONENTS)
            team.recentOpponents.pop_front();
        team.recentResults += outcome == ArenaMath::Outcome::WIN ? 'W' : outcome == ArenaMath::Outcome::LOSS ? 'L' : 'D';
        if (team.recentResults.size() > RECENT_RESULTS)
            team.recentResults.erase(0, 1);
    };
    Team const beforeA = a;
    Team const beforeB = b;
    apply(a, beforeB, outA, changeA);
    apply(b, beforeA, outB, changeB);

    row.ratingANew = uint32(std::max(0, a.rating));
    row.ratingBNew = uint32(std::max(0, b.rating));
    st.history.push_back(row);
    while (st.history.size() > HISTORY_IN_RAM)
        st.history.pop_front();
    Store::SaveMatch(row, &a, &b);
    MarkDirty(st);

    SF_LOG_INFO("playerbots", "[ARENALADDER] match %u: %u <%s> %s against %u <%s> after %u s on map %u: %u -> %u (%+d) and %u -> %u (%+d)",
        row.id, a.id, a.name.c_str(), kind == ArenaMath::Winner::DRAW ? "drew" : kind == ArenaMath::Winner::TEAM_A ? "won" : "lost",
        b.id, b.name.c_str(), seconds, live.mapId, row.ratingA, row.ratingANew, changeA, row.ratingB, row.ratingBNew, changeB);

    // What the chat may say about it.
    booked.any = true;
    booked.draw = kind == ArenaMath::Winner::DRAW;
    if (!booked.draw)
    {
        bool const aWon = kind == ArenaMath::Winner::TEAM_A;
        Team const& won = aWon ? a : b;
        Team const& lost = aWon ? b : a;
        booked.winner = won;
        booked.loser = lost;

        auto meetings = [&](uint32 x, uint32 y, uint32& winsX, uint32& winsY)
        {
            uint32 count = 0;
            winsX = winsY = 0;
            for (MatchRow const& m : st.history)
            {
                bool const pair = (m.teamA == x && m.teamB == y) || (m.teamA == y && m.teamB == x);
                if (!pair || m.winner == uint8(ArenaMath::Winner::VOIDED) || now - m.ended > RIVAL_WINDOW_SECONDS)
                    continue;
                ++count;
                if (m.winner == uint8(ArenaMath::Winner::TEAM_A))
                    ++(m.teamA == x ? winsX : winsY);
                else if (m.winner == uint8(ArenaMath::Winner::TEAM_B))
                    ++(m.teamB == x ? winsX : winsY);
            }
            return count;
        };

        ArenaChatMath::ResultFacts facts;
        facts.oldStreak = aWon ? streakBeforeA : streakBeforeB;
        facts.newStreak = won.streak;
        facts.oldRank = aWon ? rankBeforeA : rankBeforeB;
        facts.newRank = RankOfLocked(st, won.id);
        facts.meetings = meetings(won.id, lost.id, facts.winsOfTeam, facts.winsOfOpponent);
        booked.winnerFacts = facts;

        ArenaChatMath::ResultFacts lostFacts;
        lostFacts.oldStreak = aWon ? streakBeforeB : streakBeforeA;
        booked.loserFacts = lostFacts;
    }
    return booked;
}

// The core's idea of who is in the arena: how many of the four bots are inside, and the side of each team.
struct ArenaView
{
    Battleground* bg = nullptr;
    uint32 inside = 0;
    bool human = false;
};

ArenaView ViewArena(State const& st, Live const& live, Battleground* bg)
{
    ArenaView view;
    view.bg = bg;
    for (uint32 teamId : { live.teamA, live.teamB })
    {
        auto const team = st.teams.find(teamId);
        if (team == st.teams.end())
            continue;
        for (Member const& m : team->second.members)
            if (bg->IsPlayerInBattleground(GuidOf(m.guid)))
                ++view.inside;
    }

    for (auto const& entry : bg->GetPlayers())
    {
        Player* player = ObjectAccessor::FindPlayer(entry.first);
        if (player && !GET_PLAYERBOT_AI(player))
            view.human = true;
    }
    return view;
}

// The teams of a match enter the arena: remember the instance and which side each fights on.
void NoteEnteredLocked(State& st, Live& live, Battleground* bg, int64 now)
{
    live.state = LiveState::RUNNING;
    live.instanceId = bg->GetInstanceID();
    live.bgTypeId = uint32(bg->GetTypeID());
    live.mapId = bg->GetMapId();
    MarkDirty(st);
    SF_LOG_INFO("playerbots", "[ARENALADDER] %u <%s> against %u <%s> entered arena map %u (instance %u) after %lld s", live.teamA,
        st.teams[live.teamA].name.c_str(), live.teamB, st.teams[live.teamB].name.c_str(), live.mapId, live.instanceId,
        static_cast<long long>(now - live.orderedAt));
}

void SetSidesLocked(State const& st, Live& live, Battleground* bg)
{
    live.sideA = live.sideB = 0;
    auto sideOf = [&](uint32 teamId)
    {
        auto const team = st.teams.find(teamId);
        if (team == st.teams.end())
            return 0u;
        for (Member const& m : team->second.members)
            if (bg->IsPlayerInBattleground(GuidOf(m.guid)))
                return bg->GetPlayerTeam(GuidOf(m.guid));
        return 0u;
    };
    live.sideA = sideOf(live.teamA);
    live.sideB = sideOf(live.teamB);
}

// Looks at every live match (world tick). Returns the results to tell the chat about.
std::vector<Booked> PollLocked(State& st, int64 now)
{
    std::vector<Booked> results;
    Config const& cfg = st.cfg;

    for (size_t i = 0; i < st.live.size();)
    {
        Live& live = st.live[i];
        auto const itA = st.teams.find(live.teamA);
        auto const itB = st.teams.find(live.teamB);
        if (itA == st.teams.end() || itB == st.teams.end())
        {
            EndLiveLocked(st, i);
            continue;
        }

        if (live.state == LiveState::QUEUED)
        {
            Battleground* bg = nullptr;
            for (Team const* team : { &itA->second, &itB->second })
            {
                Roster const roster = RosterOf(*team);
                for (Player* bot : roster.bot)
                    if (bot && bot->InArena() && bot->GetBattleground())
                        bg = bot->GetBattleground();
            }

            if (bg)
            {
                NoteEnteredLocked(st, live, bg, now);
                live.runningSinceMs = getMSTime();  // used as the entry clock until the arena is in progress
            }
            else if (now - live.orderedAt > int64(cfg.orderTimeoutSeconds))
            {
                SF_LOG_WARN("playerbots", "[ARENALADDER] match of %u <%s> and %u <%s> did not start within %u s; released",
                    live.teamA, itA->second.name.c_str(), live.teamB, itB->second.name.c_str(), cfg.orderTimeoutSeconds);
                DropQueue(itA->second);
                DropQueue(itB->second);
                itA->second.backoffUntil = itB->second.backoffUntil = now + BACKOFF_AFTER_QUEUE_FAIL_SECONDS;
                EndLiveLocked(st, i);
                continue;
            }
            ++i;
            continue;
        }

        if (live.state == LiveState::RUNNING)
        {
            Battleground* bg = sBattlegroundMgr->GetBattleground(live.instanceId, BattlegroundTypeId(live.bgTypeId));
            if (!bg)
            {
                // The arena is gone and no result was seen: nothing to book.
                live.voided = true;
                Booked booked = BookLocked(st, live, ArenaMath::Winner::VOIDED, 0, now);
                live.state = LiveState::FINISHED;
                live.finishedAt = now;
                results.push_back(booked);
                ++i;
                continue;
            }

            ArenaView const view = ViewArena(st, live, bg);
            BattlegroundStatus const status = bg->GetStatus();

            if (!live.sideA || !live.sideB)
            {
                if (view.inside == 4)
                {
                    SetSidesLocked(st, live, bg);
                    if (!live.sideA || live.sideA == live.sideB)
                        live.voided = true;
                }
                else if (getMSTimeDiff(live.runningSinceMs, getMSTime()) > ENTRY_GRACE_SECONDS * MS_PER_SECOND &&
                    status != STATUS_WAIT_LEAVE)
                {
                    live.voided = true;
                    SF_LOG_WARN("playerbots", "[ARENALADDER] match of %u and %u: not all four bots entered the arena; voided",
                        live.teamA, live.teamB);
                }
            }
            else if (status == STATUS_IN_PROGRESS && view.inside < 4)
            {
                live.voided = true;  // a member left before the end
            }

            if (view.human)
                live.voided = true;  // a human is in the arena: not a bot-only ladder match

            if (status == STATUS_IN_PROGRESS)
            {
                if (!live.seenInProgress)
                {
                    live.seenInProgress = true;
                    live.runningSinceMs = getMSTime();
                }
                uint32 const elapsed = getMSTimeDiff(live.runningSinceMs, getMSTime()) / MS_PER_SECOND;
                if (!live.cutAsDraw && ArenaMath::IsCutAsDraw(elapsed, view.human, cfg.botMatchCutSeconds))
                {
                    live.cutAsDraw = true;
                    SF_LOG_INFO("playerbots", "[ARENALADDER] match of %u and %u cut as a draw after %u s", live.teamA, live.teamB, elapsed);
                    bg->EndBattleground(WINNER_NONE);
                }
            }
            else if (status == STATUS_WAIT_LEAVE)
            {
                uint32 const seconds = live.seenInProgress ? getMSTimeDiff(live.runningSinceMs, getMSTime()) / MS_PER_SECOND : 0;
                ArenaMath::Winner kind = ArenaMath::Winner::VOIDED;
                uint8 const raw = bg->GetWinner();
                if (!live.voided)
                {
                    if (raw == CORE_WINNER_DRAW)
                        kind = ArenaMath::Winner::DRAW;
                    else if (raw == WINNER_ALLIANCE || raw == WINNER_HORDE)
                    {
                        uint32 const winnerSide = raw == WINNER_ALLIANCE ? uint32(ALLIANCE) : uint32(HORDE);
                        kind = winnerSide == live.sideA ? ArenaMath::Winner::TEAM_A
                            : winnerSide == live.sideB ? ArenaMath::Winner::TEAM_B : ArenaMath::Winner::VOIDED;
                    }
                }

                Booked booked = BookLocked(st, live, kind, seconds, now);
                live.state = LiveState::FINISHED;
                live.finishedAt = now;
                results.push_back(booked);
            }
            ++i;
            continue;
        }

        // FINISHED: free the teams when the bots have left the arena, or at the latest after releaseAfterSeconds.
        bool anyInside = false;
        for (Team const* team : { &itA->second, &itB->second })
        {
            Roster const roster = RosterOf(*team);
            for (Player* bot : roster.bot)
                anyInside = anyInside || (bot && bot->InBattleground());
        }
        if (!anyInside || now - live.finishedAt > int64(cfg.releaseAfterSeconds))
        {
            EndLiveLocked(st, i);
            continue;
        }
        ++i;
    }
    return results;
}

// ---- the console ------------------------------------------------------------------------------------------------

std::string TeamLine(State const& st, Team const& team)
{
    std::ostringstream line;
    line << "[" << team.id << "] " << team.name << " | " << (team.faction == 0 ? "A" : "H") << " band " << uint32(team.band) << " ("
         << ArenaMath::BandMinLevel(team.band) << "-" << ArenaMath::BandMaxLevel(team.band) << ") | "
         << ArenaText::TierName(TierOfNumber(team.tier)) << " | " << team.rating << " (peak " << team.peak << ") | "
         << ArenaText::RecordText(team.wins, team.losses, team.draws);
    std::string const run = ArenaText::StreakText(team.streak);
    if (!run.empty())
        line << ", " << run;
    line << " | ";
    for (size_t i = 0; i < team.members.size(); ++i)
        line << (i ? " + " : "") << MemberNameOf(team.members[i].guid) << " ("
             << ArenaText::SpecAndClass(team.members[i].classId, team.members[i].specId) << ")";
    if (team.status != STATUS_ACTIVE)
        line << " | RETIRED";
    else if (team.dormant)
        line << " | resting";
    if (IsBusyLocked(st, team.id))
        line << " | in a match";
    return line.str();
}
}  // namespace

// ---- shared helpers -----------------------------------------------------------------------------------------------

State& S()
{
    static State state;
    return state;
}

BotSkillMath::Tier TierOfNumber(uint8 tier)
{
    return tier == 0 ? BotSkillMath::Tier::EXPERT : tier == 1 ? BotSkillMath::Tier::GOOD : BotSkillMath::Tier::CASUAL;
}

ArenaMath::LocalTime OwnerNow(Config const& cfg, int64 utcNow)
{
    return ArenaMath::OwnerLocalTime(utcNow, cfg.ownerUtcOffsetHours, cfg.ownerUsDaylight, cfg.clockOffsetMinutes);
}

Config ReadConfig()
{
    auto number = [](char const* key, int32 fallback, int32 low = 0, int32 high = 1000000)
    {
        return std::clamp(sConfigMgr->GetIntDefault(key, fallback), low, high);
    };

    Config cfg;
    cfg.enable = sConfigMgr->GetBoolDefault("AiPlayerbot.ArenaTeams.Enable", true);
    cfg.minLevel = uint32(number("AiPlayerbot.ArenaTeams.MinLevel", 60, 10, 90));
    cfg.teamsPerFactionAndBand = uint32(number("AiPlayerbot.ArenaTeams.PerFaction", 4, 0, 50));
    cfg.minPairScore = number("AiPlayerbot.ArenaTeams.MinPairScore", ArenaComp::DEFAULT_MIN_PAIR_SCORE, -1000, 1000);
    cfg.mixExpert = uint32(number("AiPlayerbot.ArenaTeams.MixExpert", 1, 0, 50));
    cfg.mixGood = uint32(number("AiPlayerbot.ArenaTeams.MixGood", 3, 0, 50));
    cfg.mixCasual = uint32(number("AiPlayerbot.ArenaTeams.MixCasual", 2, 0, 50));
    cfg.startupDelaySeconds = uint32(number("AiPlayerbot.ArenaTeams.StartupDelaySeconds", 600, 0, 86400));
    cfg.formationIntervalSeconds = uint32(number("AiPlayerbot.ArenaTeams.FormationIntervalSeconds", 600, 10, 86400));
    cfg.peakMatchesPerHour = uint32(number("AiPlayerbot.ArenaTeams.PeakMatchesPerHour", 4, 0, 60));
    cfg.maxConcurrentBotMatches = uint32(number("AiPlayerbot.ArenaTeams.MaxConcurrentBotMatches", 1, 0, 8));
    cfg.maxUpdateMs = uint32(number("AiPlayerbot.ArenaTeams.MaxUpdateMs", 80, 1, 10000));
    cfg.botMatchCutSeconds = uint32(number("AiPlayerbot.ArenaTeams.BotMatchCutMinutes", 12, 0, 60)) * 60;
    cfg.ownerUtcOffsetHours = number("AiPlayerbot.ArenaTeams.OwnerUtcOffsetHours", 0, -12, 14);
    cfg.ownerUsDaylight = sConfigMgr->GetBoolDefault("AiPlayerbot.ArenaTeams.OwnerUsDst", false);
    cfg.clockOffsetMinutes = number("AiPlayerbot.ArenaTeams.ClockOffsetMinutes", 0, -1440, 1440);
    cfg.retireAfterChecks = uint32(number("AiPlayerbot.ArenaTeams.RetireAfterChecks", 12, 1, 10000));
    cfg.replacePartnerEveryDays = uint32(number("AiPlayerbot.ArenaTeams.ReplacePartnerDays", 7, 0, 365));
    cfg.orderTimeoutSeconds = uint32(number("AiPlayerbot.ArenaTeams.QueueTimeoutSeconds", 240, 30, 3600));
    cfg.releaseAfterSeconds = uint32(number("AiPlayerbot.ArenaTeams.ReleaseAfterSeconds", 300, 30, 3600));
    cfg.snapshotFile = sConfigMgr->GetStringDefault("AiPlayerbot.ArenaTeams.SnapshotFile", "arena-ladder.json");
    cfg.snapshotMinSeconds = uint32(number("AiPlayerbot.ArenaTeams.SnapshotMinSeconds", 30, 1, 3600));
    cfg.snapshotMaxSeconds = uint32(number("AiPlayerbot.ArenaTeams.SnapshotMaxSeconds", 300, 10, 86400));
    cfg.chatEnable = sConfigMgr->GetBoolDefault("AiPlayerbot.ArenaChat.Enable", true);
    cfg.chatLimits.ladderGapSeconds = int64(number("AiPlayerbot.ArenaChat.LadderGapMinutes", 15, 0, 1440)) * 60;
    cfg.chatLimits.teamGapSeconds = int64(number("AiPlayerbot.ArenaChat.TeamGapMinutes", 45, 0, 1440)) * 60;
    cfg.chatLimits.perDay = uint32(number("AiPlayerbot.ArenaChat.PerDay", 12, 0, 1000));
    return cfg;
}

bool Enabled()
{
    return gEnabled.load();
}

void OnStartup()
{
    State& st = S();
    Config const cfg = ReadConfig();
    gEnabled = cfg.enable;
    if (!cfg.enable)
    {
        SF_LOG_INFO("playerbots", ">> Arena ladder: off (AiPlayerbot.ArenaTeams.Enable = 0)");
        return;
    }

    Store::EnsureTables();

    std::lock_guard<std::mutex> guard(st.lock);
    st.cfg = cfg;
    st.chat = ArenaChatMath::ChatLimiter(cfg.chatLimits);
    Store::LoadAll(st);
    st.untilFormationMs = cfg.startupDelaySeconds * MS_PER_SECOND;
    st.untilSchedulerMs = (cfg.startupDelaySeconds + SCHEDULER_BASE_MS / MS_PER_SECOND) * MS_PER_SECOND;
    st.loaded = true;
    st.started = true;
    MarkDirty(st);

    uint32 active = 0;
    for (auto const& entry : st.teams)
        active += entry.second.status == STATUS_ACTIVE ? 1 : 0;
    SF_LOG_INFO("playerbots", ">> Arena ladder: %zu teams (%u active), %zu matches in memory; teams from level %u, first formation in %u s",
        st.teams.size(), active, st.history.size(), cfg.minLevel, cfg.startupDelaySeconds);

    // A self check of the band numbering against the core's own brackets (the census and the queue use the core's).
    if (Battleground* arenaTemplate = sBattlegroundMgr->GetBattlegroundTemplate(BATTLEGROUND_AA))
        for (uint32 level : { 60u, 64u, 65u, 80u, 89u, 90u })
            if (PvPDifficultyEntry const* entry = GetBattlegroundBracketByLevel(arenaTemplate->GetMapId(), level))
                if (uint32(entry->GetBracketId()) != ArenaMath::BandOfLevel(level))
                    SF_LOG_ERROR("playerbots", "[ARENALADDER] band of level %u is %u here but the core's bracket is %u", level,
                        ArenaMath::BandOfLevel(level), uint32(entry->GetBracketId()));
}

void OnUpdate(uint32 diff)
{
    if (!gEnabled.load())
        return;

    State& st = S();
    std::vector<Booked> booked;
    std::vector<Team> founded;
    bool writeSnapshot = false;
    Config cfg;
    int64 const now = UtcNow();
    {
        std::lock_guard<std::mutex> guard(st.lock);
        if (!st.loaded)
            return;

        cfg = st.cfg;

        double const sample = double(std::min(diff, UPDATE_SAMPLE_CAP_MS));
        st.avgUpdateMs = st.avgUpdateSeen ? st.avgUpdateMs * (1.0 - UPDATE_SMOOTHING) + sample * UPDATE_SMOOTHING : sample;
        st.avgUpdateSeen = true;

        if (!st.live.empty())
            booked = PollLocked(st, now);

        if (st.untilFormationMs > diff && !st.forceFormation)
            st.untilFormationMs -= diff;
        else
        {
            st.untilFormationMs = cfg.formationIntervalSeconds * MS_PER_SECOND;
            st.forceFormation = false;
            st.cfg = ReadConfig();
            cfg = st.cfg;
            gEnabled = cfg.enable;
            if (cfg.enable)
            {
                std::vector<PoolBot> const pool = CollectPool(cfg, MembersInActiveTeamsLocked(st));
                ValidateTeamsLocked(st, pool, now);
                founded = FormTeamsLocked(st, pool, now);
            }
        }

        if (st.untilSchedulerMs > diff && !st.forceMatch)
            st.untilSchedulerMs -= diff;
        else
        {
            bool const force = st.forceMatch;
            st.forceMatch = false;
            st.untilSchedulerMs = SCHEDULER_BASE_MS - SCHEDULER_JITTER_MS + urand(0, 2 * SCHEDULER_JITTER_MS);
            RunSchedulerLocked(st, now, force);
        }

        st.sinceSnapshotMs += diff;
        bool const due = st.sinceSnapshotMs >= cfg.snapshotMaxSeconds * MS_PER_SECOND ||
            (st.snapshotDirty && st.sinceSnapshotMs >= cfg.snapshotMinSeconds * MS_PER_SECOND) || st.forceSnapshot;
        if (due)
        {
            writeSnapshot = true;
            st.forceSnapshot = false;
            st.snapshotDirty = false;
            st.sinceSnapshotMs = 0;
        }
    }

    for (Team const& team : founded)
        Chat::OnFormed(team, now);

    for (Booked const& result : booked)
    {
        if (!result.any || result.draw)
            continue;
        // One line per result: the winner's own notable moment first, else the streak it has just ended.
        if (ArenaChatMath::ChooseMoment(result.winnerFacts, true) != ArenaChatMath::Moment::NONE)
            Chat::OnWon(result.winner, result.loser, result.winnerFacts, now);
        else if (ArenaChatMath::BrokeAStreak(result.loserFacts.oldStreak))
            Chat::OnStreakBroken(result.winner, result.loser, result.loserFacts.oldStreak, now);
    }

    if (writeSnapshot)
    {
        std::string json;
        {
            std::lock_guard<std::mutex> guard(st.lock);
            json = BuildSnapshotJson(st, now);
        }
        // BT-700: built under the lock, written by a worker (a few hundred KB of disk I/O off the world thread).
        QueueSnapshotWrite(cfg, std::move(json));
    }
}

bool TeamTierOf(uint32 guidLow, BotSkillMath::Tier& tier)
{
    State& st = S();
    std::shared_lock<std::shared_mutex> guard(st.tierLock);
    auto const found = st.tierByGuid.find(guidLow);
    if (found == st.tierByGuid.end())
        return false;
    tier = TierOfNumber(found->second);
    return true;
}

bool IsInLadderMatch(uint32 guidLow)
{
    State& st = S();
    std::shared_lock<std::shared_mutex> guard(st.tierLock);
    return st.tierByGuid.count(guidLow) != 0;
}

std::vector<TeamSummary> Ladder(uint8 bracket, bool includeRetired)
{
    State& st = S();
    std::lock_guard<std::mutex> guard(st.lock);
    std::vector<TeamSummary> out;
    for (auto const& entry : st.teams)
    {
        Team const& team = entry.second;
        if (team.bracket != bracket || (!includeRetired && team.status != STATUS_ACTIVE))
            continue;
        out.push_back(SummaryOf(st, team, RankOfLocked(st, team.id)));
    }
    std::sort(out.begin(), out.end(), [](TeamSummary const& a, TeamSummary const& b)
    {
        return a.status != b.status ? a.status < b.status : a.rating != b.rating ? a.rating > b.rating : a.id < b.id;
    });
    return out;
}

bool FindTeam(uint32 teamId, TeamSummary& out)
{
    State& st = S();
    std::lock_guard<std::mutex> guard(st.lock);
    auto const found = st.teams.find(teamId);
    if (found == st.teams.end())
        return false;
    out = SummaryOf(st, found->second, RankOfLocked(st, teamId));
    return true;
}

std::vector<LiveSummary> LiveMatches()
{
    State& st = S();
    std::lock_guard<std::mutex> guard(st.lock);
    std::vector<LiveSummary> out;
    int64 const now = UtcNow();
    for (Live const& live : st.live)
    {
        auto const a = st.teams.find(live.teamA);
        auto const b = st.teams.find(live.teamB);
        if (a == st.teams.end() || b == st.teams.end())
            continue;
        LiveSummary item;
        item.teamA = a->second.name;
        item.teamB = b->second.name;
        item.state = live.state == LiveState::QUEUED ? "queued" : live.state == LiveState::RUNNING ? "fighting" : "finished";
        item.seconds = uint32(std::max<int64>(0, now - live.orderedAt));
        out.push_back(item);
    }
    return out;
}

namespace
{
MatchSummary MatchSummaryOf(State const& st, MatchRow const& row)
{
    MatchSummary out;
    out.ended = row.ended;
    auto const a = st.teams.find(row.teamA);
    auto const b = st.teams.find(row.teamB);
    out.teamA = a != st.teams.end() ? a->second.name : std::string("?");
    out.teamB = b != st.teams.end() ? b->second.name : std::string("?");
    out.winner = row.winner;
    out.ratingA = row.ratingA;
    out.ratingANew = row.ratingANew;
    out.ratingB = row.ratingB;
    out.ratingBNew = row.ratingBNew;
    out.seconds = row.seconds;
    return out;
}
}  // namespace

std::vector<MatchSummary> RecentMatches(uint32 limit)
{
    State& st = S();
    std::lock_guard<std::mutex> guard(st.lock);
    std::vector<MatchSummary> out;
    for (auto it = st.history.rbegin(); it != st.history.rend() && out.size() < limit; ++it)
        out.push_back(MatchSummaryOf(st, *it));
    return out;
}

std::vector<MatchSummary> RecentMatchesOf(uint32 teamId, uint32 limit)
{
    State& st = S();
    std::lock_guard<std::mutex> guard(st.lock);
    std::vector<MatchSummary> out;
    for (auto it = st.history.rbegin(); it != st.history.rend() && out.size() < limit; ++it)
        if (it->teamA == teamId || it->teamB == teamId)
            out.push_back(MatchSummaryOf(st, *it));
    return out;
}

std::vector<std::string> Console(std::string const& args)
{
    State& st = S();
    std::string command = ArenaNames::Lower(args);
    while (!command.empty() && command.front() == ' ')
        command.erase(command.begin());
    while (!command.empty() && command.back() == ' ')
        command.pop_back();

    std::vector<std::string> lines;
    if (!gEnabled.load())
    {
        lines.push_back("Arena ladder is off (AiPlayerbot.ArenaTeams.Enable = 0).");
        return lines;
    }

    std::lock_guard<std::mutex> guard(st.lock);
    int64 const now = UtcNow();

    if (command == "form")
    {
        st.forceFormation = true;
        lines.push_back("A team formation and check run will happen on the next world tick.");
    }
    else if (command == "match")
    {
        st.forceMatch = true;
        lines.push_back("A scheduler pass (clock, roll and world load skipped) will run on the next world tick.");
    }
    else if (command == "snapshot")
    {
        st.forceSnapshot = true;
        lines.push_back("The snapshot will be written on the next world tick.");
    }
    else if (command == "matches")
    {
        lines.push_back("Live: " + std::to_string(st.live.size()) + ", newest results:");
        uint32 shown = 0;
        for (auto it = st.history.rbegin(); it != st.history.rend() && shown < 15; ++it, ++shown)
        {
            MatchSummary const m = MatchSummaryOf(st, *it);
            static char const* const WINNER[] = { "draw", "first team", "second team", "voided" };
            lines.push_back("match " + std::to_string(it->id) + ": " + m.teamA + " (" + std::to_string(m.ratingA) + " to " +
                std::to_string(m.ratingANew) + ") against " + m.teamB + " (" + std::to_string(m.ratingB) + " to " +
                std::to_string(m.ratingBNew) + "), " + WINNER[std::min<uint8>(m.winner, 3)] + ", " + std::to_string(m.seconds) + " s");
        }
    }
    else if (command.empty() || command == "show")
    {
        ArenaMath::LocalTime const local = OwnerNow(st.cfg, now);
        char clock[96];
        snprintf(clock, sizeof(clock), "%02u:%02u (weekday %u)", local.hour, local.minute, local.weekday);
        lines.push_back("Arena ladder: owner clock " + std::string(clock) + ", activity " +
            std::to_string(ArenaMath::ActivityWeightPercent(local)) + " percent, " + std::to_string(st.teams.size()) +
            " teams, " + std::to_string(st.live.size()) + " live match(es), teams from level " + std::to_string(st.cfg.minLevel));
        std::vector<Team const*> teams;
        for (auto const& entry : st.teams)
            teams.push_back(&entry.second);
        std::sort(teams.begin(), teams.end(), [](Team const* a, Team const* b)
        {
            return a->status != b->status ? a->status < b->status : a->rating != b->rating ? a->rating > b->rating : a->id < b->id;
        });
        for (Team const* team : teams)
            lines.push_back(TeamLine(st, *team));
        if (teams.empty())
            lines.push_back("No teams yet (the first formation run is " + std::to_string(st.untilFormationMs / MS_PER_SECOND) + " s away).");
    }
    else
    {
        lines.push_back("Usage: .playerbots ladder [show|matches|form|match|snapshot]");
    }
    return lines;
}
}  // namespace ArenaLadder
