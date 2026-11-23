/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE). See LfmLeadAction.h.
 */

#include "LfmLeadAction.h"

#include "BotLfm.h"
#include "BotVoice.h"
#include "Event.h"
#include "Group.h"
#include "LFGMgr.h"
#include "Playerbots.h"
#include "Timer.h"
#include "TravelToGroupAction.h"

namespace
{
    // A member this close to the leader counts as present at the entrance.
    float const PRESENT_DISTANCE = 50.0f;
    // While recruiting the leader checks again this often.
    uint32 const RECRUIT_TICK_MS = 2000;
    // A "waiting for ..." line at most this often and this many times.
    uint32 const WAIT_POST_GAP_MS = 60 * IN_MILLISECONDS;
    uint32 const MAX_WAIT_POSTS = 3;
}

bool LfmLeadAction::isUseful() { return BotLfm::IsLeader(bot->GetGUIDLow()); }

bool LfmLeadAction::Execute(Event /*event*/)
{
    BotLfm::PartyInfo info;
    if (!BotLfm::GetParty(bot->GetGUIDLow(), info))
        return false;

    // Recruiting: the leader stays where it is, so the bots that answer can find it.
    if (info.phase == BotLfm::Phase::RECRUIT)
    {
        botAI->SetNextCheckDelay(RECRUIT_TICK_MS);
        return true;
    }

    if (info.phase == BotLfm::Phase::INSIDE)
        return false;

    GroupTravel::EntranceInfo entrance;
    if (!GroupTravel::FindEntranceFor(info.mapId, bot, entrance))
        return false;

    bool const there = bot->GetMapId() == entrance.pos.GetMapId() &&
        bot->GetExactDist(entrance.pos.GetPositionX(), entrance.pos.GetPositionY(), entrance.pos.GetPositionZ()) <=
            GroupTravel::EnterDistance(entrance.radius);
    lfg::LFGDungeonData const* dungeon = sLFGMgr->GetLFGDungeon(info.dungeonId);
    std::string const dungeonName = dungeon ? dungeon->name : std::string("the dungeon");

    if (!there)
    {
        // The leader keeps the tick (the roaming actions below would walk it away), so it mounts here itself.
        if (!bot->IsMounted())
            botAI->DoSpecificAction("check mount state", Event(), true);

        MoveFarTo(entrance.pos);
        return true;
    }

    if (info.phase == BotLfm::Phase::TRAVEL)
    {
        BotLfm::SetPhase(bot->GetGUIDLow(), BotLfm::Phase::WAIT);
        BotVoice::Say(botAI, "voice_lfm_wait", "I'm at the entrance, waiting for everyone", { { "%dungeon", dungeonName } },
                      BotVoice::Channel::PARTY, BotVoice::Priority::NOTICE);
        return true;
    }

    // Waiting: everyone alive within reach, or the timeout.
    Group* group = bot->GetGroup();
    Player* missing = nullptr;
    if (group)
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || member == bot || !member->IsAlive())
                continue;

            if (member->GetMapId() != bot->GetMapId() || bot->GetExactDist2d(member) > PRESENT_DISTANCE)
            {
                missing = member;
                break;
            }
        }
    }

    uint32 const now = getMSTime();
    uint32 const waitedMs = getMSTimeDiff(info.phaseSinceMs, now);
    // The timeout is the config value in seconds; it is applied by the party's own clock here.
    bool const timedOut = waitedMs >= BotLfm::StragglerSeconds() * IN_MILLISECONDS;
    if (missing && !timedOut)
    {
        if (info.waitPosts < MAX_WAIT_POSTS && getMSTimeDiff(info.lastWaitPostMs ? info.lastWaitPostMs : info.phaseSinceMs, now) >= WAIT_POST_GAP_MS)
        {
            BotLfm::NotePostedWait(bot->GetGUIDLow());
            BotVoice::Say(botAI, "voice_lfm_waiting", "Still waiting for %name", { { "%name", missing->GetName() } },
                          BotVoice::Channel::PARTY, BotVoice::Priority::NOTICE);
        }
        botAI->SetNextCheckDelay(RECRUIT_TICK_MS);
        return true;
    }

    BotVoice::Say(botAI, missing ? "voice_lfm_enter_late" : "voice_lfm_enter",
                  missing ? "Can't wait any longer, going in" : "Everyone is here, going in", {}, BotVoice::Channel::PARTY,
                  BotVoice::Priority::NOTICE);
    SF_LOG_INFO("playerbots", "[BOTLFM] %s walks into %s%s", bot->GetName().c_str(), dungeonName.c_str(),
                missing ? " without everyone" : "");
    BotLfm::SetPhase(bot->GetGUIDLow(), BotLfm::Phase::INSIDE);
    GroupTravel::SendEntranceTrigger(bot, entrance.triggerId);
    return true;
}
