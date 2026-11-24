/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 *
 * Dungeon Finder fill and bot-only runs. See LfgFill.h.
 */

#include "LfgFill.h"
#include "LfgFillPolicy.h"
#include "SpecRolePolicy.h"
#include "LfgLeavePolicy.h"
#include "LfgStallPolicy.h"

#include "BotFactory.h"
#include "BotSale.h"
#include "BotVoice.h"
#include "Config.h"
#include "Group.h"
#include "LFGMgr.h"
#include "LfgActions.h"
#include "LfgAnyLevelRequests.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "PlayerbotSpec.h"
#include "Playerbots.h"
#include "RandomPlayerbotMgr.h"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <ctime>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
    using lfg::LfgDungeonSet;

    // LfgJoinAction's own rule: no random bot queues below this level.
    uint32 const MIN_FINDER_LEVEL = 15;
    // A bot JoinLfg refused (locked dungeon, deserter, gear) is not picked again for this long.
    time_t const REFUSED_BACKOFF = 5 * 60;
    // A line about a request that has no bot to give is repeated at most this often.
    time_t const NO_BOT_LOG_GAP = 60;
    // A bot-only party that is gone from the finder this long ago is forgotten.
    time_t const PARTY_FORGET_AFTER = 90;
    // Bots of a wrapped-up party that have not left by then are left alone.
    time_t const WRAP_UP_GRACE = 3 * 60;
    // How many dungeons a bot-only run tries to fill before it gives up until the next time.
    uint32 const BOT_RUN_TRIES = 8;
    uint32 const SECONDS_PER_MINUTE = 60;
    // A bot picks its first specialization at level 10, so it can switch from then on.
    uint32 const MIN_SWITCH_LEVEL = 10;
    // BT-178: added to the order of a mana user low on drink, so every bot of any level distance that holds drink
    // comes first (the level distance counts 4 per level, at most a few hundred).
    int const LOW_DRINK_ORDER_PENALTY = 1000;

    enum RoleIndex
    {
        ROLE_TANK = 0,
        ROLE_HEALER = 1,
        ROLE_DAMAGE = 2,
        ROLE_COUNT = 3
    };

    struct SpecCensusEntry
    {
        uint32 guid, team, level, role;
        time_t expires;
    };
    std::mutex specCensusLock;
    std::vector<SpecCensusEntry> specCensus;
    std::vector<SpecCensusEntry> specReservations;
    bool specCensusReady = false;
    uint32 specWindow = 6;

    // What a five-player group holds.
    uint32 const ROLE_WANTED[ROLE_COUNT] = { 1, 1, 3 };

    struct Settings
    {
        bool enable = true;
        uint32 intervalSeconds = 10;
        uint32 levelWindow = 6;
        // A filled tank is at most this many levels under the requesting player (LfgFillPolicy::TankLevelFloor).
        uint32 tankLevelBelowPlayer = 1;
        uint32 maxJoinsPerPass = 5;
        uint32 queueTimeoutSeconds = 10 * SECONDS_PER_MINUTE;
        bool tankSwitch = true;

        bool botRuns = true;
        uint32 maxParties = 1;
        uint32 minIntervalSeconds = 60 * SECONDS_PER_MINUTE;
        uint32 maxIntervalSeconds = 180 * SECONDS_PER_MINUTE;
        uint32 maxRunSeconds = 40 * SECONDS_PER_MINUTE;
        uint32 partyLevelWindow = 5;
        uint32 failureCooldownHours = 4;
        std::set<uint32> botRunDungeons;
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

    Settings ReadSettings()
    {
        Settings s;
        s.enable = sConfigMgr->GetBoolDefault("AiPlayerbot.LfgFill.Enable", true);
        s.intervalSeconds = uint32(std::max(2, sConfigMgr->GetIntDefault("AiPlayerbot.LfgFill.IntervalSeconds", 10)));
        s.levelWindow = uint32(std::max(0, sConfigMgr->GetIntDefault("AiPlayerbot.LfgFill.LevelWindow", 6)));
        s.tankLevelBelowPlayer = uint32(std::max(0, sConfigMgr->GetIntDefault("AiPlayerbot.LfgFill.TankLevelBelowPlayer", 1)));
        s.maxJoinsPerPass = uint32(std::max(1, sConfigMgr->GetIntDefault("AiPlayerbot.LfgFill.MaxJoinsPerPass", 5)));
        s.queueTimeoutSeconds =
            uint32(std::max(1, sConfigMgr->GetIntDefault("AiPlayerbot.LfgFill.QueueTimeoutMinutes", 10))) * SECONDS_PER_MINUTE;

        s.tankSwitch = sConfigMgr->GetBoolDefault("AiPlayerbot.LfgFill.TankSwitch", true);
        s.botRuns = sConfigMgr->GetBoolDefault("AiPlayerbot.LfgBotRuns.Enable", true);
        s.maxParties = uint32(std::max(0, sConfigMgr->GetIntDefault("AiPlayerbot.LfgBotRuns.MaxParties", 1)));
        s.minIntervalSeconds =
            uint32(std::max(1, sConfigMgr->GetIntDefault("AiPlayerbot.LfgBotRuns.MinIntervalMinutes", 60))) * SECONDS_PER_MINUTE;
        s.maxIntervalSeconds = std::max(s.minIntervalSeconds,
            uint32(std::max(1, sConfigMgr->GetIntDefault("AiPlayerbot.LfgBotRuns.MaxIntervalMinutes", 180))) * SECONDS_PER_MINUTE);
        s.maxRunSeconds =
            uint32(std::max(5, sConfigMgr->GetIntDefault("AiPlayerbot.LfgBotRuns.MaxRunMinutes", 40))) * SECONDS_PER_MINUTE;
        s.partyLevelWindow = uint32(std::max(0, sConfigMgr->GetIntDefault("AiPlayerbot.LfgBotRuns.LevelWindow", 5)));
        s.failureCooldownHours = uint32(std::max(0, sConfigMgr->GetIntDefault("AiPlayerbot.LfgBotRuns.FailureCooldownHours", 4)));
        s.botRunDungeons = ParseIdList(sConfigMgr->GetStringDefault(
            "AiPlayerbot.LfgBotRuns.Dungeons", "4,1,10,16,26,273,20,24,8,12,163,6,34,36,38,171,170,311,312,209,305"));
        return s;
    }

    std::atomic<bool>& EnabledFlag()
    {
        static std::atomic<bool> flag{ sConfigMgr->GetBoolDefault("AiPlayerbot.LfgFill.Enable", true) };
        return flag;
    }

    std::atomic<uint32> maxRunSeconds{ 40 * SECONDS_PER_MINUTE };
    std::atomic<uint32> failureCooldownHours{ 4 };

    // Dungeons whose run takes longer by design (the Black Morass is a timed defence of 18 rifts, Old Hillsbrad an escort):
    // their runs get this long instead.
    std::set<uint32> const& LongRunMaps()
    {
        static std::set<uint32> const maps = ParseIdList(sConfigMgr->GetStringDefault("AiPlayerbot.LfgBotRuns.LongRunMaps", "269,560,595,644"));
        return maps;
    }

    uint32 LongRunSeconds()
    {
        static uint32 const seconds = uint32(std::max(5, sConfigMgr->GetIntDefault("AiPlayerbot.LfgBotRuns.LongRunMinutes", 75))) * SECONDS_PER_MINUTE;
        return seconds;
    }

    // Map threads read these two sets (LfgMannersAction), the world thread writes them.
    std::mutex runLock;
    std::set<uint32> botRunBots;
    std::set<uint32> wrapUpBots;
    // Dungeons whose bot-led run could not reach a boss, by finder dungeon id: the time (time_t) their rest ends.
    std::unordered_map<uint32, time_t> failedDungeonUntil;

    // World thread only from here.
    struct Party
    {
        std::vector<uint32> members;
        uint32 dungeonId = 0;
        time_t startedAt = 0;
        time_t wrappedAt = 0;
    };

    std::vector<Party> parties;
    std::unordered_map<uint32, time_t> filledAt;       // bots this pass queued, by low guid
    std::set<uint32> inProposal;                       // BT-205: of those, the ones a role check or proposal holds now
    std::unordered_map<uint32, time_t> refusedUntil;   // bots JoinLfg refused
    std::unordered_map<uint64, time_t> noBotLogAt;     // per request, by player guid
    // BT-178: free mana users of this pass that are low on drink and cannot shop now, by low guid.
    struct LowDrink
    {
        BotSalePolicy::FinderSupply supply;
        uint32 drink;
    };
    std::unordered_map<uint32, LowDrink> lowDrink;
    time_t lastPassAt = 0;
    time_t nextBotRunAt = 0;

    int RoleIndexOf(uint32 roles)
    {
        if (roles & lfg::PLAYER_ROLE_TANK)
            return ROLE_TANK;
        if (roles & lfg::PLAYER_ROLE_HEALER)
            return ROLE_HEALER;
        if (roles & lfg::PLAYER_ROLE_DAMAGE)
            return ROLE_DAMAGE;
        return -1;
    }

    char const* RoleName(int role)
    {
        switch (role)
        {
            case ROLE_TANK:
                return "tank";
            case ROLE_HEALER:
                return "healer";
            default:
                return "dps";
        }
    }

    bool Intersects(LfgDungeonSet const& a, LfgDungeonSet const& b)
    {
        for (uint32 const id : a)
            if (b.count(id))
                return true;
        return false;
    }

    template <typename T>
    void Shuffle(std::vector<T>& items)
    {
        for (size_t i = items.size(); i > 1; --i)
            std::swap(items[i - 1], items[urand(0, uint32(i - 1))]);
    }

    // Someone queued, a real player or a bot of this pass, as the role pool sees it.
    struct Pooled
    {
        uint64 guid;
        TeamId team;
        int role;
        LfgDungeonSet dungeons;
        bool isBot;
    };

    // A real player (or the leader of a real group) waiting in the finder.
    struct Request
    {
        Player* player = nullptr;
        TeamId team = TEAM_ALLIANCE;
        uint32 level = 0;
        float itemLevel = 0.0f;
        LfgDungeonSet dungeons;
        std::set<uint64> members;
        uint32 have[ROLE_COUNT] = { 0, 0, 0 };
    };

    // The healing specialization of a hybrid class, for a finder queue that lacks a healer and has no healer-spec bot
    // to give. Priest has two healing specs; Holy is the one a switching bot takes. 0 for a class that cannot heal.
    Specializations HealingSpecOf(uint8 playerClass)
    {
        switch (playerClass)
        {
            case CLASS_DRUID:
                return SPEC_DRUID_RESTORATION;
            case CLASS_PALADIN:
                return SPEC_PALADIN_HOLY;
            case CLASS_PRIEST:
                return SPEC_PRIEST_HOLY;
            case CLASS_SHAMAN:
                return SPEC_SHAMAN_RESTORATION;
            case CLASS_MONK:
                return SPEC_MONK_MISTWEAVER;
            default:
                return SPEC_NONE;
        }
    }

    // The tank specialization of a class that has one, for a finder queue that lacks a tank and has no tank-spec bot to give.
    // SPEC_NONE for a class that cannot tank.
    Specializations TankSpecOf(uint8 playerClass)
    {
        switch (playerClass)
        {
            case CLASS_WARRIOR:
                return SPEC_WARRIOR_PROTECTION;
            case CLASS_PALADIN:
                return SPEC_PALADIN_PROTECTION;
            case CLASS_DEATH_KNIGHT:
                return SPEC_DEATH_KNIGHT_BLOOD;
            case CLASS_DRUID:
                return SPEC_DRUID_GUARDIAN;
            case CLASS_MONK:
                return SPEC_MONK_BREWMASTER;
            default:
                return SPEC_NONE;
        }
    }

    // A bot takes the specialization of the role the queue lacks and queues for it, like a player would. The bot keeps the
    // new spec afterwards: the spec change costs a talent reset, new gear and new strategies, and switching back after the
    // run would repeat all of it for the next fill, which would need the same role again anyway. Returns the role mask the
    // bot queues with, 0 when the switch did not give it the wanted role.
    uint32 SwitchSpecFor(Player* bot, Request const& request, Specializations spec, int wantedRole, char const* what)
    {
        ChrSpecializationEntry const* entry = spec == SPEC_NONE ? nullptr : sChrSpecializationStore.LookupEntry(uint32(spec));
        if (!entry)
            return 0;

        uint32 const before = uint32(BotGetSpecialization(bot));
        BotFactory::SwitchSpecialization(bot, entry->TabPage);

        uint32 const roleMask = LfgGetBotRoles(bot);
        SF_LOG_INFO("playerbots", "[LFGFILL] %s (level %u, class %u) switches to %s for %s (spec %u -> %u)",
                    bot->GetName().c_str(), bot->getLevel(), uint32(bot->getClass()), what, request.player->GetName().c_str(), before,
                    uint32(BotGetSpecialization(bot)));
        return RoleIndexOf(roleMask) == wantedRole ? roleMask : 0;
    }

    uint32 SwitchToHealing(Player* bot, Request const& request)
    {
        return SwitchSpecFor(bot, request, HealingSpecOf(bot->getClass()), ROLE_HEALER, "healing");
    }

    uint32 SwitchToTanking(Player* bot, Request const& request)
    {
        return SwitchSpecFor(bot, request, TankSpecOf(bot->getClass()), ROLE_TANK, "tanking");
    }

    bool IsHuman(Player* player)
    {
        return player && player->GetSession() && !player->GetSession()->IsBot() && !GET_PLAYERBOT_AI(player);
    }

    // A bot the finder may ever take: a random bot in the world from the finder's level on, not a self-bot.
    bool FinderBot(Player* bot)
    {
        if (!bot || !bot->IsInWorld() || !sRandomPlayerbotMgr->IsRandomBot(bot) || bot->getLevel() < MIN_FINDER_LEVEL)
            return false;

        PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
        return botAI && botAI->GetMaster() != bot;
    }

    // Why a finder bot cannot be taken now, the first reason found (BT-207 counts them); Hold::Free when it can. The drink
    // supply is read last and handed back.
    LfgFillPolicy::Hold HoldOf(Player* bot, time_t now, BotSalePolicy::FinderSupply& supply, uint32& drink, bool allowGroup = false)
    {
        if ((!allowGroup && bot->GetGroup()) || bot->GetGroupInvite())
            return LfgFillPolicy::Hold::Grouped;

        if (!bot->IsAlive() || bot->IsInCombat() || bot->IsInFlight() || bot->IsBeingTeleported())
            return LfgFillPolicy::Hold::Busy;

        if (bot->InBattleground() || bot->InBattlegroundQueue() || bot->InArena())
            return LfgFillPolicy::Hold::Battleground;

        Map* map = bot->GetMap();
        if (!map || map->Instanceable())
            return LfgFillPolicy::Hold::Instance;

        if (bot->HasAura(lfg::LFG_SPELL_DUNGEON_DESERTER) || bot->HasAura(lfg::LFG_SPELL_DUNGEON_COOLDOWN))
            return LfgFillPolicy::Hold::Blocked;

        if (sLFGMgr->GetState(bot->GetGUID()) != lfg::LFG_STATE_NONE)
            return LfgFillPolicy::Hold::Queued;

        auto const refused = refusedUntil.find(bot->GetGUIDLow());
        if (refused != refusedUntil.end() && refused->second > now)
            return LfgFillPolicy::Hold::Refused;

        // BT-178: a mana user on its way to buy drink finishes the trip first.
        supply = BotSale::FinderSupplyOf(GET_PLAYERBOT_AI(bot), bot, drink);
        return BotSalePolicy::FinderMayTake(supply) ? LfgFillPolicy::Hold::Free : LfgFillPolicy::Hold::Shopping;
    }

    bool FreeForFinder(Player* bot, time_t now)
    {
        if (!FinderBot(bot))
            return false;

        uint32 drink = 0;
        BotSalePolicy::FinderSupply supply = BotSalePolicy::FinderSupply::Ready;
        LfgFillPolicy::Hold const hold = HoldOf(bot, now, supply, drink);
        if (hold == LfgFillPolicy::Hold::Shopping)
            BotSale::LogFinderSupply(bot, supply, drink, "waits");
        if (hold != LfgFillPolicy::Hold::Free)
            return false;

        // BT-178: one low on drink that cannot shop now stays free but is taken last (FillRequest).
        if (BotSalePolicy::FinderTakesLast(supply))
            lowDrink[bot->GetGUIDLow()] = { supply, drink };
        return true;
    }

    // BT-207: where the finder bots of the player's side near the player's level are, for the "no free bot fits" line.
    std::string CensusOf(Player* requester, uint32 requestLevel, TeamId team, std::vector<Player*> const& bots,
                         Settings const& settings, time_t now)
    {
        LfgFillPolicy::Census census;
        for (Player* bot : bots)
        {
            if (!FinderBot(bot) || bot == requester || bot->GetTeamId() != team ||
                !LfgFillPolicy::InReach(bot->getLevel(), requestLevel, settings.levelWindow, MIN_FINDER_LEVEL))
                continue;

            uint32 drink = 0;
            BotSalePolicy::FinderSupply supply = BotSalePolicy::FinderSupply::Ready;
            LfgFillPolicy::Hold const hold = HoldOf(bot, now, supply, drink);
            census.Add(hold, hold == LfgFillPolicy::Hold::Free ? RoleIndexOf(LfgGetBotRoles(bot)) : -1);
        }

        auto held = [&census](LfgFillPolicy::Hold hold) { return census.held[uint32(hold)]; };
        std::ostringstream out;
        out << census.Total() << " bot(s) within " << settings.levelWindow << " levels: free " << held(LfgFillPolicy::Hold::Free)
            << " (tank " << census.freeByRole[ROLE_TANK] << ", healer " << census.freeByRole[ROLE_HEALER] << ", dps "
            << census.freeByRole[ROLE_DAMAGE] << "), grouped " << held(LfgFillPolicy::Hold::Grouped) << ", busy "
            << held(LfgFillPolicy::Hold::Busy) << ", battleground " << held(LfgFillPolicy::Hold::Battleground)
            << ", in an instance " << held(LfgFillPolicy::Hold::Instance) << ", deserter or cooldown "
            << held(LfgFillPolicy::Hold::Blocked) << ", queued " << held(LfgFillPolicy::Hold::Queued) << ", refused "
            << held(LfgFillPolicy::Hold::Refused) << ", buying drink " << held(LfgFillPolicy::Hold::Shopping);
        return out.str();
    }

    // The [SUPPLY] line of a bot the finder takes while it is low on drink.
    void LogLowDrinkJoin(Player* bot)
    {
        auto const itr = lowDrink.find(bot->GetGUIDLow());
        if (itr != lowDrink.end())
            BotSale::LogFinderSupply(bot, itr->second.supply, itr->second.drink, "joins");
    }

    bool InLevelRange(lfg::LFGDungeonData const& dungeon, uint32 level)
    {
        return level >= uint32(dungeon.minlevel) && (!dungeon.maxlevel || level <= uint32(dungeon.maxlevel));
    }

    // The dungeons of the request this bot may queue for. The stock rule of LfgJoinAction (inside the dungeon's own
    // range, a specific dungeon only within ten levels of its minimum), the any-level rule when that is on, and bots
    // near the requester's level so a random pick is not a level 50 in a level 20's run. One random dungeon alone, or
    // specific dungeons of one category, as 5.4.8 JoinLfg accepts.
    LfgDungeonSet DungeonsFor(Player* bot, Request const& request, Settings const& settings, bool anyLevel,
                              std::vector<LfgAnyLevel::Request> const& anyLevelRequests, int role = -1)
    {
        LfgDungeonSet out;
        uint8 category = 0;
        bool haveRandom = false;
        uint32 const botLevel = bot->getLevel();
        float const botItemLevel = anyLevel ? bot->GetAverageItemLevel() : 0.0f;

        for (uint32 const dungeonId : request.dungeons)
        {
            lfg::LFGDungeonData const* dungeon = sLFGMgr->GetLFGDungeon(dungeonId);
            if (!dungeon || (dungeon->type != lfg::LFG_TYPE_RANDOM && dungeon->type != lfg::LFG_TYPE_DUNGEON))
                continue;

            bool const nativeOk = !((dungeon->minlevel && !InLevelRange(*dungeon, botLevel)) ||
                (botLevel > uint32(dungeon->minlevel) + 10 && dungeon->type == lfg::LFG_TYPE_DUNGEON));
            bool const accepted = anyLevel ?
                LfgAnyLevel::AcceptBot(anyLevelRequests, dungeon->id, botLevel, nativeOk, botItemLevel) : nativeOk;
            if (!accepted)
                continue;

            // The window only means something while the requester is inside the dungeon's range; a requester above it
            // (a scaled queue) takes the bots of the range.
            if (InLevelRange(*dungeon, request.level) &&
                uint32(std::abs(int32(botLevel) - int32(request.level))) > settings.levelWindow)
                continue;

            // Same scope as the window: a requester above the range gets the bots of the range, whatever their level.
            if (role == ROLE_TANK && InLevelRange(*dungeon, request.level) &&
                !LfgFillPolicy::TankLevelOk(botLevel, request.level, settings.tankLevelBelowPlayer, uint32(dungeon->minlevel)))
                continue;

            if (!out.empty() && (dungeon->type == lfg::LFG_TYPE_RANDOM || haveRandom || dungeon->category != category))
                continue;

            category = dungeon->category;
            if (dungeon->type == lfg::LFG_TYPE_RANDOM)
                haveRandom = true;
            out.insert(dungeon->id);
        }

        return out;
    }

    std::string DungeonNames(LfgDungeonSet const& dungeons)
    {
        std::string names;
        for (uint32 const id : dungeons)
        {
            lfg::LFGDungeonData const* dungeon = sLFGMgr->GetLFGDungeon(id);
            if (!names.empty())
                names += ", ";
            names += dungeon ? dungeon->name : std::to_string(id);
        }
        return names;
    }

    // Whoever queued (alone, or as a real group's members) and has a role the group would use.
    int RoleOfQueued(uint64 guid)
    {
        int const role = RoleIndexOf(sLFGMgr->GetRoles(guid));
        return role < 0 ? int(ROLE_DAMAGE) : role;
    }

    void CollectRequests(std::vector<Player*> const& players, std::vector<Request>& requests,
                         std::vector<Pooled>& pool)
    {
        for (Player* player : players)
        {
            if (!player || !player->IsInWorld() || !IsHuman(player))
                continue;

            if (sLFGMgr->GetState(player->GetGUID()) != lfg::LFG_STATE_QUEUED)
                continue;

            lfg::LfgDungeonSet const& selected = sLFGMgr->GetSelectedDungeons(player->GetGUID());
            if (selected.empty())
                continue;

            Group* group = player->GetGroup();
            if (!group)
            {
                pool.push_back({ player->GetGUID(), player->GetTeamId(), RoleOfQueued(player->GetGUID()), selected, false });
                continue;
            }

            // Only the leader speaks for a queued group.
            if (group->GetLeaderGUID() != player->GetGUID())
                continue;

            Request request;
            request.player = player;
            request.team = player->GetTeamId();
            request.dungeons = selected;
            Player* reference = player;
            for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            {
                Player* member = ref->GetSource();
                if (!member)
                    continue;

                request.members.insert(member->GetGUID());
                ++request.have[RoleOfQueued(member->GetGUID())];
                if (member->getLevel() > reference->getLevel())
                    reference = member;
            }
            request.level = reference->getLevel();
            request.itemLevel = reference->GetAverageItemLevel();
            requests.push_back(std::move(request));
        }

        // A solo player is a request too (the pool entry above is only what OTHER requests count on).
        for (Pooled const& solo : pool)
        {
            if (solo.isBot)
                continue;

            Player* player = ObjectAccessor::FindPlayer(solo.guid);
            if (!player)
                continue;

            Request request;
            request.player = player;
            request.team = solo.team;
            request.level = player->getLevel();
            request.itemLevel = player->GetAverageItemLevel();
            request.dungeons = solo.dungeons;
            request.members.insert(solo.guid);
            ++request.have[solo.role];
            requests.push_back(std::move(request));
        }
    }

    // How many of each role the request still lacks, counting what is queued for the same dungeons on its side.
    void CountMissing(Request const& request, std::vector<Pooled> const& pool, uint32 (&missing)[ROLE_COUNT])
    {
        uint32 have[ROLE_COUNT] = { request.have[0], request.have[1], request.have[2] };
        for (Pooled const& other : pool)
            if (other.team == request.team && !request.members.count(other.guid) &&
                Intersects(other.dungeons, request.dungeons))
                ++have[other.role];

        for (int role = 0; role < ROLE_COUNT; ++role)
            missing[role] = have[role] >= ROLE_WANTED[role] ? 0 : ROLE_WANTED[role] - have[role];

        // A group already holds five players at most; roles it holds twice cannot be made up for by the others.
        uint32 total = have[0] + have[1] + have[2];
        if (total >= MAXGROUPSIZE)
            missing[ROLE_TANK] = missing[ROLE_HEALER] = missing[ROLE_DAMAGE] = 0;
    }

    void FillRequest(Request const& request, std::vector<Player*>& freeBots, std::vector<Player*> const& bots,
                     std::vector<Pooled>& pool, Settings const& settings, time_t now, bool anyLevel,
                     std::vector<LfgAnyLevel::Request> const& anyLevelRequests, uint32& joinsLeft)
    {
        uint32 missing[ROLE_COUNT];
        CountMissing(request, pool, missing);
        uint32 const missingTotal = missing[0] + missing[1] + missing[2];
        if (!missingTotal)
            return;

        // Bots near the requester's level first, with a little noise so the same bots are not picked every time.
        std::vector<std::pair<int, Player*>> order;
        for (Player* bot : freeBots)
            if (bot && bot->GetTeamId() == request.team)
                order.emplace_back(int(std::abs(int32(bot->getLevel()) - int32(request.level))) * 4 + int(urand(0, 3)) +
                                       (lowDrink.count(bot->GetGUIDLow()) ? LOW_DRINK_ORDER_PENALTY : 0), bot);
        std::sort(order.begin(), order.end(),
                  [](std::pair<int, Player*> const& a, std::pair<int, Player*> const& b) { return a.first < b.first; });

        uint32 queued = 0;
        std::string queuedNames;

        // What a queued bot changes: the role slot it takes, the pass budget, the pool and the log line.
        auto recordQueued = [&](Player* bot, int role, LfgDungeonSet const& dungeons, char const* note)
        {
            --missing[role];
            --joinsLeft;
            ++queued;
            filledAt[bot->GetGUIDLow()] = now;
            LogLowDrinkJoin(bot);
            pool.push_back({ bot->GetGUID(), bot->GetTeamId(), role, dungeons, true });
            std::replace(freeBots.begin(), freeBots.end(), bot, static_cast<Player*>(nullptr));
            if (!queuedNames.empty())
                queuedNames += ", ";
            queuedNames += bot->GetName() + " (" + RoleName(role) + " " + std::to_string(bot->getLevel()) + note + ")";
        };

        // Free bots in order that queue for a role this request still lacks, `onlyRole` when it is >= 0.
        auto fillFromFree = [&](int onlyRole)
        {
            for (auto const& entry : order)
            {
                if (!joinsLeft || !(missing[0] + missing[1] + missing[2]))
                    break;

                Player* bot = entry.second;
                if (!bot || sLFGMgr->GetState(bot->GetGUID()) != lfg::LFG_STATE_NONE)
                    continue;

                uint32 const roleMask = LfgGetBotRoles(bot);
                int const role = RoleIndexOf(roleMask);
                if (role < 0 || !missing[role] || (onlyRole >= 0 && role != onlyRole))
                    continue;

                LfgDungeonSet const dungeons = DungeonsFor(bot, request, settings, anyLevel, anyLevelRequests, role);
                if (dungeons.empty())
                    continue;

                if (!LfgQueueBotNow(bot, roleMask, dungeons))
                {
                    refusedUntil[bot->GetGUIDLow()] = now + REFUSED_BACKOFF;
                    continue;
                }

                recordQueued(bot, role, dungeons, "");
            }
        };

        // A hybrid bot of the same window (DungeonsFor applies it) switches to healing. Damage specs first; a tank spec
        // only while no tank is missing.
        auto switchFromFree = [&]()
        {
            for (int pass = 0; pass < 2 && missing[ROLE_HEALER]; ++pass)
            {
                for (auto const& entry : order)
                {
                    if (!joinsLeft || !missing[ROLE_HEALER])
                        break;

                    Player* bot = entry.second;
                    if (!bot || sLFGMgr->GetState(bot->GetGUID()) != lfg::LFG_STATE_NONE ||
                        HealingSpecOf(bot->getClass()) == SPEC_NONE || bot->getLevel() < MIN_SWITCH_LEVEL)
                        continue;

                    int const currentRole = RoleIndexOf(LfgGetBotRoles(bot));
                    if (currentRole == ROLE_HEALER || (pass == 0) != (currentRole == ROLE_DAMAGE))
                        continue;

                    if (currentRole == ROLE_TANK && missing[ROLE_TANK])
                        continue;

                    LfgDungeonSet const dungeons = DungeonsFor(bot, request, settings, anyLevel, anyLevelRequests);
                    if (dungeons.empty())
                        continue;

                    uint32 const roleMask = SwitchToHealing(bot, request);
                    if (!roleMask || !LfgQueueBotNow(bot, roleMask, dungeons))
                    {
                        refusedUntil[bot->GetGUIDLow()] = now + REFUSED_BACKOFF;
                        continue;
                    }

                    recordQueued(bot, ROLE_HEALER, dungeons, ", switched");
                }
            }
        };

        // No tank-spec bot is free: a free damage bot of a class with a tank spec (warrior, paladin, death knight, druid, monk)
        // switches to it, as a hybrid does for the healer. Only damage specs: a healer is not taken from its slot.
        auto switchTankFromFree = [&]()
        {
            if (!settings.tankSwitch)
                return;

            for (auto const& entry : order)
            {
                if (!joinsLeft || !missing[ROLE_TANK])
                    break;

                Player* bot = entry.second;
                if (!bot || sLFGMgr->GetState(bot->GetGUID()) != lfg::LFG_STATE_NONE ||
                    TankSpecOf(bot->getClass()) == SPEC_NONE || bot->getLevel() < MIN_SWITCH_LEVEL ||
                    RoleIndexOf(LfgGetBotRoles(bot)) != ROLE_DAMAGE)
                    continue;

                LfgDungeonSet const dungeons = DungeonsFor(bot, request, settings, anyLevel, anyLevelRequests, ROLE_TANK);
                if (dungeons.empty())
                    continue;

                uint32 const roleMask = SwitchToTanking(bot, request);
                if (!roleMask || !LfgQueueBotNow(bot, roleMask, dungeons))
                {
                    refusedUntil[bot->GetGUIDLow()] = now + REFUSED_BACKOFF;
                    continue;
                }

                recordQueued(bot, ROLE_TANK, dungeons, ", switched");
            }
        };

        // A native tank or healer may leave a bot-only questing party when the band has none free.
        auto fillFromParty = [&](int role)
        {
            uint32 freeInBand = 0;
            for (Player* bot : freeBots)
                if (bot && bot->GetTeamId() == request.team && sLFGMgr->GetState(bot->GetGUID()) == lfg::LFG_STATE_NONE &&
                    LfgFillPolicy::InReach(bot->getLevel(), request.level, settings.levelWindow, MIN_FINDER_LEVEL) &&
                    RoleIndexOf(LfgGetBotRoles(bot)) == role)
                    ++freeInBand;
            if (!joinsLeft || !LfgFillPolicy::RoleShortage(role, missing[role], freeInBand))
                return;

            for (Player* bot : bots)
            {
                if (!FinderBot(bot) || bot->GetTeamId() != request.team ||
                    !LfgFillPolicy::InReach(bot->getLevel(), request.level, settings.levelWindow, MIN_FINDER_LEVEL) ||
                    RoleIndexOf(LfgGetBotRoles(bot)) != role)
                    continue;
                Group* group = bot->GetGroup();
                if (!group)
                    continue;
                bool allBots = true;
                for (auto const& slot : group->GetMemberSlots())
                {
                    Player* member = ObjectAccessor::FindPlayer(slot.guid);
                    if (!member || !sRandomPlayerbotMgr->IsRandomBot(member) || IsHuman(member))
                    {
                        allBots = false;
                        break;
                    }
                }
                uint32 drink = 0;
                BotSalePolicy::FinderSupply supply = BotSalePolicy::FinderSupply::Ready;
                bool const otherwiseFree = HoldOf(bot, now, supply, drink, true) == LfgFillPolicy::Hold::Free;
                if (!LfgFillPolicy::PartyEligible(true, allBots, (group->isLFGGroup() || group->isBGGroup() || group->isRaidGroup()), otherwiseFree))
                    continue;
                LfgDungeonSet const dungeons = DungeonsFor(bot, request, settings, anyLevel, anyLevelRequests, role);
                if (dungeons.empty())
                    continue;
                Player* leader = ObjectAccessor::FindPlayer(group->GetLeaderGUID());
                if (!leader)
                    continue;
                std::string const leaderName = leader->GetName();
                PlayerbotAI* ai = GET_PLAYERBOT_AI(bot);
                char const* const lines[] = { "I have to go, thanks for the group!", "Off to a dungeon, see you later!", "Thanks everyone, take care!" };
                if (!BotVoice::Say(ai, "lfg_fill_party_bye", lines[urand(0, 2)], {}, BotVoice::Channel::PARTY,
                                   BotVoice::Priority::NOTICE, 100))
                    continue;
                bot->RemoveFromGroup();
                if (bot->GetGroup())
                    continue;
                ai->SetMaster(nullptr);
                ai->ResetStrategies();
                ai->Reset();
                SF_LOG_INFO("playerbots", "[LFGFILL] %s leaves party of %s to fill a %s slot for %s",
                            bot->GetName().c_str(), leaderName.c_str(), RoleName(role), request.player->GetName().c_str());
                if (!LfgQueueBotNow(bot, LfgGetBotRoles(bot), dungeons))
                    refusedUntil[bot->GetGUIDLow()] = now + REFUSED_BACKOFF;
                else
                    recordQueued(bot, role, dungeons, "");
                return;
            }
        };

        // The healer first: a free hybrid is the only healer many level bands have, so the damage fill must not take it
        // as a dps. Native healer specs, then a switch.
        if (missing[ROLE_HEALER])
        {
            fillFromFree(ROLE_HEALER);
            fillFromParty(ROLE_HEALER);
            switchFromFree();
        }

        // The tank next, before the damage fill takes the free damage bots a switch would need: native tank specs, then a
        // damage bot of a class that can tank.
        if (missing[ROLE_TANK])
        {
            fillFromFree(ROLE_TANK);
            fillFromParty(ROLE_TANK);
            switchTankFromFree();
        }

        fillFromFree(-1);

        // Still no healer and the group is otherwise complete: a hybrid bot an earlier pass queued as dps leaves the queue,
        // switches to healing and queues again as the healer, and a free damage bot takes its place.
        if (missing[ROLE_HEALER] && !missing[ROLE_TANK] && joinsLeft)
        {
            bool freeDamageBot = false;
            for (auto const& entry : order)
                if (entry.second && sLFGMgr->GetState(entry.second->GetGUID()) == lfg::LFG_STATE_NONE &&
                    RoleIndexOf(LfgGetBotRoles(entry.second)) == ROLE_DAMAGE &&
                    !DungeonsFor(entry.second, request, settings, anyLevel, anyLevelRequests).empty())
                    freeDamageBot = true;

            // Copied: queueing a bot below adds to filledAt.
            std::vector<uint32> queuedByUs;
            for (auto const& entry : filledAt)
                queuedByUs.push_back(entry.first);

            for (uint32 const low : queuedByUs)
            {
                if (!freeDamageBot || !missing[ROLE_HEALER])
                    break;

                Player* bot = sRandomPlayerbotMgr->GetPlayerBot(low);
                if (!bot || bot->GetTeamId() != request.team || bot->GetGroup() || HealingSpecOf(bot->getClass()) == SPEC_NONE ||
                    bot->getLevel() < MIN_SWITCH_LEVEL || sLFGMgr->GetState(bot->GetGUID()) != lfg::LFG_STATE_QUEUED ||
                    RoleOfQueued(bot->GetGUID()) != ROLE_DAMAGE)
                    continue;

                Pooled* pooled = nullptr;
                for (Pooled& entry : pool)
                    if (entry.guid == bot->GetGUID() && entry.role == ROLE_DAMAGE && Intersects(entry.dungeons, request.dungeons))
                        pooled = &entry;
                if (!pooled)
                    continue;

                LfgDungeonSet const dungeons = DungeonsFor(bot, request, settings, anyLevel, anyLevelRequests);
                if (dungeons.empty())
                    continue;

                sLFGMgr->LeaveLfg(bot->GetGUID());
                uint32 const roleMask = SwitchToHealing(bot, request);
                if (!roleMask || !LfgQueueBotNow(bot, roleMask, dungeons))
                {
                    refusedUntil[bot->GetGUIDLow()] = now + REFUSED_BACKOFF;
                    continue;
                }

                pooled->role = ROLE_HEALER;
                pooled->dungeons = dungeons;
                --missing[ROLE_HEALER];
                ++missing[ROLE_DAMAGE];
                ++queued;
                filledAt[bot->GetGUIDLow()] = now;
                if (!queuedNames.empty())
                    queuedNames += ", ";
                queuedNames += bot->GetName() + " (healer " + std::to_string(bot->getLevel()) + ", re-queued from dps)";
                fillFromFree(ROLE_DAMAGE);
            }
        }

        if (queued)
        {
            SF_LOG_INFO("playerbots", "[LFGFILL] %s (level %u) waits for %s: queued %u bot(s): %s. Still missing: %u tank, %u healer, %u dps",
                        request.player->GetName().c_str(), request.level, DungeonNames(request.dungeons).c_str(), queued,
                        queuedNames.c_str(), missing[ROLE_TANK], missing[ROLE_HEALER], missing[ROLE_DAMAGE]);
            noBotLogAt.erase(request.player->GetGUID());
            return;
        }

        // Nothing to give: no free bot of that side, level and role. Said now and then, so it can be seen in the log.
        time_t& lastLog = noBotLogAt[request.player->GetGUID()];
        if (joinsLeft && now >= lastLog + NO_BOT_LOG_GAP)
        {
            lastLog = now;
            SF_LOG_INFO("playerbots", "[LFGFILL] %s (level %u) waits for %s: no free bot fits. Missing: %u tank, %u healer, %u dps",
                        request.player->GetName().c_str(), request.level, DungeonNames(request.dungeons).c_str(),
                        missing[ROLE_TANK], missing[ROLE_HEALER], missing[ROLE_DAMAGE]);
            SF_LOG_INFO("playerbots", "[LFGFILL] %s (level %u) sees %s", request.player->GetName().c_str(), request.level,
                        CensusOf(request.player, request.level, request.team, bots, settings, now).c_str());
        }
    }

    // The policy mirrors the core's finder states by number.
    static_assert(uint8(lfg::LFG_STATE_NONE) == uint8(LfgFillPolicy::QueueState::None) &&
                  uint8(lfg::LFG_STATE_ROLECHECK) == uint8(LfgFillPolicy::QueueState::RoleCheck) &&
                  uint8(lfg::LFG_STATE_QUEUED) == uint8(LfgFillPolicy::QueueState::Queued) &&
                  uint8(lfg::LFG_STATE_PROPOSAL) == uint8(LfgFillPolicy::QueueState::Proposal) &&
                  uint8(lfg::LFG_STATE_BOOT) == uint8(LfgFillPolicy::QueueState::Boot) &&
                  uint8(lfg::LFG_STATE_DUNGEON) == uint8(LfgFillPolicy::QueueState::Dungeon) &&
                  uint8(lfg::LFG_STATE_FINISHED_DUNGEON) == uint8(LfgFillPolicy::QueueState::FinishedDungeon) &&
                  uint8(lfg::LFG_STATE_RAIDBROWSER) == uint8(LfgFillPolicy::QueueState::RaidBrowser),
                  "LfgFillPolicy::QueueState must follow lfg::LfgState");

    // A bot this pass queued leaves the queue when no real request wants its dungeons any more, or when it waited too
    // long. It is forgotten once it is out of the queue, and kept while a role check or proposal holds it (BT-205).
    void ReleaseStaleBots(std::vector<Pooled> const& pool, std::vector<Request> const& requests,
                          Settings const& settings, time_t now)
    {
        for (auto itr = filledAt.begin(); itr != filledAt.end();)
        {
            Player* bot = sRandomPlayerbotMgr->GetPlayerBot(itr->first);
            LfgFillPolicy::Track const track = bot ?
                LfgFillPolicy::TrackQueuedBot(LfgFillPolicy::QueueState(uint8(sLFGMgr->GetState(bot->GetGUID())))) :
                LfgFillPolicy::Track::Forget;
            if (track == LfgFillPolicy::Track::Forget)
            {
                inProposal.erase(itr->first);
                itr = filledAt.erase(itr);
                continue;
            }

            if (track == LfgFillPolicy::Track::Wait)
            {
                inProposal.insert(itr->first);
                ++itr;
                continue;
            }

            if (LfgFillPolicy::BackFromFailedProposal(inProposal.erase(itr->first) != 0, track))
                SF_LOG_INFO("playerbots", "[LFGFILL] %s is back in the finder queue after a failed proposal", bot->GetName().c_str());

            bool wanted = false;
            for (Pooled const& entry : pool)
            {
                if (entry.guid != bot->GetGUID())
                    continue;

                for (Request const& request : requests)
                    if (request.team == entry.team && Intersects(request.dungeons, entry.dungeons))
                        wanted = true;
            }

            {
                std::lock_guard<std::mutex> guard(runLock);
                if (botRunBots.count(itr->first))
                    wanted = true;
            }

            if (!wanted || now >= itr->second + time_t(settings.queueTimeoutSeconds))
            {
                SF_LOG_INFO("playerbots", "[LFGFILL] %s leaves the finder queue (%s)", bot->GetName().c_str(),
                            wanted ? "waited too long" : "nobody is waiting for its dungeons");
                sLFGMgr->LeaveLfg(bot->GetGUID());
                itr = filledAt.erase(itr);
                continue;
            }

            ++itr;
        }
    }

    bool PartyActive(Party const& party)
    {
        for (uint32 const low : party.members)
        {
            Player* bot = sRandomPlayerbotMgr->GetPlayerBot(low);
            if (!bot)
                continue;

            if (sLFGMgr->GetState(bot->GetGUID()) != lfg::LFG_STATE_NONE)
                return true;

            Group* group = bot->GetGroup();
            if (group && group->isLFGGroup())
                return true;
        }
        return false;
    }

    // The time limit of a party's dungeon (its map may be one of the long ones).
    uint32 RunSecondsOf(Settings const& settings, uint32 dungeonId)
    {
        lfg::LFGDungeonData const* dungeon = sLFGMgr->GetLFGDungeon(dungeonId);
        return dungeon && LongRunMaps().count(dungeon->map) ? std::max(settings.maxRunSeconds, LongRunSeconds()) : settings.maxRunSeconds;
    }

    bool PartyHasHuman(Party const& party)
    {
        for (uint32 const low : party.members)
            if (Player* bot = sRandomPlayerbotMgr->GetPlayerBot(low))
                if (Group* group = bot->GetGroup())
                    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
                        if (IsHuman(ref->GetSource()))
                            return true;
        return false;
    }

    void ManageParties(Settings const& settings, time_t now)
    {
        for (auto itr = parties.begin(); itr != parties.end();)
        {
            bool forget = false;
            if (itr->wrappedAt)
                forget = now >= itr->wrappedAt + WRAP_UP_GRACE || !PartyActive(*itr);
            else if (now >= itr->startedAt + PARTY_FORGET_AFTER && !PartyActive(*itr))
                forget = true;
            else if (LfgLeavePolicy::MayTimeOut(
                    now >= itr->startedAt + time_t(RunSecondsOf(settings, itr->dungeonId)), PartyHasHuman(*itr)))
            {
                itr->wrappedAt = now;
                SF_LOG_INFO("playerbots", "[BOTPARTY] dungeon %u ran %u minutes, the party wraps up", itr->dungeonId,
                            RunSecondsOf(settings, itr->dungeonId) / SECONDS_PER_MINUTE);
                std::lock_guard<std::mutex> guard(runLock);
                for (uint32 const low : itr->members)
                    wrapUpBots.insert(low);
            }

            if (!forget)
            {
                ++itr;
                continue;
            }

            {
                std::lock_guard<std::mutex> guard(runLock);
                for (uint32 const low : itr->members)
                {
                    botRunBots.erase(low);
                    wrapUpBots.erase(low);
                }
            }
            SF_LOG_INFO("playerbots", "[BOTPARTY] party of dungeon %u is over", itr->dungeonId);
            itr = parties.erase(itr);
        }
    }

    // Five free bots of one side near one level with the roles of a group, for one dungeon.
    bool PickParty(lfg::LFGDungeonData const& dungeon, TeamId team, std::vector<Player*> const& freeBots,
                   Settings const& settings, std::vector<std::pair<Player*, int>>& members)
    {
        std::vector<Player*> candidates;
        for (Player* bot : freeBots)
            if (bot && bot->GetTeamId() == team && InLevelRange(dungeon, bot->getLevel()) &&
                bot->getLevel() <= uint32(dungeon.minlevel) + 10)
                candidates.push_back(bot);

        if (candidates.size() < MAXGROUPSIZE)
            return false;

        Player* anchor = candidates[urand(0, uint32(candidates.size() - 1))];
        std::vector<Player*> byRole[ROLE_COUNT];
        for (Player* bot : candidates)
        {
            if (uint32(std::abs(int32(bot->getLevel()) - int32(anchor->getLevel()))) > settings.partyLevelWindow)
                continue;

            int const role = RoleIndexOf(LfgGetBotRoles(bot));
            if (role >= 0)
                byRole[role].push_back(bot);
        }

        for (int role = 0; role < ROLE_COUNT; ++role)
            if (byRole[role].size() < ROLE_WANTED[role])
                return false;

        uint32 remaining[ROLE_COUNT];
        for (int role = 0; role < ROLE_COUNT; ++role)
            remaining[role] = uint32(byRole[role].size()) - ROLE_WANTED[role];
        if (!LfgFillPolicy::KeepsFreeGroup(remaining[0], remaining[1], remaining[2]))
        {
            uint32 const lo = anchor->getLevel() > settings.partyLevelWindow ? anchor->getLevel() - settings.partyLevelWindow : 0;
            uint32 const hi = anchor->getLevel() + settings.partyLevelWindow;
            SF_LOG_INFO("playerbots", "[LFGFILL] bot run skipped: band %u-%u would keep no free group (tank %u, healer %u, dps %u)",
                        lo, hi, remaining[0], remaining[1], remaining[2]);
            return false;
        }

        members.clear();
        for (int role = 0; role < ROLE_COUNT; ++role)
        {
            Shuffle(byRole[role]);
            for (uint32 i = 0; i < ROLE_WANTED[role]; ++i)
                members.emplace_back(byRole[role][i], role);
        }
        return true;
    }

    void StartBotRun(std::vector<Player*> const& freeBots, Settings const& settings, time_t now)
    {
        std::vector<uint32> dungeonIds;
        for (uint32 const id : settings.botRunDungeons)
        {
            lfg::LFGDungeonData const* dungeon = sLFGMgr->GetLFGDungeon(id);
            if (dungeon && dungeon->type == lfg::LFG_TYPE_DUNGEON && !dungeon->seasonal && !LfgFill::DungeonCoolingDown(id))
                dungeonIds.push_back(id);
        }
        if (dungeonIds.empty())
            return;

        Shuffle(dungeonIds);
        TeamId const firstTeam = urand(0, 1) ? TEAM_ALLIANCE : TEAM_HORDE;
        uint32 tries = 0;
        for (uint32 const id : dungeonIds)
        {
            lfg::LFGDungeonData const* dungeon = sLFGMgr->GetLFGDungeon(id);
            for (int side = 0; side < 2 && tries < BOT_RUN_TRIES; ++side, ++tries)
            {
                TeamId const team = side == 0 ? firstTeam : (firstTeam == TEAM_ALLIANCE ? TEAM_HORDE : TEAM_ALLIANCE);
                std::vector<std::pair<Player*, int>> members;
                if (!PickParty(*dungeon, team, freeBots, settings, members))
                    continue;

                LfgDungeonSet const set = { dungeon->id };
                Party party;
                party.dungeonId = dungeon->id;
                party.startedAt = now;
                bool failed = false;
                for (auto const& member : members)
                {
                    if (!LfgQueueBotNow(member.first, LfgGetBotRoles(member.first), set))
                    {
                        refusedUntil[member.first->GetGUIDLow()] = now + REFUSED_BACKOFF;
                        failed = true;
                        break;
                    }
                    party.members.push_back(member.first->GetGUIDLow());
                    filledAt[member.first->GetGUIDLow()] = now;
                }

                if (failed)
                {
                    for (uint32 const low : party.members)
                    {
                        if (Player* bot = sRandomPlayerbotMgr->GetPlayerBot(low))
                            sLFGMgr->LeaveLfg(bot->GetGUID());
                        filledAt.erase(low);
                    }
                    continue;
                }

                {
                    std::lock_guard<std::mutex> guard(runLock);
                    for (uint32 const low : party.members)
                        botRunBots.insert(low);
                }

                std::string names;
                for (auto const& member : members)
                {
                    LogLowDrinkJoin(member.first);
                    if (!names.empty())
                        names += ", ";
                    names += member.first->GetName() + " (" + RoleName(member.second) + " " +
                        std::to_string(member.first->getLevel()) + ")";
                }
                SF_LOG_INFO("playerbots", "[BOTPARTY] five bots of the %s queue for %s: %s",
                            team == TEAM_ALLIANCE ? "Alliance" : "Horde", dungeon->name.c_str(), names.c_str());
                parties.push_back(std::move(party));
                return;
            }
        }
    }
}  // namespace

