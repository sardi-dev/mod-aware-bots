/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE). See BotGroupSweep.h.
 */

#include "BotGroupSweep.h"

#include "BotLfm.h"
#include "CharacterCache.h"
#include "BotScenario.h"
#include "Config.h"
#include "Group.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "RandomPlayerbotMgr.h"
#include "StaleGroupPolicy.h"

#include <algorithm>
#include <ctime>
#include <set>
#include <unordered_map>
#include <vector>

namespace
{
    time_t const PASS_SECONDS = 30;
    unsigned const SECONDS_PER_MINUTE = 60;

    struct Settings
    {
        bool enable = true;
        unsigned apartSeconds = StaleGroupPolicy::APART_SECONDS;
        float apartYards = StaleGroupPolicy::APART_YARDS;
    };

    Settings ReadSettings()
    {
        Settings s;
        s.enable = sConfigMgr->GetBoolDefault("AiPlayerbot.StaleGroups.Enable", true);
        s.apartSeconds = unsigned(std::max(1, sConfigMgr->GetIntDefault("AiPlayerbot.StaleGroups.ApartMinutes", 5))) * SECONDS_PER_MINUTE;
        s.apartYards = std::max(100.0f, sConfigMgr->GetFloatDefault("AiPlayerbot.StaleGroups.ApartYards", StaleGroupPolicy::APART_YARDS));
        return s;
    }

    // World thread only: when each member (by low guid) was first found apart from its leader.
    std::unordered_map<uint32, time_t> apartSince;
    time_t lastPassAt = 0;

    // By account, so a member that is logged out right now (random bots come and go) still counts as a bot.
    bool AllRandomBots(Group* group)
    {
        for (Group::MemberSlot const& slot : group->GetMemberSlots())
        {
            BotGuid const guid = BotGuid::Create<BotHighGuid::Player>(uint32(GUID_LOPART(slot.guid)));
            if (!sPlayerbotAIConfig->IsInRandomAccountList(sCharacterCache->GetCharacterAccountIdByGuid(guid)))
                return false;
        }
        return true;
    }

    bool AnyBotLfmMember(Group* group)
    {
        for (Group::MemberSlot const& slot : group->GetMemberSlots())
        {
            uint32 const low = uint32(GUID_LOPART(slot.guid));
            if (BotLfm::IsLeader(low) || BotLfm::IsMember(low))
                return true;
        }
        return false;
    }
}  // namespace

void BotGroupSweep::Update()
{
    time_t const now = time(nullptr);
    if (lastPassAt && now < lastPassAt + PASS_SECONDS)
        return;
    lastPassAt = now;

    Settings const settings = ReadSettings();
    if (!settings.enable)
    {
        apartSince.clear();
        return;
    }

    std::vector<Player*> const bots = sRandomPlayerbotMgr->GetPlayerBotsSnapshot();
    std::set<uint32> stillApart;
    std::set<uint64> judged;
    bool const scenario = BotScenario::AnyActive();
    for (Player* bot : bots)
    {
        Group* group = bot ? bot->GetGroup() : nullptr;
        if (!group || !bot->IsInWorld() || !judged.insert(group->GetGUID()).second)
            continue;

        bool const special = group->isLFGGroup() || group->isBGGroup() || group->isBFGroup() || group->isRaidGroup();
        if (!StaleGroupPolicy::SweepsGroup(AllRandomBots(group), special, AnyBotLfmMember(group), scenario))
            continue;

        Player* leader = ObjectAccessor::FindPlayer(group->GetLeaderGUID());
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || !member->IsInWorld() || member == leader)
                continue;

            bool const leaderOnline = leader && leader->IsInWorld();
            bool const sameMap = leaderOnline && leader->GetMapId() == member->GetMapId();
            float const distance = sameMap ? member->GetDistance(leader) : 0.0f;
            if (!StaleGroupPolicy::Apart(leaderOnline, sameMap, distance, settings.apartYards))
                continue;

            // A bot in a battleground or battlefield has its raid as current group and this party as its original
            // group; RemoveFromGroup() would act on the raid. Leave such a member alone until it is back.
            if (member->GetGroup() != group)
                continue;

            uint32 const low = member->GetGUIDLow();
            stillApart.insert(low);
            time_t& since = apartSince[low];
            if (!since)
                since = now;
            if (!StaleGroupPolicy::MayLeave(long(since), long(now), settings.apartSeconds))
                continue;

            SF_LOG_INFO("playerbots", "[STALEGROUP] %s leaves its group: the leader has been %s for %u s and no player is in it",
                        member->GetName().c_str(), !leaderOnline ? "offline" : (!sameMap ? "on another map" : "far away"),
                        unsigned(now - since));
            member->RemoveFromGroup();
            if (PlayerbotAI* botAI = GET_PLAYERBOT_AI(member))
            {
                botAI->SetMaster(nullptr);
                botAI->ResetStrategies();
                botAI->Reset();
            }
            stillApart.erase(low);
        }
    }

    for (auto itr = apartSince.begin(); itr != apartSince.end();)
        itr = stillApart.count(itr->first) ? std::next(itr) : apartSince.erase(itr);
}
