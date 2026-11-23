/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_TPPLANLOGIC_H
#define _PLAYERBOT_TPPLANLOGIC_H

// The Twin Peaks playbook (docs/bg-sheets/twin-peaks.md, owner approved 2026-11-18) as pure logic, free of game types so
// that tests/ can run it. Twin Peaks plays the Warsong Gulch plan (WsgPlan.cpp, Utils/WsgPlanLogic.h) on its own ground;
// this file holds what is Twin Peaks only: the map data (BT-471), and in later steps the lanes, the posts, the kill
// window, the respawn model and the wipe states.

#include "BgFlagMap.h"
#include "BgPlanCommon.h"
#include "WsgPlanLogic.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

namespace TpLogic
{
using BgFlagMap::Pos;

constexpr std::uint32_t TP_MAP_ID = 726;

// The core ends the match when the instance clock reaches 30 minutes (BattlegroundTP.cpp:63 in the core, 28 of play after
// the 2 minute preparation). The owner decided on 2026-11-18 (question 2, answer a) that the fork moves it to 27 minutes
// (BT-461), the value of the 5.4.8 reference core. The plan counts the shorter limit and reads the longer one live
// (BgFlagMap::Clock), so it is right before and after the core change: on a core still at 30 it closes the match up to
// 3 minutes early in its own rules, which costs little, while the other way round it would be late.
constexpr std::uint32_t TP_LIMIT_MS = 27u * 60u * 1000u;
constexpr std::uint32_t TP_LEGACY_LIMIT_MS = 30u * 60u * 1000u;

// Map 726, index = TeamId (Alliance 0 holds the high plateau, Horde 1 the low base). Every number is the server's own
// data (sheet section 2): stands from BattlegroundTP::SetupBattleground (BattlegroundTP.cpp:708-709), the middle
// graveyards from WorldSafeLocs 1749 / 1750, the entrance and hide posts from the module's navmesh-snapped points
// (TP_GUARD_POSTS_*, TP_FLAG_HIDE_* in BattleGroundTactics.cpp), the river centre from TP_ROAM_POS (the lowest point of
// the valley, on the navmesh).
//
// tunnelExit is the graveyard hop target: the main-mesh landing next to the team's middle graveyard island (what
// BGTactics::tpJumpDown jumps to; the graveyards stand on islands with no walkable link, module issue 747).
// gather is where the ENEMY strike group stops outside this base, 65 to 70 yd from the entrance it uses, on the lane it
// arrives by: the Alliance strike group rides lane H and arrives at the Horde base, the Horde group rides lane A and
// arrives at the Alliance base (the shortest routes make a loop, sheet section 2). Both gather points are vertices of
// the authored paths vPath_TP_AllianceFlagRoom_to_HordeFlagRoom and vPath_TP_HordeFlagRoom_to_AllianceFlagRoom, so they
// lie on the navmesh.
inline BgFlagMap::MapData const& Data()
{
    static BgFlagMap::MapData const data = {
        TP_MAP_ID,
        {
            {
                { 2118.210f, 191.621f, 44.052f },
                { 1875.200f, 429.100f, -13.300f },
                { 1877.150f, 439.111f, -4.000f },
                { { 2116.267f, 158.933f, 43.721f }, { 2151.200f, 220.000f, 43.721f }, { 2122.933f, 222.400f, 43.721f } },
                { { 2101.066f, 183.733f, 43.721f }, { 2112.000f, 173.067f, 43.721f }, { 2114.400f, 213.067f, 43.721f } },
                { true, true, true },
                { 2060.000f, 248.533f, 41.055f },
            },
            {
                { 1578.380f, 344.037f, 2.419f },
                { 1797.000f, 165.500f, -5.400f },
                { 1816.870f, 160.083f, 1.810f },
                { { 1560.800f, 316.800f, 1.855f }, { 1586.667f, 322.133f, 1.588f }, { 1569.067f, 368.267f, 1.777f } },
                { { 1596.533f, 334.933f, 1.423f }, { 1594.667f, 354.133f, 1.588f }, { 1576.000f, 362.667f, 1.855f } },
                { true, true, true },
                { 1632.000f, 271.467f, -5.079f },
            }
        },
        { 1840.000f, 320.000f, -24.945f },
        { TP_LIMIT_MS, TP_LEGACY_LIMIT_MS },
        0.0f,   // no roof zone: the Twin Peaks flag rooms have none, and 'roof' is not a place word of the sheet
    };
    return data;
}

// ---------------------------------------------------------------------------------------------------------------------
// The respawn model (BT-473, sheet gap G5 and owner answer 1a). In play this core sends every death to the team's middle
// graveyard (BattlegroundTP::GetClosestGraveYard, 301 to 345 yd from the own stand), the 4.3.4 core sends a death nearer the
// enemy flag room to the own start graveyard (132 to 149 yd), and which of the two a build 18414 server has is not proven
// (BT-460). The plan does not assume: it looks at where a bot stands the moment it is alive again and learns the rule
// from the last few respawns of the team, so the wipe states and the run-back times are right under either rule.
// ---------------------------------------------------------------------------------------------------------------------
enum class RespawnSide : std::uint8_t
{
    UNKNOWN,   // too far from both graveyards (a bot that moved on before it was read, or a resurrection spell)
    MIDDLE,    // the team's middle graveyard on the lane (WorldSafeLocs 1749 / 1750)
    START      // the team's start graveyard inside its own base area (1729 / 1728)
};

inline char const* RespawnSideName(RespawnSide side)
{
    switch (side)
    {
        case RespawnSide::MIDDLE: return "middle";
        case RespawnSide::START: return "start";
        case RespawnSide::UNKNOWN: break;
    }
    return "unknown";
}

// The two graveyards a team of a rule may respawn at. The middle one is the map data's mainGraveyard.
struct RespawnPoints
{
    Pos middle;
    Pos start;
};

// Index = TeamId. Start graveyards from WorldSafeLocs: 1729 (2168.10, 332.04, 34.70) Alliance, 1728 (1561.16, 212.76,
// 13.86) Horde; they carry spirit guides in the script (BattlegroundTP.cpp:742-761) but no rule of this core points at them.
inline RespawnPoints RespawnPointsOf(std::uint32_t teamIndex)
{
    BgFlagMap::MapData const& d = Data();
    if (teamIndex == 0)
        return { d.sides[0].mainGraveyard, { 2168.100f, 332.040f, 34.700f } };
    return { d.sides[1].mainGraveyard, { 1561.160f, 212.760f, 13.860f } };
}

// A bot read within this distance of a graveyard counts as respawned there. The graveyards of a team are over 300 yd
// apart, and a bot moves at most 14 yd/s mounted between the respawn and the plan's next read (2 s).
constexpr float RESPAWN_MATCH_YARDS = 60.0f;

inline RespawnSide ClassifyRespawn(Pos const& at, RespawnPoints const& points)
{
    auto const dist = [&at](Pos const& p)
    {
        float const dx = at.x - p.x;
        float const dy = at.y - p.y;
        return std::sqrt(dx * dx + dy * dy);
    };
    float const toMiddle = dist(points.middle);
    float const toStart = dist(points.start);
    if (toMiddle > RESPAWN_MATCH_YARDS && toStart > RESPAWN_MATCH_YARDS)
        return RespawnSide::UNKNOWN;
    return toMiddle <= toStart ? RespawnSide::MIDDLE : RespawnSide::START;
}

// The rule a team's respawns follow, learned from the last RESPAWN_MEMORY classified respawns: the side most of them
// were on. Until one is seen the core as it is today is assumed (the middle graveyard, owner answer 1a). A tie goes to the
// latest respawn.
constexpr std::size_t RESPAWN_MEMORY = 6;

struct RespawnRule
{
    RespawnSide last[RESPAWN_MEMORY] = { RespawnSide::UNKNOWN, RespawnSide::UNKNOWN, RespawnSide::UNKNOWN,
                                         RespawnSide::UNKNOWN, RespawnSide::UNKNOWN, RespawnSide::UNKNOWN };
    std::size_t next = 0;

