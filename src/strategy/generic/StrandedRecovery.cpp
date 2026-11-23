/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "StrandedRecovery.h"

#include "BotBreak.h"
#include "BotScenario.h"
#include "BotTerrain.h"
#include "CheckMountStateAction.h"
#include "Config.h"
#include "CrossMapTripPolicy.h"
#include "DetourNavMesh.h"
#include "DetourNavMeshQuery.h"
#include "DungeonUnstuck.h"
#include "Map.h"
#include "MMapFactory.h"
#include "MotionMaster.h"
#include "NewRpgRoamTransport.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "StrandedPolicy.h"
#include "Timer.h"
#include "WorldPosition.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
struct StrandedSettings
{
    bool enabled = true;
    uint32 stuckMs = StrandedPolicy::STUCK_MS;
};

StrandedSettings const& GetSettings()
{
    static StrandedSettings const settings = []
    {
        StrandedSettings s;
        s.enabled = sConfigMgr->GetBoolDefault("AiPlayerbot.StrandedRecovery.Enable", true);
        int32 const seconds = sConfigMgr->GetIntDefault("AiPlayerbot.StrandedRecovery.StuckSeconds",
                                                        int32(StrandedPolicy::STUCK_MS / IN_MILLISECONDS));
        s.stuckMs = uint32(std::max(30, seconds)) * IN_MILLISECONDS;
        return s;
    }();
    return settings;
}

// A real player's party stays where its player is: the deck of the barge is a quest hub people use. A bot with a human
// master within this range is with him, not stranded.
constexpr float WITH_MASTER_YARDS = 100.0f;
// The side step of a try that found no landing stays this near.
constexpr float SIDE_STEP_YARDS = 20.0f;
// How far below the bot the ground is looked for: the bed under a flooded barge is 65 yd down.
constexpr float GROUND_SEARCH_YARDS = 120.0f;
// A standing player's eye height, for the sight test from the bot to a landing.
constexpr float LANDING_EYE_HEIGHT = 1.5f;
// A hearth bound this near the bot does not take it out of its pocket.
constexpr float HEARTH_USELESS_YARDS = 150.0f;

struct MeshRead
{
    StrandedPolicy::Reading reading = StrandedPolicy::Reading::Skip;
    unsigned polygons = 0;
    // The polygons of a closed pocket, sorted (empty for any other reading).
    std::vector<dtPolyRef> pocket;
};

dtNavMeshQuery const* MeshQuery(Player* bot)
{
    MMAP::MMapManager* mmap = MMAP::MMapFactory::createOrGetMMapManager();
    return mmap->GetNavMeshQuery(bot->GetMapId(), bot->GetInstanceId(), bot->GetTerrainSwaps());
}

dtQueryFilter WalkFilter()
{
    dtQueryFilter filter;
    filter.setIncludeFlags(NAV_GROUND | NAV_WATER | NAV_MAGMA | NAV_SLIME);  // players, as PathGenerator::CreateFilter
    filter.setExcludeFlags(0);
    return filter;
}

// The polygon the point stands on (Detour works in (y, z, x)), or 0 when none lies within STANDING_YARDS of it.
dtPolyRef StandingPolygon(dtNavMeshQuery const* query, dtQueryFilter const& filter, float x, float y, float z,
                          float yExtent)
{
    float const point[3] = { y, z, x };
    float const extents[3] = { StrandedPolicy::STANDING_YARDS, yExtent, StrandedPolicy::STANDING_YARDS };
    float closest[3];
    dtPolyRef ref = 0;
    if (dtStatusFailed(query->findNearestPoly(point, extents, &filter, &ref, closest)) || !ref)
        return 0;
    float const dx = closest[0] - point[0];
    float const dy = closest[1] - point[1];
    float const dz = closest[2] - point[2];
    if (dx * dx + dy * dy + dz * dz > StrandedPolicy::STANDING_YARDS * StrandedPolicy::STANDING_YARDS)
        return 0;
    return ref;
}

