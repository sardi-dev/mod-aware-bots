/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE). See AcceptSummonAction.h.
 *
 * The packet layout of SMSG_SUMMON_REQUEST is the writer in Spell::EffectSummonPlayer (SpellEffectsSummon.cpp): the
 * summoner's guid in a mask and byte sequence, with the zone and the auto-decline time in between.
 */

#include "AcceptSummonAction.h"

#include "BotVoice.h"
#include "Config.h"
#include "Event.h"
#include "GameObject.h"
#include "Group.h"
#include "LastMovementValue.h"
#include "ObjectAccessor.h"
#include "Playerbots.h"
#include "Timer.h"
#include "WorldPacket.h"

#include <atomic>
#include <mutex>
#include <unordered_map>

namespace
{
    struct SummonSettings
    {
        bool acceptEnabled;
        uint32 delayMinMs;
        uint32 delayMaxMs;
        uint32 combatWaitMs;
        bool stoneEnabled;
        uint32 stoneCooldownMs;
    };

    SummonSettings const& Settings()
    {
        static SummonSettings const settings = []
        {
            SummonSettings s;
            s.acceptEnabled = sConfigMgr->GetBoolDefault("AiPlayerbot.AcceptSummons.Enable", true);
            s.delayMinMs = uint32(sConfigMgr->GetIntDefault("AiPlayerbot.AcceptSummons.DelayMinMs", 1500));
            s.delayMaxMs = std::max(s.delayMinMs, uint32(sConfigMgr->GetIntDefault("AiPlayerbot.AcceptSummons.DelayMaxMs", 4000)));
            s.combatWaitMs = uint32(sConfigMgr->GetIntDefault("AiPlayerbot.AcceptSummons.CombatWaitSeconds", 10)) * IN_MILLISECONDS;
            s.stoneEnabled = sConfigMgr->GetBoolDefault("AiPlayerbot.StoneSummons.Enable", true);
            s.stoneCooldownMs = uint32(sConfigMgr->GetIntDefault("AiPlayerbot.StoneSummons.CooldownSeconds", 120)) * IN_MILLISECONDS;
            return s;
        }();
        return settings;
    }

    // A stone is used from this close, like a player's click; the bot walks up to it from this far.
    float const STONE_WALK_DISTANCE = 30.0f;
    // Bot summon requests are auto-declined by the core after this long; stop waiting a little before.
    uint32 const REQUEST_LIFETIME_MS = 100 * IN_MILLISECONDS;
    size_t const TRIM_SIZE = 256;

    struct PendingSummon
    {
        uint32 delayMs = 0;      // the pause before the answer
        uint32 receivedMs = 0;
        bool accept = true;      // the summoner is a group mate
    };

    std::mutex lock;
    std::unordered_map<uint32, PendingSummon> pending;
    std::atomic<uint32> pendingCount{ 0 };
    std::unordered_map<uint32, uint32> groupStoneMs;  // group id -> last stone summon, a group summons one mate at a time
}

namespace AcceptSummon
{
    bool Enabled() { return Settings().acceptEnabled; }

    bool StoneSummonsEnabled() { return Settings().stoneEnabled; }

    bool HasPending(Player* bot)
    {
        if (!pendingCount.load(std::memory_order_relaxed))
            return false;

        std::lock_guard<std::mutex> guard(lock);
        return pending.find(bot->GetGUIDLow()) != pending.end();
    }
}

bool AcceptSummonAction::Execute(Event event)
{
    SummonSettings const& settings = Settings();
    if (!settings.acceptEnabled)
    {
        // The old behaviour: a request nobody answers runs out by itself.
        return false;
    }

    WorldPacket p = BotFreshPacket(event.getPacket());
    p.rpos(0);

    BotGuid guid;
    guid[0] = p.ReadBit();
    guid[6] = p.ReadBit();
    guid[3] = p.ReadBit();
    guid[2] = p.ReadBit();
    guid[1] = p.ReadBit();
    guid[4] = p.ReadBit();
    guid[7] = p.ReadBit();
    guid[5] = p.ReadBit();

    p.ReadByteSeq(guid[4]);
    p.read_skip<uint32>();  // the summoner's zone
    p.ReadByteSeq(guid[7]);
    p.ReadByteSeq(guid[3]);
    p.ReadByteSeq(guid[1]);
    p.read_skip<uint32>();  // auto-decline time
    p.ReadByteSeq(guid[2]);
    p.ReadByteSeq(guid[6]);
    p.ReadByteSeq(guid[5]);
    p.ReadByteSeq(guid[0]);

    Player* summoner = ObjectAccessor::FindPlayer(guid);
    Group* group = bot->GetGroup();
    PendingSummon request;
    request.receivedMs = getMSTime();
    request.delayMs = urand(settings.delayMinMs, settings.delayMaxMs);
    request.accept = summoner && group && group->IsMember(summoner->GetGUID());

    {
        std::lock_guard<std::mutex> guard(lock);
        if (pending.emplace(bot->GetGUIDLow(), request).second)
            ++pendingCount;
        else
            pending[bot->GetGUIDLow()] = request;

        if (pending.size() >= TRIM_SIZE)
        {
            for (auto itr = pending.begin(); itr != pending.end();)
            {
                if (getMSTimeDiff(itr->second.receivedMs, request.receivedMs) > REQUEST_LIFETIME_MS)
                {
                    itr = pending.erase(itr);
                    --pendingCount;
                }
                else
                    ++itr;
            }
        }
    }

    SF_LOG_DEBUG("playerbots", "[SUMMON] %s got a summon request from %s (%s), answers in %u ms", bot->GetName().c_str(),
                 summoner ? summoner->GetName().c_str() : "unknown", request.accept ? "group mate" : "stranger",
                 request.delayMs);
    return true;
}

