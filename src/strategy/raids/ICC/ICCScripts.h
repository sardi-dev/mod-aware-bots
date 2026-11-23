/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCSCRIPTS_H
#define PLAYERBOTS_ICCSCRIPTS_H

#include "BotGuid.h"
#include "Position.h"
#include <map>
#include <set>
#include <unordered_map>
#include <vector>

class Player;

namespace IcecrownHelpers
{
    struct MalleableGooImpact
    {
        Position position;
        uint32 castTime;
    };

    struct DefileCastInfo
    {
        BotGuid targetGuid;
        uint32 castTime = 0;
    };

    struct VileGasVictim
    {
        BotGuid victimGuid;
        uint32 castTime = 0;
    };

    struct WinterStageState
    {
        uint32 startMs;
        Position const* pos;
    };

    struct StackChoice
    {
        uint32 evaluatedMs;
        int slotIdx;
    };

    struct LastLosMove
    {
        uint32 timestampMs = 0;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    struct ValithriaCloudSync
    {
        BotGuid targetCloudGuid;
        uint32 moveOnAfterMs = 0;
    };

    struct SpreadSlotState
    {
        uint32 lastCallMs = 0;
        int currentSlot = 1;
    };

    struct BloatDir
    {
        float x;
        float y;
    };

    struct IccInstanceState
    {
        std::vector<MalleableGooImpact> malleableGoo;
        std::map<BotGuid, uint32> festergutGooWaitUntil;
        DefileCastInfo defileCast;
        VileGasVictim rotfaceVileGas;
        std::map<BotGuid, uint32> rotfaceVileGasWaitUntil;
        std::map<BotGuid, uint32> lastTauntMs;
        std::map<BotGuid, uint32> lastCcMs;
        std::map<BotGuid, uint32> lastBuffMs;
        uint32 lastBossCombatMs = 0;

        std::map<BotGuid, WinterStageState> winterStage;
        std::map<BotGuid, bool> winterStageInbound;
        std::map<BotGuid, bool> mtAddInbound;
        std::map<BotGuid, bool> rangedInbound;
        std::map<BotGuid, StackChoice> mtStackChoice;
        float defileLastUnsafeX = 0.0f;
        float defileLastUnsafeY = 0.0f;
        uint32 defileLastUnsafeTime = 0;
        uint32 lkLastYellMs = 0;
        int lkSharedSlot = -1;

        std::set<int> sgFlaredRedThisPhase;
        bool sgFlaredBluePhase3 = false;
        bool sgLastPhase3 = false;
        std::map<BotGuid, int> sgGroupAssignments;
        std::map<BotGuid, BotGuid> sgTombAssignments;
        std::set<BotGuid> sgFreedFallback;
        std::map<BotGuid, LastLosMove> sgLastLosMove;

        std::map<BotGuid, int> bqlBotCurrentCurve;
        bool bqlGroundPhaseEstablished = false;
        bool bqlBossWasAirborne = false;
        bool bqlPostAirLanding = false;
        std::map<BotGuid, int> bqlAirSlotMemory;
        std::map<BotGuid, int> bqlBotSlotMemory;

        ValithriaCloudSync vtCloudSync;
        std::map<BotGuid, BotGuid> vtPortalClaim;

        Position gsbLastStarPos;
        std::map<BotGuid, uint32> gsbLastRocketPackUse;

        std::map<BotGuid, int> dbsBotSlotMemory;

        std::map<BotGuid, uint32> bpcSpreadLockTimers;

        SpreadSlotState fgSlotState;

        std::map<BotGuid, int32> rfExplosionSlotMemory;

        std::map<BotGuid, BloatDir> ppBloatLastDir;
    };

    IccInstanceState& IccState(uint32 instanceId);
    void IccResetInstance(uint32 instanceId);

    std::vector<Position> ActiveGooPositions(uint32 instanceId, uint32 lifetimeMs);

    // Fills the shared state from what the bot sees (see ICCScripts.cpp); cheap outside Icecrown Citadel
    void ObserveEncounter(Player* bot);
}

constexpr uint32 ICC_MAP_ID = 631;
constexpr uint32 ICC_RESET_GRACE_MS = 5000;

// Putricide - Mutated Abomination vehicle
constexpr uint32 GO_PUTRICIDE_DRINK_ME = 201584;
constexpr uint32 NPC_MUTATED_ABOMINATION_10 = 37672;
constexpr uint32 NPC_MUTATED_ABOMINATION_25 = 38285;
constexpr uint32 SPELL_MUTATED_TRANSFORMATION = 70311;
constexpr uint32 SPELL_ABO_EAT_OOZE = 70346;
constexpr uint32 SPELL_ABO_REGURGITATED_OOZE = 70539;


#endif
