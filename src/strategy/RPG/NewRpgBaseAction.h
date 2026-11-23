/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 *
 * Ported from the mod-playerbots module for AzerothCore (Ai/World/Rpg/Action/NewRpgBaseAction.h)
 * into the Legends of Azeroth Pandaria fork. Quest/NPC interaction helpers are not ported here;
 * only the runtime roaming movement and status-change helpers are implemented.
 */

#ifndef _PLAYERBOT_NEWRPGBASEACTION_H
#define _PLAYERBOT_NEWRPGBASEACTION_H

#include "LastMovementValue.h"
#include "MovementActions.h"
#include "NewRpgInfo.h"
#include "BotGuid.h"
#include "PlayerbotAI.h"
#include "Object.h"
#include "BotGuid.h"
#include "QuestDef.h"
#include "TravelMgr.h"
#include "WorldPosition.h"

#include <vector>

class Quest;
class WorldObject;

/// One quest-POI blob reduced to a position plus the objective it belongs to
/// (-1 for the hand-in location).
struct POIInfo
{
    G3D::Vector2 pos;
    int32 objectiveIdx;
};

/// A base (composition) class for all new rpg actions.
/// All functions that may be shared by multiple actions are declared here.
class NewRpgBaseAction : public MovementAction
{
public:
    NewRpgBaseAction(PlayerbotAI* botAI, std::string const name) : MovementAction(botAI, name) {}

protected:
    /* MOVEMENT RELATED */
    bool MoveFarTo(WorldPosition dest);
    bool MoveWorldObjectTo(BotGuid guid, float distance = INTERACTION_DISTANCE);
    bool MoveRandomNear(float moveStep = 50.0f, MovementPriority priority = MovementPriority::MOVEMENT_NORMAL);
    bool ForceToWait(uint32 duration, MovementPriority priority = MovementPriority::MOVEMENT_NORMAL);
    /// BT-252 (RoamPolicy.h): one patrol step within leash yards of anchor: at least MIN_HOP_YARDS long, on ground a
    /// player would walk to look around (no mountain climb, no long detour, no water). Walks back first when the bot
    /// was pulled outside the leash. Falls back to MoveRandomNear when no step qualifies.
    bool PatrolNear(WorldPosition const& anchor, float leash, MovementPriority priority = MovementPriority::MOVEMENT_NORMAL);
    /// #218 (beyond AC, MoP only - see NewRpgRoamTransport.h): follow a travel-graph route with transport /
    /// flight / portal legs. Returns -1 when it does not handle this move (AC's MoveFarTo code runs), else 0/1
    /// as the move result. mayStart: build a route if no trip runs for dest; needSpecialLeg: only start a trip
    /// whose route has a transport, flight-path or portal leg.
    int32 RoamTransportMove(WorldPosition const& dest, bool mayStart, bool needSpecialLeg);

    /* QUEST RELATED CHECK */
    BotGuid ChooseNpcOrGameObjectToInteract(bool questgiverOnly = false, float distanceLimit = 0.0f);
    bool HasQuestToAcceptOrReward(WorldObject* object);
    bool InteractWithNpcOrGameObjectForQuest(BotGuid guid);
    bool CanInteractWithQuestGiver(Object* questGiver);
    bool IsWithinInteractionDist(Object* object);
    uint32 BestRewardIndex(Quest const* quest);
    bool IsQuestWorthDoing(Quest const* quest);
    bool IsQuestCapableDoing(Quest const* quest);

