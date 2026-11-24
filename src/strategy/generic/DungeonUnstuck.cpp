/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "DungeonUnstuck.h"

#include "BotTerrain.h"
#include "BotVoice.h"
#include "Group.h"
#include <vector>
#include "Map.h"
#include "Player.h"
#include "PathGenerator.h"
#include "Playerbots.h"
#include "RandomPlayerbotMgr.h"

#include <cmath>
#include <cstdio>

namespace
{
// A bot this far from its player in an instance is behind (the follow places are 3 to 10 yards back).
constexpr float BEHIND_DISTANCE = 30.0f;
// Getting this much closer to the player counts as progress.
constexpr float PROGRESS_STEP = 3.0f;
// Seconds without progress before each step: walk straight to the player, then a side step, then the teleport.
constexpr time_t WALK_AFTER = 15;
constexpr time_t SIDE_STEP_AFTER = 30;
constexpr time_t TELEPORT_AFTER = 45;
constexpr float SIDE_STEP_DISTANCE = 6.0f;
// The action normally runs every 5 seconds (the trigger interval); a longer gap means the bot stopped qualifying.
constexpr time_t STALE_AFTER = 15;
// Where the teleport puts the bot: behind the player, out of the way of the next pull.
constexpr float ARRIVAL_DISTANCE = 3.0f;
// The landing may differ this much in height from the player and still count as the same floor.
constexpr float ARRIVAL_MAX_DZ = 3.0f;
// A human within this range who has the bot in line of sight would see it vanish.
constexpr float SEEN_RANGE = 100.0f;
// One party line at most this often.
constexpr time_t ANNOUNCE_COOLDOWN = 300;
// One "[UNSTUCK]" line at most this often per bot, so a bot that stays behind shows why (the path type needs one path
// search, which is why the line is rate limited).
constexpr time_t UNSTUCK_LOG_GAP = 30;
// A standing player's eye height, for the sight test from the bot to a landing.
constexpr float LANDING_EYE_HEIGHT = 1.5f;
// The "I'm stuck" lines (BotVoice key voice_stuck; the bot text table may carry variants). Rule-based: a bot always
// says the same one, picked by its guid, so it never reads like a roll. No dashes.
constexpr char const* STUCK_LINES[] = {
    "I'm stuck up here, can't find the way down",
    "stuck on a ledge, a little help?",
    "I'm stuck, no path from here",
    "can't get down from here, I'm stuck",
};
constexpr unsigned STUCK_LINE_COUNT = sizeof(STUCK_LINES) / sizeof(STUCK_LINES[0]);

// The real player the bot follows in a dungeon or raid, or nullptr (bot-led groups keep their own rules).
Player* InstanceMaster(PlayerbotAI* botAI, Player* bot)
{
    Player* master = botAI->GetMaster();
    if (!master || master == bot || GET_PLAYERBOT_AI(master) || !master->IsAlive() || master->GetMap() != bot->GetMap() ||
        master->IsBeingTeleported())
        return nullptr;

    // Dungeons and raids only: battlegrounds and arenas have their own movement tactics.
    Map* map = bot->GetMap();
    return map && map->Instanceable() && !map->IsBattlegroundOrArena() ? master : nullptr;
}


// A walkable landing behind the player on its floor, in its sight; otherwise the player's own spot.
void ArrivalPoint(Player* master, float& x, float& y, float& z)
{
    float const behind = master->GetOrientation() + float(M_PI);
    x = master->GetPositionX() + std::cos(behind) * ARRIVAL_DISTANCE;
    y = master->GetPositionY() + std::sin(behind) * ARRIVAL_DISTANCE;
    z = master->GetMap()->GetHeight(x, y, master->GetPositionZ() + 2.0f);
    if (std::fabs(z - master->GetPositionZ()) > ARRIVAL_MAX_DZ || !master->IsWithinLOS(x, y, z + 1.0f))
    {
        x = master->GetPositionX();
        y = master->GetPositionY();
        z = master->GetPositionZ();
    }
}
}  // namespace

bool BotSeenByRealPlayer(Player* bot)
{
    for (Player* player : sRandomPlayerbotMgr->GetPlayers())
    {
        if (!player || player->GetMap() != bot->GetMap() || (player->IsGameMaster() && !player->isGMVisible()))
            continue;

        if (player->IsWithinDistInMap(bot, SEEN_RANGE) && player->IsWithinLOSInMap(bot))
            return true;
    }
    return false;
}