// Floods the walkable graph from `start`. A search that ended by itself with fewer polygons than the buffer holds, and
// whose polygons have no edge on a tile border (a tile that is not loaded would cut a big mesh short), is a pocket.
void FloodWalkable(dtNavMeshQuery const* query, dtQueryFilter const& filter, dtPolyRef start, float x, float y, float z,
           MeshRead& out)
{
    dtPolyRef refs[StrandedPolicy::PROBE_MAX_POLYS];
    int count = 0;
    float const center[3] = { y, z, x };
    dtStatus const status = query->findPolysAroundCircle(start, center, StrandedPolicy::PROBE_RADIUS_YARDS, &filter,
                                                         refs, nullptr, nullptr, &count,
                                                         int(StrandedPolicy::PROBE_MAX_POLYS));
    if (dtStatusFailed(status) || count <= 0)
    {
        out.reading = StrandedPolicy::Reading::Skip;
        return;
    }

    bool complete = !dtStatusDetail(status, DT_BUFFER_TOO_SMALL | DT_OUT_OF_NODES);
    dtNavMesh const* nav = query->getAttachedNavMesh();
    if (!nav)
        complete = false;
    for (int i = 0; complete && i < count; ++i)
    {
        dtMeshTile const* tile = nullptr;
        dtPoly const* poly = nullptr;
        if (dtStatusFailed(nav->getTileAndPolyByRef(refs[i], &tile, &poly)) || !poly)
        {
            complete = false;
            break;
        }
        for (int j = 0; j < int(poly->vertCount); ++j)
        {
            if (poly->neis[j] & DT_EXT_LINK)
            {
                complete = false;
                break;
            }
        }
    }

    out.polygons = unsigned(count);
    out.reading = StrandedPolicy::Classify(true, complete, out.polygons);
    if (out.reading == StrandedPolicy::Reading::Closed)
    {
        out.pocket.assign(refs, refs + count);
        std::sort(out.pocket.begin(), out.pocket.end());
    }
}

// The mesh under a point (the bot's feet, or a landing).
MeshRead ReadMeshAt(Player* bot, float x, float y, float z, float yExtent)
{
    MeshRead read;
    dtNavMeshQuery const* query = MeshQuery(bot);
    if (!query)
        return read;
    dtQueryFilter const filter = WalkFilter();
    dtPolyRef const start = StandingPolygon(query, filter, x, y, z, yExtent);
    if (!start)
        return read;
    FloodWalkable(query, filter, start, x, y, z, read);
    return read;
}

MeshRead ReadBotMesh(Player* bot)
{
    return ReadMeshAt(bot, bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(),
                      StrandedPolicy::STANDING_YARDS);
}

bool WithHumanMaster(PlayerbotAI* botAI, Player* bot)
{
    if (!botAI->HasRealPlayerMaster())
        return false;
    Player* master = botAI->GetMaster();
    return master && master->GetMap() == bot->GetMap() && bot->GetExactDist(master) < WITH_MASTER_YARDS;
}

// A bot whose judgement belongs to something else, or that is not on its feet in the open world, is not read.
bool Judged(PlayerbotAI* botAI, Player* bot)
{
    Map* map = bot->GetMap();
    if (!bot->IsAlive() || !map || bot->IsBeingTeleported() || bot->IsInCombat() || bot->IsInFlight() ||
        bot->GetTransport() || bot->GetVehicle() || bot->IsFlying() || bot->IsFalling())
        return false;
    if (map->IsDungeon() || map->IsRaid() || map->IsBattlegroundOrArena() || bot->InBattleground())
        return false;
    if (bot->IsInWater() || bot->IsUnderWater() || BotIsSwimming(bot) || bot->IsSitState())
        return false;
    if (botAI->HasStrategy("stay", botAI->GetState()) || BotBreak::IsOnBreak(bot->GetGUIDLow()) ||
        BotScenario::IsStatusLocked(bot) || RoamTransport::OnTransportLeg(bot) || RoamTransport::HoldsForTransportLeg(bot))
        return false;
    return !WithHumanMaster(botAI, bot);
}

