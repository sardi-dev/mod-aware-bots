/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

// BT-154: the run-time spawns of the ladder NPC. See ArenaNpcSpawn.h for the thread rule and ArenaNpcSpots.h for the spots.
//
// Why a pass in the world tick and not a grid hook: the core's map script hooks (OnLoadGridMap and the like) fire for the
// terrain tile, not for the loaded objects, and run on a map thread; the creature of a grid is added later. Looking at the
// four spots after the map update is cheap, needs no extra script classes per map and runs where the module already moves
// bots and battlegrounds.
//
// Lifetime. The NPC is a TempSummon without owner and without duration: a normal grid creature that is never saved to the
// database (no spawn id, so no respawn row and no SaveToDB), that stays until its grid is unloaded (the core deletes every
// creature of an unloading grid) and that the next pass creates again when the grid loads again. A creature of the NPC's
// entry that already stands at a spot, for example a database spawn from before the module owned the NPC, is adopted
// instead of doubled.

#include "ArenaNpcSpawn.h"

#include "ArenaLadder.h"
#include "ArenaNpcSpots.h"
#include "GridObject.h"  // 5.4.8: before CellImpl.h
#include "CellImpl.h"
#include "Config.h"
#include "Creature.h"
#include "Log.h"
#include "Map.h"
#include "MapManager.h"
#include "ObjectMgr.h"
#include "TemporarySummon.h"

#include <array>

namespace
{
// World thread only: written by OnStartup and OnUpdate, read nowhere else, so no lock.
bool gActive = false;
std::uint64_t gClockMs = 0;
std::uint32_t gUntilPassMs = 0;
std::array<ArenaNpcSpots::SpotState, ArenaNpcSpots::SPOT_COUNT> gStates;
std::array<bool, ArenaNpcSpots::SPOT_COUNT> gAnnounced;

// Finds a creature of the NPC's entry within the claim radius of a spot (Map::VisitGrid calls Visit per object type).
struct CopyVisitor
{
    ArenaNpcSpots::Spot const* spot = nullptr;
    uint64 guid = 0;

    void Visit(CreatureMapType& creatures)
    {
        for (CreatureMapType::iterator itr = creatures.begin(); itr != creatures.end() && !guid; ++itr)
        {
            Creature* creature = itr->GetSource();
            if (creature->GetEntry() == ArenaNpcSpots::NPC_ENTRY && creature->IsInWorld() &&
                ArenaNpcSpots::ClaimsSpot(*spot, creature->GetPositionX(), creature->GetPositionY()))
                guid = creature->GetGUID();
        }
    }