void LogLastResort(Player* bot, char const* what, float toX, float toY, float toZ)
{
    SF_LOG_INFO("playerbots", "[LASTRESORT] %s %s at map %u zone %u (%.1f, %.1f, %.1f) -> (%.1f, %.1f, %.1f)",
                bot->GetName().c_str(), what, bot->GetMapId(), bot->GetZoneId(), bot->GetPositionX(), bot->GetPositionY(),
                bot->GetPositionZ(), toX, toY, toZ);
}

bool FarFromMasterInInstanceTrigger::IsActive()
{
    if (!bot->IsAlive() || bot->IsInCombat() || bot->GetTransport() || bot->GetVehicle() || bot->IsBeingTeleported() ||
        bot->IsInWater())
        return false;

    // A bot told to stay, or sitting to eat or drink, is behind on purpose.
    if (botAI->HasStrategy("stay", botAI->GetState()) || bot->IsSitState())
        return false;

    Player* master = InstanceMaster(botAI, bot);
    return master && !master->IsInCombat() && !bot->IsWithinDistInMap(master, BEHIND_DISTANCE);
}

bool DungeonUnstuckAction::isUseful()
{
    Player* master = InstanceMaster(botAI, bot);
    return master && !bot->IsWithinDistInMap(master, BEHIND_DISTANCE);
}

void DungeonUnstuckAction::Reset(float distanceToMaster, time_t now)
{
    bestDistance = distanceToMaster;
    lastProgress = now;
}

void DungeonUnstuckAction::SayStuck()
{
    BotVoice::Say(botAI, "voice_stuck", STUCK_LINES[bot->GetGUID() % STUCK_LINE_COUNT], {}, BotVoice::Channel::PARTY,
                  BotVoice::Priority::NOTICE);
}