bool HearthReady(PlayerbotAI* botAI, Player* bot)
{
    if (!bot->IsAlive() || bot->IsInCombat() || bot->IsBeingTeleported())
        return false;
    if (!bot->HasItemCount(CrossMapTripPolicy::HEARTHSTONE_ITEM, 1) ||
        bot->HasSpellCooldown(CrossMapTripPolicy::HEARTHSTONE_SPELL))
        return false;
    WorldPosition const home = botAI->GetAiObjectContext()->GetValue<WorldPosition>("home bind")->Get();
    if (home == WorldPosition())
        return false;
    return home.GetMapId() != bot->GetMapId() ||
           bot->GetExactDist2d(home.GetPositionX(), home.GetPositionY()) >= HEARTH_USELESS_YARDS;
}

void LogStrandedStep(PlayerbotAI* botAI, Player* bot, uint32 polygons, char const* step)
{
    uint32 const clock = botAI->GetQuestClockMs();
    SF_LOG_INFO("playerbots", "[UNSTUCK] %s stuck %u s in a closed pocket of %u polygons, seen=%u: %s, at (%.1f, %.1f, %.1f) map %u",
                bot->GetName().c_str(), StrandedPolicy::StuckFor(botAI->stranded, clock) / IN_MILLISECONDS, polygons,
                BotSeenByRealPlayer(bot) ? 1u : 0u, step, bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(),
                bot->GetMapId());
}
}  // namespace

bool StrandedTrigger::IsActive()
{
    StrandedSettings const& settings = GetSettings();
    if (!settings.enabled || !bot->IsInWorld())
        return false;

    // A cast in progress (the Hearthstone's own included) is neither read nor judged: it must not end the episode.
    if (bot->IsNonMeleeSpellCasted(false))
        return false;

    StrandedPolicy::State& state = botAI->stranded;
    uint32 const clock = botAI->GetQuestClockMs();
    if (!state.lastReadMs || clock - state.lastReadMs >= StrandedPolicy::READ_GAP_MS)
    {
        MeshRead read;
        if (Judged(botAI, bot))
            read = ReadBotMesh(bot);
        StrandedPolicy::Observe(state, read.reading, read.polygons, clock);
    }

    return StrandedPolicy::Decide(state, clock, getMSTime(), HearthReady(botAI, bot), settings.stuckMs) !=
           StrandedPolicy::Step::None;
}

bool StrandedRecoveryAction::isUseful()
{
    StrandedSettings const& settings = GetSettings();
    return settings.enabled && StrandedPolicy::Decide(botAI->stranded, botAI->GetQuestClockMs(), getMSTime(),
                                                      HearthReady(botAI, bot), settings.stuckMs) !=
                                   StrandedPolicy::Step::None;
}

bool StrandedRecoveryAction::Execute(Event /*event*/)
{
    StrandedSettings const& settings = GetSettings();
    StrandedPolicy::State& state = botAI->stranded;
    uint32 const clock = botAI->GetQuestClockMs();
    switch (StrandedPolicy::Decide(state, clock, getMSTime(), HearthReady(botAI, bot), settings.stuckMs))
    {
        case StrandedPolicy::Step::GetOut:
            return GetOut();
        case StrandedPolicy::Step::Hearth:
            return CastHearthstone();
        case StrandedPolicy::Step::NoHearth:
            if (StrandedPolicy::NoteNoHearth(state, clock))
                LogStrandedStep(botAI, bot, state.polygons,
                        "no Hearthstone ready (none, on cooldown or bound here), left to the unseen-only teleport rules");
            return false;
        case StrandedPolicy::Step::None:
            break;
    }
    return false;
}