    template<class SKIP> void Visit(GridRefManager<SKIP>&) { }
};

// True while the remembered creature of a spot is still in the world. A guid that does not resolve any more means the grid
// was unloaded, and the spot starts over.
bool TrackedPresent(Map* map, ArenaNpcSpots::SpotState& state)
{
    if (!state.guid)
        return false;

    if (map)
    {
        Creature* creature = map->GetCreature(state.guid);
        if (creature && creature->IsInWorld())
            return true;
    }

    ArenaNpcSpots::RecordGone(state);
    return false;
}

ArenaNpcSpots::Facts FactsOf(ArenaNpcSpots::Spot const& spot, Map* map, bool trackedPresent)
{
    ArenaNpcSpots::Facts facts;
    facts.mapLoaded = map != nullptr;
    if (map)
    {
        facts.gridLoaded = map->IsGridLoaded(spot.x, spot.y);
        facts.gridLeaving = map->IsRemovalGrid(spot.x, spot.y);
    }
    facts.trackedPresent = trackedPresent;
    return facts;
}

void Announce(std::size_t index, ArenaNpcSpots::Spot const& spot, char const* what)
{
    // The first one of a spot per run is worth an info line; the grids of a capital load and unload all day.
    if (!gAnnounced[index])
    {
        gAnnounced[index] = true;
        SF_LOG_INFO("playerbots", "[ARENANPC] %s at %s", what, spot.label);
        return;
    }
    SF_LOG_DEBUG("playerbots", "[ARENANPC] %s at %s", what, spot.label);
}

void ServeSpot(std::size_t index)
{
    ArenaNpcSpots::Spot const& spot = ArenaNpcSpots::SPOTS[index];
    ArenaNpcSpots::SpotState& state = gStates[index];

    Map* map = sMapMgr->FindBaseNonInstanceMap(spot.map);
    ArenaNpcSpots::Facts facts = FactsOf(spot, map, TrackedPresent(map, state));

    ArenaNpcSpots::Step step = ArenaNpcSpots::Decide(facts, state, gClockMs);
    if (step == ArenaNpcSpots::Step::SCAN)
    {
        CopyVisitor visitor;
        visitor.spot = &spot;
        map->VisitGrid(spot.x, spot.y, ArenaNpcSpots::CLAIM_RADIUS, visitor);
        facts.nearby = visitor.guid ? ArenaNpcSpots::NearbyCopy::FOUND : ArenaNpcSpots::NearbyCopy::NONE;
        step = ArenaNpcSpots::Decide(facts, state, gClockMs);
        if (step == ArenaNpcSpots::Step::ADOPT)
        {
            ArenaNpcSpots::RecordSpawned(state, visitor.guid);
            Announce(index, spot, "adopted a copy that is not the module's, apply world_arena_ladder_npc_remove.sql to clear the database spawn");
            return;
        }
    }

    if (step != ArenaNpcSpots::Step::SPAWN)
        return;

    Position position;
    position.Relocate(spot.x, spot.y, spot.z, spot.orientation);
    TempSummon* npc = map->SummonCreature(ArenaNpcSpots::NPC_ENTRY, position);
    if (!npc)
    {
        ArenaNpcSpots::RecordFailed(state, gClockMs);
        SF_LOG_ERROR("playerbots", "[ARENANPC] the core refused to create creature %u at %s, next try in %u s",
            ArenaNpcSpots::NPC_ENTRY, spot.label, ArenaNpcSpots::SPAWN_RETRY_MS / 1000);
        return;
    }

    ArenaNpcSpots::RecordSpawned(state, npc->GetGUID());
    Announce(index, spot, "spawned");
}
}  // namespace

namespace ArenaNpcSpawn
{
void OnStartup()
{
    gActive = false;
    gClockMs = 0;
    gUntilPassMs = 0;
    gStates.fill(ArenaNpcSpots::SpotState());
    gAnnounced.fill(false);

    // The ladder's own switch first: no teams, no ladder NPC pages, nothing to read.
    if (!ArenaLadder::Enabled())
        return;

    if (!sConfigMgr->GetBoolDefault("AiPlayerbot.ArenaTeams.LadderNpc", true))
    {
        SF_LOG_INFO("playerbots", ">> Arena ladder NPC: off (AiPlayerbot.ArenaTeams.LadderNpc = 0)");
        return;
    }

    CreatureTemplate const* info = sObjectMgr->GetCreatureTemplate(ArenaNpcSpots::NPC_ENTRY);
    if (!info)
    {
        SF_LOG_ERROR("playerbots", "[ARENANPC] creature template %u is missing, so the ladder NPC will not stand anywhere. "
            "Apply data/sql/db-world/2026_11_12_00_arena_ladder_npc.sql of the bots module to the world database.", ArenaNpcSpots::NPC_ENTRY);
        return;
    }
    if (!info->ScriptID)
    {
        SF_LOG_ERROR("playerbots", "[ARENANPC] creature template %u has no ScriptName, so the ladder NPC would have no gossip. "
            "Apply data/sql/db-world/2026_11_12_00_arena_ladder_npc.sql of the bots module again.", ArenaNpcSpots::NPC_ENTRY);
        return;
    }

    gActive = true;
    SF_LOG_INFO("playerbots", ">> Arena ladder NPC: %zu spots, spawned when their grid loads", ArenaNpcSpots::SPOT_COUNT);
}

void OnUpdate(uint32 diff)
{
    if (!gActive || !ArenaLadder::Enabled())
        return;

    gClockMs += diff;
    if (!ArenaNpcSpots::PassDue(gUntilPassMs, diff))
        return;

    for (std::size_t index = 0; index < ArenaNpcSpots::SPOT_COUNT; ++index)
        ServeSpot(index);
}
}  // namespace ArenaNpcSpawn
