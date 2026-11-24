/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE). See BotLfm.h.
 */

#include "BotLfm.h"

#include "BotVoice.h"
#include "Config.h"
#include "DBCStores.h"
#include "Group.h"
#include "LFGMgr.h"
#include "LfgActions.h"
#include "LfgFill.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RandomPlayerbotMgr.h"
#include "Timer.h"
#include "TravelToGroupAction.h"
#include "WorldSession.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
    uint32 const SECONDS_PER_MINUTE = 60;
    // One bookkeeping pass every this many seconds.
    time_t const PASS_SECONDS = 5;
    // LFG's own rule: nobody queues for a dungeon below this level, and neither does a bot lead one.
    uint32 const MIN_LEADER_LEVEL = 15;
    uint32 const GROUP_SIZE = 5;
    // A bot invited and not in the group after this long is dropped from the invite list.
    time_t const INVITE_EXPIRES = 60;
    // A bot that answered is invited after this pause, as a player would take a moment to read the line.
    uint32 const ANSWER_DELAY_MIN = 3;
    uint32 const ANSWER_DELAY_MAX = 8;
    // The gap between two invites of bots.
    uint32 const INVITE_GAP_MIN = 6;
    uint32 const INVITE_GAP_MAX = 14;
    // A player asked at most this often by one party; "inv" in the channel counts for a post this recent.
    time_t const HUMAN_RETRY = 5 * SECONDS_PER_MINUTE;
    time_t const POST_FRESH = 3 * SECONDS_PER_MINUTE;
    // A leader that cannot start retries after this long.
    time_t const START_RETRY = 5 * SECONDS_PER_MINUTE;
    // A party that walked in is forgotten after this long.
    time_t const INSIDE_FORGET = 2 * 60 * SECONDS_PER_MINUTE;
    uint32 const MAX_PARTY_LIFETIME_FACTOR = 3;
    // StartParty tries tanks as the leader for this many attempts before it takes anyone.
    uint32 const TANK_LEADER_ATTEMPTS = 3;
    // The Stockade's entrance lies inside Stormwind: a Horde party that walked there would be killed by the guards.
    uint32 const STOCKADE_MAP = 34;

    enum Role
    {
        ROLE_TANK = 0,
        ROLE_HEALER = 1,
        ROLE_DAMAGE = 2,
        ROLE_COUNT = 3
    };
    uint32 const ROLE_WANTED[ROLE_COUNT] = { 1, 1, 3 };

    char const* RoleName(int role)
    {
        return role == ROLE_TANK ? "tank" : role == ROLE_HEALER ? "healer" : "dps";
    }

    int RoleOfMask(uint32 mask)
    {
        if (mask & lfg::PLAYER_ROLE_TANK)
            return ROLE_TANK;
        if (mask & lfg::PLAYER_ROLE_HEALER)
            return ROLE_HEALER;
        return ROLE_DAMAGE;
    }

    struct Settings
    {
        bool enable = true;
        bool requirePlayer = true;
        uint32 maxParties = 1;
        uint32 minIntervalS = 30 * SECONDS_PER_MINUTE;
        uint32 maxIntervalS = 90 * SECONDS_PER_MINUTE;
        uint32 recruitS = 8 * SECONDS_PER_MINUTE;
        uint32 minMembers = 4;
        uint32 levelWindow = 6;
        float radius = 400.0f;
        uint32 stragglerS = 3 * SECONDS_PER_MINUTE;
        uint32 postGapMinS = 90;
        uint32 postGapMaxS = 200;
        uint32 maxPosts = 4;
        std::set<uint32> dungeons;
    };

    std::set<uint32> ParseIdList(std::string const& list)
    {
        std::set<uint32> ids;
        std::istringstream in(list);
        std::string token;
        while (std::getline(in, token, ','))
        {
            uint32 const id = uint32(std::strtoul(token.c_str(), nullptr, 10));
            if (id)
                ids.insert(id);
        }
        return ids;
    }

    Settings const& Cfg()
    {
        static Settings const settings = []
        {
            Settings s;
            s.enable = sConfigMgr->GetBoolDefault("AiPlayerbot.BotLfm.Enable", true);
            s.requirePlayer = sConfigMgr->GetBoolDefault("AiPlayerbot.BotLfm.RequirePlayer", true);
            s.maxParties = uint32(std::max(0, sConfigMgr->GetIntDefault("AiPlayerbot.BotLfm.MaxParties", 1)));
            s.minIntervalS = uint32(std::max(1, sConfigMgr->GetIntDefault("AiPlayerbot.BotLfm.MinIntervalMinutes", 30))) * SECONDS_PER_MINUTE;
            s.maxIntervalS = std::max(s.minIntervalS,
                uint32(std::max(1, sConfigMgr->GetIntDefault("AiPlayerbot.BotLfm.MaxIntervalMinutes", 90))) * SECONDS_PER_MINUTE);
            s.recruitS = uint32(std::max(1, sConfigMgr->GetIntDefault("AiPlayerbot.BotLfm.RecruitMinutes", 8))) * SECONDS_PER_MINUTE;
            s.minMembers = uint32(std::max(2, std::min(5, sConfigMgr->GetIntDefault("AiPlayerbot.BotLfm.MinMembers", 4))));
            s.levelWindow = uint32(std::max(0, sConfigMgr->GetIntDefault("AiPlayerbot.BotLfm.LevelWindow", 6)));
            s.radius = sConfigMgr->GetFloatDefault("AiPlayerbot.BotLfm.RecruitRadius", 400.0f);
            s.stragglerS = uint32(std::max(1, sConfigMgr->GetIntDefault("AiPlayerbot.BotLfm.StragglerSeconds", 180)));
            s.postGapMinS = uint32(std::max(10, sConfigMgr->GetIntDefault("AiPlayerbot.BotLfm.PostGapMinSeconds", 90)));
            s.postGapMaxS = std::max(s.postGapMinS, uint32(sConfigMgr->GetIntDefault("AiPlayerbot.BotLfm.PostGapMaxSeconds", 200)));
            s.maxPosts = uint32(std::max(1, sConfigMgr->GetIntDefault("AiPlayerbot.BotLfm.MaxPosts", 4)));
            s.dungeons = ParseIdList(sConfigMgr->GetStringDefault("AiPlayerbot.BotLfm.Dungeons", "4,1,10,16,272,26,273,20,24,8,12,163,6,14,34,36,38,171,170,311,312,209,305"));
            return s;
        }();
        return settings;
    }

    struct Answer
    {
        uint32 botLow;
        int role;
        time_t inviteAt;
    };

    struct Party
    {
        uint32 leader = 0;
        uint32 dungeonId = 0;
        uint32 mapId = 0;
        TeamId team = TEAM_ALLIANCE;
        BotLfm::Phase phase = BotLfm::Phase::RECRUIT;
        time_t startedAt = 0;
        time_t phaseAt = 0;
        uint32 phaseSinceMs = 0;
        time_t nextPostAt = 0;
        time_t nextInviteAt = 0;
        time_t lastPostAt = 0;
        uint32 posts = 0;
        uint32 waitPosts = 0;
        uint32 lastWaitPostMs = 0;
        std::vector<std::pair<uint32, time_t>> invitedBots;  // invited, not yet seen in the group
        std::vector<Answer> answers;                          // bots that whispered, to be invited
        std::unordered_map<uint64, time_t> humansAsked;       // players handled, with the time
    };

    std::recursive_mutex lock;
    std::vector<Party> parties;
    std::unordered_set<uint32> leaders;
    std::unordered_set<uint32> members;
    std::atomic<uint32> partyCount{ 0 };
    time_t lastPassAt = 0;
    time_t nextStartAt = 0;

    bool IsHuman(Player* player)
    {
        return player && player->GetSession() && !player->GetSession()->IsBot() && !GET_PLAYERBOT_AI(player);
    }

    bool InLevelRange(lfg::LFGDungeonData const& dungeon, uint32 level)
    {
        return level >= uint32(dungeon.minlevel) && (!dungeon.maxlevel || level <= uint32(dungeon.maxlevel));
    }

    bool InCapital(Player* bot)
    {
        AreaTableEntry const* zone = GetAreaEntryByAreaID(bot->GetZoneId());
        return zone && (zone->m_flags & AREA_FLAG_CAPITAL);
    }

    // A random bot that may lead or join a party now.
    bool FreeBot(Player* bot, std::unordered_set<uint32> const& busy)
    {
        if (!bot || !bot->IsInWorld() || !sRandomPlayerbotMgr->IsRandomBot(bot))
            return false;

        PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
        if (!botAI || botAI->GetMaster() == bot || busy.count(bot->GetGUIDLow()))
            return false;

        if (bot->GetGroup() || bot->GetGroupInvite() || !bot->IsAlive() || bot->IsInCombat() || bot->IsInFlight() ||
            bot->IsBeingTeleported() || bot->InBattleground() || bot->InBattlegroundQueue() || bot->InArena())
            return false;

        Map* map = bot->GetMap();
        if (!map || map->Instanceable())
            return false;

        if (bot->HasAura(lfg::LFG_SPELL_DUNGEON_DESERTER) || bot->HasAura(lfg::LFG_SPELL_DUNGEON_COOLDOWN))
            return false;

        return sLFGMgr->GetState(bot->GetGUID()) == lfg::LFG_STATE_NONE;
    }

    std::unordered_set<uint32> BusyBots()
    {
        std::unordered_set<uint32> busy = leaders;
        busy.insert(members.begin(), members.end());
        return busy;
    }

    // The group of the party: its leader's group once the first member joined, else only the leader.
    Group* GroupOf(Player* leader) { return leader ? leader->GetGroup() : nullptr; }

    uint32 CountMembers(Player* leader)
    {
        Group* group = GroupOf(leader);
        return group ? group->GetMembersCount() : 1;
    }

    void CountRoles(Player* leader, uint32 (&have)[ROLE_COUNT])
    {
        have[ROLE_TANK] = have[ROLE_HEALER] = have[ROLE_DAMAGE] = 0;
        auto add = [&](Player* member)
        {
            PlayerbotAI* ai = GET_PLAYERBOT_AI(member);
            ++have[ai && !ai->IsRealPlayer() ? RoleOfMask(LfgGetBotRoles(member)) : ROLE_DAMAGE];
        };

        Group* group = GroupOf(leader);
        if (!group)
        {
            add(leader);
            return;
        }

        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            if (Player* member = ref->GetSource())
                add(member);
    }

    // The first role the group still lacks; -1 when it lacks none.
    int MissingRole(Player* leader, uint32 pendingExtra[ROLE_COUNT])
    {
        uint32 have[ROLE_COUNT];
        CountRoles(leader, have);
        for (int role = 0; role < ROLE_COUNT; ++role)
            if (have[role] + pendingExtra[role] < ROLE_WANTED[role])
                return role;
        return -1;
    }

    bool InvitePlayer(Player* inviter, Player* target)
    {
        if (!inviter || !target || inviter == target || target->GetGroup() || target->GetGroupInvite())
            return false;

        Group* group = inviter->GetGroup();
        if (!group)
            group = inviter->GetGroupInvite();

        if (group)
        {
            if (group->IsFull() || group->GetLeaderGUID() != inviter->GetGUID() || !group->AddInvite(target))
                return false;
        }
        else
        {
            group = new Group();
            if (!group->AddLeaderInvite(inviter) || !group->AddInvite(target))
            {
                delete group;
                return false;
            }
        }

        target->GetSession()->SendGroupInviteNotification(inviter->GetName(), "", false);
        return true;
    }

    Party* FindParty(uint32 leaderLow)
    {
        for (Party& party : parties)
            if (party.leader == leaderLow)
                return &party;
        return nullptr;
    }

    void Rebuild()
    {
        leaders.clear();
        members.clear();
        for (Party const& party : parties)
        {
            leaders.insert(party.leader);
            for (auto const& invited : party.invitedBots)
                members.insert(invited.first);
            if (Player* leader = sRandomPlayerbotMgr->GetPlayerBot(party.leader))
                if (Group* group = GroupOf(leader))
                    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
                    {
                        Player* member = ref->GetSource();
                        PlayerbotAI* ai = member ? GET_PLAYERBOT_AI(member) : nullptr;
                        if (member && member != leader && ai && !ai->IsRealPlayer())
                            members.insert(member->GetGUIDLow());
                    }
        }
        partyCount.store(uint32(parties.size()), std::memory_order_relaxed);
    }

    void Say(Player* bot, std::string const& key, std::string const& fallback,
             std::map<std::string, std::string> const& placeholders, BotVoice::Channel channel, Player* to = nullptr)
    {
        BotVoice::Say(GET_PLAYERBOT_AI(bot), key, fallback, placeholders, channel, BotVoice::Priority::NOTICE, 100, to);
    }

    std::map<std::string, std::string> Placeholders(lfg::LFGDungeonData const& dungeon, int role)
    {
        std::map<std::string, std::string> placeholders = { { "%dungeon", dungeon.name } };
        if (role >= 0)
            placeholders["%role"] = RoleName(role);
        return placeholders;
    }

    // One LFM line in the LookingForGroup channel, and in General while the leader stands in a capital.
    void Post(Party& party, Player* leader, lfg::LFGDungeonData const& dungeon, time_t now)
    {
        uint32 none[ROLE_COUNT] = { 0, 0, 0 };
        int const role = MissingRole(leader, none);
        auto placeholders = Placeholders(dungeon, role < 0 ? ROLE_DAMAGE : role);
        placeholders["%level"] = std::to_string(leader->getLevel());
        Say(leader, "voice_lfm", "LFM %dungeon, need %role", placeholders, BotVoice::Channel::LFG);
        if (InCapital(leader))
            Say(leader, "voice_lfm", "LFM %dungeon, need %role", placeholders, BotVoice::Channel::GENERAL);

        ++party.posts;
        party.lastPostAt = now;
        party.nextPostAt = now + urand(Cfg().postGapMinS, Cfg().postGapMaxS);
        SF_LOG_DEBUG("playerbots", "[BOTLFM] %s posts for %s (post %u)", leader->GetName().c_str(), dungeon.name.c_str(),
                     party.posts);
    }

    // The party ends: its group is disbanded when it never left the recruiting, the leader and bots go back to roaming.
    void EndParty(Party& party, Player* leader, char const* why, bool disband)
    {
        SF_LOG_INFO("playerbots", "[BOTLFM] party of %u for dungeon %u ends: %s", party.leader, party.dungeonId, why);
        if (!leader)
            return;

        if (disband)
        {
            if (Group* group = GroupOf(leader))
                group->Disband(true);
            else
                leader->UninviteFromGroup();
        }
    }

    // Whether a player may be invited to the party: faction, level band, free, awake.
    bool HumanFits(Party const& party, Player* leader, Player* player, lfg::LFGDungeonData const& dungeon, time_t now)
    {
        if (!IsHuman(player) || player->GetTeamId() != party.team || player->GetGroup() || player->GetGroupInvite())
            return false;

        if (!player->IsAlive() || player->IsInCombat() || player->isAFK() || player->isDND() || player->InBattleground() ||
            player->InBattlegroundQueue() || player->GetMap()->Instanceable())
            return false;

        uint32 const level = player->getLevel();
        if (!InLevelRange(dungeon, level) ||
            uint32(std::abs(int32(level) - int32(leader->getLevel()))) > Cfg().levelWindow)
            return false;

        auto const asked = party.humansAsked.find(player->GetGUID());
        return asked == party.humansAsked.end() || now - asked->second >= HUMAN_RETRY;
    }

    // A player asks to join: invited when it fits, answered either way with a whisper.
    bool AnswerHuman(Party& party, Player* player, time_t now)
    {
        Player* leader = sRandomPlayerbotMgr->GetPlayerBot(party.leader);
        lfg::LFGDungeonData const* dungeon = sLFGMgr->GetLFGDungeon(party.dungeonId);
        if (!leader || !dungeon || party.phase != BotLfm::Phase::RECRUIT || !IsHuman(player))
            return false;

        party.humansAsked[player->GetGUID()] = now;
        auto placeholders = Placeholders(*dungeon, -1);
        if (CountMembers(leader) + party.invitedBots.size() >= GROUP_SIZE)
        {
            Say(leader, "voice_lfm_full", "Sorry, the group is full", placeholders, BotVoice::Channel::WHISPER, player);
            return true;
        }

        if (!HumanFits(party, leader, player, *dungeon, now) || !InvitePlayer(leader, player))
        {
            Say(leader, "voice_lfm_refuse", "Sorry, I can't take you for this one", placeholders, BotVoice::Channel::WHISPER,
                player);
            return true;
        }

        Say(leader, "voice_lfm_invited", "Inviting you, we meet at the %dungeon entrance", placeholders,
            BotVoice::Channel::WHISPER, player);
        SF_LOG_INFO("playerbots", "[BOTLFM] %s invites player %s for %s", leader->GetName().c_str(), player->GetName().c_str(),
                    dungeon->name.c_str());
        return true;
    }

    std::string Lower(std::string const& text)
    {
        std::string out;
        for (char c : text)
            out += char(std::tolower(static_cast<unsigned char>(c)));
        return out;
    }

    // "inv", "invite me", "inv pls", "+": a request to join.
    bool IsJoinRequest(std::string const& message, bool strict)
    {
        std::string text = Lower(message);
        while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())))
            text.pop_back();

        static char const* const exact[] = { "inv", "invite", "+", "inv pls", "inv please", "invite me", "inv me", "me", "i can" };
        for (char const* word : exact)
            if (text == word)
                return true;

        if (strict)
            return false;

        return text.find("inv") == 0 || text.find(" inv") != std::string::npos || text.find("invite") != std::string::npos;
    }

    void Recruit(Party& party, Player* leader, lfg::LFGDungeonData const& dungeon, time_t now)
    {
        Settings const& settings = Cfg();
        uint32 const joined = CountMembers(leader);

        // Invites that did not turn into members expire.
        Group* group = GroupOf(leader);
        for (auto itr = party.invitedBots.begin(); itr != party.invitedBots.end();)
        {
            Player* invited = sRandomPlayerbotMgr->GetPlayerBot(itr->first);
            bool const inGroup = invited && group && group->IsMember(invited->GetGUID());
            bool const stale = now - itr->second > INVITE_EXPIRES;
            if (inGroup || !invited || stale)
            {
                if (stale && invited && !inGroup)
                {
                    if (Group* pending = invited->GetGroupInvite())
                        pending->RemoveInvite(invited);
                }
                itr = party.invitedBots.erase(itr);
            }
            else
                ++itr;
        }

        if (now >= party.nextPostAt && party.posts < settings.maxPosts && joined + party.invitedBots.size() < GROUP_SIZE)
            Post(party, leader, dungeon, now);

        // Bots that answered are invited after their pause.
        for (auto itr = party.answers.begin(); itr != party.answers.end();)
        {
            if (now < itr->inviteAt)
            {
                ++itr;
                continue;
            }

            Player* bot = sRandomPlayerbotMgr->GetPlayerBot(itr->botLow);
            std::unordered_set<uint32> const busy = BusyBots();
            if (bot && FreeBot(bot, busy) && joined + party.invitedBots.size() < GROUP_SIZE && InvitePlayer(leader, bot))
            {
                party.invitedBots.push_back({ itr->botLow, now });
                members.insert(itr->botLow);
                SF_LOG_DEBUG("playerbots", "[BOTLFM] %s invites bot %s (%s)", leader->GetName().c_str(), bot->GetName().c_str(),
                             RoleName(itr->role));
            }
            itr = party.answers.erase(itr);
        }

        // A free bot of the right band answers the post with a whisper.
        if (now >= party.nextInviteAt && party.posts > 0 &&
            joined + party.invitedBots.size() + party.answers.size() < GROUP_SIZE)
        {
            party.nextInviteAt = now + urand(INVITE_GAP_MIN, INVITE_GAP_MAX);

            uint32 pending[ROLE_COUNT] = { 0, 0, 0 };
            for (Answer const& answer : party.answers)
                ++pending[answer.role];
            for (auto const& invited : party.invitedBots)
                if (Player* bot = sRandomPlayerbotMgr->GetPlayerBot(invited.first))
                    ++pending[RoleOfMask(LfgGetBotRoles(bot))];
            int const missing = MissingRole(leader, pending);

            std::unordered_set<uint32> const busy = BusyBots();
            std::vector<Player*> candidates;
            for (Player* bot : sRandomPlayerbotMgr->GetPlayerBotsSnapshot())
            {
                if (!FreeBot(bot, busy) || bot->GetTeamId() != party.team || bot->GetMapId() != leader->GetMapId() ||
                    !InLevelRange(dungeon, bot->getLevel()) ||
                    uint32(std::abs(int32(bot->getLevel()) - int32(leader->getLevel()))) > settings.levelWindow ||
                    bot->GetExactDist2d(leader) > settings.radius)
                    continue;

                if (missing >= 0 && RoleOfMask(LfgGetBotRoles(bot)) != missing)
                    continue;

                candidates.push_back(bot);
            }

            if (!candidates.empty())
            {
                Player* bot = candidates[urand(0, uint32(candidates.size() - 1))];
                int const role = RoleOfMask(LfgGetBotRoles(bot));
                Say(bot, "voice_lfm_answer", "inv, %role", Placeholders(dungeon, role), BotVoice::Channel::WHISPER, leader);
                party.answers.push_back({ bot->GetGUIDLow(), role, now + urand(ANSWER_DELAY_MIN, ANSWER_DELAY_MAX) });
            }
        }
    }

    // The recruiting is over: go, or give up.
    void FinishRecruit(Party& party, Player* leader, lfg::LFGDungeonData const& dungeon, time_t now, bool& end)
    {
        Settings const& settings = Cfg();
        uint32 const joined = CountMembers(leader);
        bool const full = joined >= GROUP_SIZE;
        bool const timeUp = now - party.startedAt >= time_t(settings.recruitS);
        if (!full && !timeUp)
            return;

        if (joined < settings.minMembers)
        {
            Say(leader, joined > 1 ? "voice_lfm_cancel_party" : "voice_lfm_cancel", "Nobody else came, never mind",
                Placeholders(dungeon, -1), joined > 1 ? BotVoice::Channel::PARTY : BotVoice::Channel::LFG);
            EndParty(party, leader, "not enough members", true);
            end = true;
            return;
        }

        party.phase = BotLfm::Phase::TRAVEL;
        party.phaseAt = now;
        party.phaseSinceMs = getMSTime();
        Say(leader, "voice_lfm_go", "Let's go, meet me at the %dungeon entrance", Placeholders(dungeon, -1),
            BotVoice::Channel::PARTY);
        SF_LOG_INFO("playerbots", "[BOTLFM] %s leads %u to %s", leader->GetName().c_str(), joined, dungeon.name.c_str());
    }

    void StartParty(Settings const& settings, time_t now, std::vector<Player*> const& bots)
    {
        std::unordered_set<uint32> const busy = BusyBots();
        std::vector<Player*> pool;
        for (Player* bot : bots)
            if (bot->getLevel() >= MIN_LEADER_LEVEL && FreeBot(bot, busy))
                pool.push_back(bot);
        if (pool.empty())
            return;

        // A tank leads when one is free (it pulls and holds, which a bot-led run inside the dungeon needs); the first
        // attempts take one, the later ones anyone.
        std::vector<Player*> tanks;
        for (Player* bot : pool)
            if (RoleOfMask(LfgGetBotRoles(bot)) == ROLE_TANK)
                tanks.push_back(bot);

        // A few leaders are tried so one without a dungeon in its band does not block the start.
        for (uint32 attempt = 0; attempt < 6; ++attempt)
        {
            std::vector<Player*> const& from = attempt < TANK_LEADER_ATTEMPTS && !tanks.empty() ? tanks : pool;
            Player* leader = from[urand(0, uint32(from.size() - 1))];
            std::vector<lfg::LFGDungeonData const*> options;
            for (uint32 const id : settings.dungeons)
            {
                lfg::LFGDungeonData const* dungeon = sLFGMgr->GetLFGDungeon(id);
                if (dungeon && dungeon->type == lfg::LFG_TYPE_DUNGEON && !dungeon->seasonal && !LfgFill::DungeonCoolingDown(id) &&
                    InLevelRange(*dungeon, leader->getLevel()) && GroupTravel::HasEntrance(dungeon->map) &&
                    !(dungeon->map == STOCKADE_MAP && leader->GetTeamId() == TEAM_HORDE))
                    options.push_back(dungeon);
            }
            if (options.empty())
                continue;

            lfg::LFGDungeonData const* dungeon = options[urand(0, uint32(options.size() - 1))];
            Party party;
            party.leader = leader->GetGUIDLow();
            party.dungeonId = dungeon->id;
            party.mapId = dungeon->map;
            party.team = leader->GetTeamId();
            party.startedAt = now;
            party.phaseAt = now;
            party.phaseSinceMs = getMSTime();
            party.nextPostAt = now;
            party.nextInviteAt = now + urand(INVITE_GAP_MIN, INVITE_GAP_MAX);
            parties.push_back(party);
            SF_LOG_INFO("playerbots", "[BOTLFM] %s (level %u) starts a group for %s", leader->GetName().c_str(), leader->getLevel(),
                        dungeon->name.c_str());
            return;
        }
    }
}

