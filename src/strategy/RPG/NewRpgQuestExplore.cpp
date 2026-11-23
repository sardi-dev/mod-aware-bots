/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * BT-372: random bots finish exploration quests: they walk into the quest's area trigger and send the CMSG_AREATRIGGER
 * a game client sends there; the decisions are in Utils/QuestExplorePolicy.h.
 */

#include "NewRpgBaseAction.h"

#include <mutex>
#include <unordered_map>
#include <vector>

#include "BotCompat_S.h"
#include "DBCStores.h"
#include "DBCStructure.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "QuestExplorePolicy.h"
#include "QuestRetryPolicy.h"
#include "WorldPacket.h"
#include "WorldSession.h"

namespace QuestExploreIndex
{
std::once_flag indexOnce;
// Written once under indexOnce, then read-only: quest id -> its exploration triggers (teleporting triggers left out,
// since the packet would also move the bot through them).
std::unordered_map<uint32, std::vector<uint32>> triggersByQuest;

void Build()
{
    uint32 total = 0;
    for (uint32 triggerId = 0; triggerId < sAreaTriggerStore.GetNumRows(); ++triggerId)
    {
        if (!sAreaTriggerStore.LookupEntry(triggerId) || sObjectMgr->GetAreaTrigger(triggerId))
            continue;
        if (uint32 const questId = sObjectMgr->GetQuestForAreaTrigger(triggerId))
        {
            triggersByQuest[questId].push_back(triggerId);
            ++total;
        }
    }
    SF_LOG_INFO("playerbots", "[New RPG] quest exploration index: %u area triggers of %u quests", total,
                uint32(triggersByQuest.size()));
}

std::vector<uint32> const* Triggers(uint32 questId)
{
    std::call_once(indexOnce, Build);
    auto const itr = triggersByQuest.find(questId);
    return itr == triggersByQuest.end() ? nullptr : &itr->second;
}

QuestExplorePolicy::Shape ShapeOf(AreaTriggerEntry const* entry)
{
    QuestExplorePolicy::Shape shape;
    shape.x = entry->x;
    shape.y = entry->y;
    shape.z = entry->z;
    shape.radius = entry->radius;
    shape.boxX = entry->box_x;
    shape.boxY = entry->box_y;
    shape.boxZ = entry->box_z;
    shape.boxOrientation = entry->box_orientation;
    return shape;
}
}  // namespace QuestExploreIndex

namespace
{
// An exploration objective is a DUMMY objective whose counter the core fills on the trigger (AreaExploredOrEventHappens).
bool ExplorationOpen(Player* bot, Quest const* quest)
{
    if (bot->GetQuestStatus(quest->GetQuestId()) != QUEST_STATUS_INCOMPLETE)
        return false;
    for (QuestObjective const& objective : BotQuestObjectives(quest))
        if (objective.Type == QUEST_OBJECTIVE_TYPE_DUMMY && !BotIsQuestObjectiveComplete(bot, quest, objective))
            return true;
    return false;
}
}  // namespace