    void Record(RespawnSide side)
    {
        if (side == RespawnSide::UNKNOWN)
            return;
        last[next] = side;
        next = (next + 1) % RESPAWN_MEMORY;
    }

    RespawnSide Latest() const
    {
        return last[(next + RESPAWN_MEMORY - 1) % RESPAWN_MEMORY];
    }

    // MIDDLE or START: never UNKNOWN.
    RespawnSide Current() const
    {
        std::size_t middle = 0;
        std::size_t start = 0;
        for (RespawnSide side : last)
        {
            if (side == RespawnSide::MIDDLE)
                ++middle;
            else if (side == RespawnSide::START)
                ++start;
        }
        if (middle == 0 && start == 0)
            return RespawnSide::MIDDLE;
        if (middle == start)
            return Latest();
        return middle > start ? RespawnSide::MIDDLE : RespawnSide::START;
    }
};

inline Pos RespawnPoint(RespawnPoints const& points, RespawnSide side)
{
    return side == RespawnSide::START ? points.start : points.middle;
}

// The way a respawned bot has back to its own flag room, in yards of straight line (the route is longer; the sheet's
// numbers are 345 and 301 from the middle graveyards, 149 and 132 from the start graveyards).
inline float RunBackYards(RespawnPoints const& points, RespawnSide side, Pos const& stand)
{
    Pos const from = RespawnPoint(points, side);
    float const dx = from.x - stand.x;
    float const dy = from.y - stand.y;
    return std::sqrt(dx * dx + dy * dy);
}

// Seconds on foot for a distance (yd), at the unmounted run speed 7 yd/s; mounted bots take half (to check live: mounts
// in a battleground).
constexpr float RUN_YARDS_PER_SECOND = 7.0f;

inline float RunBackSeconds(float yards, bool mounted)
{
    return yards / (mounted ? RUN_YARDS_PER_SECOND * 2.0f : RUN_YARDS_PER_SECOND);
}

// ---------------------------------------------------------------------------------------------------------------------
// Lanes and posts (BT-474, sheet gap G2, sections 2 and 4). The two valley lanes are named by the middle graveyard they
// hold: lane A (the Alliance graveyard, y about 380-450) and lane H (the Horde graveyard, y about 160-240). The only fixed
// objects on the lanes are the buffs, so the posts of MID are the buffs' places (sheet section 2, server data).
//
//  - CUT: on the enemy carrier's way home, near our base, inside its kill window. The Alliance's is the lane H berserk, the
//    Horde's the lane A berserk (BattlegroundTP.cpp:715-716; the world DB places the second one at 1737.6, 435.9, 16 yd off:
//    owner question 5, to check live). The Alliance's own outbound lane is lane H, so it also is the regroup post.
//  - COVER: on our own carrier's way home. The Alliance's is the lane A restoration, the Horde's the lane H restoration
//    (:713-714).
// ---------------------------------------------------------------------------------------------------------------------
struct LanePosts
{
    Pos cut;
    Pos cover;
};

// Index = TeamId.
inline LanePosts LanePostsOf(std::uint32_t teamIndex)
{
    if (teamIndex == 0)
        return { { 1934.369f, 226.064f, -17.044f }, { 1952.121f, 383.857f, -10.287f } };
    return { { 1725.240f, 446.431f, -7.833f }, { 1753.957f, 242.092f, -14.117f } };
}

enum class MidPost : std::uint8_t
{
    CUT,
    COVER
};

// Where one MID bot stands: the floater (the third healer, owner answer 4a) always at COVER; otherwise one bot at COVER when
// the team has two or more MID bots and none of them is a floater (the last one of the list), all the rest at CUT. With 3 MID
// bots that is 2 at CUT and 1 at COVER, with 2 one each, with 1 the CUT (sheet section 4).
inline MidPost MidPostOf(bool isFloater, std::uint32_t roleIndex, std::uint32_t midAlive, bool floaterAmongMid)
{
    if (isFloater)
        return MidPost::COVER;
    if (!floaterAmongMid && midAlive >= 2 && roleIndex + 1 == midAlive)
        return MidPost::COVER;
    return MidPost::CUT;
}

// ---------------------------------------------------------------------------------------------------------------------
// The kill window (sheet section 2). Along the enemy carrier's run home the first stretch lies more than 100 yd from its own
// respawn point, where a fight is between our hunters and the carrier's escort only; then the carrier passes its own
// respawn point, where the 30 s waves of its team join; once it is closer to its stand than that respawn point is, the hunt
// is over and the hunters fall back to the posts (state C). The phase is read from the straight line, which follows the
// route closely on both lanes (the two routes measured in the sheet).
// ---------------------------------------------------------------------------------------------------------------------
enum class EfcPhase : std::uint8_t
{
    WINDOW,         // far from the carrier's own respawn point and not yet past it: the hunt
    AT_GRAVEYARD,   // within RESPAWN_JOIN_YARDS of the respawn point: its own team joins every 30 s, the hunt goes on
    PAST            // closer to its stand than its respawn point is: the hunters stop
};

constexpr float RESPAWN_JOIN_YARDS = 100.0f;
// A carrier is past its respawn point only when it is this much nearer its stand than the respawn point is.
constexpr float PAST_MARGIN_YARDS = 15.0f;

inline float Dist2(Pos const& a, Pos const& b)
{
    float const dx = a.x - b.x;
    float const dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

inline EfcPhase EfcPhaseOf(Pos const& efc, Pos const& efcStand, Pos const& efcRespawn)
{
    if (Dist2(efc, efcStand) + PAST_MARGIN_YARDS < Dist2(efcRespawn, efcStand))
        return EfcPhase::PAST;
    if (Dist2(efc, efcRespawn) <= RESPAWN_JOIN_YARDS)
        return EfcPhase::AT_GRAVEYARD;
    return EfcPhase::WINDOW;
}

inline char const* EfcPhaseName(EfcPhase phase)
{
    switch (phase)
    {
        case EfcPhase::WINDOW: return "window";
        case EfcPhase::AT_GRAVEYARD: return "at-graveyard";
        case EfcPhase::PAST: return "past";
    }
    return "window";
}

// ---------------------------------------------------------------------------------------------------------------------
// States G and H (sheet section 6): a wiped attack. G is ours: 3 or more of the strike group and MID died within 15 s. H is
// theirs: 3 or more of their players died within 15 s near our CUT post or at the enemy stand. Both hold for 45 s.
// ---------------------------------------------------------------------------------------------------------------------
constexpr std::uint32_t WIPE_WINDOW_MS = 15000;
constexpr std::uint32_t WIPE_MIN_DEATHS = 3;
constexpr std::uint32_t WIPE_HOLD_MS = 45000;
// A death counts for state H when it was this close to our CUT post or to the enemy stand (the place of the last read while
// alive: the corpse is where the carrier of the death fell).
constexpr float WIPE_ZONE_YARDS = 60.0f;
// State G: the base counts as threatened when the enemy carries our flag or this many enemies stand this close to our stand.
constexpr float BASE_THREAT_RADIUS = 80.0f;
constexpr std::uint32_t BASE_THREAT_MIN_ENEMIES = 2;

struct DeathMark
{
    std::uint32_t diedMs = 0;   // 0 for none
    Pos at = { 0.0f, 0.0f, 0.0f };
};

// How many of the deaths fall into the last WIPE_WINDOW_MS (and, when `zones` is not empty, within WIPE_ZONE_YARDS of one of
// the zone centres).
inline std::uint32_t DeathsInWindow(std::vector<DeathMark> const& deaths, std::uint32_t nowMs, std::vector<Pos> const& zones)
{
    std::uint32_t count = 0;
    for (DeathMark const& d : deaths)
    {
        if (!d.diedMs || nowMs - d.diedMs > WIPE_WINDOW_MS)
            continue;
        bool inZone = zones.empty();
        for (Pos const& z : zones)
            if (Dist2(d.at, z) <= WIPE_ZONE_YARDS)
                inZone = true;
        if (inZone)
            ++count;
    }
    return count;
}

inline bool AttackWiped(std::uint32_t deathsInWindow)
{
    return deathsInWindow >= WIPE_MIN_DEATHS;
}

inline bool BaseThreatened(bool enemyCarriesOurFlag, std::uint32_t enemiesNearStand)
{
    return enemyCarriesOurFlag || enemiesNearStand >= BASE_THREAT_MIN_ENEMIES;
}

// ---------------------------------------------------------------------------------------------------------------------
// Decision points as candidate lists (sheet section 16, design decision 0014 and 0026): each returns the options with an integer score,
// highest first, ties by the lower id; the FIRST is the rule's answer. A later decision layer may pick another id from the
// same list; the plan uses the first until then.
// ---------------------------------------------------------------------------------------------------------------------
template <class Id>
struct Scored
{
    Id id;
    int score;
};

template <class Id>
std::vector<Scored<Id>> Ranked(std::vector<Scored<Id>> list)
{
    std::stable_sort(list.begin(), list.end(), [](Scored<Id> const& a, Scored<Id> const& b)
    {
        if (a.score != b.score)
            return a.score > b.score;
        return static_cast<int>(a.id) < static_cast<int>(b.id);
    });
    return list;
}

// D7, the wipe response after state G (what the wiped bots do when they are back).
enum class WipeReply : std::uint8_t
{
    DEFEND_FIRST,    // run home, hold the entrance posts until relieved
    PUSH_NOW,        // their offense is dead as well: the pickup cannot be stopped, go on
    REGROUP_LANE     // gather at the own lane post and go again as one group, never one by one
};

struct WipeFacts
{
    bool baseThreatened = false;     // BaseThreatened()
    bool enemyAttackAlsoDead = false;    // state H holds as well
};

inline std::vector<Scored<WipeReply>> WipeReplies(WipeFacts const& f)
{
    std::vector<Scored<WipeReply>> list;
    list.push_back({ WipeReply::DEFEND_FIRST, f.baseThreatened ? 100 : 40 });
    list.push_back({ WipeReply::PUSH_NOW, f.enemyAttackAlsoDead ? 90 : 30 });
    list.push_back({ WipeReply::REGROUP_LANE, 70 });
    return Ranked(list);
}

// D6, where the floater goes.
enum class FloaterJob : std::uint8_t
{
    TO_STRIKE,
    TO_HOME,
    STAY_COVER
};

struct FloaterFacts
{
    bool theirAttackWiped = false;   // state H
    bool ourAttackWiped = false;     // state G
    std::uint32_t homeAlive = 3;
};

inline std::vector<Scored<FloaterJob>> FloaterJobs(FloaterFacts const& f)
{
    std::vector<Scored<FloaterJob>> list;
    list.push_back({ FloaterJob::TO_STRIKE, f.theirAttackWiped ? 90 : 20 });
    list.push_back({ FloaterJob::TO_HOME, (f.ourAttackWiped || f.homeAlive < 2) ? 95 : 10 });
    list.push_back({ FloaterJob::STAY_COVER, 60 });
    return Ranked(list);
}

// D4, which lane our carrier runs home by. The own lane is the shortest route and the one our respawn waves appear on; the
// other lane only for an expert carrier with 3 or more enemies on the first 100 yd of the own lane (sheet section 6).
enum class CarrierLane : std::uint8_t
{
    OWN_LANE,
    OTHER_LANE
};

constexpr std::uint32_t LANE_BLOCK_ENEMIES = 3;

inline std::vector<Scored<CarrierLane>> CarrierLanes(bool expertCarrier, std::uint32_t enemiesOnOwnLane)
{
    std::vector<Scored<CarrierLane>> list;
    list.push_back({ CarrierLane::OWN_LANE, 100 });
    list.push_back({ CarrierLane::OTHER_LANE, expertCarrier && enemiesOnOwnLane >= LANE_BLOCK_ENEMIES ? 110 : 20 });
    return Ranked(list);
}

// The floater: the MID healer that does not follow the carrier, the lowest guid hash first (stable like the tiers), 0 when the
// team has none. Without a healer the nearest free MID bot takes the job only in state H (the plan does that).
struct FloaterCandidate
{
    std::uint32_t guidLow = 0;
    bool midHealer = false;          // a MID bot that heals
    bool followsCarrier = false;     // one of the two healers on our carrier
};

inline std::uint32_t PickFloater(std::vector<FloaterCandidate> const& candidates)
{
    std::uint32_t best = 0;
    for (FloaterCandidate const& c : candidates)
    {
        if (!c.midHealer || c.followsCarrier)
            continue;
        if (!best || BotSkillMath::Mix(c.guidLow) < BotSkillMath::Mix(best) ||
            (BotSkillMath::Mix(c.guidLow) == BotSkillMath::Mix(best) && c.guidLow < best))
            best = c.guidLow;
    }
    return best;
}

// ---------------------------------------------------------------------------------------------------------------------
// One team's wipe and floater jobs, from a roster (the plan fills TeamFacts from its record and applies TeamJobs back). The
// whole decision is here so that tests/ can run it: who floats, who is back from a wipe and what each does (states G and H).
// ---------------------------------------------------------------------------------------------------------------------
struct MemberFacts
{
    std::uint32_t guid = 0;
    bool bot = false;
    bool alive = false;
    WsgLogic::Role role = WsgLogic::Role::NONE;
    std::uint32_t roleIndex = 0;       // among the alive bots of the same role, by guid
    bool healer = false;
    bool healerFollow = false;         // one of the two healers on our carrier
    DeathMark death;                   // the last death the roster saw
};

struct TeamFacts
{
    std::vector<MemberFacts> members;          // our team
    std::vector<DeathMark> enemyDeaths;        // the last death of every enemy player
    std::uint32_t nowMs = 0;
    bool inProgress = false;                   // false in the preparation: only the floater is read
    bool enemyCarriesOurFlag = false;
    std::uint32_t enemiesNearBase = 0;         // enemies within BASE_THREAT_RADIUS of our stand
    Pos ourCutPost;
    Pos enemyStand;
    std::uint32_t wipeGUntilMs = 0;            // the states as the last read left them
    std::uint32_t wipeHUntilMs = 0;
    // BT-502: the answers of a decision layer for the wipe reply (D7) and the floater job (D6), already judged and still
    // standing. They replace the rule's first candidate; an id the list does not hold is ignored.
    std::optional<WipeReply> wipeOverride;
    std::optional<FloaterJob> floaterOverride;
};

struct MemberJobs
{
    std::uint32_t guid = 0;
    bool floater = false;
    bool wipeDefend = false;     // state G: run home, hold an entrance post
    bool wipeRegroup = false;    // state G: gather at the own lane post
    bool wipeJoin = false;       // state H: join the strike group
};

struct TeamJobs
{
    std::vector<MemberJobs> members;
    std::uint32_t wipeGUntilMs = 0;   // 0 when the state is over
    std::uint32_t wipeHUntilMs = 0;
    std::uint32_t midAlive = 0;
    bool floaterAmongMid = false;
    // BT-502: the candidate lists D7 and D6 as this read built them (empty outside the match), and the facts they were built
    // from, so that the plan can publish them to a decision layer.
    bool listsValid = false;
    std::vector<Scored<WipeReply>> wipeList;
    std::vector<Scored<FloaterJob>> floaterList;
    bool wipeG = false;
    bool wipeH = false;
    bool baseThreatened = false;
    std::uint32_t homeAlive = 0;
};

// The first candidate of a list, or the layer's answer when the list holds it.
template <class Id>
Id Chosen(std::vector<Scored<Id>> const& list, std::optional<Id> const& answer)
{
    if (answer)
        for (Scored<Id> const& s : list)
            if (s.id == *answer)
                return *answer;
    return list.front().id;
}

inline TeamJobs EvaluateTeam(TeamFacts const& f)
{
    using WsgLogic::Role;
    TeamJobs out;
    out.wipeGUntilMs = f.wipeGUntilMs;
    out.wipeHUntilMs = f.wipeHUntilMs;

    // The floater (the third healer) and the MID headcount.
    std::vector<FloaterCandidate> floaters;
    for (MemberFacts const& m : f.members)
    {
        if (!m.bot || !m.alive || m.role != Role::MID)
            continue;
        ++out.midAlive;
        floaters.push_back({ m.guid, m.healer, m.healerFollow });
    }
    std::uint32_t const floaterGuid = PickFloater(floaters);
    out.floaterAmongMid = floaterGuid != 0;

    out.members.reserve(f.members.size());
    for (MemberFacts const& m : f.members)
    {
        MemberJobs job;
        job.guid = m.guid;
        job.floater = floaterGuid && m.guid == floaterGuid;
        out.members.push_back(job);
    }
    if (!f.inProgress)
        return out;

    // State G: 3 or more of our strike group and MID died within 15 s. State H: 3 or more of theirs died near our cut post or
    // at their stand. Each holds for WIPE_HOLD_MS after the last count.
    std::vector<DeathMark> ours;
    for (MemberFacts const& m : f.members)
        if (m.bot && (WsgLogic::IsStrikeRole(m.role) || m.role == Role::MID))
            ours.push_back(m.death);
    if (AttackWiped(DeathsInWindow(ours, f.nowMs, {})))
        out.wipeGUntilMs = f.nowMs + WIPE_HOLD_MS;
    if (AttackWiped(DeathsInWindow(f.enemyDeaths, f.nowMs, { f.ourCutPost, f.enemyStand })))
        out.wipeHUntilMs = f.nowMs + WIPE_HOLD_MS;
    bool const wipeG = out.wipeGUntilMs && static_cast<std::int32_t>(out.wipeGUntilMs - f.nowMs) > 0;
    bool const wipeH = out.wipeHUntilMs && static_cast<std::int32_t>(out.wipeHUntilMs - f.nowMs) > 0;
    if (!wipeG)
        out.wipeGUntilMs = 0;
    if (!wipeH)
        out.wipeHUntilMs = 0;

    bool const threatened = BaseThreatened(f.enemyCarriesOurFlag, f.enemiesNearBase);
    out.wipeList = WipeReplies({ threatened, wipeH });
    WipeReply const reply = Chosen(out.wipeList, f.wipeOverride);
    out.wipeG = wipeG;
    out.wipeH = wipeH;
    out.baseThreatened = threatened;

    // The spare home guard: the last of the list. Without a healer to float, the last MID bot that is not on the carrier takes
    // the floater's part in state H.
    std::uint32_t homeAlive = 0;
    std::uint32_t spareHomeGuid = 0;
    std::uint32_t spareHomeIndex = 0;
    std::uint32_t standInGuid = 0;
    std::uint32_t standInIndex = 0;
    for (MemberFacts const& m : f.members)
    {
        if (!m.bot || !m.alive)
            continue;
        if (m.role == Role::HOME)
        {
            ++homeAlive;
            if (!spareHomeGuid || m.roleIndex >= spareHomeIndex)
            {
                spareHomeGuid = m.guid;
                spareHomeIndex = m.roleIndex;
            }
        }
        else if (m.role == Role::MID && !floaterGuid && !m.healerFollow && (!standInGuid || m.roleIndex >= standInIndex))
        {
            standInGuid = m.guid;
            standInIndex = m.roleIndex;
        }
    }

    out.homeAlive = homeAlive;
    out.floaterList = FloaterJobs({ wipeH, wipeG, homeAlive });
    out.listsValid = true;
    FloaterJob const floaterJob = Chosen(out.floaterList, f.floaterOverride);
    for (std::size_t i = 0; i < f.members.size(); ++i)
    {
        MemberFacts const& m = f.members[i];
        MemberJobs& job = out.members[i];
        if (!m.bot || !m.alive)
            continue;

        if (job.floater || (standInGuid && m.guid == standInGuid))
        {
            // State G sends the floater to the base, state H to the strike group (candidate list D6 of the sheet).
            job.wipeDefend = floaterJob == FloaterJob::TO_HOME && (wipeG || homeAlive < 2);
            job.wipeJoin = floaterJob == FloaterJob::TO_STRIKE && wipeH;
            continue;
        }
        if (m.role == Role::HOME)
        {
            job.wipeJoin = wipeH && !wipeG && homeAlive >= 3 && m.guid == spareHomeGuid;
            continue;
        }
        // The wiped bots that are back: they died inside the hold time of state G.
        bool const wiped = wipeG && (WsgLogic::IsStrikeRole(m.role) || m.role == Role::MID) && m.death.diedMs &&
                           f.nowMs - m.death.diedMs <= WIPE_HOLD_MS;
        if (!wiped)
            continue;
        job.wipeDefend = reply == WipeReply::DEFEND_FIRST;
        job.wipeRegroup = reply == WipeReply::REGROUP_LANE;
    }
    return out;
}

// ---------------------------------------------------------------------------------------------------------------------
// Place words (sheet section 8): the zones a call-out names. A named spot (flag room, graveyard, buff, door) is named before
// the wide lane and middle zones (WsgLogic::ClassifyPlace, priority), and the nearest spot by its radius wins among them.
// ---------------------------------------------------------------------------------------------------------------------
constexpr float PLACE_FLAG_ROOM_YARDS = 45.0f;
constexpr float PLACE_GRAVEYARD_YARDS = 40.0f;
constexpr float PLACE_LANE_YARDS = 120.0f;
constexpr float PLACE_MID_YARDS = 70.0f;
constexpr float PLACE_BUFF_YARDS = 30.0f;
constexpr float PLACE_DOOR_YARDS = 30.0f;

inline std::vector<WsgLogic::PlaceAnchor> PlaceAnchors()
{
    using WsgLogic::Place;
    BgFlagMap::MapData const& d = Data();
    std::vector<WsgLogic::PlaceAnchor> list;
    // The named spots (priority 1) come before the wide lane and middle zones (priority 0): the graveyard is inside its lane.
    for (BgFlagMap::Side const& side : d.sides)
        list.push_back({ Place::FLAG_ROOM, side.standWalk.x, side.standWalk.y, PLACE_FLAG_ROOM_YARDS, 1 });
    for (BgFlagMap::Side const& side : d.sides)
        list.push_back({ Place::GRAVEYARD, side.mainGraveyard.x, side.mainGraveyard.y, PLACE_GRAVEYARD_YARDS, 1 });
    // Lane A is the Alliance graveyard's lane, lane H the Horde's.
    list.push_back({ Place::LANE_A, d.sides[0].mainGraveyard.x, d.sides[0].mainGraveyard.y, PLACE_LANE_YARDS });
    list.push_back({ Place::LANE_H, d.sides[1].mainGraveyard.x, d.sides[1].mainGraveyard.y, PLACE_LANE_YARDS });
    list.push_back({ Place::MID, d.mid.x, d.mid.y, PLACE_MID_YARDS });
    // Buffs: the berserk of lane H is the Alliance's CUT post, of lane A the Horde's; the restoration of lane A is the
    // Alliance's COVER post, of lane H the Horde's.
    LanePosts const alliance = LanePostsOf(0);
    LanePosts const horde = LanePostsOf(1);
    list.push_back({ Place::BERS_H, alliance.cut.x, alliance.cut.y, PLACE_BUFF_YARDS, 1 });
    list.push_back({ Place::BERS_A, horde.cut.x, horde.cut.y, PLACE_BUFF_YARDS, 1 });
    list.push_back({ Place::LEAF_A, alliance.cover.x, alliance.cover.y, PLACE_BUFF_YARDS, 1 });
    list.push_back({ Place::LEAF_H, horde.cover.x, horde.cover.y, PLACE_BUFF_YARDS, 1 });
    for (BgFlagMap::Side const& side : d.sides)
        for (Pos const& post : side.post)
            list.push_back({ Place::DOOR, post.x, post.y, PLACE_DOOR_YARDS, 1 });
    return list;
}
}  // namespace TpLogic

#endif