    /* QUEST RELATED ACTION */
    bool SearchQuestGiverAndAcceptOrReward();
    bool AcceptQuest(Quest const* quest, BotGuid guid);
    bool TurnInQuest(Quest const* quest, BotGuid guid);
    bool OrganizeQuestLog();
    bool DropQuest(uint32 questId);
    bool QuestRetryWaiting(uint32 questId);
    bool GetQuestPOIPosAndObjectiveIdx(uint32 questId, std::vector<POIInfo>& poiInfo, bool toComplete = false);
    /// BT-210: start (or resume) the one trip to the nearest ender spawn of a rewardable quest on this continent and
    /// switch to RPG_DO_QUEST for it. False when there is nothing to hand in that way.
    bool StartQuestHandInTrip();
    /// BT-210: the hand-in trip ends without a turn-in; the quest waits in the retry book and the bot goes idle.
    void FailHandInTrip(char const* why);
    /// BT-210: dest is the running hand-in trip's ender, so a stuck move fails the trip instead of teleporting.
    bool HandInTripOwns(WorldPosition const& dest);
    /// BT-381: the bot stood still for minutes on RPG_DO_QUEST. The quest work it was on (a trip of any kind, or the
    /// plain walk to the objective) fails into the retry book the way a stuck move fails it, so the next pick leaves
    /// that quest alone for a while. Returns the quest id (0 when the status is not a quest).
    uint32 FailQuestWorkStoodStill();
    /// BT-210/BT-211: the walk of a quest trip: a route with a flight path or transport leg from far away, else
    /// MoveFarTo, which fails the trip instead of teleporting when the bot is stuck.
    bool MoveFarOnQuestTrip(WorldPosition const& dest);
    /// BT-211: run the trip to a source of one of the quest's open item objectives, or start one when mayStart.
    /// handled stays false when no objective has a usable source: the POI walk runs as before.
    bool DoQuestItemFetch(Quest const* quest, bool mayStart, bool& handled);
    /// BT-211: the item fetch ends without the items; the quest waits in the retry book and the bot goes idle.
    void FailItemFetch(char const* why);
    /// BT-211: dest is the running item fetch's spot, so a stuck move fails the fetch instead of teleporting.
    bool ItemFetchOwns(WorldPosition const& dest);

    /* BT-211 ITEM FETCH STEPS (NewRpgQuestItemFetch.cpp) */
    bool StartQuestItemFetch(Quest const* quest);
    bool NextItemFetchSpot(uint32 count);
    bool WorkFetchCreatures(WorldPosition const& dest, float dist, uint32 count);
    bool WorkFetchObject(uint32 count);
    bool WorkFetchVendor(uint32 count);

    /// BT-212: run the trip to the creature or object that gives one of the quest's creature credits by an interaction
    /// (talk, click, quest item, open), or start one when mayStart. handled stays false when no open creature objective
    /// needs one: the item fetch and the POI walk run as before.
    bool DoQuestInteraction(Quest const* quest, bool mayStart, bool& handled);
    /// BT-212: the interaction trip ends without the credit; the quest waits in the retry book and the bot goes idle.
    void FailInteractTrip(char const* why);
    /// BT-212: dest is the running interaction trip's spot, so a stuck move fails the trip instead of teleporting.
    bool InteractTripOwns(WorldPosition const& dest);

    /* BT-212 INTERACTION STEPS (NewRpgQuestInteract.cpp) */
    bool StartQuestInteraction(Quest const* quest, std::vector<uint32> const& deferred);
    bool NextInteractSpot();
    bool WorkInteraction(Quest const* quest);

    /// BT-372: walk into the area trigger of one of the quest's open exploration objectives and send the packet a game
    /// client sends there (the core credits the quest). handled stays false when no open objective has a trigger in
    /// reach: the other steps and the POI walk run as before.
    bool DoQuestExploration(Quest const* quest, NewRpgInfo::DoQuest& data, bool& handled);
    /// BT-372: the walk into the trigger ends without the credit; the quest waits in the retry book and the bot goes idle.
    void FailExploreTrip(char const* why);
    /// BT-372: dest is the running exploration's trigger, so a stuck move fails it instead of teleporting into it.
    bool ExploreTripOwns(WorldPosition const& dest);

    /* STATUS RELATED */
    bool RandomChangeStatus(std::vector<NewRpgStatus> candidateStatus);
    bool CheckRpgStatusAvailable(NewRpgStatus status);

    /* DESTINATION SELECTION */
    static WorldPosition SelectRandomGrindPos(Player* bot);
    static WorldPosition SelectRandomCampPos(Player* bot);
    bool SelectRandomFlightTaxiNode(uint32& flightMasterEntry, WorldPosition& flightMasterPos,
                                    std::vector<uint32>& path);

protected:
    /* FOR MOVE FAR */
    const float pathFinderDis = 70.0f;
    // Time without real progress toward dest before MoveFarTo
    // falls back to teleport recovery. Kept short enough that a
    // bot truly oscillating around an unreachable destination
    // (mmap returning non-progressing partial paths, or NOPATH +
    // cone fallback wandering) doesn't spin for 5 minutes before
    // the teleport fires, but long enough that a genuine long
    // walk that is slowly making progress never triggers it.
    const uint32 stuckTime = 90 * 1000;

};

#endif
