/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE). See BotBreak.h.
 */

#include "BotBreak.h"

#include "BotLfm.h"
#include "BotVoice.h"
#include "CellImpl.h"
#include "Config.h"
#include "DBCStores.h"
#include "Group.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "LFGMgr.h"
#include "Log.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RandomPlayerbotMgr.h"
#include "Random.h"
#include "Timer.h"

#include <algorithm>
#include <atomic>
#include <ctime>
#include <list>
#include <map>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace
{
    uint32 const SECONDS_PER_MINUTE = 60;
    time_t const PASS_SECONDS = 5;
    // An offer the bot did not pick up on its own tick in this time is dropped (idle ticks are slow).
    uint32 const OFFER_LIFETIME_MS = 60 * IN_MILLISECONDS;
    // A break still running this long after its time is ended by the world thread (the bot is on idle ticks).
    uint32 const OVERDUE_MS = 20 * IN_MILLISECONDS;
    // The hold-still tick of a bot on break.
    uint32 const HOLD_TICK_MS = 3000;
    // Solo breaks look around this far for anything hostile when they are not in a town, group breaks this far.
    float const SOLO_SAFE_RADIUS = 30.0f;
    float const GROUP_SAFE_RADIUS = 45.0f;
    // A group break ends when the player walks this far from the bot.
    float const MASTER_FAR_DISTANCE = 40.0f;
    // A bot that turned an offer down is not offered another for this long.
    uint32 const REFUSED_COOLDOWN_S = 5 * SECONDS_PER_MINUTE;
    uint32 const FULL_PERCENT = 100;
    // The in-world countdown of a bot that is going to log off: long enough for a goodbye.
    uint32 const LOGOFF_DELAY_MIN_S = 30;
    uint32 const LOGOFF_DELAY_MAX_S = 90;
    size_t const TRIM_SIZE = 512;

    struct Settings
    {
        bool enable = true;
        uint32 soloMinutesBetween = 120;
        uint32 soloMinMinutes = 2;
        uint32 soloMaxMinutes = 10;
        bool soloOutdoors = true;
        uint32 soloLineChance = 60;
        uint32 groupMinutesBetween = 240;
        uint32 groupMinMinutes = 1;
        uint32 groupMaxMinutes = 3;
        bool groupInInstance = true;
        uint32 bodyCooldownS = 30 * SECONDS_PER_MINUTE;
        uint32 logoffPerHour = 4;
        uint32 logoffMinOnline = 50;
        uint32 awayMinMinutes = 20;
        uint32 awayMaxMinutes = 120;
    };

    Settings const& Cfg()
    {
        static Settings const settings = []
        {
            Settings s;
            s.enable = sConfigMgr->GetBoolDefault("AiPlayerbot.BotBreaks.Enable", true);
            s.soloMinutesBetween = uint32(std::max(1, sConfigMgr->GetIntDefault("AiPlayerbot.BotBreaks.SoloMinutesBetween", 120)));
            s.soloMinMinutes = uint32(std::max(1, sConfigMgr->GetIntDefault("AiPlayerbot.BotBreaks.SoloMinMinutes", 2)));
            s.soloMaxMinutes = std::max(s.soloMinMinutes, uint32(sConfigMgr->GetIntDefault("AiPlayerbot.BotBreaks.SoloMaxMinutes", 10)));
            s.soloOutdoors = sConfigMgr->GetBoolDefault("AiPlayerbot.BotBreaks.SoloOutdoors", true);
            s.soloLineChance = uint32(std::min(100, std::max(0, sConfigMgr->GetIntDefault("AiPlayerbot.BotBreaks.SoloLineChance", 60))));
            s.groupMinutesBetween = uint32(std::max(1, sConfigMgr->GetIntDefault("AiPlayerbot.BotBreaks.GroupMinutesBetween", 240)));
            s.groupMinMinutes = uint32(std::max(1, sConfigMgr->GetIntDefault("AiPlayerbot.BotBreaks.GroupMinMinutes", 1)));
            s.groupMaxMinutes = std::max(s.groupMinMinutes, uint32(sConfigMgr->GetIntDefault("AiPlayerbot.BotBreaks.GroupMaxMinutes", 3)));
            s.groupInInstance = sConfigMgr->GetBoolDefault("AiPlayerbot.BotBreaks.GroupInInstance", true);
            s.bodyCooldownS = uint32(std::max(1, sConfigMgr->GetIntDefault("AiPlayerbot.BotBreaks.CooldownMinutes", 30))) * SECONDS_PER_MINUTE;
            s.logoffPerHour = uint32(std::max(0, sConfigMgr->GetIntDefault("AiPlayerbot.BotBreaks.LogoffPerHour", 4)));
            s.logoffMinOnline = uint32(std::max(0, sConfigMgr->GetIntDefault("AiPlayerbot.BotBreaks.LogoffMinOnline", 50)));
            s.awayMinMinutes = uint32(std::max(1, sConfigMgr->GetIntDefault("AiPlayerbot.BotBreaks.AwayMinMinutes", 20)));
            s.awayMaxMinutes = std::max(s.awayMinMinutes, uint32(sConfigMgr->GetIntDefault("AiPlayerbot.BotBreaks.AwayMaxMinutes", 120)));
            return s;
        }();
        return settings;
    }

    enum class State : uint8
    {
        OFFERED,
        ON
    };

    struct Break
    {
        State state = State::OFFERED;
        bool group = false;
        uint32 offeredMs = 0;
        uint32 durationMs = 0;
        uint32 endMs = 0;
        uint32 startMs = 0;
    };

    std::mutex lock;
    std::unordered_map<uint32, Break> breaks;
    std::unordered_map<uint32, time_t> nextAllowed;  // per bot: not offered another break before this
    std::atomic<uint32> breakCount{ 0 };
    time_t lastPassAt = 0;

    // Whole numbers out of an expected fractional count: the integer part, plus one with the fraction as chance.
    uint32 RollCount(double expected)
    {
        uint32 count = uint32(expected);
        if (urand(0, 9999) < uint32((expected - double(count)) * 10000.0))
            ++count;
        return count;
    }

    bool InTown(Player* bot)
    {
        if (bot->GetRestType() != REST_TYPE_NO)
            return true;

        AreaTableEntry const* zone = GetAreaEntryByAreaID(bot->GetZoneId());
        return zone && (zone->m_flags & AREA_FLAG_CAPITAL);
    }

    // Nothing hostile and awake within the radius.
    bool NothingHostileNear(Player* bot, float radius)
    {
        std::list<Unit*> units;
        Skyfire::AnyUnfriendlyUnitInObjectRangeCheck check(bot, bot, radius);
        Skyfire::UnitListSearcher<Skyfire::AnyUnfriendlyUnitInObjectRangeCheck> searcher(bot, units, check);
        bot->VisitNearbyObject(radius, searcher);
        for (Unit* unit : units)
            if (unit && unit != bot && unit->IsAlive() && unit->GetCreatureType() != CREATURE_TYPE_CRITTER && bot->IsValidAttackTarget(unit))
                return false;
        return true;
    }

    bool AnyMemberFighting(Group* group)
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            if (Player* member = ref->GetSource())
                if (member->IsInCombat())
                    return true;
        return false;
    }

    bool AnyMemberOnBreak(Group* group)
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            if (Player* member = ref->GetSource())
                if (breaks.count(member->GetGUIDLow()))
                    return true;
        return false;
    }

    // The common conditions of a bot that may take a break now: nothing else is using it.
    bool Free(Player* bot, std::unordered_map<uint32, time_t> const& cooldowns, time_t now)
    {
        if (!bot || !bot->IsInWorld() || !bot->IsAlive() || bot->IsInCombat() || bot->IsInFlight() || bot->IsBeingTeleported() ||
            bot->InBattleground() || bot->InBattlegroundQueue() || bot->InArena() || bot->GetVehicle() || bot->duel)
            return false;

        uint32 const low = bot->GetGUIDLow();
        if (breaks.count(low) || BotLfm::IsLeader(low) || BotLfm::IsMember(low))
            return false;

        auto const itr = cooldowns.find(low);
        if (itr != cooldowns.end() && itr->second > now)
            return false;

        return sLFGMgr->GetState(bot->GetGUID()) == lfg::LFG_STATE_NONE;
    }

    void StartLine(PlayerbotAI* botAI, Break const& entry, uint32 minutes)
    {
        if (entry.group)
            BotVoice::Say(botAI, "voice_break_group_start", "brb %minutes min", { { "%minutes", std::to_string(minutes) } },
                          BotVoice::Channel::PARTY, BotVoice::Priority::NOTICE);
        else if (urand(1, FULL_PERCENT) <= Cfg().soloLineChance)
            BotVoice::Say(botAI, "voice_break_start", "brb, food", {}, BotVoice::Channel::SAY, BotVoice::Priority::NOTICE);
    }

    // Ends a break: flags and a line only, so it is safe from the world thread. `key` picks the line.
    void EndBreak(PlayerbotAI* botAI, Player* bot, char const* why)
    {
        Break entry;
        {
            std::lock_guard<std::mutex> guard(lock);
            auto itr = breaks.find(bot->GetGUIDLow());
            if (itr == breaks.end())
                return;

            entry = itr->second;
            breaks.erase(itr);
            breakCount.store(uint32(breaks.size()), std::memory_order_relaxed);
            nextAllowed[bot->GetGUIDLow()] = time(nullptr) + Cfg().bodyCooldownS;
            if (nextAllowed.size() >= TRIM_SIZE)
                nextAllowed.clear();
        }

        if (entry.state == State::ON)
        {
            if (bot->isAFK())
                bot->ToggleAFK();
            bot->autoReplyMsg.clear();
            if (bot->getStandState() != UNIT_STAND_STATE_STAND && !bot->IsInCombat())
                bot->SetStandState(UNIT_STAND_STATE_STAND);

            if (botAI && bot->IsAlive())
            {
                if (entry.group)
                    BotVoice::Say(botAI, std::string(why) == "fight" ? "voice_break_group_cut" : "voice_break_group_end",
                                  std::string(why) == "fight" ? "Back, sorry" : "Back", {}, BotVoice::Channel::PARTY,
                                  BotVoice::Priority::NOTICE);
                else if (urand(1, FULL_PERCENT) <= Cfg().soloLineChance)
                    BotVoice::Say(botAI, "voice_break_end", "Back", {}, BotVoice::Channel::SAY, BotVoice::Priority::NOTICE);
            }
        }

        SF_LOG_DEBUG("playerbots", "[BREAK] %s is back (%s, %u s)", bot->GetName().c_str(), why,
                     entry.state == State::ON ? getMSTimeDiff(entry.startMs, getMSTime()) / IN_MILLISECONDS : 0u);
    }

    bool BreakOffered(Player* bot)
    {
        std::lock_guard<std::mutex> guard(lock);
        return breaks.count(bot->GetGUIDLow()) != 0;
    }

    void Offer(Player* bot, bool group, Settings const& settings)
    {
        Break entry;
        entry.group = group;
        entry.offeredMs = getMSTime();
        uint32 const minutes = group ? urand(settings.groupMinMinutes, settings.groupMaxMinutes)
                                     : urand(settings.soloMinMinutes, settings.soloMaxMinutes);
        entry.durationMs = minutes * SECONDS_PER_MINUTE * IN_MILLISECONDS;
        breaks[bot->GetGUIDLow()] = entry;
        breakCount.store(uint32(breaks.size()), std::memory_order_relaxed);
        SF_LOG_DEBUG("playerbots", "[BREAK] %s is offered a %s break of %u min", bot->GetName().c_str(), group ? "group" : "solo",
                     minutes);
    }
}

