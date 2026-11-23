/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_TPMIDLOCKPOLICY_H
#define _PLAYERBOT_TPMIDLOCKPOLICY_H

// Twin Peaks middle deadlock (BT-694, docs/bg-sheets/twin-peaks.md section 6, owner decision 2026-11-19: "home bots join after
// 60 s"). Live runs of five against five bots (builds 24 and 25): no flag was ever taken, three bots of each team fought in the
// middle of the valley with a healer on each side and nobody died, while two per team stood at their flag room posts. The plan
// only changed with the clock, so nothing ever tipped the fight.
//
// The rule: when both teams have bots fighting in the middle zone, no flag is out, nobody died and neither team's takers got
// through, for LOCK_JOIN_MS, the home defenders that nothing threatens leave their posts and join the fight. The lock ends on a
// kill, a flag pickup, the takers getting through, or the fight being over (the enemy collapsed or both sides left it); the
// defenders then go back to their posts or follow the plan's next state.
//
// Pure logic, free of game types so that tests/ can run it (tests/test_tp_mid_lock.cpp). The plan (TpPlan.cpp) fills the facts
// from the roster it already reads at every refresh and applies the answer; nothing here scans the map.

#include "BgFlagMap.h"
#include "TpPlanLogic.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace TpMidLock
{
using BgFlagMap::Pos;

// The owner's 60 s: how long the fight has to stay locked before the defenders join.
constexpr std::uint32_t LOCK_JOIN_MS = 60u * 1000u;
// A fight that looks over for less than this is not over: a healer between casts, a bot that stepped out of combat for a
// moment. Without it the defenders would walk home and back at every flicker of the combat flag. A kill and a flag pickup
// are not softened: they end the lock at once.
constexpr std::uint32_t LOCK_BREAK_GRACE_MS = 10u * 1000u;
// The middle zone: round the river centre (the lowest point of the valley, 1840, 320). The two lanes, both middle graveyards
// (162 yd and 125 yd from the centre) and all four buffs (116 to 170 yd) lie inside; the flag rooms (about 280 yd) do not. The
// live fight stood 100 to 140 yd south of the centre, outside the sheet's 70 yd "mid" place word, so the lock has its own zone.
constexpr float MID_ZONE_YARDS = 180.0f;
// Bots of each team in combat inside the zone for it to count as a fight (a lone skirmish is not a deadlock).
constexpr std::uint32_t MIN_FIGHTERS_PER_SIDE = 2;
// One of our takers this close to the enemy stand, or an enemy this close to ours, means someone got through: the flag fight
// has begun and the defenders stay where the plan has them (the plan's own base threat radius).
constexpr float THROUGH_YARDS = TpLogic::BASE_THREAT_RADIUS;
// Defenders that stay at their posts while the lock is on. Zero: the owner's rule sends the idle ones, and "idle" already
// excludes a base that anything threatens (below). Raise it to keep a guard in a room nobody can threaten yet.
constexpr std::uint32_t HOME_KEPT_AT_POST = 0;
// The point the joiners walk to is never inside the enemy's respawn graveyard zone (the graveyard rule forbids standing there):
// it is moved out to this many yards from the graveyard, towards the fight.
constexpr float GY_KEEP_OUT_YARDS = 55.0f;

inline bool InMidZone(Pos const& at, Pos const& mid)
{
    return TpLogic::Dist2(at, mid) <= MID_ZONE_YARDS;
}

// ---------------------------------------------------------------------------------------------------------------------
// The detour outranks the fight (BT-705). Live run, build 30: the takers on the other lane's route stayed in the middle brawl,
// because a bot in combat does not move to its objective (only a flag carrier does). A taker on the detour walks on while it is
// in combat, like a carrier; below DETOUR_MIN_HEALTH_PCT it stays in the normal combat rules (defensives, heals, the retreat).
// ---------------------------------------------------------------------------------------------------------------------
constexpr float DETOUR_MIN_HEALTH_PCT = 30.0f;

inline bool DetourOverridesCombat(bool onDetour, bool alive, float healthPct)
{
    return onDetour && alive && healthPct >= DETOUR_MIN_HEALTH_PCT;
}

enum class Release : std::uint8_t
{
    NONE,
    KILL,         // someone died: the fight is no longer locked
    FLAG,         // a flag left its stand
    THROUGH,      // a taker of either team reached the other team's base
    FIGHT_OVER,   // fewer than MIN_FIGHTERS_PER_SIDE on a side for LOCK_BREAK_GRACE_MS (the enemy collapsed, or both left)
    MATCH         // the match is not in progress
};

inline char const* ReleaseName(Release why)
{
    switch (why)
    {
        case Release::KILL: return "kill";
        case Release::FLAG: return "flag";
        case Release::THROUGH: return "through";
        case Release::FIGHT_OVER: return "fight-over";
        case Release::MATCH: return "match";
        case Release::NONE: break;
    }
    return "none";
}

struct Facts
{
    std::uint32_t nowMs = 0;
    bool inProgress = false;
    std::uint32_t ownFighters = 0;     // alive, in combat, inside the zone: our team
    std::uint32_t enemyFighters = 0;   // the same for the enemy team
    bool flagOut = false;              // any flag off its stand (carried, dropped or waiting to respawn)
    std::uint32_t latestDeathMs = 0;   // the newest death of either team (0 for none yet)
    bool takersThrough = false;        // a taker of ours near their stand, or an enemy near ours
    std::uint8_t fightLane = 0;        // the lane (Lane) the fight stands on, 0 when our fighters are not in the zone
};

struct State
{
    std::uint32_t lockedSinceMs = 0;   // when the lock began, 0 for none
    std::uint32_t quietSinceMs = 0;    // since when the fight looks over, 0 while it goes on
    std::uint32_t seenDeathMs = 0;     // the death time last answered
    bool joining = false;              // the defenders are called
    std::uint8_t lastFightLane = 0;    // the lane of our fighters at the last read that had any
    std::uint8_t avoidLane = 0;        // the lane (Lane) the strike group does not walk again, 0 for none
    std::uint32_t avoidSinceMs = 0;    // when the avoidance began
};

struct Verdict
{
    bool locked = false;
    bool join = false;                 // the defenders are called now
    bool joinStarted = false;          // this read called them (for the call-out and the log line)
    Release released = Release::NONE;  // set on the read that ended a call (not a lock that never called anyone)
    std::uint32_t lockedForMs = 0;
    // BT-695: the strike group's lane after a kill in a locked fight.
    std::uint8_t avoidLane = 0;        // the lane (Lane) to stay off now, 0 for none
    bool avoidStarted = false;         // this read began the avoidance (a kill in a locked fight)
    bool avoidEnded = false;           // this read ended it
    Release avoidEndedWhy = Release::NONE;   // NONE with avoidEnded: the window ran out
};

inline void Reset(State& state)
{
    state.lockedSinceMs = 0;
    state.quietSinceMs = 0;
    state.joining = false;
}

// ---------------------------------------------------------------------------------------------------------------------
// The other lane (BT-695, owner direction 2026-11-19 after the build 26 run: the lock was released by kills, 19 minutes, no pickup,
// because the re-formed strike groups walked back into the same fight). The valley has two lanes, named by the middle graveyard
// they hold (docs/bg-sheets/twin-peaks.md section 2). After a kill in a locked fight the takers go round by the other lane for as
// long as the fight in the first one holds; the fight over, a flag out, a taker through or the end of the match ends it, and so does
// the window running out.
// ---------------------------------------------------------------------------------------------------------------------
enum class Lane : std::uint8_t
{
    NONE = 0,
    A = 1,   // the Alliance graveyard's lane (1749, y about 380 to 450)
    H = 2    // the Horde graveyard's lane (1750, y about 160 to 240)
};

constexpr std::uint32_t LANE_AVOID_MAX_MS = 180u * 1000u;
// A taker this close to the other lane's far post has gone round: it walks on to the stand (and does not turn back).
constexpr float VIA_REACHED_YARDS = 45.0f;

inline char const* LaneName(std::uint8_t lane)
{
    return lane == static_cast<std::uint8_t>(Lane::A) ? "A" : lane == static_cast<std::uint8_t>(Lane::H) ? "H" : "-";
}

// The lane a point stands on: the nearer of the two middle graveyards.
inline std::uint8_t LaneOf(Pos const& at, Pos const& laneAGraveyard, Pos const& laneHGraveyard)
{
    return TpLogic::Dist2(at, laneAGraveyard) <= TpLogic::Dist2(at, laneHGraveyard) ? static_cast<std::uint8_t>(Lane::A)
                                                                                    : static_cast<std::uint8_t>(Lane::H);
}

inline std::uint8_t OtherLane(std::uint8_t lane)
{
    return lane == static_cast<std::uint8_t>(Lane::A) ? static_cast<std::uint8_t>(Lane::H)
                                                       : lane == static_cast<std::uint8_t>(Lane::H) ? static_cast<std::uint8_t>(Lane::A)
                                                                                                   : static_cast<std::uint8_t>(Lane::NONE);
}

// The post of the lane that lies on the enemy's half of the valley, for a team walking to the enemy stand: the Alliance (east)
// goes west, so the Horde's posts (cut on lane A, cover on lane H); the Horde goes east, so the Alliance's (cover on lane A, cut
// on lane H). `ownPosts` and `foePosts` are TpLogic::LanePostsOf of the two teams.
inline Pos ViaPost(std::uint8_t walkLane, bool allianceTeam, TpLogic::LanePosts const& allianceTeamPosts,
                   TpLogic::LanePosts const& hordeTeamPosts)
{
    bool const laneA = walkLane == static_cast<std::uint8_t>(Lane::A);
    if (allianceTeam)
        return laneA ? hordeTeamPosts.cut : hordeTeamPosts.cover;
    return laneA ? allianceTeamPosts.cover : allianceTeamPosts.cut;
}

inline bool ViaReached(Pos const& bot, Pos const& via)
{
    return TpLogic::Dist2(bot, via) <= VIA_REACHED_YARDS;
}

// One read. A kill, a flag out, the takers through or a match that is not running end the lock at once; a fight that is not
// there any more (fewer than MIN_FIGHTERS_PER_SIDE on a side) ends it after LOCK_BREAK_GRACE_MS. While both sides fight in the
// zone the lock starts or goes on; after LOCK_JOIN_MS of it the defenders are called. `released` names why a call ended.
inline Verdict Update(State& state, Facts const& f)
{
    Verdict out;
    std::uint32_t const now = f.nowMs ? f.nowMs : 1;
    bool const wasJoining = state.joining;
    bool const wasLocked = state.lockedSinceMs != 0;

    auto const endAvoid = [&](Release why)
    {
        if (state.avoidLane)
        {
            out.avoidEnded = true;
            out.avoidEndedWhy = why;
        }
        state.avoidLane = 0;
        state.avoidSinceMs = 0;
    };
    auto const end = [&](Release why)
    {
        out.released = wasJoining ? why : Release::NONE;
        Reset(state);
        // The lane rule outlives a kill (that is its start) but not the end of the fight, a flag, the takers getting through or the match.
        if (why != Release::KILL)
            endAvoid(why);
    };

    // A newer death time than the last answered. A newest death that vanishes (the player left the match) is no kill, and the
    // server's millisecond clock wraps, so the order is read from the signed difference.
    bool const died = static_cast<std::int32_t>(f.latestDeathMs - state.seenDeathMs) > 0;
    if (died)
        state.seenDeathMs = f.latestDeathMs;
    if (f.fightLane)
        state.lastFightLane = f.fightLane;

    if (!f.inProgress)
    {
        end(Release::MATCH);
        return out;
    }
    if (f.flagOut)
    {
        end(Release::FLAG);
        return out;
    }
    if (f.takersThrough)
    {
        end(Release::THROUGH);
        return out;
    }
    // A kill ends this lock; the clock of the next one starts at this read when the fight goes on. A kill in a locked fight also
    // names the lane the strike groups stay off (BT-695): they walk the other one while the fight holds.
    if (died)
    {
        end(Release::KILL);
        if (wasLocked && state.lastFightLane)
        {
            out.avoidStarted = state.avoidLane != state.lastFightLane;
            state.avoidLane = state.lastFightLane;
            state.avoidSinceMs = now;
        }
    }

    bool const fight = f.ownFighters >= MIN_FIGHTERS_PER_SIDE && f.enemyFighters >= MIN_FIGHTERS_PER_SIDE;
    if (fight)
    {
        state.quietSinceMs = 0;
        if (!state.lockedSinceMs)
            state.lockedSinceMs = now;
    }
    else if (state.lockedSinceMs || state.avoidLane)
    {
        if (!state.quietSinceMs)
            state.quietSinceMs = now;
        if (now - state.quietSinceMs >= LOCK_BREAK_GRACE_MS)
        {
            end(Release::FIGHT_OVER);
            return out;
        }
    }
    if (state.avoidLane && now - state.avoidSinceMs >= LANE_AVOID_MAX_MS)
        endAvoid(Release::NONE);

    out.locked = state.lockedSinceMs != 0;
    out.lockedForMs = out.locked ? now - state.lockedSinceMs : 0;
    if (out.locked && out.lockedForMs >= LOCK_JOIN_MS)
    {
        out.join = true;
        out.joinStarted = !state.joining;
        state.joining = true;
    }
    out.avoidLane = state.avoidLane;
    return out;
}

// ---------------------------------------------------------------------------------------------------------------------
// Who joins (a pure choice from the home bots the plan lists).
// ---------------------------------------------------------------------------------------------------------------------
struct HomeBot
{
    std::uint32_t guid = 0;
    std::uint32_t roleIndex = 0;   // among the home bots of the team, the plan's order
    bool alive = false;
    bool goodTier = false;         // expert or good: casual bots keep their post (they are the ones who make mistakes)
    bool inCombat = false;
    bool joined = false;           // answered the call at the last read: it stays until the lock ends, whatever it fights
};

// The defence is never stripped while our flag is carried, and never while enemies stand at our base: those defenders are not
// idle. The caller passes the plan's own base threat (BaseThreatened: the enemy carries our flag, or 2 or more enemies are
// within the base radius, call-outs included).
inline std::vector<std::uint32_t> Joiners(std::vector<HomeBot> const& homeBots, bool joinCalled, bool enemyCarriesOurFlag,
                                          bool baseThreatened)
{
    std::vector<std::uint32_t> out;
    if (!joinCalled || enemyCarriesOurFlag || baseThreatened)
        return out;

    // The guards that stay: the lowest role indexes of the alive ones (a fixed order, so the same bots stay every read).
    std::vector<HomeBot const*> alive;
    for (HomeBot const& b : homeBots)
        if (b.alive)
            alive.push_back(&b);
    std::sort(alive.begin(), alive.end(), [](HomeBot const* a, HomeBot const* b)
    {
        return a->roleIndex != b->roleIndex ? a->roleIndex < b->roleIndex : a->guid < b->guid;
    });

    for (std::size_t i = HOME_KEPT_AT_POST; i < alive.size(); ++i)
    {
        HomeBot const& b = *alive[i];
        // A bot that left its post already stays in the fight; a new one needs to be idle and of the tier that plays this.
        if (b.joined || (b.goodTier && !b.inCombat))
            out.push_back(b.guid);
    }
    return out;
}

// ---------------------------------------------------------------------------------------------------------------------
// The decision point as a candidate list (design decision 0014 and 0026): the plan plays the first, a later decision layer may pick the
// other id. Not yet offered to the layer.
// ---------------------------------------------------------------------------------------------------------------------
enum class Reply : std::uint8_t
{
    HOLD_POSTS,   // the defenders stay at the flag room posts
    JOIN_FIGHT    // the idle defenders join the fight in the middle
};

inline std::vector<TpLogic::Scored<Reply>> Replies(bool joinCalled, bool baseThreatened)
{
    std::vector<TpLogic::Scored<Reply>> list;
    list.push_back({ Reply::JOIN_FIGHT, joinCalled && !baseThreatened ? 100 : 10 });
    list.push_back({ Reply::HOLD_POSTS, 60 });
    return TpLogic::Ranked(list);
}

// ---------------------------------------------------------------------------------------------------------------------
// The strike group's route after a kill in a locked fight (BT-695), as candidate options (design decision 0014 and 0026): the plan plays the
// first, a later decision layer may pick the other id.
//
// The takers' route out: both teams' takers walk by lane H (live run, build 28: the Horde taker, backup and escort stood at lane H
// with the Alliance's, 1760 to 1790, 200 to 250; the sheet's lane A for the Horde is the carrier's way home). The fight the lock saw
// stood on one lane; when it stands on lane H the takers go round by lane A. When it stands on lane A they would not walk it
// anyway, nothing changes. A casual bot keeps its route (it makes the human mistake); an
// expert or good bot detours. A taker that already stood at the other lane's post for this avoidance walks on to the stand.
// ---------------------------------------------------------------------------------------------------------------------
enum class RouteChoice : std::uint8_t
{
    OWN_ROUTE,    // the route the plan always had
    OTHER_LANE    // walk to the post of the other lane first, then on to the stand
};

constexpr int ROUTE_OWN_SCORE = 100;
constexpr int ROUTE_DETOUR_SCORE = 140;
constexpr int ROUTE_DETOUR_CASUAL_SCORE = 90;
constexpr int ROUTE_DETOUR_DONE_SCORE = 10;

inline std::uint8_t OutboundLane()
{
    return static_cast<std::uint8_t>(Lane::H);
}

inline std::vector<TpLogic::Scored<RouteChoice>> RouteChoices(std::uint8_t avoidLane, bool goodTier, bool detourDone)
{
    bool const fightOnOurRoute = avoidLane != 0 && avoidLane == OutboundLane();
    int const detour = !fightOnOurRoute ? ROUTE_DETOUR_DONE_SCORE : detourDone ? ROUTE_DETOUR_DONE_SCORE
                       : goodTier ? ROUTE_DETOUR_SCORE : ROUTE_DETOUR_CASUAL_SCORE;
    std::vector<TpLogic::Scored<RouteChoice>> list;
    list.push_back({ RouteChoice::OWN_ROUTE, ROUTE_OWN_SCORE });
    list.push_back({ RouteChoice::OTHER_LANE, detour });
    return TpLogic::Ranked(list);
}

// ---------------------------------------------------------------------------------------------------------------------
// The escort after a kill release (BT-695): one of the guards that joined walks with the strike group's taker for that run instead
// of walking home. Never while a flag is out (ours carried, or theirs: the carrier has its own escorts) and never while the base is
// threatened; it ends when any of that starts, when the taker or the guard dies, when the takers get through (the flag fight has
// begun) or after ESCORT_MAX_MS.
// ---------------------------------------------------------------------------------------------------------------------
constexpr std::uint32_t ESCORT_MAX_MS = 150u * 1000u;

enum class ReleaseReply : std::uint8_t
{
    WALK_HOME,     // the guard goes back to its post
    ESCORT_TAKER   // the guard walks with the taker
};

inline std::vector<TpLogic::Scored<ReleaseReply>> ReleaseReplies(bool takerAvailable, bool guardAvailable, bool flagOut, bool baseThreatened)
{
    bool const ok = takerAvailable && guardAvailable && !flagOut && !baseThreatened;
    std::vector<TpLogic::Scored<ReleaseReply>> list;
    list.push_back({ ReleaseReply::ESCORT_TAKER, ok ? 100 : 5 });
    list.push_back({ ReleaseReply::WALK_HOME, 60 });
    return TpLogic::Ranked(list);
}

struct TakerBot
{
    std::uint32_t guid = 0;
    std::uint32_t rank = 0;   // 0 the taker, 1 a backup, 2 an escort of the strike group
    bool alive = false;
};

struct EscortPick
{
    std::uint32_t guard = 0;
    std::uint32_t taker = 0;
};

// The guard: the lowest role index of the alive guards that joined (a fixed order). The taker: the best ranked alive striker.
inline EscortPick PickEscort(std::vector<HomeBot> const& homeBots, std::vector<TakerBot> const& takers)
{
    EscortPick pick;
    HomeBot const* guard = nullptr;
    for (HomeBot const& b : homeBots)
    {
        if (!b.alive || !b.joined)
            continue;
        if (!guard || b.roleIndex < guard->roleIndex || (b.roleIndex == guard->roleIndex && b.guid < guard->guid))
            guard = &b;
    }
    TakerBot const* taker = nullptr;
    for (TakerBot const& t : takers)
    {
        if (!t.alive)
            continue;
        if (!taker || t.rank < taker->rank || (t.rank == taker->rank && t.guid < taker->guid))
            taker = &t;
    }
    if (guard && taker)
    {
        pick.guard = guard->guid;
        pick.taker = taker->guid;
    }
    return pick;
}

enum class EscortEnd : std::uint8_t
{
    NONE,
    MATCH,
    FLAG,
    BASE_THREAT,
    TAKER_GONE,
    GUARD_GONE,
    THROUGH,
    WINDOW
};

inline char const* EscortEndName(EscortEnd why)
{
    switch (why)
    {
        case EscortEnd::MATCH: return "match";
        case EscortEnd::FLAG: return "flag";
        case EscortEnd::BASE_THREAT: return "base-threat";
        case EscortEnd::TAKER_GONE: return "taker-gone";
        case EscortEnd::GUARD_GONE: return "guard-gone";
        case EscortEnd::THROUGH: return "through";
        case EscortEnd::WINDOW: return "window";
        case EscortEnd::NONE: break;
    }
    return "none";
}

struct EscortState
{
    std::uint32_t guard = 0;
    std::uint32_t taker = 0;
    std::uint32_t sinceMs = 0;
};

struct EscortFacts
{
    std::uint32_t nowMs = 0;
    bool inProgress = false;
    bool flagOut = false;
    bool baseThreatened = false;
    bool takerAlive = false;
    bool guardAlive = false;
    bool takersThrough = false;
};

struct EscortVerdict
{
    bool active = false;
    bool started = false;
    EscortEnd ended = EscortEnd::NONE;
};

// One read. `start` is the pick of this read (a kill release), zeros for none; it only starts when ReleaseReplies plays the escort.
inline EscortVerdict EscortStep(EscortState& state, EscortFacts const& f, EscortPick const& start)
{
    EscortVerdict out;
    std::uint32_t const now = f.nowMs ? f.nowMs : 1;

    if (!state.guard)
    {
        if (!start.guard || !start.taker)
            return out;
        auto const replies = ReleaseReplies(true, true, f.flagOut, f.baseThreatened);
        if (replies.front().id != ReleaseReply::ESCORT_TAKER || !f.inProgress)
            return out;
        state.guard = start.guard;
        state.taker = start.taker;
        state.sinceMs = now;
        out.started = true;
        out.active = true;
        return out;
    }

    EscortEnd why = EscortEnd::NONE;
    if (!f.inProgress)
        why = EscortEnd::MATCH;
    else if (f.flagOut)
        why = EscortEnd::FLAG;
    else if (f.baseThreatened)
        why = EscortEnd::BASE_THREAT;
    else if (f.takersThrough)
        why = EscortEnd::THROUGH;
    else if (!f.takerAlive)
        why = EscortEnd::TAKER_GONE;
    else if (!f.guardAlive)
        why = EscortEnd::GUARD_GONE;
    else if (now - state.sinceMs >= ESCORT_MAX_MS)
        why = EscortEnd::WINDOW;

    if (why != EscortEnd::NONE)
    {
        state = EscortState();
        out.ended = why;
        return out;
    }
    out.active = true;
    return out;
}

// ---------------------------------------------------------------------------------------------------------------------
// Where the joiners go: the middle of our fighters in the zone, kept out of the enemy's respawn graveyard zone.
// ---------------------------------------------------------------------------------------------------------------------
inline Pos KeepOut(Pos const& point, Pos const& graveyard, float zoneYards)
{
    float const d = TpLogic::Dist2(point, graveyard);
    if (d >= zoneYards)
        return point;
    if (d < 0.01f)
        return { graveyard.x + zoneYards, graveyard.y, point.z };

    float const f = zoneYards / d;
    return { graveyard.x + (point.x - graveyard.x) * f, graveyard.y + (point.y - graveyard.y) * f, point.z };
}
}  // namespace TpMidLock

#endif