namespace BotLfm
{
    bool Enabled() { return Cfg().enable; }

    uint32 StragglerSeconds() { return Cfg().stragglerS; }

    bool IsLeader(uint32 botLowGuid)
    {
        if (!partyCount.load(std::memory_order_relaxed))
            return false;

        std::lock_guard<std::recursive_mutex> guard(lock);
        return leaders.count(botLowGuid) != 0;
    }

    bool IsMember(uint32 botLowGuid)
    {
        if (!partyCount.load(std::memory_order_relaxed))
            return false;

        std::lock_guard<std::recursive_mutex> guard(lock);
        return members.count(botLowGuid) != 0;
    }

    bool InsideParty(Player* bot)
    {
        if (!partyCount.load(std::memory_order_relaxed))
            return false;

        std::lock_guard<std::recursive_mutex> guard(lock);
        uint32 const low = bot->GetGUIDLow();
        Party* party = nullptr;
        if (leaders.count(low))
            party = FindParty(low);
        else if (members.count(low))
        {
            Group* group = bot->GetGroup();
            Player* leader = group ? ObjectAccessor::FindPlayer(group->GetLeaderGUID()) : nullptr;
            party = leader ? FindParty(leader->GetGUIDLow()) : nullptr;
        }
        return party && party->phase == Phase::INSIDE;
    }