namespace BotBreak
{
    bool Enabled() { return Cfg().enable; }

    bool Wants(uint32 botLowGuid)
    {
        if (!breakCount.load(std::memory_order_relaxed))
            return false;

        std::lock_guard<std::mutex> guard(lock);
        return breaks.count(botLowGuid) != 0;
    }

    bool IsOnBreak(uint32 botLowGuid)
    {
        if (!breakCount.load(std::memory_order_relaxed))
            return false;

        std::lock_guard<std::mutex> guard(lock);
        auto const itr = breaks.find(botLowGuid);
        return itr != breaks.end() && itr->second.state == State::ON;
    }

    void Tick(PlayerbotAI* botAI)
    {
        Player* bot = botAI->GetBot();
        Break entry;
        {
            std::lock_guard<std::mutex> guard(lock);
            auto const itr = breaks.find(bot->GetGUIDLow());
            if (itr == breaks.end())
                return;
            entry = itr->second;
        }

        uint32 const now = getMSTime();
        Group* group = bot->GetGroup();
        Player* master = botAI->GetMaster();

        if (entry.state == State::OFFERED)
        {
            // The bot looks for itself: quiet, safe and not busy, else it turns the offer down.
            bool ok = getMSTimeDiff(entry.offeredMs, now) < OFFER_LIFETIME_MS && bot->IsAlive() && !bot->IsInCombat() &&
                !bot->IsInFlight() && !bot->IsInWater() && !bot->IsFlying() && !bot->GetVehicle() && !bot->IsMounted();
            if (ok && entry.group)
                ok = group && !group->isBGGroup() && !AnyMemberFighting(group) && NothingHostileNear(bot, GROUP_SAFE_RADIUS);
            else if (ok)
                ok = !group && (InTown(bot) || (Cfg().soloOutdoors && NothingHostileNear(bot, SOLO_SAFE_RADIUS)));

            if (!ok)
            {
                std::lock_guard<std::mutex> guard(lock);
                breaks.erase(bot->GetGUIDLow());
                breakCount.store(uint32(breaks.size()), std::memory_order_relaxed);
                nextAllowed[bot->GetGUIDLow()] = time(nullptr) + REFUSED_COOLDOWN_S;
                return;
            }

            bot->StopMoving();
            bot->GetMotionMaster()->Clear();
            bot->SetStandState(UNIT_STAND_STATE_SIT);
            if (!bot->isAFK())
                bot->ToggleAFK();
            bot->autoReplyMsg = "brb";
            {
                std::lock_guard<std::mutex> guard(lock);
                auto itr = breaks.find(bot->GetGUIDLow());
                if (itr == breaks.end())
                    return;
                itr->second.state = State::ON;
                itr->second.startMs = now;
                itr->second.endMs = now + entry.durationMs;
                entry = itr->second;
            }

            StartLine(botAI, entry, entry.durationMs / (SECONDS_PER_MINUTE * IN_MILLISECONDS));
            SF_LOG_DEBUG("playerbots", "[BREAK] %s starts a %s break at zone %u", bot->GetName().c_str(),
                         entry.group ? "group" : "solo", bot->GetZoneId());
            botAI->SetNextCheckDelay(HOLD_TICK_MS);
            return;
        }

        // On break: a fight, the player walking off, the end of the time, or an AFK flag cleared by something else.
        if (!bot->isAFK())
        {
            EndBreak(botAI, bot, "woken");
            return;
        }

        if (!bot->IsAlive() || bot->IsInCombat() || (entry.group && group && AnyMemberFighting(group)))
        {
            EndBreak(botAI, bot, "fight");
            return;
        }

        if (entry.group && master && master->IsInWorld() &&
            (master->GetMapId() != bot->GetMapId() || bot->GetExactDist2d(master) > MASTER_FAR_DISTANCE))
        {
            EndBreak(botAI, bot, "player left");
            return;
        }

        if (getMSTimeDiff(entry.startMs, now) >= entry.durationMs)
        {
            EndBreak(botAI, bot, "time up");
            return;
        }

        if (bot->isMoving())
        {
            bot->StopMoving();
            bot->GetMotionMaster()->Clear();
        }
        if (bot->getStandState() != UNIT_STAND_STATE_SIT)
            bot->SetStandState(UNIT_STAND_STATE_SIT);
        botAI->SetNextCheckDelay(HOLD_TICK_MS);
    }