bool NewRpgBaseAction::DoQuestExploration(Quest const* quest, NewRpgInfo::DoQuest& data, bool& handled)
{
    handled = false;
    if (!quest)
        return false;

    if (!ExplorationOpen(bot, quest))
        return false;

    std::vector<uint32> const* triggers = QuestExploreIndex::Triggers(quest->GetQuestId());
    if (!triggers)
        return false;

    std::vector<QuestExplorePolicy::Shape> shapes;
    std::vector<uint32> ids;
    for (uint32 triggerId : *triggers)
    {
        AreaTriggerEntry const* entry = sAreaTriggerStore.LookupEntry(triggerId);
        if (!entry || entry->mapid != bot->GetMapId())
            continue;
        shapes.push_back(QuestExploreIndex::ShapeOf(entry));
        ids.push_back(triggerId);
    }
    int const nearest = QuestExplorePolicy::Nearest(shapes, bot->GetPositionX(), bot->GetPositionY());
    if (nearest < 0)
        return false;

    handled = true;
    uint32 const triggerId = ids[nearest];
    QuestExplorePolicy::Shape const& shape = shapes[nearest];
    if (data.exploreTrigger != triggerId)
    {
        data.exploreTrigger = triggerId;
        data.exploreArrivedMs = 0;
    }

    if (QuestExplorePolicy::Inside(shape, bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ()))
    {
        // The packet of a client entering the trigger (trigger id, then the from-client and entered bits, as
        // ReachAreaTriggerAction builds it); the handler checks the position again, runs the trigger's script and
        // credits the quest.
        WorldPacket packet(CMSG_AREATRIGGER);
        packet << uint32(triggerId);
        packet.WriteBit(0);
        packet.WriteBit(1);
        packet.FlushBits();
        packet.rpos(0);
        bot->GetSession()->HandleAreaTriggerOpcode(packet);

        if (ExplorationOpen(bot, quest))
        {
            // A trigger whose script gives the credit only with more (an event, an escort step): retried later.
            FailExploreTrip("no credit inside the trigger");
            return true;
        }
        SF_LOG_INFO("playerbots", "[QUEST] %s explores area trigger %u for %u %s", bot->GetName().c_str(), triggerId,
                    quest->GetQuestId(), quest->GetTitle().c_str());
        data.exploreTrigger = 0;
        return true;
    }

    WorldPosition const dest(bot->GetMapId(), shape.x, shape.y, shape.z);
    if (bot->GetExactDist2d(shape.x, shape.y) > QuestExplorePolicy::ARRIVE_DIST)
    {
        data.exploreArrivedMs = 0;
        if (MoveFarTo(dest))
            return true;
        return MoveRandomNear(10.0f);
    }

    // Near the centre but not inside: step onto the centre itself; a trigger above or below the reachable ground
    // stays out of reach and the arrival time runs out.
    uint32 const now = botAI->GetQuestClockMs();
    if (!data.exploreArrivedMs)
        data.exploreArrivedMs = now;
    else if (now - data.exploreArrivedMs >= QuestExplorePolicy::ARRIVAL_WAIT_MS)
    {
        FailExploreTrip("out of reach");
        return true;
    }
    if (bot->GetExactDist2d(shape.x, shape.y) > QuestExplorePolicy::CENTRE_DIST && !bot->isMoving())
        return MoveFarTo(dest);
    return false;
}

bool NewRpgBaseAction::ExploreTripOwns(WorldPosition const& dest)
{
    auto const* data = std::get_if<NewRpgInfo::DoQuest>(&botAI->GetRpgInfo().data);
    if (!data || !data->exploreTrigger)
        return false;
    AreaTriggerEntry const* entry = sAreaTriggerStore.LookupEntry(data->exploreTrigger);
    return entry && entry->mapid == dest.GetMapId() &&
           dest.GetExactDist2d(entry->x, entry->y) < 1.0f;
}

void NewRpgBaseAction::FailExploreTrip(char const* why)
{
    auto* data = std::get_if<NewRpgInfo::DoQuest>(&botAI->GetRpgInfo().data);
    if (!data || !data->exploreTrigger)
        return;
    uint32 const questId = data->questId;
    uint32 const triggerId = data->exploreTrigger;
    Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
    QuestRetryPolicy::Failure& failure = botAI->GetQuestRetries()[questId];
    bool const abandon = QuestRetryPolicy::RecordFailure(failure, getMSTime(), bot->GetZoneId(), bot->getLevel(), false,
                                                         QuestRetryPolicy::IsStuckReason(why));
    SF_LOG_INFO("playerbots", "[QUEST] %s gives up exploring area trigger %u for %u %s: %s, failed tries %u",
                bot->GetName().c_str(), triggerId, questId, quest ? quest->GetTitle().c_str() : "", why, failure.tries);
    if (abandon && bot->GetQuestStatus(questId) == QUEST_STATUS_INCOMPLETE && DropQuest(questId))
    {
        botAI->GetRpgStatistic().questAbandoned++;
        SF_LOG_INFO("playerbots", "[QUEST] %s abandons %u %s after %u failed tries", bot->GetName().c_str(), questId,
                    quest ? quest->GetTitle().c_str() : "", failure.tries);
    }
    botAI->GetRpgInfo().ChangeToIdle();
}