int LfgFill::PickSpecRole(uint32 guid, uint32 team, uint32 level, bool const available[3])
{
    if (!sConfigMgr->GetBoolDefault("AiPlayerbot.SpecRoleBalance.Enable", true))
        return -1;
    uint32 shares[3] = {
        uint32(std::max(0, std::min(100, sConfigMgr->GetIntDefault("AiPlayerbot.SpecRoleBalance.TankShare", 25)))),
        uint32(std::max(0, std::min(100, sConfigMgr->GetIntDefault("AiPlayerbot.SpecRoleBalance.HealerShare", 20)))),
        uint32(std::max(0, std::min(100, sConfigMgr->GetIntDefault("AiPlayerbot.SpecRoleBalance.DamageShare", 55)))) };
    std::lock_guard<std::mutex> guard(specCensusLock);
    if (!specCensusReady)
        return -1;
    time_t const now = time(nullptr);
    specReservations.erase(std::remove_if(specReservations.begin(), specReservations.end(),
        [now, guid](SpecCensusEntry const& e) { return e.expires <= now || e.guid == guid; }), specReservations.end());
    uint32 counts[3] = {};
    uint32 const targetLevel = std::max(uint32(15), level);
    for (SpecCensusEntry const& e : specCensus)
    {
        if (e.guid == guid || e.team != team || !LfgFillPolicy::InReach(e.level, targetLevel, specWindow, MIN_FINDER_LEVEL))
            continue;
        bool reserved = false;
        for (SpecCensusEntry const& r : specReservations)
            if (r.guid == e.guid) { reserved = true; break; }
        if (!reserved)
            ++counts[e.role];
    }
    for (SpecCensusEntry const& e : specReservations)
        if (e.team == team && LfgFillPolicy::InReach(e.level, targetLevel, specWindow, MIN_FINDER_LEVEL))
            ++counts[e.role];
    int const role = SpecRolePolicy::Choose(counts, shares, available, targetLevel);
    if (role >= 0)
        specReservations.push_back({ guid, team, targetLevel, uint32(role), now + 60 });
    return role;
}