bool StrandedRecoveryAction::GetOut()
{
    StrandedPolicy::State& state = botAI->stranded;
    uint32 const clock = botAI->GetQuestClockMs();
    Map* map = bot->GetMap();
    if (!map)
        return false;

    // The pocket as it is now: a landing inside it is no way out. A bot that already stands on the open mesh is done.
    MeshRead const pocket = ReadBotMesh(bot);
    if (pocket.reading != StrandedPolicy::Reading::Closed)
        return false;
    dtNavMeshQuery const* query = MeshQuery(bot);
    if (!query)
        return false;
    dtQueryFilter const filter = WalkFilter();

    float const botX = bot->GetPositionX();
    float const botY = bot->GetPositionY();
    float const botZ = bot->GetPositionZ();
    StrandedPolicy::Candidate candidates[StrandedPolicy::CANDIDATE_COUNT];
    float meshZ[StrandedPolicy::CANDIDATE_COUNT];
    unsigned count = 0;
    for (unsigned i = 0; i < StrandedPolicy::CANDIDATE_COUNT; ++i)
    {
        float dx = 0.0f, dy = 0.0f;
        StrandedPolicy::Offset(i, dx, dy);
        float const x = botX + dx;
        float const y = botY + dy;
        // The surface under the point: a deck or a floor, else the terrain, which under water is the bed.
        float const ground = map->GetHeight(bot->GetPhaseMask(), x, y, botZ + BOT_Z_OFFSET_FIND_HEIGHT, true,
                                            GROUND_SEARCH_YARDS);
        if (ground <= INVALID_HEIGHT)
            continue;

        LiquidData liquid{};
        ZLiquidStatus const status = map->getLiquidStatus(x, y, ground, MAP_ALL_LIQUIDS, &liquid);
        bool water = false;
        if (status != LIQUID_MAP_NO_WATER)
        {
            // Never a landing in lava or slime.
            if (!(liquid.type_flags & (MAP_LIQUID_TYPE_WATER | MAP_LIQUID_TYPE_OCEAN)))
                continue;
            water = liquid.level - ground >= StrandedPolicy::MIN_WATER_DEPTH_YARDS;
        }

        StrandedPolicy::Candidate& candidate = candidates[count];
        candidate.x = x;
        candidate.y = y;
        candidate.z = water ? liquid.level : ground;
        candidate.flat = std::hypot(dx, dy);
        candidate.drop = botZ - candidate.z;
        candidate.water = water;
        candidate.clear = bot->IsWithinLOS(x, y, candidate.z + LANDING_EYE_HEIGHT);
        meshZ[count] = ground;
        // A landing on a polygon of the pocket is more of the same deck; one on no polygon has no mesh to walk on.
        // Dropping `clear` takes it out of the shortlist.
        if (candidate.clear && StrandedPolicy::Eligible(candidate, state.hasLast, state.lastX, state.lastY))
        {
            dtPolyRef const poly = StandingPolygon(query, filter, x, y, ground, 5.0f);
            if (!poly || std::binary_search(pocket.pocket.begin(), pocket.pocket.end(), poly))
                candidate.clear = false;
        }
        ++count;
    }

    unsigned picks[StrandedPolicy::PROBES_PER_TRY];
    unsigned const found = StrandedPolicy::Shortlist(candidates, count, state.hasLast, state.lastX, state.lastY, picks);
    for (unsigned k = 0; k < found; ++k)
    {
        StrandedPolicy::Candidate const& landing = candidates[picks[k]];
        // The landing must be on the open mesh: its own flood overflows the buffer.
        MeshRead const open = ReadMeshAt(bot, landing.x, landing.y, meshZ[picks[k]], 5.0f);
        if (open.reading != StrandedPolicy::Reading::Open)
            continue;

        char text[192];
        if (StrandedPolicy::InJumpReach(landing.flat))
        {
            if (bot->IsSitState())
                bot->SetStandState(UNIT_STAND_STATE_STAND);
            bool const jumped = JumpTo(bot->GetMapId(), landing.x, landing.y, landing.z, MovementPriority::MOVEMENT_COMBAT);
            std::snprintf(text, sizeof(text), "%s %.1f yd %s to the open mesh at (%.1f, %.1f, %.1f), try %u",
                          jumped ? "jumps" : "cannot jump", landing.flat, landing.water ? "into the water" : "down",
                          landing.x, landing.y, landing.z, state.getOutTries + 1);
            LogStrandedStep(botAI, bot, state.polygons, text);
            StrandedPolicy::NoteGetOut(state, jumped ? StrandedPolicy::Outcome::Jumped : StrandedPolicy::Outcome::Failed,
                                       landing.x, landing.y, clock);
            return jumped;
        }

        // Too far to jump from here: walk toward it at the bot's own height. The path ends at the rail of the deck, and
        // the next try jumps from there.
        bool const walking = MoveTo(bot->GetMapId(), landing.x, landing.y, botZ, false, false, false, true,
                                    MovementPriority::MOVEMENT_COMBAT);
        std::snprintf(text, sizeof(text), "%s %.1f yd toward the open mesh at (%.1f, %.1f, %.1f), try %u",
                      walking ? "walks" : "cannot walk", landing.flat, landing.x, landing.y, landing.z,
                      state.getOutTries + 1);
        LogStrandedStep(botAI, bot, state.polygons, text);
        StrandedPolicy::NoteGetOut(state, walking ? StrandedPolicy::Outcome::Walking : StrandedPolicy::Outcome::Failed,
                                   landing.x, landing.y, clock);
        return walking;
    }

    // Nothing within reach leads out: the step to the side a player makes, then the Hearthstone.
    char text[96];
    std::snprintf(text, sizeof(text), "finds no landing on the open mesh (%u read), side steps", count);
    LogStrandedStep(botAI, bot, state.polygons, text);
    StrandedPolicy::NoteGetOut(state, StrandedPolicy::Outcome::NoLanding, 0.0f, 0.0f, clock);
    return PatrolNear(WorldPosition(bot), SIDE_STEP_YARDS, MovementPriority::MOVEMENT_COMBAT);
}

