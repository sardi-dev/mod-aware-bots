/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE). See TpPlan.h for what this is.
 */

#include "TpPlan.h"

#include "AiPvpDecisionPolicy.h"
#include "BgLeaderAdapters.h"
#include "Log.h"
#include "PvpMatchState.h"
#include "PvpObservationMath.h"
#include "TpMidLockPolicy.h"
#include "TpPlanLogic.h"
#include "WsgPlanRecord.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace WsgPlanRec;

namespace
{
float Dist2d(float ax, float ay, float bx, float by)
{
    float const dx = ax - bx;
    float const dy = ay - by;
    return std::sqrt(dx * dx + dy * dy);
}
}  // namespace

namespace TpPlan
{
namespace
{
// The wipe reply (D7) and the floater job (D6) of one side, as the decision layer's candidate lists. The wipe reply exists only
// while our attack is wiped (state G): outside it there is nothing to reply to. The floater list is published whenever the
// match runs, but only asks when the rule's own list has a real alternative (home short of guards, a wipe).
void PublishLists(PvpMatch::State& match, uint32 side, TpLogic::TeamJobs const& jobs, uint32 nowMs)
{
    AiPvp::Book& book = match.Side(side).ai.book;
    if (!jobs.listsValid)
    {
        AiPvp::Withdraw(book, BgLeader::DecisionPoint::D7_WIPE_REPLY);
        AiPvp::Withdraw(book, BgLeader::DecisionPoint::D6_FLOATER);
        return;
    }

    std::vector<std::string> facts;
    facts.push_back(std::string("our_attack_wiped:") + (jobs.wipeG ? "yes" : "no"));
    facts.push_back(std::string("their_attack_wiped:") + (jobs.wipeH ? "yes" : "no"));
    facts.push_back(std::string("base_threatened:") + (jobs.baseThreatened ? "yes" : "no"));
    facts.push_back("home_guards_alive:" + std::to_string(jobs.homeAlive));

    if (jobs.wipeG)
        AiPvp::Publish(book, BgLeader::OfferFrom(BgLeader::DecisionPoint::D7_WIPE_REPLY, 0, jobs.wipeList), facts, true, nowMs);
    else
        AiPvp::Withdraw(book, BgLeader::DecisionPoint::D7_WIPE_REPLY);
    AiPvp::Publish(book, BgLeader::OfferFrom(BgLeader::DecisionPoint::D6_FLOATER, 0, jobs.floaterList), facts, true, nowMs);
}

// BT-694: the middle deadlock of one side. Reads the roster the refresh already holds (no scan of its own), advances the clock
// and marks the idle home guards that join. A base that anything threatens keeps its guards (the plan's own base threat test).
void MidLockTurn(Record const& r, MapData const& map, TeamPlan& team, TeamPlan const& enemy, uint32 t, bool inProgress, uint32 nowMs,
                 bool enemyCarriesOurFlag, bool baseThreatened)
{
    Side const& foe = map.sides[1 - t];

    TpMidLock::Facts facts;
    facts.nowMs = nowMs;
    facts.inProgress = inProgress;
    facts.flagOut = team.ourFlag != FLAG_ON_BASE || team.theirFlag != FLAG_ON_BASE;

    // Fighters of both teams in the zone, the newest death of either team, and whether a taker of ours stands at their base.
    float sumX = 0.0f;
    float sumY = 0.0f;
    float sumZ = 0.0f;
    for (auto const& entry : team.members)
    {
        Member const& m = entry.second;
        facts.latestDeathMs = std::max(facts.latestDeathMs, m.death.diedMs);
        if (!m.alive)
            continue;
        if (m.inCombat && TpMidLock::InMidZone({ m.x, m.y, m.z }, map.mid))
        {
            ++facts.ownFighters;
            sumX += m.x;
            sumY += m.y;
            sumZ += m.z;
        }
        if (m.bot && WsgLogic::IsStrikeRole(m.role) && Dist2d(m.x, m.y, foe.standWalk.x, foe.standWalk.y) <= TpMidLock::THROUGH_YARDS)
            facts.takersThrough = true;
    }
    for (auto const& entry : enemy.members)
    {
        Member const& m = entry.second;
        facts.latestDeathMs = std::max(facts.latestDeathMs, m.death.diedMs);
        if (m.alive && m.inCombat && TpMidLock::InMidZone({ m.x, m.y, m.z }, map.mid))
            ++facts.enemyFighters;
    }
    // An enemy at our base (seen or reported) means their takers got through: the flag fight has begun.
    facts.takersThrough = facts.takersThrough || baseThreatened;
    // BT-695: the lane our fighters stand on (the nearer middle graveyard of their middle point).
    if (facts.ownFighters)
    {
        float const n = static_cast<float>(facts.ownFighters);
        facts.fightLane = TpMidLock::LaneOf({ sumX / n, sumY / n, sumZ / n }, map.sides[0].mainGraveyard, map.sides[1].mainGraveyard);
    }

    TpMidLock::Verdict const verdict = TpMidLock::Update(team.midLock, facts);

    if (facts.ownFighters)
    {
        float const n = static_cast<float>(facts.ownFighters);
        team.midJoinPos = TpMidLock::KeepOut({ sumX / n, sumY / n, sumZ / n }, EnemyRespawnPoint(r, map, t),
                                             TpMidLock::GY_KEEP_OUT_YARDS);
    }
    else if (team.midJoinPos.x == 0.0f && team.midJoinPos.y == 0.0f)
        team.midJoinPos = map.mid;

    std::vector<TpMidLock::HomeBot> home;
    for (auto const& entry : team.members)
    {
        Member const& m = entry.second;
        if (m.bot && m.role == WsgLogic::Role::HOME && !m.midEscort)
            home.push_back({ m.guid, m.roleIndex, m.alive, BotSkillMath::Uses(m.tier, WsgLogic::Tier::GOOD), m.inCombat, m.midJoin });
    }
    std::vector<uint32> const joiners = TpMidLock::Joiners(home, verdict.join, enemyCarriesOurFlag, baseThreatened);

    bool any = false;
    for (auto& entry : team.members)
    {
        Member& m = entry.second;
        m.midJoin = std::find(joiners.begin(), joiners.end(), m.guid) != joiners.end();
        any = any || m.midJoin;
    }

    char const* const side = t == uint32(TEAM_ALLIANCE) ? "A" : "H";
    bool const alliance = t == uint32(TEAM_ALLIANCE);

    // BT-695: the lane the strike groups stay off after a kill in a locked fight, and who detours (a route choice per striker).
    if (verdict.avoidStarted)
        ++team.avoidSerial;
    if (verdict.avoidEnded)
        SF_LOG_INFO("playerbots", "[BGTP] inst=%u %s midlock lane over (%s): the takers take their own route again", r.instanceId, side,
                    verdict.avoidEndedWhy == TpMidLock::Release::NONE ? "window" : TpMidLock::ReleaseName(verdict.avoidEndedWhy));

    Pos via = { 0.0f, 0.0f, 0.0f };
    if (verdict.avoidLane)
        via = TpMidLock::ViaPost(TpMidLock::OtherLane(verdict.avoidLane), alliance, TpLogic::LanePostsOf(0), TpLogic::LanePostsOf(1));
    uint32 detouring = 0;
    for (auto& entry : team.members)
    {
        Member& m = entry.second;
        m.viaLane = false;
        if (!verdict.avoidLane || !m.bot || !m.alive || !WsgLogic::IsStrikeRole(m.role))
            continue;
        if (TpMidLock::ViaReached({ m.x, m.y, m.z }, via))
            m.viaSerialDone = team.avoidSerial;
        auto const choices = TpMidLock::RouteChoices(verdict.avoidLane, BotSkillMath::Uses(m.tier, WsgLogic::Tier::GOOD),
                                                     m.viaSerialDone == team.avoidSerial);
        m.viaLane = choices.front().id == TpMidLock::RouteChoice::OTHER_LANE;
        detouring += m.viaLane ? 1 : 0;
    }

    // BT-695: one guard of the joined ones walks with the taker after a kill release (never with a flag out or a threatened base).
    std::vector<TpMidLock::TakerBot> takers;
    for (auto const& entry : team.members)
    {
        Member const& m = entry.second;
        if (m.bot && WsgLogic::IsStrikeRole(m.role))
            takers.push_back({ m.guid, m.role == WsgLogic::Role::TAKER ? 0u : m.role == WsgLogic::Role::BACKUP ? 1u : 2u, m.alive });
    }
    TpMidLock::EscortPick pick;
    if (verdict.released == TpMidLock::Release::KILL)
        pick = TpMidLock::PickEscort(home, takers);
    TpMidLock::EscortFacts escortFacts;
    escortFacts.nowMs = nowMs;
    escortFacts.inProgress = inProgress;
    escortFacts.flagOut = facts.flagOut || enemyCarriesOurFlag;
    escortFacts.baseThreatened = baseThreatened;
    escortFacts.takersThrough = facts.takersThrough;
    for (auto const& entry : team.members)
    {
        if (entry.first == team.escort.taker)
            escortFacts.takerAlive = entry.second.alive;
        if (entry.first == team.escort.guard)
            escortFacts.guardAlive = entry.second.alive;
    }
    TpMidLock::EscortVerdict const escort = TpMidLock::EscortStep(team.escort, escortFacts, pick);
    for (auto& entry : team.members)
        entry.second.midEscort = escort.active && entry.first == team.escort.guard;
    if (escort.started)
        SF_LOG_INFO("playerbots", "[BGTP] inst=%u %s midlock escort: guard %u walks with taker %u instead of going home", r.instanceId, side,
                    team.escort.guard, team.escort.taker);
    if (escort.ended != TpMidLock::EscortEnd::NONE)
        SF_LOG_INFO("playerbots", "[BGTP] inst=%u %s midlock escort over (%s): the guard goes back to the plan", r.instanceId, side,
                    TpMidLock::EscortEndName(escort.ended));
    if (verdict.avoidStarted)
        SF_LOG_INFO("playerbots", "[BGTP] inst=%u %s midlock lane: a kill in the locked fight on lane %s, %u takers go round by lane %s while it holds",
                    r.instanceId, side, TpMidLock::LaneName(verdict.avoidLane), detouring, TpMidLock::LaneName(TpMidLock::OtherLane(verdict.avoidLane)));

    if (verdict.joinStarted && any)
    {
        team.midPushCallDue = true;
        SF_LOG_INFO("playerbots", "[BGTP] inst=%u %s midlock join after %u s: %u guards leave their posts (fighters %u v %u, at %.0f,%.0f)",
                    r.instanceId, side, verdict.lockedForMs / 1000, uint32(joiners.size()), facts.ownFighters, facts.enemyFighters,
                    team.midJoinPos.x, team.midJoinPos.y);
    }
    else if (verdict.released != TpMidLock::Release::NONE)
        SF_LOG_INFO("playerbots", "[BGTP] inst=%u %s midlock released (%s): the guards go back to the plan", r.instanceId, side,
                    TpMidLock::ReleaseName(verdict.released));
}
}  // namespace

void NoteLifeChange(Record const& r, MapData const& map, TeamPlan& team, uint32 teamIndex, Member& m, bool wasAlive, uint32 nowMs)
{
    if (wasAlive && !m.alive)
    {
        m.diedMs = nowMs ? nowMs : 1;
        m.death.diedMs = m.diedMs;
        m.death.at = m.aliveAt;
        return;
    }
    if (wasAlive || !m.alive)
        return;

    m.diedMs = 0;
    m.respawnedMs = nowMs ? nowMs : 1;
    if (map.mapId != TpLogic::TP_MAP_ID)
        return;

    TpLogic::RespawnSide const before = team.respawnRule.Current();
    m.respawnSide = TpLogic::ClassifyRespawn({ m.x, m.y, m.z }, TpLogic::RespawnPointsOf(teamIndex));
    team.respawnRule.Record(m.respawnSide);
    if (team.respawnRule.Current() != before)
        SF_LOG_INFO("playerbots", "[BGWSG] inst=%u %s respawn rule now %s (a respawn read at %s, %.0f,%.0f)", r.instanceId,
                    teamIndex == uint32(TEAM_ALLIANCE) ? "A" : "H", TpLogic::RespawnSideName(team.respawnRule.Current()),
                    TpLogic::RespawnSideName(m.respawnSide), m.x, m.y);
}

Pos EnemyRespawnPoint(Record const& r, MapData const& map, uint32 teamIndex)
{
    uint32 const foe = 1 - teamIndex;
    if (map.mapId != TpLogic::TP_MAP_ID)
        return map.sides[foe].mainGraveyard;
    return TpLogic::RespawnPoint(TpLogic::RespawnPointsOf(foe), r.team[foe].respawnRule.Current());
}

void Refresh(Record& r, MapData const& map, uint32 nowMs, bool inProgress, PvpMatch::State* match)
{
    using namespace TpLogic;
    for (uint32 t = 0; t < 2; ++t)
    {
        TeamPlan& team = r.team[t];
        TeamPlan& enemy = r.team[1 - t];
        Side const& own = map.sides[t];
        Side const& foe = map.sides[1 - t];
        LanePosts const posts = LanePostsOf(t);

        // The enemy carrier on its run home: the kill window, its own respawn point, past it.
        team.efcPhase = team.efcSeen ? EfcPhaseOf(team.efcPos, foe.standWalk, EnemyRespawnPoint(r, map, t)) : EfcPhase::WINDOW;

        team.enemiesNearBase = 0;
        for (auto const& entry : enemy.members)
            if (entry.second.alive &&
                Dist2d(entry.second.x, entry.second.y, own.standWalk.x, own.standWalk.y) <= BASE_THREAT_RADIUS)
                ++team.enemiesNearBase;

        TeamFacts facts;
        facts.nowMs = nowMs;
        facts.inProgress = inProgress;
        facts.enemyCarriesOurFlag = team.ourFlag == FLAG_ON_PLAYER;
        facts.enemiesNearBase = PvpObs::ReconciledEnemies(team.enemiesNearBase, team.reportedBase);
        facts.ourCutPost = posts.cut;
        facts.enemyStand = foe.standWalk;
        facts.wipeGUntilMs = team.wipeGUntilMs;
        facts.wipeHUntilMs = team.wipeHUntilMs;
        // BT-502: the answers of the decision layer that still stand for the wipe reply and the floater job.
        if (match)
        {
            AiPvp::Book const& book = match->Side(t).ai.book;
            if (std::optional<std::uint8_t> const id = AiPvp::Override(book, BgLeader::DecisionPoint::D7_WIPE_REPLY, nowMs))
                facts.wipeOverride = static_cast<WipeReply>(*id);
            if (std::optional<std::uint8_t> const id = AiPvp::Override(book, BgLeader::DecisionPoint::D6_FLOATER, nowMs))
                facts.floaterOverride = static_cast<FloaterJob>(*id);
        }
        facts.members.reserve(team.members.size());
        for (auto const& entry : team.members)
        {
            Member const& m = entry.second;
            MemberFacts f;
            f.guid = m.guid;
            f.bot = m.bot;
            f.alive = m.alive;
            f.role = m.role;
            f.roleIndex = m.roleIndex;
            f.healer = m.healer;
            f.healerFollow = m.healerFollow;
            f.death = m.death;
            facts.members.push_back(f);
        }
        for (auto const& entry : enemy.members)
            facts.enemyDeaths.push_back(entry.second.death);

        TeamJobs const jobs = EvaluateTeam(facts);
        if (match)
            PublishLists(*match, t, jobs, nowMs);
        MidLockTurn(r, map, team, enemy, t, inProgress, nowMs, facts.enemyCarriesOurFlag,
                    BaseThreatened(facts.enemyCarriesOurFlag, facts.enemiesNearBase));
        team.midAlive = jobs.midAlive;
        team.floaterAmongMid = jobs.floaterAmongMid;
        team.wipeGUntilMs = jobs.wipeGUntilMs;
        team.wipeHUntilMs = jobs.wipeHUntilMs;
        for (MemberJobs const& job : jobs.members)
        {
            auto const found = team.members.find(job.guid);
            if (found == team.members.end())
                continue;
            Member& m = found->second;
            m.floater = job.floater;
            m.wipeDefend = job.wipeDefend;
            m.wipeRegroup = job.wipeRegroup;
            m.wipeJoin = job.wipeJoin;
        }
    }
}

Pos MidPostOf(TeamPlan const& team, Member const& me, uint32 teamIndex)
{
    TpLogic::LanePosts const posts = TpLogic::LanePostsOf(teamIndex);
    return TpLogic::MidPostOf(me.floater, me.roleIndex, team.midAlive, team.floaterAmongMid) == TpLogic::MidPost::COVER
               ? posts.cover
               : posts.cut;
}

void FillContext(DecisionContext& c, TeamPlan const& team, Member const& me, bool efcKnown)
{
    c.efcPastGy = efcKnown && team.efcPhase == TpLogic::EfcPhase::PAST;
    c.wipeDefend = me.wipeDefend;
    c.wipeRegroup = me.wipeRegroup;
    c.wipeJoin = me.wipeJoin;
    c.stalemateAllIn = WsgLogic::StalemateAllIn(c.bothHeldMs);
}
}  // namespace TpPlan