    bool GetParty(uint32 leaderLowGuid, PartyInfo& info)
    {
        std::lock_guard<std::recursive_mutex> guard(lock);
        Party* party = FindParty(leaderLowGuid);
        if (!party)
            return false;

        info.dungeonId = party->dungeonId;
        info.mapId = party->mapId;
        info.phase = party->phase;
        info.phaseSinceMs = party->phaseSinceMs;
        info.waitPosts = party->waitPosts;
        info.lastWaitPostMs = party->lastWaitPostMs;
        return true;
    }

    void SetPhase(uint32 leaderLowGuid, Phase phase)
    {
        std::lock_guard<std::recursive_mutex> guard(lock);
        if (Party* party = FindParty(leaderLowGuid))
        {
            party->phase = phase;
            party->phaseAt = time(nullptr);
            party->phaseSinceMs = getMSTime();
            party->waitPosts = 0;
        }
    }

    void NotePostedWait(uint32 leaderLowGuid)
    {
        std::lock_guard<std::recursive_mutex> guard(lock);
        if (Party* party = FindParty(leaderLowGuid))
        {
            ++party->waitPosts;
            party->lastWaitPostMs = getMSTime();
        }
    }

    bool OnWhisper(Player* bot, Player* from, std::string const& message)
    {
        if (!partyCount.load(std::memory_order_relaxed) || !bot || !from || !IsJoinRequest(message, false))
            return false;

        std::lock_guard<std::recursive_mutex> guard(lock);
        Party* party = FindParty(bot->GetGUIDLow());
        return party && AnswerHuman(*party, from, time(nullptr));
    }

