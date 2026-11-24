/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 *
 * The guild life of the random bots (see BotGuilds.h). The ideas (random guilds from the guild name table, a guild's
 * recruiting leader, a bot that promotes and demotes, a bot that leaves a large guild) are AzerothCore mod-playerbots'
 * (Mgr/Guild/PlayerbotGuildMgr, Ai/Base/Actions/GuildManagementActions); this file is their world-thread rewrite for the
 * guild handlers of 5.4.8.
 */

#include "BotGuilds.h"

#include "BotVoice.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "Guild.h"
#include "LikingStore.h"
#include "SocialLog.h"
#include "GuildMgr.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "RandomPlayerbotFactory.h"
#include "RandomPlayerbotMgr.h"
#include "WorldPacket.h"
#include "WorldSession.h"

#include <algorithm>
#include <ctime>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
    // What a guild charter costs in copper (the petition handler's own constant, PetitionsHandler.cpp).
    uint64 const GUILD_CHARTER_COST = 1000;
    uint32 const SECONDS_PER_DAY = 24 * 60 * 60;
    // The lowest level a random bot is invited into a guild at.
    uint32 const MIN_INVITE_LEVEL = 5;
    // The default guild ranks: 0 guild master, 1 officer, 2 veteran, 3 member, 4 initiate.
    uint8 const RANK_GUILD_MASTER = 0;
    // A pending invite is acted on this long after it was sent at the earliest, and given up after the expiry.
    uint32 const ACCEPT_DELAY_MIN_S = 15;
    uint32 const ACCEPT_DELAY_MAX_S = 90;
    uint32 const INVITE_EXPIRE_S = 5 * 60;
    // A human is offered a guild at most this often through the pass (a per-human cooldown applies besides).
    uint32 const PERCENT = 100;

    struct Settings
    {
        bool enable = true;
        uint32 intervalSeconds = 60;
        uint32 refreshSeconds = 5 * 60;
        uint32 maxGuilds = 20;
        uint32 maxMembers = 15;
        uint32 foundChance = 25;       // percent per pass
        uint32 minFoundLevel = 10;
        uint32 levelWindow = 10;
        uint32 invitesPerPass = 3;
        uint32 acceptChance = 80;      // percent of invited bots that join
        uint32 welcomeChance = 60;
        uint32 inactiveDays = 14;      // offline this long: kicked (half of it: an officer is demoted)
        uint32 promoteChance = 25;     // percent per guild per refresh
        uint32 maxPromoteRank = 2;     // promoted up to this rank (2 = veteran)
        uint32 leavePerDay = 5;        // percent of the members that leave per day
        bool inviteHumans = false;
        uint32 humanChance = 5;        // percent per pass per eligible human near a recruiting leader
        uint32 humanCooldownHours = 24;
        float humanRadius = 60.0f;
        uint32 humanMinLevel = 8;
    };

    Settings ReadSettings()
    {
        Settings s;
        s.enable = sConfigMgr->GetBoolDefault("AiPlayerbot.BotGuilds.Enable", true);
        s.intervalSeconds = uint32(std::max(10, sConfigMgr->GetIntDefault("AiPlayerbot.BotGuilds.IntervalSeconds", 60)));
        s.refreshSeconds = uint32(std::max(60, sConfigMgr->GetIntDefault("AiPlayerbot.BotGuilds.RefreshMinutes", 5) * 60));
        s.maxGuilds = uint32(std::max(0, sConfigMgr->GetIntDefault("AiPlayerbot.RandomBotGuildCount", 20)));
        s.maxMembers = uint32(std::max(2, sConfigMgr->GetIntDefault("AiPlayerbot.BotGuilds.MaxMembers", 15)));
        s.foundChance = uint32(std::min(100, std::max(0, sConfigMgr->GetIntDefault("AiPlayerbot.BotGuilds.FoundChance", 25))));
        s.minFoundLevel = uint32(std::max(1, sConfigMgr->GetIntDefault("AiPlayerbot.BotGuilds.MinFoundLevel", 10)));
        s.levelWindow = uint32(std::max(0, sConfigMgr->GetIntDefault("AiPlayerbot.BotGuilds.LevelWindow", 10)));
        s.invitesPerPass = uint32(std::max(1, sConfigMgr->GetIntDefault("AiPlayerbot.BotGuilds.InvitesPerPass", 3)));
        s.acceptChance = uint32(std::min(100, std::max(0, sConfigMgr->GetIntDefault("AiPlayerbot.BotGuilds.AcceptChance", 80))));
        s.welcomeChance = uint32(std::min(100, std::max(0, sConfigMgr->GetIntDefault("AiPlayerbot.BotGuilds.WelcomeChance", 60))));
        s.inactiveDays = uint32(std::max(1, sConfigMgr->GetIntDefault("AiPlayerbot.BotGuilds.InactiveDays", 14)));
        s.promoteChance = uint32(std::min(100, std::max(0, sConfigMgr->GetIntDefault("AiPlayerbot.BotGuilds.PromoteChance", 25))));
        s.maxPromoteRank = uint32(std::max(1, sConfigMgr->GetIntDefault("AiPlayerbot.BotGuilds.MaxPromoteRank", 2)));
        s.leavePerDay = uint32(std::min(100, std::max(0, sConfigMgr->GetIntDefault("AiPlayerbot.BotGuilds.LeavePercentPerDay", 5))));
        s.inviteHumans = sConfigMgr->GetBoolDefault("AiPlayerbot.BotGuilds.InviteHumans", false);
        s.humanChance = uint32(std::min(100, std::max(0, sConfigMgr->GetIntDefault("AiPlayerbot.BotGuilds.HumanInviteChance", 5))));
        s.humanCooldownHours = uint32(std::max(1, sConfigMgr->GetIntDefault("AiPlayerbot.BotGuilds.HumanInviteCooldownHours", 24)));
        s.humanRadius = sConfigMgr->GetFloatDefault("AiPlayerbot.BotGuilds.HumanInviteRadius", 60.0f);
        s.humanMinLevel = uint32(std::max(1, sConfigMgr->GetIntDefault("AiPlayerbot.BotGuilds.HumanMinLevel", 8)));
        return s;
    }

    // A guild led by a random bot.
    struct GuildInfo
    {
        uint32 leaderLow = 0;
    };

    struct MemberRow
    {
        uint32 guildId = 0;
        uint32 low = 0;
        uint8 rank = 0;
        uint8 level = 0;
        bool online = false;
        uint32 logoutTime = 0;
        bool isBot = false;  // a random bot; a human and an alt bot (a player's own) are not
        std::string name;
    };

    struct Pending
    {
        uint32 guildId = 0;
        time_t acceptAt = 0;
        time_t expireAt = 0;
        bool decline = false;
    };

    // World thread only, except the pending map (GuildAcceptAction asks whether an invite is ours).
    std::unordered_map<uint32, GuildInfo> botGuilds;  // by guild id
    std::mutex pendingLock;
    std::unordered_map<uint32, Pending> pending;       // by invitee low guid
    std::unordered_map<uint32, time_t> humanOffered;   // by human low guid
    time_t lastPassAt = 0;
    time_t lastRefreshAt = 0;
    bool registryLoaded = false;

    // BT-702: the two roster queries run on the database's async threads (they were synchronous on the world thread every
    // five minutes). The registry answer starts the member query; the member answer runs ManageGuilds with the settings of
    // the pass that asked. World thread only.
    QueryResultFuture registryFuture;
    QueryResultFuture membersFuture;
    bool registryPending = false;
    bool membersPending = false;
    time_t registryAskedAt = 0;
    time_t membersAskedAt = 0;
    Settings manageSettings;
    // An answer that has not come in this long is given up (the next refresh asks again).
    time_t const QUERY_GIVE_UP_SECONDS = 60;

    bool IsHuman(Player* player)
    {
        return player && player->GetSession() && !player->GetSession()->IsBot() && !GET_PLAYERBOT_AI(player);
    }

    Player* BotByLow(uint32 low) { return sRandomPlayerbotMgr->GetPlayerBot(low); }

    bool Usable(Player* bot)
    {
        return bot && bot->IsInWorld() && bot->GetSession() && bot->IsAlive() && !bot->IsInCombat() &&
            !bot->IsBeingTeleported() && !bot->InBattleground() && !bot->InBattlegroundQueue() && !bot->InArena();
    }

    // A random bot that may be invited: no guild and no open invite.
    bool GuildlessBot(Player* bot)
    {
        return Usable(bot) && sRandomPlayerbotMgr->IsRandomBot(bot) && GET_PLAYERBOT_AI(bot) && !bot->GetGuildId() &&
            !bot->GetGuildIdInvited() && bot->getLevel() >= MIN_INVITE_LEVEL;
    }

    // The online, usable leader of a guild.
    Player* OnlineLeader(GuildInfo const& info)
    {
        Player* leader = BotByLow(info.leaderLow);
        return Usable(leader) && GET_PLAYERBOT_AI(leader) ? leader : nullptr;
    }

    void Say(Player* bot, std::string const& key, std::string const& fallback,
             std::map<std::string, std::string> const& placeholders, BotVoice::Channel channel, Player* whisperTo = nullptr)
    {
        if (PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot))
            BotVoice::Say(botAI, key, fallback, placeholders, channel, BotVoice::Priority::NOTICE, PERCENT, whisperTo);
    }

    uint32 PendingFor(uint32 guildId)
    {
        std::lock_guard<std::mutex> guard(pendingLock);
        uint32 count = 0;
        for (auto const& entry : pending)
            if (entry.second.guildId == guildId)
                ++count;
        return count;
    }

    // The guilds whose leader is a random bot, from the characters tables: asked now, applied when the answer is in.
    void AskRegistry(time_t now)
    {
        lastRefreshAt = now;
        registryAskedAt = now;
        registryPending = true;
        registryFuture = CharacterDatabase.AsyncQuery(
            "SELECT g.guildid, g.leaderguid, c.account FROM guild g JOIN characters c ON c.guid = g.leaderguid");
    }

    void ApplyRegistry(QueryResult result)
    {
        std::unordered_map<uint32, GuildInfo> found;
        if (result)
        {
            do
            {
                Field* fields = result->Fetch();
                if (sPlayerbotAIConfig->IsInRandomAccountList(fields[2].GetUInt32()))
                    found[fields[0].GetUInt32()].leaderLow = fields[1].GetUInt32();
            } while (result->NextRow());
        }

        botGuilds.swap(found);
        registryLoaded = true;
    }

    // The members of those guilds: nothing to ask when there is no bot guild.
    void AskMembers(time_t now)
    {
        if (botGuilds.empty())
            return;

        std::ostringstream ids;
        for (auto const& entry : botGuilds)
            ids << (ids.tellp() > 0 ? "," : "") << entry.first;

        membersAskedAt = now;
        membersPending = true;
        membersFuture = CharacterDatabase.AsyncPQuery(
            "SELECT gm.guildid, gm.guid, gm.member_rank, c.level, c.online, c.logout_time, c.account, c.name "
            "FROM guild_member gm JOIN characters c ON c.guid = gm.guid WHERE gm.guildid IN (%s)", ids.str().c_str());
    }

    std::vector<MemberRow> ReadMembers(QueryResult result)
    {
        std::vector<MemberRow> rows;
        if (!result)
            return rows;

        do
        {
            Field* fields = result->Fetch();
            MemberRow row;
            row.guildId = fields[0].GetUInt32();
            row.low = fields[1].GetUInt32();
            row.rank = fields[2].GetUInt8();
            row.level = fields[3].GetUInt8();
            row.online = fields[4].GetUInt8() != 0;
            row.logoutTime = fields[5].GetUInt32();
            row.isBot = sPlayerbotAIConfig->IsInRandomAccountList(fields[6].GetUInt32());
            row.name = fields[7].GetString();
            rows.push_back(std::move(row));
        } while (result->NextRow());
        return rows;
    }

    uint64 PlayerGuid(uint32 low) { return MAKE_NEW_GUID(low, 0, HIGHGUID_PLAYER); }

    // A random bot founds a guild and pays the charter price, like the petition would.
    void FoundGuild(Settings const& s, std::vector<Player*> const& bots)
    {
        if (botGuilds.size() >= s.maxGuilds || urand(1, PERCENT) > s.foundChance)
            return;

        std::vector<Player*> pool;
        for (Player* bot : bots)
            if (bot->getLevel() >= s.minFoundLevel && GuildlessBot(bot) && bot->HasEnoughMoney(int64(GUILD_CHARTER_COST)))
                pool.push_back(bot);
        if (pool.empty())
            return;

        std::string const name = RandomPlayerbotFactory::CreateRandomGuildName();
        if (name.empty())
            return;

        Player* leader = pool[urand(0, uint32(pool.size() - 1))];
        Guild* guild = new Guild();
        if (!guild->Create(leader, name))
        {
            delete guild;
            return;
        }

        sGuildMgr->AddGuild(guild);
        leader->ModifyMoney(-int64(GUILD_CHARTER_COST));
        botGuilds[guild->GetId()].leaderLow = leader->GetGUIDLow();
        SF_LOG_INFO("playerbots", "[BOTGUILD] %s (level %u) founds <%s>", leader->GetName().c_str(), leader->getLevel(), name.c_str());
    }

    // Each guild with room invites one free bot of its faction and level band, a few guilds per pass.
    void Recruit(Settings const& s, std::vector<Player*> const& bots, time_t now)
    {
        std::vector<uint32> ids;
        for (auto const& entry : botGuilds)
            ids.push_back(entry.first);
        for (size_t i = ids.size(); i > 1; --i)
            std::swap(ids[i - 1], ids[urand(0, uint32(i - 1))]);

        uint32 invites = 0;
        for (uint32 const id : ids)
        {
            if (invites >= s.invitesPerPass)
                break;

            Guild* guild = sGuildMgr->GetGuildById(id);
            Player* leader = guild ? OnlineLeader(botGuilds[id]) : nullptr;
            if (!leader || guild->GetMembersCount() + PendingFor(id) >= s.maxMembers)
                continue;

            std::vector<Player*> candidates;
            for (Player* bot : bots)
            {
                if (bot == leader || bot->GetTeamId() != leader->GetTeamId() || !GuildlessBot(bot) ||
                    uint32(std::abs(int32(bot->getLevel()) - int32(leader->getLevel()))) > s.levelWindow)
                    continue;

                std::lock_guard<std::mutex> guard(pendingLock);
                if (!pending.count(bot->GetGUIDLow()))
                    candidates.push_back(bot);
            }
            if (candidates.empty())
                continue;

            Player* target = candidates[urand(0, uint32(candidates.size() - 1))];
            guild->HandleInviteMember(leader->GetSession(), target->GetName());
            if (target->GetGuildIdInvited() != int(id))
                continue;

            Pending invite;
            invite.guildId = id;
            invite.acceptAt = now + time_t(urand(ACCEPT_DELAY_MIN_S, ACCEPT_DELAY_MAX_S));
            invite.expireAt = now + time_t(INVITE_EXPIRE_S);
            invite.decline = urand(1, PERCENT) > s.acceptChance;
            {
                std::lock_guard<std::mutex> guard(pendingLock);
                pending[target->GetGUIDLow()] = invite;
            }
            ++invites;
            SF_LOG_DEBUG("playerbots", "[BOTGUILD] %s invites %s into <%s>", leader->GetName().c_str(), target->GetName().c_str(),
                         guild->GetName().c_str());
        }
    }

    // Answers the open invites a little after they were sent, as a player would, and welcomes whoever joined.
    void ProcessPending(Settings const& s, time_t now)
    {
        std::vector<std::pair<uint32, Pending>> due;
        {
            std::lock_guard<std::mutex> guard(pendingLock);
            for (auto const& entry : pending)
                due.push_back(entry);
        }

        for (auto const& [low, invite] : due)
        {
            Player* invitee = BotByLow(low);
            bool done = !invitee || !invitee->IsInWorld() || invitee->GetGuildId() != 0 ||
                invitee->GetGuildIdInvited() != int(invite.guildId);
            if (!done && now >= invite.acceptAt)
            {
                WorldPacket empty;
                if (invite.decline || now >= invite.expireAt)
                    invitee->GetSession()->HandleGuildDeclineOpcode(empty);
                else
                    invitee->GetSession()->HandleGuildAcceptOpcode(empty);
                done = true;
            }

            if (!done)
                continue;

            if (invitee && invitee->IsInWorld() && invitee->GetGuildId() == invite.guildId && urand(1, PERCENT) <= s.welcomeChance)
            {
                auto const guildInfo = botGuilds.find(invite.guildId);
                if (Player* leader = guildInfo != botGuilds.end() ? OnlineLeader(guildInfo->second) : nullptr)
                    if (leader != invitee)
                        Say(leader, "guild_welcome", "Welcome to the guild, %name", { { "%name", invitee->GetName() } },
                            BotVoice::Channel::GUILD);
            }

            std::lock_guard<std::mutex> guard(pendingLock);
            pending.erase(low);
        }
    }

    // One pass over the guild rosters: kick, demote and promote by activity, and a member that leaves now and then.
    // The leader acts through its own session, so the guild's rank rights apply; humans and alt bots are never touched.
    void ManageGuilds(Settings const& s, std::vector<MemberRow> const& rows, time_t now)
    {
        std::unordered_map<uint32, std::vector<MemberRow const*>> byGuild;
        for (MemberRow const& row : rows)
            byGuild[row.guildId].push_back(&row);

        uint32 const inactiveSeconds = s.inactiveDays * SECONDS_PER_DAY;
        // The share of a day one refresh covers, for the chance that a member leaves in it.
        double const refreshShare = double(s.refreshSeconds) / double(SECONDS_PER_DAY);

        for (auto const& [guildId, members] : byGuild)
        {
            Guild* guild = sGuildMgr->GetGuildById(guildId);
            auto const info = botGuilds.find(guildId);
            if (!guild || info == botGuilds.end())
                continue;

            Player* leader = OnlineLeader(info->second);

            if (leader)
            {
                // Kick the first bot member that has been offline for too long (never the leader, never a human).
                for (MemberRow const* member : members)
                {
                    if (!member->isBot || member->low == info->second.leaderLow || member->rank == RANK_GUILD_MASTER ||
                        member->online || now < time_t(member->logoutTime) + time_t(inactiveSeconds))
                        continue;

                    Say(leader, "guild_kick", "%name was gone too long, so I removed them", { { "%name", member->name } },
                        BotVoice::Channel::GUILD);
                    guild->HandleRemoveMember(leader->GetSession(), PlayerGuid(member->low));
                    SF_LOG_INFO("playerbots", "[BOTGUILD] %s removes %s from <%s> (offline %u days)", leader->GetName().c_str(),
                                member->name.c_str(), guild->GetName().c_str(), uint32((now - time_t(member->logoutTime)) / SECONDS_PER_DAY));
                    break;
                }

                // Demote an officer or veteran that has been away for half the kick time.
                bool acted = false;
                for (MemberRow const* member : members)
                {
                    if (!member->isBot || member->online || member->rank == RANK_GUILD_MASTER || member->rank > s.maxPromoteRank ||
                        now < time_t(member->logoutTime) + time_t(inactiveSeconds / 2))
                        continue;

                    guild->HandleUpdateMemberRank(leader->GetSession(), PlayerGuid(member->low), true);
                    SF_LOG_INFO("playerbots", "[BOTGUILD] %s demotes %s in <%s> (away)", leader->GetName().c_str(), member->name.c_str(),
                                guild->GetName().c_str());
                    acted = true;
                    break;
                }

                // Promote the most advanced online bot member below the top bot rank, now and then.
                if (!acted && urand(1, PERCENT) <= s.promoteChance)
                {
                    MemberRow const* best = nullptr;
                    for (MemberRow const* member : members)
                    {
                        if (!member->isBot || !member->online || member->low == info->second.leaderLow ||
                            member->rank <= s.maxPromoteRank)
                            continue;

                        Player* online = BotByLow(member->low);
                        if (!Usable(online))
                            continue;

                        if (!best || member->level > best->level)
                            best = member;
                    }

                    if (best)
                    {
                        guild->HandleUpdateMemberRank(leader->GetSession(), PlayerGuid(best->low), false);
                        Say(leader, "guild_promoted", "Congratulations %name, you earned it", { { "%name", best->name } },
                            BotVoice::Channel::GUILD);
                        SF_LOG_INFO("playerbots", "[BOTGUILD] %s promotes %s in <%s>", leader->GetName().c_str(), best->name.c_str(),
                                    guild->GetName().c_str());
                    }
                }
            }

            // A member leaves its guild now and then (the leader stays).
            for (MemberRow const* member : members)
            {
                if (!member->isBot || !member->online || member->low == info->second.leaderLow ||
                    urand(1, 1000000) > uint32(double(s.leavePerDay) / PERCENT * refreshShare * 1000000.0))
                    continue;

                Player* leaving = BotByLow(member->low);
                if (!Usable(leaving) || leaving->GetGuildId() != guildId)
                    continue;

                Say(leaving, "guild_leave", "I'm going to try something else, bye all", {}, BotVoice::Channel::GUILD);
                guild->HandleLeaveMember(leaving->GetSession());
                SF_LOG_INFO("playerbots", "[BOTGUILD] %s leaves <%s>", leaving->GetName().c_str(), guild->GetName().c_str());
            }
        }
    }

    // A recruiting leader near a guildless human whispers an offer and sends the ordinary guild invite, which the
    // human can decline. Rare, with a cooldown per human.
    void OfferToHumans(Settings const& s, time_t now)
    {
        std::vector<Player*> const players = sRandomPlayerbotMgr->GetPlayers();
        for (Player* human : players)
        {
            if (!IsHuman(human) || !human->IsInWorld() || human->GetGuildId() || human->GetGuildIdInvited() ||
                human->isDND() || human->HasFlag(PLAYER_FIELD_PLAYER_FLAGS, PLAYER_FLAGS_AUTO_DECLINE_GUILD) ||
                human->getLevel() < s.humanMinLevel || !human->IsAlive() || human->InBattleground() ||
                urand(1, PERCENT) > s.humanChance)
                continue;

            auto const offered = humanOffered.find(human->GetGUIDLow());
            if (offered != humanOffered.end() && now < offered->second + time_t(s.humanCooldownHours) * 3600)
                continue;

            for (auto const& entry : botGuilds)
            {
                Guild* guild = sGuildMgr->GetGuildById(entry.first);
                Player* leader = guild ? OnlineLeader(entry.second) : nullptr;
                if (!leader || guild->GetMembersCount() >= s.maxMembers || leader->GetTeamId() != human->GetTeamId() ||
                    leader->GetMapId() != human->GetMapId() || leader->GetExactDist2d(human) > s.humanRadius)
                    continue;

                // With the liking score on, only a leader the human has played with enough offers, once an hour at most and
                // never inside the persistent cooldown (AiPlayerbot.Social.Liking.*); true at once when the score is off.
                if (!LikingStore::MayOfferGuild(leader, human, s.humanCooldownHours * 3600))
                    continue;

                Say(leader, "guild_invite_offer", "Hey %name, we have a friendly guild, <%guildname>. Want to join?",
                    { { "%name", human->GetName() }, { "%guildname", guild->GetName() } }, BotVoice::Channel::WHISPER, human);
                guild->HandleInviteMember(leader->GetSession(), human->GetName());
                SocialLog::Count(SocialSoak::Counter::GuildOffer);
                humanOffered[human->GetGUIDLow()] = now;
                SF_LOG_INFO("playerbots", "[BOTGUILD] %s offers <%s> to %s", leader->GetName().c_str(), guild->GetName().c_str(),
                            human->GetName().c_str());
                break;
            }
        }

        // The cooldown list does not grow for good.
        for (auto itr = humanOffered.begin(); itr != humanOffered.end();)
            itr = now >= itr->second + time_t(s.humanCooldownHours) * 3600 ? humanOffered.erase(itr) : std::next(itr);
    }

    // BT-702: takes in the answers that have come, every call (a future's ready check is one atomic read).
    void PollQueries(time_t now)
    {
        if (registryPending)
        {
            if (registryFuture.ready())
            {
                QueryResult result;
                registryFuture.get(result);
                registryPending = false;
                ApplyRegistry(result);
                AskMembers(now);
            }
            else if (now >= registryAskedAt + QUERY_GIVE_UP_SECONDS)
            {
                registryFuture.cancel();
                registryPending = false;
                SF_LOG_WARN("playerbots", "[BOTGUILD] the guild list query has not answered in %lld s, given up",
                            static_cast<long long>(QUERY_GIVE_UP_SECONDS));
            }
        }

        if (membersPending)
        {
            if (membersFuture.ready())
            {
                QueryResult result;
                membersFuture.get(result);
                membersPending = false;
                ManageGuilds(manageSettings, ReadMembers(result), now);
            }
            else if (now >= membersAskedAt + QUERY_GIVE_UP_SECONDS)
            {
                membersFuture.cancel();
                membersPending = false;
                SF_LOG_WARN("playerbots", "[BOTGUILD] the guild member query has not answered in %lld s, given up",
                            static_cast<long long>(QUERY_GIVE_UP_SECONDS));
            }
        }
    }
}  // namespace