bool StrandedRecoveryAction::CastHearthstone()
{
    StrandedPolicy::State& state = botAI->stranded;
    Item* stone = bot->GetItemByEntry(CrossMapTripPolicy::HEARTHSTONE_ITEM);
    if (!stone || bot->HasSpellCooldown(CrossMapTripPolicy::HEARTHSTONE_SPELL) || bot->IsInCombat())
        return false;

    if (MotionMaster* mm = bot->GetMotionMaster())
        mm->Clear(true);
    bot->StopMoving();
    botAI->RemoveShapeshift();
    if (bot->IsMounted())
        CheckMountStateAction::DismountLikeClient(bot);
    // Not this tick (still moving, a cast in progress): the next tick tries again, as a player would.
    if (bot->CanUseItem(stone) != EQUIP_ERR_OK || bot->IsNonMeleeSpellCasted(false) ||
        !botAI->CanCastSpell(CrossMapTripPolicy::HEARTHSTONE_SPELL, bot, false))
        return true;

    // What WorldSession::HandleUseItemOpcode does once the client's packet is parsed (UseItemAction::UseItem).
    SpellCastTargets targets;
    targets.SetUnitTarget(bot);
    if (!sScriptMgr->OnItemUse(bot, stone, targets))
        bot->CastItemUseSpell(stone, targets, 1, 0);

    StrandedPolicy::NoteHearth(state, getMSTime());
    char text[96];
    std::snprintf(text, sizeof(text), "casts the Hearthstone, try %u", state.hearthTries);
    LogStrandedStep(botAI, bot, state.polygons, text);
    return true;
}