bool LfgFill::Enabled() { return EnabledFlag().load(std::memory_order_relaxed); }

uint32 LfgFill::MaxRunSeconds() { return maxRunSeconds.load(std::memory_order_relaxed); }

uint32 LfgFill::MaxRunSecondsFor(uint32 mapId)
{
    uint32 const normal = MaxRunSeconds();
    return LongRunMaps().count(mapId) ? std::max(normal, LongRunSeconds()) : normal;
}

bool LfgFill::IsBotRun(uint32 botLowGuid)
{
    std::lock_guard<std::mutex> guard(runLock);
    return botRunBots.count(botLowGuid) != 0;
}

bool LfgFill::WrapUpRequested(uint32 botLowGuid)
{
    std::lock_guard<std::mutex> guard(runLock);
    return wrapUpBots.count(botLowGuid) != 0;
}

void LfgFill::NoteRunFailed(uint32 dungeonId, std::vector<uint32> const& finderBotLowGuids)
{
    uint32 const hours = failureCooldownHours.load(std::memory_order_relaxed);
    {
        std::lock_guard<std::mutex> guard(runLock);
        time_t const until = time_t(LfgStallPolicy::CooldownEnd(time(nullptr), hours));
        if (until)
            failedDungeonUntil[dungeonId] = until;
        // The party's own pass erases these when the bots have left (ManageParties).
        for (uint32 const low : finderBotLowGuids)
            wrapUpBots.insert(low);
    }
    SF_LOG_INFO("playerbots", "[BOTPARTY] dungeon %u: no boss can be reached, the run wraps up%s", dungeonId,
                hours ? ", and the dungeon is left alone for a while" : "");
}

