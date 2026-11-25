/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 *
 * Ported from the mod-playerbots module for AzerothCore (Ai/World/Rpg/NewRpgInfo.h) into the
 * Legends of Azeroth Pandaria fork. Only the runtime roaming status store is kept; quest/flight
 * related sub-states are retained for future work.
 */

#ifndef _PLAYERBOT_NEWRPGINFO_H
#define _PLAYERBOT_NEWRPGINFO_H

#include "BotGuid.h"
#include "PlayerbotAIConfig.h"
#include "QuestClockPolicy.h"
#include "QuestMoveProgress.h"
#include "RoamPolicy.h"
#include "Timer.h"
#include "WorldPosition.h"

#include <unordered_map>
#include <unordered_set>
#include <variant>

class Quest;

// NewRpgStatus is defined in PlayerbotAIConfig.h (needed there for RpgStatusProbWeight[]).

struct NewRpgInfo
{
    NewRpgInfo() : data(Idle{}) {}
    ~NewRpgInfo() = default;

    // RPG_GO_GRIND
    struct GoGrind
    {
        WorldPosition pos{};
    };
    // RPG_GO_CAMP
    struct GoCamp
    {
        WorldPosition pos{};
    };
    // RPG_WANDER_NPC
    struct WanderNpc
    {
        BotGuid npcOrGo{};
        uint32 lastReach{0};
        // BT-253 town errands (ErrandPolicy.h): the errand of npcOrGo (ErrandPolicy::Errand, 0 = a quest or random NPC),
        // how long the bot stays there, when it gestures next, and how many errands it has run.
        uint8 errand{0};
        uint32 dwellMs{0};
        uint32 nextGestureMs{0};
        uint8 errandsDone{0};
    };
    // RPG_WANDER_RANDOM
    struct WanderRandom
    {
        WanderRandom() = default;
        // BT-252: the patrol stays around where the wander began (set on its first step); the experience then tells
        // whether there is anything to fight here (RoamPolicy::WanderHasNoWork).
        WorldPosition anchor{};
        uint32 startXp{0};
        uint32 startLevel{0};
    };
    // RPG_DO_QUEST
    struct DoQuest
    {
        uint32 questId{0};
        Quest const* quest{nullptr};
        int32 objectiveIdx{0};
        WorldPosition pos{};
        uint32 lastReachPOI{0};
        std::vector<uint32> objectiveProgress;
        // BT-372: the area trigger of an exploration objective the bot walks into, and when it reached its centre
        uint32 exploreTrigger{0};
        uint32 exploreArrivedMs{0};
    };
    // RPG_TRAVEL_FLIGHT
    struct TravelFlight
    {
        uint32 flightMasterEntry{0};
        WorldPosition flightMasterPos{};
        std::vector<uint32> path;
        bool inFlight{false};
    };
    // RPG_REST
    struct Rest
    {
        Rest() = default;
    };
    // RPG_OUTDOOR_PVP
    struct OutdoorPvP
    {
        BotGuid::LowType capturePointSpawnId{0};
    };
    // RPG_DUEL_SPOT
    // #201 (#612): AC NewRpgInfo.h:68-71 (OutdoorPvP) pattern. spotIndex = index into
    // sPlayerbotAIConfig->duelSpots; seeded = placed there by DuelSpotMgr::Update (#615); nextScanMs = getMSTime() of
    // the next candidate scan (0 = scan now); noRematchUntil = opponent -> getMSTime() until which not to challenge again.
    struct DuelSpot
    {
        uint32 spotIndex = 0;
        bool seeded = false;
        uint32 nextScanMs = 0;
        std::unordered_map<BotGuid, uint32> noRematchUntil;
    };
    // RPG_DO_GATHER
    struct DoGather
    {
        uint32 nodeSpawnId{0};
        uint32 nodeEntry{0};
        WorldPosition nodePos{};
        std::unordered_set<uint32> visited;
        uint32 lastReach{0};
        uint32 lastPassiveCheck{0};
        uint32 lastSwitchedFrom{0};
    };
    struct Idle
    {
    };

    uint32 startT{0};  // start timestamp of the current status
    // BT-371: bumped on every status change; activeWatch measures the status in active time (QuestClockPolicy.h)
    uint32 serial{0};
    QuestClock::StatusWatch activeWatch;

    // MOVE_FAR stuck tracking
    float nearestMoveFarDis{FLT_MAX};
    uint32 stuckTs{0};
    uint32 stuckAttempts{0};
    WorldPosition moveFarPos;
    uint32 moveFarSerial{0};
    uint32 moveFarMap{0};
    QuestMoveProgress::Watch moveFarProgress;
    // END MOVE_FAR

    // BT-254: where and since when the bot has stood still while its status wants it to move
    RoamPolicy::StillWatch stillWatch;
    uint32 stillWatchSerial{0};

    // #BT-015: getMSTime() of the last trip to a camp hub to repair worn gear (0 = none yet)
    uint32 lastRepairTripMs{0};

    // Debug scenario (.botscenario) / probe (.botprobe) state used to live here (scenarioStartT ...
    // scenarioAttackEntry, ArmWanderScenario) so NewRpgStatusUpdateAction/RandomChangeStatus/etc.
    // could read it inline. HARNESS-SPEC-botscenario-v3-module moved all of it into Scenario/
    // BotScenario.cpp's own side table (keyed by bot guid), so a harness/probe change no longer
    // touches this header - see BotScenario.h for the replacement API.

    using RpgData = std::variant<
        Idle,
        GoGrind,
        GoCamp,
        WanderNpc,
        WanderRandom,
        DoQuest,
        Rest,
        TravelFlight,
        OutdoorPvP,
        DuelSpot,  // #201 (#612): AC NewRpgInfo.h:114 pattern
        DoGather
    >;
    RpgData data;

    NewRpgStatus GetStatus() const;
    static NewRpgStatus StatusFromString(std::string const& name);
    bool HasStatusPersisted(uint32 maxDuration) const { return GetMSTimeDiffToNow(startT) > maxDuration; }
    // BT-371: the same in minutes of play (the bot's quest clock), for the statuses a rotating bot works through.
    uint32 StatusActiveMs(uint32 clock) { return activeWatch.Elapsed(serial, clock); }
    bool HasStatusPersistedActive(uint32 maxDuration, uint32 clock) { return StatusActiveMs(clock) > maxDuration; }
    void ChangeToGoGrind(WorldPosition pos);
    void ChangeToGoCamp(WorldPosition pos);
    void ChangeToWanderNpc();
    void ChangeToWanderRandom();
    void ChangeToDoQuest(uint32 questId, Quest const* quest);
    void ChangeToTravelFlight(uint32 flightMasterEntry, WorldPosition flightMasterPos, std::vector<uint32> path);
    void ChangeToOutdoorPvp(BotGuid::LowType capturePointSpawnId = 0);
    void ChangeToDuelSpot(uint32 spotIndex, bool seeded = false);  // #201 (#612)
    void ChangeToDoGather();
    void ChangeToRest();
    void ChangeToIdle();
    void Reset();
    void SetMoveFarTo(WorldPosition pos);
    std::string ToString() const;
};

struct NewRpgStatistic
{
    uint32 questAccepted{0};
    uint32 questCompleted{0};
    uint32 questAbandoned{0};
    uint32 questRewarded{0};
    uint32 questDropped{0};
};

#endif