    void Update()
    {
        time_t const now = time(nullptr);
        if (lastPassAt && now < lastPassAt + PASS_SECONDS)
            return;
        lastPassAt = now;

        Settings const& settings = Cfg();
        if (!settings.enable)
            return;

        std::vector<Player*> const bots = sRandomPlayerbotMgr->GetPlayerBotsSnapshot();
        if (bots.empty())
            return;

        // Offers nobody picked up and breaks long overdue are cleared (an idle bot does not tick its AI often).
        std::vector<uint32> overdue;
        {
            std::lock_guard<std::mutex> guard(lock);
            uint32 const nowMs = getMSTime();
            for (auto itr = breaks.begin(); itr != breaks.end();)
            {
                Player* bot = sRandomPlayerbotMgr->GetPlayerBot(itr->first);
                bool const gone = !bot || !bot->IsInWorld();
                bool const staleOffer =
                    itr->second.state == State::OFFERED && getMSTimeDiff(itr->second.offeredMs, nowMs) > OFFER_LIFETIME_MS;
                if (itr->second.state == State::ON && !gone &&
                    getMSTimeDiff(itr->second.startMs, nowMs) > itr->second.durationMs + OVERDUE_MS)
                    overdue.push_back(itr->first);

                if (gone || staleOffer)
                    itr = breaks.erase(itr);
                else
                    ++itr;
            }
            breakCount.store(uint32(breaks.size()), std::memory_order_relaxed);
        }

        for (uint32 const low : overdue)
            if (Player* bot = sRandomPlayerbotMgr->GetPlayerBot(low))
                EndBreak(GET_PLAYERBOT_AI(bot), bot, "overdue");

        // Who may take a break now, split by solo and in a group with a player.
        std::vector<Player*> solo;
        std::vector<Player*> grouped;
        std::vector<Player*> leaving;
        uint32 online = 0;
        {
            std::lock_guard<std::mutex> guard(lock);
            for (Player* bot : bots)
            {
                if (!bot || !bot->IsInWorld() || !sRandomPlayerbotMgr->IsRandomBot(bot))
                    continue;

                ++online;
                if (!Free(bot, nextAllowed, now))
                    continue;

                PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
                Group* group = bot->GetGroup();
                if (!botAI)
                    continue;

                // Only a bot on full ticks can take a break offer up; an idle one would let it run out. Any free solo bot
                // can log off.
                bool const active = botAI->AllowActivity();
                if (!group && !bot->GetGroupInvite())
                {
                    Map* map = bot->GetMap();
                    if (map && !map->Instanceable())
                    {
                        leaving.push_back(bot);
                        if (active)
                            solo.push_back(bot);
                    }
                }
                else if (active && group && !group->isBGGroup() && botAI->HasRealPlayerMaster() &&
                         group->GetMembersCount() >= 2 &&
                         (settings.groupInInstance || !(bot->GetMap() && bot->GetMap()->Instanceable())) &&
                         !AnyMemberOnBreak(group) && !AnyMemberFighting(group))
                    grouped.push_back(bot);
            }

            // Solo: every bot takes a break about every soloMinutesBetween minutes.
            double const passesPerMinute = double(SECONDS_PER_MINUTE) / double(PASS_SECONDS);
            if (!solo.empty())
            {
                uint32 starts = RollCount(double(solo.size()) / (double(settings.soloMinutesBetween) * passesPerMinute));
                for (uint32 i = 0; i < std::min<uint32>(starts, 2) && !solo.empty(); ++i)
                {
                    size_t const index = urand(0, uint32(solo.size() - 1));
                    Offer(solo[index], false, settings);
                    solo.erase(solo.begin() + index);
                }
            }

            if (!grouped.empty())
            {
                uint32 starts = RollCount(double(grouped.size()) / (double(settings.groupMinutesBetween) * passesPerMinute));
                if (starts)
                {
                    Player* bot = grouped[urand(0, uint32(grouped.size() - 1))];
                    Offer(bot, true, settings);
                }
            }
        }

        // Log-offs: through the manager's own "add" and "logout" events.
        if (settings.logoffPerHour && online > settings.logoffMinOnline && !leaving.empty())
        {
            double const passesPerHour = 3600.0 / double(PASS_SECONDS);
            if (RollCount(double(settings.logoffPerHour) / passesPerHour))
            {
                Player* bot = leaving[urand(0, uint32(leaving.size() - 1))];
                if (!BreakOffered(bot))
                {
                    uint32 const delay = urand(LOGOFF_DELAY_MIN_S, LOGOFF_DELAY_MAX_S);
                    uint32 const away = urand(settings.awayMinMinutes, settings.awayMaxMinutes) * SECONDS_PER_MINUTE;
                    sRandomPlayerbotMgr->ScheduleLogoff(bot->GetGUIDLow(), delay, away);
                    {
                        std::lock_guard<std::mutex> guard(lock);
                        nextAllowed[bot->GetGUIDLow()] = now + delay + away;
                    }
                    BotVoice::Say(GET_PLAYERBOT_AI(bot), "voice_logoff", "Gotta go, bye all", {}, BotVoice::Channel::SAY,
                                  BotVoice::Priority::NOTICE);
                    SF_LOG_INFO("playerbots", "[BREAK] %s logs off in %u s for %u min", bot->GetName().c_str(), delay,
                                away / SECONDS_PER_MINUTE);
                }
            }
        }
    }
}