bool LfgFill::DungeonCoolingDown(uint32 dungeonId)
{
    std::lock_guard<std::mutex> guard(runLock);
    auto const itr = failedDungeonUntil.find(dungeonId);
    return itr != failedDungeonUntil.end() && LfgStallPolicy::CoolingDown(time(nullptr), itr->second);
}

void LfgFill::Update()
{
    // The settings are read once a pass, not on every world tick.
    static uint32 intervalSeconds = 10;
    time_t const now = time(nullptr);
    if (lastPassAt && now < lastPassAt + time_t(intervalSeconds))
        return;
    lastPassAt = now;

    Settings const settings = ReadSettings();
    intervalSeconds = settings.intervalSeconds;
    EnabledFlag().store(settings.enable, std::memory_order_relaxed);
    maxRunSeconds.store(settings.maxRunSeconds, std::memory_order_relaxed);
    failureCooldownHours.store(settings.failureCooldownHours, std::memory_order_relaxed);
    // Collect live state only on the world thread; map threads read counts under the lock.
    std::vector<Player*> const bots = sRandomPlayerbotMgr->GetPlayerBotsSnapshot();
    std::vector<SpecCensusEntry> census;
    for (Player* candidate : bots)
    {
        if (!FinderBot(candidate) || BotGetSpecialization(candidate) == SPEC_NONE)
            continue;
        uint32 drink = 0;
        BotSalePolicy::FinderSupply supply = BotSalePolicy::FinderSupply::Ready;
        if (HoldOf(candidate, now, supply, drink) != LfgFillPolicy::Hold::Free)
            continue;
        int const role = RoleIndexOf(LfgGetBotRoles(candidate));
        if (role >= 0 && role < ROLE_COUNT)
            census.push_back({ candidate->GetGUIDLow(), uint32(candidate->GetTeamId()), candidate->getLevel(), uint32(role), 0 });
    }
    {
        std::lock_guard<std::mutex> guard(specCensusLock);
        specCensus.swap(census);
        specWindow = settings.levelWindow;
        specCensusReady = true;
    }
    if (!settings.enable || !sPlayerbotAIConfig->randomBotJoinLfg)
        return;

    lowDrink.clear();

    // Expired refusals.
    for (auto itr = refusedUntil.begin(); itr != refusedUntil.end();)
        itr = itr->second <= now ? refusedUntil.erase(itr) : std::next(itr);

    std::vector<Player*> const players = sRandomPlayerbotMgr->GetPlayers();
    std::vector<Request> requests;
    std::vector<Pooled> pool;
    CollectRequests(players, requests, pool);

    // Bots already queued (by this pass) count for the roles of the requests on their side.
    for (Player* bot : bots)
    {
        if (!bot || !bot->IsInWorld() || bot->GetGroup() || !sRandomPlayerbotMgr->IsRandomBot(bot))
            continue;

        if (sLFGMgr->GetState(bot->GetGUID()) != lfg::LFG_STATE_QUEUED)
            continue;

        lfg::LfgDungeonSet const& selected = sLFGMgr->GetSelectedDungeons(bot->GetGUID());
        if (!selected.empty())
            pool.push_back({ bot->GetGUID(), bot->GetTeamId(), RoleOfQueued(bot->GetGUID()), selected, true });
    }

    ManageParties(settings, now);

    if (!requests.empty())
    {
        // Free bots, only when there is someone to fill for.
        std::vector<Player*> freeBots;
        for (Player* bot : bots)
            if (FreeForFinder(bot, now))
                freeBots.push_back(bot);
        Shuffle(freeBots);

        bool const anyLevel = LfgAnyLevel::Enabled();
        std::vector<LfgAnyLevel::Request> anyLevelRequests;
        if (anyLevel)
        {
            for (Request const& request : requests)
                for (uint32 const dungeonId : request.dungeons)
                    if (lfg::LFGDungeonData const* dungeon = sLFGMgr->GetLFGDungeon(dungeonId))
                        anyLevelRequests.push_back({ request.team, dungeon->id, uint8(request.level),
                                                     !InLevelRange(*dungeon, request.level), request.itemLevel });
        }

        uint32 joinsLeft = settings.maxJoinsPerPass;
        for (Request const& request : requests)
            FillRequest(request, freeBots, bots, pool, settings, now, anyLevel, anyLevelRequests, joinsLeft);
    }

    ReleaseStaleBots(pool, requests, settings, now);

    // Bot-only runs: only while no player waits, within the cap, and on their own schedule.
    if (settings.botRuns && settings.maxParties && requests.empty() && parties.size() < settings.maxParties)
    {
        if (!nextBotRunAt)
        {
            nextBotRunAt = now + time_t(urand(settings.minIntervalSeconds, settings.maxIntervalSeconds));
        }
        else if (now >= nextBotRunAt)
        {
            nextBotRunAt = now + time_t(urand(settings.minIntervalSeconds, settings.maxIntervalSeconds));
            std::vector<Player*> freeBots;
            for (Player* bot : bots)
                if (FreeForFinder(bot, now))
                    freeBots.push_back(bot);
            Shuffle(freeBots);
            StartBotRun(freeBots, settings, now);
        }
    }
}