bool DungeonUnstuckAction::TryWalkOff(Player* master, time_t now)
{
    // Found or not, the next try waits (a path search is not free).
    LedgeWalkOffPolicy::NoteAttempt(episode, now);
    Map* const map = bot->GetMap();
    if (!map || !LedgeWalkOffPolicy::HealthOk(bot->GetHealthPct()))
        return false;

    // A whole path to the player means the bot is not on a ledge the mesh does not join (BT-378 is another cause).
    PathGenerator route(bot);
    route.CalculatePath(master->GetPositionX(), master->GetPositionY(), master->GetPositionZ(), false);
    uint32 const routeType = uint32(route.GetPathType());
    if ((routeType & PATHFIND_NORMAL) && !(routeType & (PATHFIND_NOPATH | PATHFIND_INCOMPLETE | PATHFIND_SHORT)))
        return false;

    float const botX = bot->GetPositionX();
    float const botY = bot->GetPositionY();
    float const botZ = bot->GetPositionZ();
    // No path at all and the leader far below: the floor it stands on may be further out than the nearest edge.
    bool const wide = LedgeWalkOffPolicy::WantsLeaderFloor((routeType & PATHFIND_NOPATH) != 0, botZ - master->GetPositionZ());
    episode.wide = wide;
    float const maxDrop = LedgeWalkOffPolicy::MaxDrop(wide);
    LedgeWalkOffPolicy::Candidate candidates[LedgeWalkOffPolicy::CANDIDATE_COUNT];
    unsigned count = 0;
    for (unsigned i = 0; i < LedgeWalkOffPolicy::CandidateCount(wide); ++i)
    {
        float dx = 0.0f, dy = 0.0f;
        LedgeWalkOffPolicy::Offset(i, dx, dy);
        float const x = botX + dx;
        float const y = botY + dy;
        // The ground under the point, searched this far down only (the drop bound), before any sight test.
        float const z = map->GetHeight(x, y, botZ + BOT_Z_OFFSET_FIND_HEIGHT, true, maxDrop + 2.0f);
        float const drop = botZ - z;
        if (z <= INVALID_HEIGHT || drop > maxDrop || drop < -LedgeWalkOffPolicy::MAX_RISE_YARDS)
            continue;

        LedgeWalkOffPolicy::Candidate& candidate = candidates[count++];
        candidate.x = x;
        candidate.y = y;
        candidate.z = z;
        candidate.flat = std::hypot(dx, dy);
        candidate.drop = drop;
        candidate.clear = bot->IsWithinLOS(x, y, z + LANDING_EYE_HEIGHT);
        candidate.onLeaderFloor = wide && LedgeWalkOffPolicy::OnLeaderFloor(z, master->GetPositionZ());
    }

    unsigned shortlist[LedgeWalkOffPolicy::PATH_SEARCHES];
    unsigned const found = LedgeWalkOffPolicy::Shortlist(candidates, count, episode, shortlist);
    // What each path search said, for the "no landing" line: the proof run of 2026-11-23 logged only the counts, so the
    // reason the three short-listed spots failed (no whole path, or a jump the movement refused) was not readable.
    char tried[LedgeWalkOffPolicy::PATH_SEARCHES * 40 + 1] = {};
    size_t triedLength = 0;
    for (unsigned k = 0; k < found; ++k)
    {
        LedgeWalkOffPolicy::Candidate const& landing = candidates[shortlist[k]];
        // Remembered whatever the result: the next try searches other spots.
        LedgeWalkOffPolicy::NoteSearched(episode, landing.x, landing.y);
        // The landing must be on the mesh the player stands on: a whole path from there to the player.
        PathGenerator fromLanding(bot);
        fromLanding.CalculatePath(landing.x, landing.y, landing.z, master->GetPositionX(), master->GetPositionY(),
                                  master->GetPositionZ(), false);
        uint32 const type = uint32(fromLanding.GetPathType());
        bool const whole = (type & PATHFIND_NORMAL) && !(type & (PATHFIND_NOPATH | PATHFIND_INCOMPLETE | PATHFIND_SHORT));
        bool jumped = false;
        if (whole)
            jumped = JumpTo(bot->GetMapId(), landing.x, landing.y, landing.z, MovementPriority::MOVEMENT_COMBAT);
        if (jumped)
        {
            LedgeWalkOffPolicy::NoteJump(episode, landing.x, landing.y, now);
            SF_LOG_INFO("playerbots", "[UNSTUCK] %s walks off the ledge: jumps %.1f yd down, %.1f yd out, to "
                        "(%.1f, %.1f, %.1f) from (%.1f, %.1f, %.1f), whole path to %s, try %u",
                        bot->GetName().c_str(), landing.drop, landing.flat, landing.x, landing.y, landing.z, botX, botY,
                        botZ, master->GetName().c_str(), episode.attempts);
            return true;
        }
        if (triedLength < sizeof(tried))
        {
            int const written = std::snprintf(tried + triedLength, sizeof(tried) - triedLength, " [drop %.0f path %u%s]",
                                              landing.drop, type, whole ? " jump refused" : "");
            if (written > 0)
                triedLength += size_t(written);
        }
    }

    SF_LOG_INFO("playerbots", "[UNSTUCK] %s finds no landing within %.0f yd: %u read, %u short-listed, try %u:%s",
                bot->GetName().c_str(), LedgeWalkOffPolicy::RING_YARDS[LedgeWalkOffPolicy::CandidateCount(wide) /
                    LedgeWalkOffPolicy::DIRECTIONS - 1], count, found,
                episode.attempts, triedLength ? tried : " none");
    return false;
}