bool CompleteSummonAction::Execute(Event /*event*/)
{
    SummonSettings const& settings = Settings();
    PendingSummon request;
    {
        std::lock_guard<std::mutex> guard(lock);
        auto itr = pending.find(bot->GetGUIDLow());
        if (itr == pending.end())
            return false;

        request = itr->second;
    }

    uint32 const age = getMSTimeDiff(request.receivedMs, getMSTime());
    bool const expired = age >= REQUEST_LIFETIME_MS;
    if (!expired)
    {
        if (age < request.delayMs)
            return false;

        // A fight may end soon: wait for it, up to a limit, before saying no.
        bool const busyNow = !bot->IsAlive() || bot->IsInCombat() || bot->IsBeingTeleported();
        if (busyNow && request.accept && age < settings.combatWaitMs)
            return false;
    }

    bool const busy = !bot->IsAlive() || bot->IsInCombat() || bot->IsBeingTeleported();
    {
        std::lock_guard<std::mutex> guard(lock);
        if (pending.erase(bot->GetGUIDLow()))
            --pendingCount;
    }

    if (expired)
        return true;

    if (!request.accept || busy)
    {
        bot->SummonIfPossible(false);
        if (request.accept)
            BotVoice::Say(botAI, "voice_summon_busy", "Can't right now, I'm busy", {}, BotVoice::Channel::PARTY,
                          BotVoice::Priority::REPLY);
        return true;
    }

    // The thanks goes first: a far summon takes the bot out of the world for a moment.
    BotVoice::Say(botAI, "voice_summon_thanks", "Thanks for the summon", {}, BotVoice::Channel::PARTY,
                  BotVoice::Priority::CHATTER, 70);
    bot->GetMotionMaster()->Clear();
    AI_VALUE(LastMovement&, "last movement").clear();
    bot->SummonIfPossible(true);
    return true;
}

bool StoneSummonAction::Execute(Event /*event*/)
{
    SummonSettings const& settings = Settings();
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    // The bot mate to summon: far away, alive, and able to answer.
    Player* mate = nullptr;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        PlayerbotAI* memberAI = member ? GET_PLAYERBOT_AI(member) : nullptr;
        if (!member || member == bot || !memberAI || memberAI->IsRealPlayer() || !member->IsAlive() ||
            member->IsInCombat() || AcceptSummon::HasPending(member))
            continue;

        if (member->GetMapId() != bot->GetMapId() || bot->GetExactDist2d(member) > 150.0f)
        {
            mate = member;
            break;
        }
    }

    if (!mate)
        return false;

    uint32 const now = getMSTime();
    uint32 const groupId = group->GetLowGUID();
    {
        std::lock_guard<std::mutex> guard(lock);
        auto const last = groupStoneMs.find(groupId);
        if (last != groupStoneMs.end() && getMSTimeDiff(last->second, now) < settings.stoneCooldownMs)
            return false;
    }

    // The nearest meeting stone the bot can reach.
    GameObject* stone = nullptr;
    GuidVector const nearGos = AI_VALUE(GuidVector, "nearest game objects");
    for (BotGuid const& goGuid : nearGos)
    {
        GameObject* go = botAI->GetGameObject(goGuid);
        if (go && go->GetGoType() == GAMEOBJECT_TYPE_MEETINGSTONE && bot->GetExactDist2d(go) <= STONE_WALK_DISTANCE)
        {
            stone = go;
            break;
        }
    }

    if (!stone)
        return false;

    if (!bot->IsWithinDistInMap(stone, INTERACTION_DISTANCE))
        return MoveTo(stone, INTERACTION_DISTANCE - 1.0f);

    {
        std::lock_guard<std::mutex> guard(lock);
        groupStoneMs[groupId] = now;
        if (groupStoneMs.size() >= TRIM_SIZE)
            groupStoneMs.clear();
    }

    bot->SetTarget(mate->GetGUID());
    stone->Use(bot);
    BotVoice::Say(botAI, "voice_stone_summon", "Summoning %name at the stone, hang on", { { "%name", mate->GetName() } },
                  BotVoice::Channel::PARTY, BotVoice::Priority::NOTICE);
    SF_LOG_DEBUG("playerbots", "[SUMMON] %s uses a meeting stone for %s", bot->GetName().c_str(), mate->GetName().c_str());
    return true;
}