bool BotGuilds::OwnsInvite(uint32 botLowGuid)
{
    std::lock_guard<std::mutex> guard(pendingLock);
    return pending.count(botLowGuid) != 0;
}

void BotGuilds::Update()
{
    // The interval is read once a pass, not on every world tick.
    static uint32 intervalSeconds = 60;
    time_t const now = time(nullptr);
    PollQueries(now);
    if (lastPassAt && now < lastPassAt + time_t(intervalSeconds))
        return;
    lastPassAt = now;

    Settings const settings = ReadSettings();
    intervalSeconds = settings.intervalSeconds;
    if (!settings.enable || !sPlayerbotAIConfig->enabled)
        return;

    bool const asking = registryPending || membersPending;
    bool const refreshDue = !asking && (!registryLoaded || now >= lastRefreshAt + time_t(settings.refreshSeconds));
    if (refreshDue)
    {
        manageSettings = settings;
        AskRegistry(now);
    }
    // The first list of bot guilds is still on its way (the answer is taken in at the next call).
    if (!registryLoaded)
        return;

    std::vector<Player*> const bots = sRandomPlayerbotMgr->GetPlayerBotsSnapshot();
    ProcessPending(settings, now);
    FoundGuild(settings, bots);
    Recruit(settings, bots, now);

    if (settings.inviteHumans)
        OfferToHumans(settings, now);
}
