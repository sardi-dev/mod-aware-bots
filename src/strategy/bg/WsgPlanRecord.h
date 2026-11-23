/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_WSGPLANRECORD_H
#define _PLAYERBOT_WSGPLANRECORD_H

// BT-474: the per-match record of the two-flag plan (WsgPlan.cpp), in a header so that the Twin Peaks part of the plan
// (TpPlan.cpp) reads and writes the same members and teams. Nothing outside WsgPlan.cpp and TpPlan.cpp includes it.

#include "BgFlagMap.h"
#include "BgLeaderPolicy.h"
#include "BgPlanCommon.h"
#include "Define.h"
#include "PvpObservationMath.h"
#include "TpMidLockPolicy.h"
#include "TpPlanLogic.h"
#include "WsgPlanLogic.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace WsgPlanRec
{
using namespace WsgLogic;
using BgFlagMap::MapData;
using BgFlagMap::Pos;
using BgFlagMap::Side;

struct Member
{
    uint32 guid = 0;
    bool bot = false;
    bool alive = false;
    bool present = false;
    uint8 cls = 0;
    uint8 level = 0;
    bool healer = false;
    bool tank = false;
    bool melee = false;
    bool feral = false;
    Tier tier = Tier::GOOD;
    Role role = Role::NONE;
    uint32 roleIndex = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float hpPercent = 100.0f;
    uint32 debuff = 0;             // WsgLogic::DebuffLevel of a flag carrier, 0 for everyone else
    bool seen = false;             // read at least once: only then is a change of alive a death or a respawn
    uint32 diedMs = 0;             // when the roster saw this member die (0 while alive)
    uint32 respawnedMs = 0;        // when the roster saw it alive again
    TpLogic::RespawnSide respawnSide = TpLogic::RespawnSide::UNKNOWN;   // where it stood then (Twin Peaks)
    Pos aliveAt = { 0.0f, 0.0f, 0.0f };   // the last position read while alive: where a death happened
    TpLogic::DeathMark death;              // the last death the roster saw (Twin Peaks wipe states)
    bool floater = false;                  // the third healer (Twin Peaks, owner answer 4a)
    bool wipeDefend = false;               // Twin Peaks state G: back from a wipe, the base is threatened
    bool wipeRegroup = false;              // Twin Peaks state G: back from a wipe, gather at the own lane post
    bool wipeJoin = false;                 // Twin Peaks state H: join the strike group
    bool inCombat = false;                 // read with the roster (the middle lock counts the fighters)
    bool midJoin = false;                  // Twin Peaks, BT-694: an idle home guard called into the locked middle fight
    bool viaLane = false;                  // Twin Peaks, BT-695: walks to the other lane's post first (the plan's route choice)
    bool midEscort = false;                // Twin Peaks, BT-695: a joined guard escorting the taker after a kill release
    uint32 viaSerialDone = 0;              // the avoidance (TeamPlan::avoidSerial) whose lane post this bot already stood at
    uint32 seenEpoch = 0;
    Kind kind = Kind::HOLD_MID;
    bool hunter = false;
    bool ownGroundResponder = false;
    bool theirGroundResponder = false;
    bool healerFollow = false;     // one of the healers that follow our flag carrier (owner rule 2026-11-17)
    bool answerCall = false;
    bool dynamic = false;          // the current objective follows a moving target
    DwellTracker dwell;
    // The info-level decision lines (owner 2026-11-17): channel 0 the objective, channel 1 the carrier's choice (WsgCarrier).
    DecisionLogGate logGate[2];
    uint32 lastLogMs = 0;
    Kind lastLogKind = Kind::HOLD_MID;
    bool mustLeaveGy = false;
    uint32 gyResetMs = 0;
    uint32 lastDynamicMs = 0;
    uint32 keepRole = 0;           // BT-481: refreshes a casual bot still keeps its old role after a stance change
};

struct TeamPlan
{
    Plan plan = Plan::BALANCED;
    Plan rulePlan = Plan::BALANCED;    // the drawn plan: `plan` is this one unless a decision layer's stance replaced it (BT-481)
    Situation situation = Situation::NORMAL;
    uint32 leader = 0;                 // BT-481: the leader bot of this side (guid low), 0 for none
    BgLeader::Stance stance = BgLeader::Stance::BALANCED;
    bool stanceCallDue = false;        // the leader owes the team its stance
    RoleCounts counts;
    std::unordered_map<uint32, Member> members;
    uint8 ourFlag = FLAG_ON_BASE;      // state of our flag
    uint8 theirFlag = FLAG_ON_BASE;    // state of their flag
    uint64 efcFull = 0;                // the player carrying our flag
    uint64 fcFull = 0;                 // the player of ours carrying their flag
    bool efcSeen = false;
    Pos efcPos = { 0.0f, 0.0f, 0.0f };
    bool strikeGathered = false;
    uint32 gatherSinceMs = 0;
    uint32 enemiesNearStand = 0;
    uint32 fcDebuff = 0;               // debuff level of our carrier of their flag, read live from the auras (BT-472)
    uint32 efcDebuff = 0;              // debuff level of their carrier of our flag
    bool efcPressed = false;           // EFC_PRESSURE_MIN of ours within EFC_PRESSURE_RADIUS of it
    bool stalemateHunt = false;        // the stalemate rules (WsgLogic::StalemateHunts / StalemateRuns) as they stand
    bool stalemateRun = false;
    uint32 lastIncomingMs = 0;
    uint32 lastScoutMs[16] = {};       // BT-483: when a zone (WsgLogic::Place number) was last called by a scout
    TpLogic::EfcPhase prevEfcPhase = TpLogic::EfcPhase::WINDOW;
    uint32 score = 0;
    bool fcLowCalled = false;
    Place lastEfcPlace = Place::NONE;
    bool timeCalled = false;
    CallLimiter limiter;
    TpLogic::RespawnRule respawnRule;   // where this team's dead come back (Twin Peaks), learned from the roster
    uint32 wipeGUntilMs = 0;            // state G (our attack wiped) holds until this time, 0 for no
    uint32 wipeHUntilMs = 0;            // state H (their attack wiped)
    TpLogic::EfcPhase efcPhase = TpLogic::EfcPhase::WINDOW;   // where the enemy carrier is on its run home
    uint32 enemiesNearBase = 0;         // enemies within BASE_THREAT_RADIUS of our stand
    uint32 reportedBase = 0;            // BT-482: enemies at our base that call-outs report (weighted), seen or not
    uint32 midAlive = 0;                // alive MID bots, and whether the floater is one of them
    TpMidLock::State midLock;           // BT-694: the middle fight deadlock clock of this side
    TpMidLock::EscortState escort;      // BT-695: the guard that walks with the taker after a kill release
    uint32 avoidSerial = 0;             // BT-695: counts the lane avoidances of this side (a member's detour is done per serial)
    Pos midJoinPos = { 0.0f, 0.0f, 0.0f };   // where the called guards go: the middle of our fighters, out of the enemy graveyard
    bool midPushCallDue = false;        // the leader owes the team the call that the guards are called
    bool floaterAmongMid = false;
    std::vector<PvpObs::Observation> inbox;   // BT-482: the human calls read since the last refresh
    bool callActive = false;
    Pos callPos = { 0.0f, 0.0f, 0.0f };
    uint32 callExpiresMs = 0;
    bool lowBracket = false;
    uint32 size = 0;
    uint32 humans = 0;
    bool hasHealer = false;
    uint32 previousFc = 0;             // low guid of the last carrier, for the capture call
    uint8 prevOur = FLAG_ON_BASE;      // last refresh, for the events
    uint8 prevTheir = FLAG_ON_BASE;
    uint32 prevScore = 0;
};

struct Pending
{
    uint32 guid = 0;
    Call kind = Call::EFC_TAKEN;
    uint32 dueMs = 0;
    uint32 seq = 0;
    std::string place;
    std::string cls;
    std::string score;
    std::string number;
    std::string minutes;
    std::string stance;
    // BT-483: what the line tells the team, kept as an observation of the side when it is said (a line the team can read is
    // the way the team learns it).
    bool observe = false;
    PvpObs::Kind obsKind = PvpObs::Kind::INCOMING;
    uint8 obsPlace = PvpObs::PLACE_UNKNOWN;
    uint8 obsCount = 0;
};

struct Record : BgPlan::RecordBase
{
    // The map data of the match, set by the first refresh (every caller has checked the map before).
    MapData const* map = nullptr;
    // A human takes part in the match: only then the decision lines go out at info level (a match of bots alone stays quiet).
    bool humanPresent = false;
    // BT-481: the epoch a stance change of a leader's side made; a casual bot adopts that one a refresh later.
    uint32 stanceEpoch = 0;
    uint32 bothHeldStartMs = 0;
    uint32 bothHeldMs = 0;
    std::vector<Pending> pending;
    TeamPlan team[2];
};
}  // namespace WsgPlanRec

#endif