bool DungeonUnstuckAction::Execute(Event /*event*/)
{
    Player* master = InstanceMaster(botAI, bot);
    if (!master)
        return false;

    time_t const now = time(nullptr);
    float const distance = bot->GetDistance(master);
    bool const stale = !lastRun || now - lastRun > STALE_AFTER || masterGuid != master->GetGUID();
    lastRun = now;
    masterGuid = master->GetGUID();

    if (stale)
        episode = LedgeWalkOffPolicy::Episode();

    // BT-388: a jump is in flight: let it land, then start the clock afresh from the new spot.
    bool landed = false;
    if (LedgeWalkOffPolicy::Settling(episode, now, landed))
        return true;
    if (landed)
    {
        Reset(distance, now);
        return false;
    }

    // A fresh start, or the bot got closer: the normal follow is working.
    if (stale || distance <= bestDistance - PROGRESS_STEP)
    {
        Reset(distance, now);
        return false;
    }

    time_t const stuckFor = now - lastProgress;
    if (stuckFor < WALK_AFTER)
        return false;

    // Shadowfang Keep proof runs (2026-11-16, three runs): a healer stood on a wall walk the navmesh does not join to the
    // floor for 5 minutes, 63 yards from its player, and neither a teleport nor a "took a wrong turn" line followed.
    // This line says which stage the bot is in, whether a human could see it (that holds the teleport back), and whether
    // the navmesh has any path to the player.
    if (!lastLogged || now - lastLogged >= UNSTUCK_LOG_GAP)
    {
        lastLogged = now;
        PathGenerator probe(bot);
        probe.CalculatePath(master->GetPositionX(), master->GetPositionY(), master->GetPositionZ(), false);
        // BT-388: the bot's own spot, so a ledge the navmesh does not join shows where it is (with the player's z).
        SF_LOG_INFO("playerbots", "[UNSTUCK] %s stuck for %us at %.1f yd from %s, stage=%s seen=%u path_type=%u "
                    "dz=%.1f at=(%.1f, %.1f, %.1f)", bot->GetName().c_str(), unsigned(stuckFor), distance,
                    master->GetName().c_str(),
                    stuckFor < SIDE_STEP_AFTER ? "walk" : (stuckFor < TELEPORT_AFTER ? "side step" : "teleport"),
                    uint32(stuckFor >= TELEPORT_AFTER && BotSeenByRealPlayer(bot)), uint32(probe.GetPathType()),
                    master->GetPositionZ() - bot->GetPositionZ(), bot->GetPositionX(), bot->GetPositionY(),
                    bot->GetPositionZ());
    }

    // BT-388: after a minute the bot says so, once, so the player can act; the walk-off below tries to help it too.
    if (LedgeWalkOffPolicy::ShouldSay(stuckFor, episode))
    {
        episode.said = true;
        SayStuck();
    }

    if (stuckFor < SIDE_STEP_AFTER)
        return MoveTo(master, ARRIVAL_DISTANCE);

    if (stuckFor < TELEPORT_AFTER)
    {
        // A short step to a random side, so the next path starts from somewhere else.
        float const angle = bot->GetOrientation() + (urand(0, 1) ? 1.0f : -1.0f) * float(M_PI) / 2.0f;
        float const x = bot->GetPositionX() + std::cos(angle) * SIDE_STEP_DISTANCE;
        float const y = bot->GetPositionY() + std::sin(angle) * SIDE_STEP_DISTANCE;
        return MoveTo(bot->GetMapId(), x, y, bot->GetPositionZ());
    }

    // Last resort, only while no human sees the bot leave; otherwise it keeps trying to walk. BT-388: and, from a
    // minute on, jumps off a ledge the mesh does not join (the teleport rule stays: no bot vanishes in a player's
    // view).
    if (BotSeenByRealPlayer(bot))
    {
        if (LedgeWalkOffPolicy::MayWalkOff(stuckFor, episode, now) && TryWalkOff(master, now))
            return true;
        return MoveTo(master, ARRIVAL_DISTANCE);
    }

    float x, y, z;
    ArrivalPoint(master, x, y, z);
    // BT-158: neither endpoint may be separated from the landing by dynamic collision.
    std::vector<G3D::Vector3> line = { G3D::Vector3(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ()),
                                      G3D::Vector3(x, y, z) };
    size_t segment;
    G3D::Vector3 hit;
    if (BotPathBlockedByClosedDoor(bot, line, segment, hit))
        return MoveTo(master, ARRIVAL_DISTANCE);
    line[0] = G3D::Vector3(master->GetPositionX(), master->GetPositionY(), master->GetPositionZ());
    if (BotPathBlockedByClosedDoor(bot, line, segment, hit))
        return MoveTo(master, ARRIVAL_DISTANCE);
    if (now - lastAnnounce >= ANNOUNCE_COOLDOWN)
    {
        botAI->SayToParty("took a wrong turn, coming");
        lastAnnounce = now;
    }
    LogLastResort(bot, "dungeon unstuck teleport", x, y, z);
    bot->NearTeleportTo(x, y, z, master->GetOrientation());
    Reset(ARRIVAL_DISTANCE, now);
    return true;
}
