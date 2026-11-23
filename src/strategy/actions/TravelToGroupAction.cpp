/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 *
 * Travel to the group (see TravelToGroupAction.h). The route work is NewRpgBaseAction::MoveFarTo: it walks over the
 * navmesh, and for another continent or a very long way it follows the travel graph by boat, zeppelin, elevator, tram,
 * flight path or portal (strategy/RPG/NewRpgRoamTransport.h). This action adds what a player in a group adds: it picks
 * the destination (the group leader's spot, kept for a while so a route is not rebuilt every step), says how long it
 * needs, reports on the way with a cooldown, gives up when it makes no progress, and says when it is there.
 */

#include "TravelToGroupAction.h"

#include "BotLfm.h"
#include "BotVoice.h"
#include "Config.h"
#include "DBCStores.h"
#include "Event.h"
#include "Group.h"
#include "ObjectMgr.h"
#include "Playerbots.h"
#include "PlayerbotTextMgr.h"
#include "Timer.h"
#include "WorldSession.h"

#include <cmath>
#include <map>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace
{
    struct TravelSettings
    {
        bool enabled;
        bool summonIntoDungeons;
        float distance;        // yards on the same map beyond which the bot travels instead of following
        float arriveDistance;  // yards from the leader at which the trip is over and the normal follow takes it
        uint32 updateMs;       // between two reports on the way
        uint32 maxUpdates;     // reports on the way per trip
        uint32 stallMs;        // without moving this long the bot gives up the trip
        uint32 giveUpMs;       // how long the bot then leaves the trip alone (the follow runs as before)
    };

    TravelSettings const& Settings()
    {
        static TravelSettings const settings = []
        {
            TravelSettings s;
            s.enabled = sConfigMgr->GetBoolDefault("AiPlayerbot.TravelToGroup.Enable", true);
            s.summonIntoDungeons = sConfigMgr->GetBoolDefault("AiPlayerbot.TravelToGroup.SummonIntoDungeons", false);
            s.distance = sConfigMgr->GetFloatDefault("AiPlayerbot.TravelToGroup.Distance", 100.0f);
            s.arriveDistance = sConfigMgr->GetFloatDefault("AiPlayerbot.TravelToGroup.ArriveDistance", 40.0f);
            s.updateMs = uint32(sConfigMgr->GetIntDefault("AiPlayerbot.TravelToGroup.UpdateSeconds", 180)) * IN_MILLISECONDS;
            s.maxUpdates = uint32(sConfigMgr->GetIntDefault("AiPlayerbot.TravelToGroup.MaxUpdates", 2));
            s.stallMs = uint32(sConfigMgr->GetIntDefault("AiPlayerbot.TravelToGroup.StallSeconds", 300)) * IN_MILLISECONDS;
            s.giveUpMs = uint32(sConfigMgr->GetIntDefault("AiPlayerbot.TravelToGroup.GiveUpSeconds", 300)) * IN_MILLISECONDS;
            return s;
        }();
        return settings;
    }

    // The leader moving this far from the spot the trip heads for moves the destination too.
    float const REPIN_DISTANCE = 100.0f;
    // A bot that moved less than this since the last check made no progress.
    float const PROGRESS_DISTANCE = 5.0f;
    // A gap between two runs of the action this long (a flight, a fight, a death) restarts the progress clock.
    uint32 const RESUME_GAP_MS = 60 * IN_MILLISECONDS;
    // Routes are not straight; a rough factor on the straight distance.
    float const ROUTE_FACTOR = 1.3f;
    // The cross-map distance stands in for the transport legs only; a boat or zeppelin is waited for as well.
    float const TRANSPORT_WAIT_SECONDS = 240.0f;
    // TravelMgr::fastMapTransDistance answers this when it knows no way between two maps.
    float const UNKNOWN_DISTANCE = 200000.0f;
    float const MIN_SPEED = 4.0f;
    float const SECONDS_PER_MINUTE = 60.0f;

    // A dungeon's entrance: an area trigger on the outside map whose teleport leads into the instance. Found once, by
    // walking the area trigger table (the teleport store only holds the targets, not the trigger positions).
    struct Entrance
    {
        uint32 triggerId;
        WorldPosition pos;
        float radius;
    };

    std::unordered_map<uint32, std::vector<Entrance>> const& Entrances()
    {
        static std::unordered_map<uint32, std::vector<Entrance>> const entrances = []
        {
            std::unordered_map<uint32, std::vector<Entrance>> result;
            for (uint32 id = 0; id < sAreaTriggerStore.GetNumRows(); ++id)
            {
                AreaTriggerEntry const* trigger = sAreaTriggerStore.LookupEntry(id);
                AreaTriggerStruct const* target = sObjectMgr->GetAreaTrigger(id);
                if (!trigger || !target || trigger->mapid == target->target_mapId)
                    continue;

                MapEntry const* from = sMapStore.LookupEntry(trigger->mapid);
                MapEntry const* to = sMapStore.LookupEntry(target->target_mapId);
                if (!from || !to || from->Instanceable() || !(to->IsDungeon() || to->IsRaid()))
                    continue;

                result[target->target_mapId].push_back(
                    { id, WorldPosition(trigger->mapid, trigger->x, trigger->y, trigger->z), trigger->radius });
            }
            return result;
        }();
        return entrances;
    }

    // The entrance of an instance map nearest to the bot (the first one when the bot is on another map).
    Entrance const* FindEntrance(uint32 instanceMapId, Player* bot)
    {
        auto const& all = Entrances();
        auto const itr = all.find(instanceMapId);
        if (itr == all.end())
            return nullptr;

        Entrance const* best = nullptr;
        float bestDistance = 0.0f;
        for (Entrance const& entrance : itr->second)
        {
            if (entrance.pos.GetMapId() != bot->GetMapId())
                continue;

            float const distance = bot->GetExactDist2d(entrance.pos.GetPositionX(), entrance.pos.GetPositionY());
            if (!best || distance < bestDistance)
            {
                best = &entrance;
                bestDistance = distance;
            }
        }

        return best ? best : &itr->second.front();
    }

    // The bot stands on the trigger when it is this close (the core accepts the trigger's radius plus 5 yards).
    float const MIN_ENTER_DISTANCE = 2.5f;
    uint32 const MAX_ENTER_ATTEMPTS = 3;
    uint32 const ENTER_RETRY_MS = 3 * IN_MILLISECONDS;

    struct Trip
    {
        bool active = false;
        WorldPosition dest;
        uint32 startMs = 0;
        uint32 reports = 0;        // lines said on the way (not counting the first one)
        uint32 lastReportMs = 0;
        uint32 lastRunMs = 0;
        uint32 progressMs = 0;     // when the bot last moved
        uint32 progressMap = 0;
        float progressX = 0.0f;
        float progressY = 0.0f;
        uint32 enterAttempts = 0;  // tries at the dungeon's area trigger
        uint32 lastEnterMs = 0;
        uint32 gaveUpMs = 0;       // 0 = not given up
    };

    std::mutex tripLock;
    std::unordered_map<uint32, Trip> trips;

    Trip LoadTrip(Player* bot)
    {
        std::lock_guard<std::mutex> guard(tripLock);
        auto itr = trips.find(bot->GetGUIDLow());
        return itr != trips.end() ? itr->second : Trip();
    }

    void StoreTrip(Player* bot, Trip const& trip)
    {
        std::lock_guard<std::mutex> guard(tripLock);
        if (!trip.active && !trip.gaveUpMs)
            trips.erase(bot->GetGUIDLow());
        else
            trips[bot->GetGUIDLow()] = trip;
    }

    // A trip of a bot that left the group, or whose group went into an instance, is over: nothing is left to say.
    void DropTrip(Player* bot)
    {
        std::lock_guard<std::mutex> guard(tripLock);
        auto itr = trips.find(bot->GetGUIDLow());
        if (itr != trips.end() && itr->second.active)
            trips.erase(itr);
    }

    bool IsGivenUp(Trip const& trip)
    {
        return trip.gaveUpMs && GetMSTimeDiffToNow(trip.gaveUpMs) < Settings().giveUpMs;
    }

    std::string ZoneName(Player* bot)
    {
        AreaTableEntry const* entry = GetAreaEntryByAreaID(bot->GetZoneId());
        if (entry && entry->m_AreaName && *entry->m_AreaName)
            return entry->m_AreaName;

        return PlayerbotTextMgr::instance().GetBotTextOrDefault("string_unknown_area", "the middle of nowhere", {});
    }

    // Minutes to the destination at the bot's current speed; false when no way between the two maps is known.
    bool EstimateMinutes(Player* bot, WorldPosition const& dest, uint32& minutes)
    {
        bool const sameMap = bot->GetMapId() == dest.GetMapId();
        float distance = 0.0f;
        float extraSeconds = 0.0f;
        if (sameMap)
            distance = bot->GetExactDist2d(dest.GetPositionX(), dest.GetPositionY());
        else
        {
            distance = WorldPosition(bot).fDist(dest);
            if (distance >= UNKNOWN_DISTANCE)
                return false;

            extraSeconds = TRANSPORT_WAIT_SECONDS;
        }

        float const speed = std::max(bot->GetSpeed(MOVE_RUN), MIN_SPEED);
        float const seconds = distance * ROUTE_FACTOR / speed + extraSeconds;
        minutes = std::max(1u, uint32(std::lround(seconds / SECONDS_PER_MINUTE)));
        return true;
    }

    void SayInParty(PlayerbotAI* botAI, std::string const& key, std::string const& fallback,
                    std::map<std::string, std::string> const& placeholders, bool reply = false)
    {
        // The members of a party a bot leads follow their leader: nobody there needs to hear where each one is.
        if (!reply && BotLfm::IsMember(botAI->GetBot()->GetGUIDLow()))
            return;

        BotVoice::Say(botAI, key, fallback, placeholders, BotVoice::Channel::PARTY,
                      reply ? BotVoice::Priority::REPLY : BotVoice::Priority::NOTICE);
    }

    void ReportStart(PlayerbotAI* botAI, Player* bot, WorldPosition const& dest)
    {
        uint32 minutes = 0;
        std::map<std::string, std::string> placeholders = { { "%zone", ZoneName(bot) } };
        if (EstimateMinutes(bot, dest, minutes))
        {
            placeholders["%minutes"] = std::to_string(minutes);
            SayInParty(botAI, "travel_group_start", "I'm in %zone, about %minutes min away. Wait for me", placeholders);
        }
        else
            SayInParty(botAI, "travel_group_start_unknown", "I'm in %zone, a long way from you. Give me a while", placeholders);
    }

    void ReportUpdate(PlayerbotAI* botAI, Player* bot, WorldPosition const& dest)
    {
        uint32 minutes = 0;
        if (!EstimateMinutes(bot, dest, minutes))
            return;

        SayInParty(botAI, "travel_group_update", "Still on my way, about %minutes min",
            { { "%zone", ZoneName(bot) }, { "%minutes", std::to_string(minutes) } });
    }
}

namespace GroupTravel
{
    bool Enabled() { return Settings().enabled; }

    bool SummonIntoDungeons() { return Settings().summonIntoDungeons; }

    bool HasEntrance(uint32 instanceMapId) { return Entrances().count(instanceMapId) != 0; }

    bool FindEntranceFor(uint32 instanceMapId, Player* near, EntranceInfo& out)
    {
        Entrance const* entrance = FindEntrance(instanceMapId, near);
        if (!entrance)
            return false;

        out.triggerId = entrance->triggerId;
        out.pos = entrance->pos;
        out.radius = entrance->radius;
        return true;
    }

    float EnterDistance(float radius) { return std::max(MIN_ENTER_DISTANCE, radius); }

    void SendEntranceTrigger(Player* bot, uint32 triggerId)
    {
        WorldPacket packet(CMSG_AREATRIGGER);
        packet << uint32(triggerId);
        packet.WriteBit(0);  // fromClient
        packet.WriteBit(1);  // entered
        packet.FlushBits();
        packet.rpos(0);
        bot->GetSession()->HandleAreaTriggerOpcode(packet);
    }

    bool WantsTravel(PlayerbotAI* botAI, Player* bot)
    {
        TravelSettings const& settings = Settings();
        if (!settings.enabled || !botAI || !bot || !bot->IsInWorld())
            return false;

        Player* leader = botAI->GetMaster();
        Group* group = bot->GetGroup();
        if (!leader || !leader->IsInWorld() || !(botAI->HasRealPlayerMaster() || BotLfm::IsMember(bot->GetGUIDLow())) || !group || group->isBGGroup() ||
            group->isBFGroup() || !group->IsMember(leader->GetGUID()))
        {
            DropTrip(bot);
            return false;
        }

        if (!bot->IsAlive() || bot->IsInCombat() || bot->IsInFlight() || bot->IsBeingTeleported() ||
            leader->IsBeingTeleported() || bot->InBattleground() || bot->GetVehicle())
            return false;

        // A bot cannot walk into an instance, or out of one, so only the open world is travelled.
        Map* botMap = bot->GetMap();
        Map* leaderMap = leader->GetMap();
        if (!botMap || !leaderMap || botMap->Instanceable())
        {
            DropTrip(bot);
            return false;
        }

        // A group inside a dungeon is reached through the dungeon's entrance; no entrance, no trip.
        bool const leaderInside = leaderMap->Instanceable();
        if (leaderInside && !((leaderMap->IsDungeon() || leaderMap->IsRaid()) && FindEntrance(leader->GetMapId(), bot)))
        {
            DropTrip(bot);
            return false;
        }

        // Alt bots are summoned, as a player's own characters would be. Checked last: it searches the bot list, and
        // this runs on every tick of every grouped bot.
        if (!sRandomPlayerbotMgr->IsRandomBot(bot))
            return false;

        Trip const trip = LoadTrip(bot);
        if (IsGivenUp(trip))
            return false;

        if (trip.active)
            return true;

        return leaderInside || bot->GetMapId() != leader->GetMapId() || bot->GetExactDist2d(leader) > settings.distance;
    }
}

bool TravelToGroupAction::isUseful() { return GroupTravel::WantsTravel(botAI, bot); }

bool TravelToGroupAction::Execute(Event /*event*/)
{
    TravelSettings const& settings = Settings();
    Player* leader = botAI->GetMaster();
    if (!leader)
        return false;

    Trip trip = LoadTrip(bot);
    uint32 const now = getMSTime();

    // Where the trip heads: the leader, or the entrance of the dungeon the leader is in.
    Map* leaderMap = leader->GetMap();
    Entrance const* entrance = nullptr;
    if (leaderMap && leaderMap->Instanceable())
    {
        entrance = FindEntrance(leader->GetMapId(), bot);
        if (!entrance)
            return false;
    }

    WorldPosition const target = entrance ? entrance->pos : WorldPosition(leader);
    bool const sameMap = bot->GetMapId() == target.GetMapId();

    if (!entrance && trip.active && sameMap && bot->GetExactDist2d(leader) <= settings.arriveDistance)
    {
        // Close enough: the follow strategy places the bot from here. The "here" is said once, for a trip long enough
        // to have been announced.
        SayInParty(botAI, "travel_group_arrived", "I'm here", {});
        SF_LOG_DEBUG("playerbots", "[GROUPTRAVEL] %s arrived at %s after %u s", bot->GetName().c_str(),
                     leader->GetName().c_str(), getMSTimeDiff(trip.startMs, now) / IN_MILLISECONDS);
        trip = Trip();
        StoreTrip(bot, trip);
        return true;
    }

    // A move to the target is kept until the target is far from it, so the route is not rebuilt every step.
    if (!trip.active)
    {
        trip = Trip();
        trip.active = true;
        trip.dest = target;
        trip.lastReportMs = now;
        trip.progressMs = now;
        trip.progressMap = bot->GetMapId();
        trip.progressX = bot->GetPositionX();
        trip.progressY = bot->GetPositionY();
        trip.startMs = now;
        SF_LOG_DEBUG("playerbots", "[GROUPTRAVEL] %s starts a trip to %s%s: map %u zone %u -> map %u, %.0f yd",
                     bot->GetName().c_str(), leader->GetName().c_str(), entrance ? " (dungeon entrance)" : "",
                     bot->GetMapId(), bot->GetZoneId(), target.GetMapId(),
                     sameMap ? bot->GetExactDist2d(target.GetPositionX(), target.GetPositionY()) : -1.0f);
        ReportStart(botAI, bot, target);
    }
    else if (trip.dest.GetMapId() != target.GetMapId() ||
             trip.dest.GetExactDist2d(target.GetPositionX(), target.GetPositionY()) > REPIN_DISTANCE)
        trip.dest = target;

    // The bot was elsewhere for a while (flying, fighting, dead): that is not a stall.
    if (trip.lastRunMs && getMSTimeDiff(trip.lastRunMs, now) > RESUME_GAP_MS)
        trip.progressMs = now;
    trip.lastRunMs = now;

    bool const movedEnough = bot->GetMapId() != trip.progressMap ||
        bot->GetExactDist2d(trip.progressX, trip.progressY) >= PROGRESS_DISTANCE;
    if (movedEnough || bot->GetTransport())
    {
        trip.progressMs = now;
        trip.progressMap = bot->GetMapId();
        trip.progressX = bot->GetPositionX();
        trip.progressY = bot->GetPositionY();
    }
    else if (getMSTimeDiff(trip.progressMs, now) >= settings.stallMs)
    {
        SayInParty(botAI, "travel_group_lost", "I can't find a way to you from here. Can you summon me?", {}, true);
        SF_LOG_DEBUG("playerbots", "[GROUPTRAVEL] %s gives up the trip to %s: no progress for %u s", bot->GetName().c_str(),
                     leader->GetName().c_str(), settings.stallMs / IN_MILLISECONDS);
        trip = Trip();
        trip.gaveUpMs = now ? now : 1;
        StoreTrip(bot, trip);
        return false;
    }

    // On the entrance trigger: walk in like a player, by sending the trigger. A refused entry (level, lockout) is tried
    // a few times and then given up.
    if (entrance && sameMap &&
        bot->GetExactDist(target.GetPositionX(), target.GetPositionY(), target.GetPositionZ()) <=
            GroupTravel::EnterDistance(entrance->radius))
    {
        if (trip.enterAttempts >= MAX_ENTER_ATTEMPTS)
        {
            SayInParty(botAI, "travel_group_cant_enter", "I can't get in there. Can you summon me?", {}, true);
            SF_LOG_DEBUG("playerbots", "[GROUPTRAVEL] %s cannot enter the dungeon of %s (trigger %u)",
                         bot->GetName().c_str(), leader->GetName().c_str(), entrance->triggerId);
            trip = Trip();
            trip.gaveUpMs = now ? now : 1;
            StoreTrip(bot, trip);
            return false;
        }

        if (trip.enterAttempts && getMSTimeDiff(trip.lastEnterMs, now) < ENTER_RETRY_MS)
        {
            StoreTrip(bot, trip);
            return true;
        }

        if (!trip.enterAttempts)
            SayInParty(botAI, "travel_group_entering", "I'm at the entrance, coming in", {});

        ++trip.enterAttempts;
        trip.lastEnterMs = now;
        StoreTrip(bot, trip);

        GroupTravel::SendEntranceTrigger(bot, entrance->triggerId);
        return true;
    }

    if (trip.reports < settings.maxUpdates && getMSTimeDiff(trip.lastReportMs, now) >= settings.updateMs)
    {
        ReportUpdate(botAI, bot, target);
        ++trip.reports;
        trip.lastReportMs = now;
    }

    StoreTrip(bot, trip);
    return MoveFarTo(trip.dest);
}
