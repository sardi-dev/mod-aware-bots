/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "RSScripts.h"

#include <algorithm>
#include <cmath>
#include <list>
#include <mutex>

#include "Player.h"
#include "RaidBotHelpers.h"
#include "RSActions.h"
#include "RSTriggers.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "Timer.h"
#include "Vehicle.h"

// AC Ai/Raid/RS/RSScripts.cpp. 5.4.8: AC runs an AllCreatureScript (a hook this core lacks) that both records what
// happens in the fight and changes it: it roots Halion and teleports him home, makes bots immune to breaths and
// Consumption, dispels Combustion and Consumption, and hands out tank auras. Only the recording is kept, read by the
// bots from what they see (RubySanctumHelpers::ObserveEncounter, run by the "rs watch" trigger): meteor marks, both
// Halions' corporeality and health, the cutter orbs' timing and spin, and when the portal adds were last alive.

namespace RubySanctumHelpers
{
    static RaidBotHelpers::InstanceStateStore<RsInstanceState> instanceStates;

    RsInstanceState& RsState(uint32 instanceId)
    {
        return instanceStates.Get(instanceId);
    }

    void ResetInstance(uint32 instanceId)
    {
        instanceStates.Reset(instanceId);
    }
}

static uint8 RsHalionReadCorporealityIndex(Creature* creature)
{
    using namespace RubySanctumHelpers;
    for (uint8 i = 0; i <= 10; ++i)
    {
        if (creature->HasAura(HALION_CORPOREALITY_AURAS[i]))
            return i;
    }
    return 5;
}

namespace
{
void ObserveCutterTiming(Creature* carrier)
{
    uint32 const instanceId = carrier->GetMap()->GetInstanceId();
    RubySanctumHelpers::CutterTiming& state = RubySanctumHelpers::RsState(instanceId).cutterTiming;

    bool firing = false;
    Unit* tracked = nullptr;
    if (Vehicle* vehicle = carrier->GetVehicleKit())
    {
        tracked = vehicle->GetPassenger(0);
        for (int8 seat = 0; seat < 4; ++seat)
        {
            if (Unit* orb = vehicle->GetPassenger(seat); orb && orb->HasAura(SPELL_TWILIGHT_PULSE_PERIODIC))
            {
                firing = true;
                break;
            }
        }
    }

    if (firing && !state.active)
        state.lastShootTime = getMSTime();

    state.active = firing;

    if (tracked)
    {
        float const angle = std::atan2(tracked->GetPositionY() - carrier->GetPositionY(),
                                       tracked->GetPositionX() - carrier->GetPositionX());
        if (state.hasOrbAngle)
        {
            float const delta = RsAngleDiff(angle, state.lastOrbAngle);
            if (std::fabs(delta) > 0.0005f)
                state.spinSign = delta > 0.0f ? 1.0f : -1.0f;
        }
        state.lastOrbAngle = angle;
        state.hasOrbAngle = true;
    }
    else
    {
        state.hasOrbAngle = false;
    }
}


void ObserveMeteorMark(Creature* mark)
{
    if (!mark->IsAlive())
        return;

    RubySanctumHelpers::RsInstanceState& instState = RubySanctumHelpers::RsState(mark->GetMap()->GetInstanceId());

    if (!instState.seenMeteorMarks.insert(mark->GetGUID()).second)
        return;

    RubySanctumHelpers::MeteorPingPong& state = instState.meteorPingPong;
    state.lastCastTime = getMSTime();

    bool const tankMeteor = mark->GetExactDist2d(RS_HALION_TANK_POSITION.GetPositionX(), RS_HALION_TANK_POSITION.GetPositionY()) <= 10.0f;
    bool const tankEscapeMeteor = mark->GetExactDist2d(RS_HALION_TANK_METEOR_SPOT.GetPositionX(), RS_HALION_TANK_METEOR_SPOT.GetPositionY()) <= 10.0f;
    if (tankMeteor)
        state.tankMeteorTime = getMSTime();
    else if (tankEscapeMeteor)
        state.tankReturnTime = getMSTime();
    else
        ++state.count;
}


void ObserveHalion(Creature* creature)
{
    uint32 const instanceId = creature->GetMap()->GetInstanceId();
    if (!creature->IsAlive() || !creature->IsInCombat())
    {
        RubySanctumHelpers::ResetInstance(instanceId);
        return;
    }

    RubySanctumHelpers::RsInstanceState& instState = RubySanctumHelpers::RsState(instanceId);
    RubySanctumHelpers::HalionCorporeality& corp = instState.halionCorporeality;
    corp.physicalIndex = RsHalionReadCorporealityIndex(creature);
    corp.physicalStamp = getMSTime();
    corp.physicalGuid = creature->GetGUID();
    instState.bossHealth = { uint8(creature->GetHealthPct()), getMSTime() };
}

void ObserveTwilightHalion(Creature* creature)
{
    RubySanctumHelpers::RsInstanceState& instState =
        RubySanctumHelpers::RsState(creature->GetMap()->GetInstanceId());

    if (creature->IsAlive() && creature->IsInCombat())
    {
        RubySanctumHelpers::HalionCorporeality& corp = instState.halionCorporeality;
        corp.twilightIndex = RsHalionReadCorporealityIndex(creature);
        corp.twilightStamp = getMSTime();
        instState.bossHealth = { uint8(creature->GetHealthPct()), getMSTime() };
    }

    RubySanctumHelpers::CutterTiming& state = instState.cutterTiming;
    if (state.bossGuid != creature->GetGUID())
    {
        state = RubySanctumHelpers::CutterTiming{};
        state.bossGuid = creature->GetGUID();
    }
    if (state.encounterStart == 0 && creature->IsInCombat())
        state.encounterStart = getMSTime();
}
}

void RubySanctumHelpers::ObserveEncounter(Player* bot)
{
    if (bot->GetMapId() != RS_MAP_RUBY_SANCTUM)
        return;

    float constexpr SEARCH_RANGE = 200.0f;

    if (Creature* halion = bot->FindNearestCreature(NPC_HALION, SEARCH_RANGE, false))
        ObserveHalion(halion);
    if (Creature* twilight = bot->FindNearestCreature(NPC_TWILIGHT_HALION, SEARCH_RANGE, false))
        ObserveTwilightHalion(twilight);
    if (Creature* carrier = bot->FindNearestCreature(NPC_ORB_CARRIER, SEARCH_RANGE))
        ObserveCutterTiming(carrier);

    std::list<Creature*> marks;
    bot->GetCreatureListWithEntryInGrid(marks, NPC_METEOR_STRIKE_MARK, SEARCH_RANGE);
    for (Creature* mark : marks)
        ObserveMeteorMark(mark);

    for (uint32 entry : {uint32(NPC_LIVING_INFERNO), uint32(NPC_LIVING_EMBER)})
        if (bot->FindNearestCreature(entry, SEARCH_RANGE))
            RubySanctumHelpers::RsState(bot->GetMap()->GetInstanceId()).portalAddGate.lastAddAliveTime = getMSTime();
}