    void OnChannelChat(Player* from, std::string const& message)
    {
        if (!partyCount.load(std::memory_order_relaxed) || !from || !IsHuman(from) || !IsJoinRequest(message, true))
            return;

        time_t const now = time(nullptr);
        std::lock_guard<std::recursive_mutex> guard(lock);
        for (Party& party : parties)
        {
            if (party.phase != Phase::RECRUIT || party.posts == 0 || now - party.lastPostAt > POST_FRESH ||
                party.team != from->GetTeamId())
                continue;

            if (AnswerHuman(party, from, now))
                return;
        }
    }

    void Update()
    {
        time_t const now = time(nullptr);
        if (lastPassAt && now < lastPassAt + PASS_SECONDS)
            return;
        lastPassAt = now;

        Settings const& settings = Cfg();
        if (!settings.enable || !settings.maxParties)
            return;

        std::lock_guard<std::recursive_mutex> guard(lock);

        for (auto itr = parties.begin(); itr != parties.end();)
        {
            Party& party = *itr;
            Player* leader = sRandomPlayerbotMgr->GetPlayerBot(party.leader);
            lfg::LFGDungeonData const* dungeon = sLFGMgr->GetLFGDungeon(party.dungeonId);
            bool end = !leader || !leader->IsInWorld() || !dungeon;

            if (!end && party.phase != Phase::INSIDE)
            {
                if (leader->GetMap() && leader->GetMap()->Instanceable())
                {
                    // Walked in: the party is handed over to the dungeon logic.
                    party.phase = Phase::INSIDE;
                    party.phaseAt = now;
                    SF_LOG_INFO("playerbots", "[BOTLFM] %s's party entered %s with %u", leader->GetName().c_str(),
                               dungeon->name.c_str(), CountMembers(leader));
                }
                else if (!leader->IsAlive() && party.phase == Phase::RECRUIT)
                {
                    EndParty(party, leader, "the leader died", true);
                    end = true;
                }
                else if (now - party.startedAt > time_t(settings.recruitS) * MAX_PARTY_LIFETIME_FACTOR + SECONDS_PER_MINUTE * 15)
                {
                    EndParty(party, leader, "took too long", false);
                    end = true;
                }
                else if (party.phase == Phase::RECRUIT)
                {
                    Recruit(party, leader, *dungeon, now);
                    FinishRecruit(party, leader, *dungeon, now, end);
                }
                else if (CountMembers(leader) < 2 && !party.invitedBots.size() && now - party.phaseAt > 30)
                {
                    // Everyone left on the way.
                    EndParty(party, leader, "the group dissolved", false);
                    end = true;
                }
            }
            else if (!end && now - party.phaseAt > INSIDE_FORGET)
                end = true;
            else if (!end && !(leader->GetMap() && leader->GetMap()->Instanceable()) && now - party.phaseAt > 30)
                end = true;  // came back out

            if (end)
                itr = parties.erase(itr);
            else
                ++itr;
        }

        if (parties.size() < settings.maxParties && (!nextStartAt || now >= nextStartAt))
        {
            bool const playerOnline = !sRandomPlayerbotMgr->GetPlayers().empty();
            if (!nextStartAt)
                nextStartAt = now + urand(settings.minIntervalS, settings.maxIntervalS);
            else if (playerOnline || !settings.requirePlayer)
            {
                size_t const before = parties.size();
                StartParty(settings, now, sRandomPlayerbotMgr->GetPlayerBotsSnapshot());
                nextStartAt = now + (parties.size() > before ? urand(settings.minIntervalS, settings.maxIntervalS) : START_RETRY);
            }
            else
                nextStartAt = now + START_RETRY;
        }

        Rebuild();
    }
}
