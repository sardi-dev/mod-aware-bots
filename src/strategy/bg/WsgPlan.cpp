/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE). See WsgPlan.h for what this is.
 */

#include "WsgPlan.h"

// DatabaseEnv.h must precede the battleground headers (see BattleGroundTactics.cpp).
#include "DatabaseEnv.h"

#include "AiPvpDecision.h"
#include "BgCalloutPolicy.h"
#include "BgFlagMap.h"
#include "BgFlagView.h"
#include "BgLeaderAdapters.h"
#include "BgScoutPolicy.h"
#include "BotSkill.h"
#include "BotVoice.h"
#include "Battleground.h"
#include "Config.h"
#include "GameObject.h"
#include "Group.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotSpec.h"
#include "Playerbots.h"
#include "PvpConfig.h"
#include "PvpMatchState.h"
#include "Random.h"
#include "Timer.h"
#include "TpPlan.h"
#include "TpPlanLogic.h"
#include "WorldSession.h"
#include "WsgPlanRecord.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

using namespace WsgLogic;
using namespace WsgPlanRec;

namespace
{
// ---------------------------------------------------------------------------------------------------------------------
// Map data (BT-470): the sides, the middle and the clock of each map the plan plays come from Utils/BgFlagMap.h, picked by
// map id. The flag state comes from BgFlagView.h, so nothing in this file knows which core class a map has.
// ---------------------------------------------------------------------------------------------------------------------
using BgFlagMap::MapData;
using BgFlagMap::Pos;
using BgFlagMap::Side;

// The map data of a map the plan plays, nullptr for any other map.
MapData const* MapOf(uint32 mapId)
{
    if (mapId == BgFlagMap::WSG_MAP_ID)
        return &BgFlagMap::Warsong();
    if (mapId == TpLogic::TP_MAP_ID)
        return &TpLogic::Data();
    return nullptr;
}

constexpr float MID_RADIUS = 20.0f;
constexpr float EXIT_RADIUS = 12.0f;
// Twin Peaks lane posts: a spread around the buff object, small enough to stay on the navmesh of the lane.
constexpr float LANE_POST_RADIUS = 6.0f;
constexpr float REGROUP_RADIUS = 12.0f;
constexpr float POST_RADIUS = 3.0f;
constexpr float GY_EXIT_JITTER = 4.0f;
constexpr float LOOSE_HOME_RADIUS = 35.0f;
constexpr float COVER_REAR_YARDS = 25.0f;
constexpr float CUT_LANE_YARDS = 40.0f;
constexpr float MID_ENEMY_SIDE_YARDS = 40.0f;
// A random point is used only when its ground height is near the origin's: a wall top or a roof is not "the same spot".
constexpr float MAX_JITTER_Z = 12.0f;
// Call-out places.
constexpr float PLACE_STAND_RADIUS = 45.0f;
constexpr float PLACE_GY_RADIUS = 40.0f;
constexpr float PLACE_TUNNEL_RADIUS = 50.0f;
constexpr float PLACE_MID_RADIUS = 70.0f;
constexpr float INCOMING_RADIUS = 40.0f;
constexpr uint32 INCOMING_MIN_ENEMIES = 3;
constexpr uint32 INCOMING_REPEAT_MS = 20000;
constexpr float FC_LOW_PERCENT = 40.0f;
constexpr float EFC_LANE_RADIUS = 80.0f;
// Dropped flag responders must be within this many yards.
constexpr float DROP_RESPONDER_RANGE = 120.0f;
// Human call: the answer is valid this long.
constexpr uint32 HUMAN_CALL_MS = 20000;
// Strike roles stay close to the carrier; casual escorts keep the old 20 yd rule.
constexpr float CASUAL_ESCORT_DISTANCE = 20.0f;
constexpr float PEEL_RANGE = 35.0f;
// Opening buff reach for the low brackets.
constexpr float LOW_BRACKET_SPEED_RANGE = 50.0f;
constexpr uint32 LOW_BRACKET_SPEED_MS = 150u * 1000u;
constexpr uint32 HEAL_PRIORITY_POINTS = 25;
constexpr uint32 TAKER_NEAR_YARDS = 80;
// BT-156: the refresh interval and the record table are BgPlan's (Utils/BgPlanCommon.h), shared with Arathi Basin.
using BgPlan::REFRESH_MS;
constexpr uint32 LOG_REPEAT_MS = 30000;
constexpr uint32 STALE_CALL_MS = 4000;
constexpr uint32 PENDING_MAX_AGE_MS = 15000;
constexpr uint32 TIME_CALL_LEFT_MS = 3u * 60u * 1000u;

BgPlan::RecordTable<Record> g_table;
std::mutex& g_lock = g_table.lock;
std::unordered_map<uint32, Record>& g_records = g_table.records;
std::atomic<uint32> g_pendingTotal{ 0 };

// AiPlayerbot.BattlegroundChat.Enable / .Replies, read once like the other local settings.
bool CallsEnabled()
{
    static bool const enabled = sConfigMgr->GetBoolDefault("AiPlayerbot.BattlegroundChat.Enable", true);
    return enabled;
}

bool RepliesEnabled()
{
    static bool const enabled = sConfigMgr->GetBoolDefault("AiPlayerbot.BattlegroundChat.Replies", true);
    return enabled;
}

uint32 TeamIndexOf(Player* player)
{
    return player->GetBGTeam() == ALLIANCE ? uint32(TEAM_ALLIANCE) : uint32(TEAM_HORDE);
}

float Dist2d(float ax, float ay, float bx, float by)
{
    float const dx = ax - bx;
    float const dy = ay - by;
    return std::sqrt(dx * dx + dy * dy);
}

float Dist2d(Pos const& a, Pos const& b) { return Dist2d(a.x, a.y, b.x, b.y); }

// The point `yards` from `from` on the way to `to` (the whole way when it is closer).
Pos Towards(Pos const& from, Pos const& to, float yards)
{
    float const total = Dist2d(from, to);
    if (total <= yards || total < 0.01f)
        return to;

    float const f = yards / total;
    return { from.x + (to.x - from.x) * f, from.y + (to.y - from.y) * f, from.z + (to.z - from.z) * f };
}

char const* ClassWord(uint8 cls)
{
    switch (cls)
    {
        case CLS_WARRIOR: return "warrior";
        case CLS_PALADIN: return "pally";
        case CLS_HUNTER: return "hunter";
        case CLS_ROGUE: return "rogue";
        case CLS_PRIEST: return "priest";
        case CLS_DEATH_KNIGHT: return "dk";
        case CLS_SHAMAN: return "shaman";
        case CLS_MAGE: return "mage";
        case CLS_WARLOCK: return "lock";
        case CLS_MONK: return "monk";
        case CLS_DRUID: return "druid";
        default: return "guy";
    }
}

char const* TierWord(Tier tier)
{
    return tier == Tier::EXPERT ? "expert" : tier == Tier::GOOD ? "good" : "casual";
}

// The call-out zones of a map: the flag rooms, the graveyards, the exits of the bases and the middle. Built once per map.
std::vector<PlaceAnchor> BuildAnchors(MapData const& map)
{
    // Twin Peaks names its zones by lane, buff and door (sheet section 8).
    if (map.mapId == TpLogic::TP_MAP_ID)
        return TpLogic::PlaceAnchors();

    std::vector<PlaceAnchor> anchors;
    for (Side const& side : map.sides)
        anchors.push_back({ Place::FLAG_ROOM, side.standWalk.x, side.standWalk.y, PLACE_STAND_RADIUS });
    for (Side const& side : map.sides)
        anchors.push_back({ Place::GRAVEYARD, side.mainGraveyard.x, side.mainGraveyard.y, PLACE_GY_RADIUS });
    for (Side const& side : map.sides)
        anchors.push_back({ Place::TUNNEL, side.tunnelExit.x, side.tunnelExit.y, PLACE_TUNNEL_RADIUS });
    anchors.push_back({ Place::MID, map.mid.x, map.mid.y, PLACE_MID_RADIUS });
    return anchors;
}

std::vector<PlaceAnchor> const& AnchorsOf(MapData const& map)
{
    static std::mutex lock;
    static std::unordered_map<uint32, std::vector<PlaceAnchor>> cache;
    std::lock_guard<std::mutex> guard(lock);
    auto itr = cache.find(map.mapId);
    if (itr == cache.end())
        itr = cache.emplace(map.mapId, BuildAnchors(map)).first;
    return itr->second;
}

// Where a thing is, in the words of a call-out. The roof is the area high above the flag room (maps with a roof zone).
Place PlaceOf(MapData const& map, Pos const& p)
{
    if (map.roofAboveStand > 0.0f)
        for (Side const& side : map.sides)
            if (p.z > side.standWalk.z + map.roofAboveStand && Dist2d(p, side.standWalk) < PLACE_STAND_RADIUS + 15.0f)
                return Place::ROOF;

    return ClassifyPlace(AnchorsOf(map), p.x, p.y);
}

// A place a human called, as a point on that team's own side of the map.
Pos PlacePosition(MapData const& map, uint32 team, Place place)
{
    Side const& own = map.sides[team];
    switch (place)
    {
        case Place::TUNNEL: return own.tunnelExit;
        case Place::MID: return map.mid;
        case Place::RAMP: return own.post[1];
        case Place::GRAVEYARD: return own.mainGraveyard;
        // Twin Peaks zones (BT-474): the lanes by their graveyard, the buffs by their object, a door by the entrance post.
        case Place::LANE_A: return map.sides[0].mainGraveyard;
        case Place::LANE_H: return map.sides[1].mainGraveyard;
        case Place::BERS_A: return TpLogic::LanePostsOf(1).cut;
        case Place::BERS_H: return TpLogic::LanePostsOf(0).cut;
        case Place::LEAF_A: return TpLogic::LanePostsOf(0).cover;
        case Place::LEAF_H: return TpLogic::LanePostsOf(1).cover;
        case Place::DOOR: return own.post[0];
        case Place::ROOF:
        case Place::FLAG_ROOM:
        case Place::NONE: break;
    }
    return own.standWalk;
}

Pos PosOf(WorldObject const* object)
{
    return { object->GetPositionX(), object->GetPositionY(), object->GetPositionZ() };
}

uint32 Low(uint64 guid) { return uint32(GUID_LOPART(guid)); }

// The position of a team's dropped flag, if it lies on the ground.
bool DroppedFlagPos(Battleground* bg, uint32 flagTeamIndex, Pos& out)
{
    BgFlagMap::FlagView flags;
    if (!BgFlagView::Fill(bg, flags) || !flags.dropped[flagTeamIndex])
        return false;
    uint64 const guid = flags.dropped[flagTeamIndex];

    GameObject* go = bg->GetBgMap()->GetGameObject(guid);
    if (!go)
        return false;

    out = PosOf(go);
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// Call-outs.
// ---------------------------------------------------------------------------------------------------------------------
struct CallText
{
    char const* key;
    std::vector<char const*> lines;
};

// The in-code lines, used when the text table has no row of the name (sql/.../2026_11_12_00.sql adds them).
CallText const& TextOf(Call kind)
{
    static std::vector<CallText> const texts = {
        { "bg_efc_taken", { "efc at %place", "they got our flag, %class carrying", "efc %place" } },
        { "bg_efc_moved", { "efc %place", "efc going %place" } },
        { "bg_fc_got", { "got it, heading home", "escorts on me", "flag is mine" } },
        { "bg_fc_low", { "fc low, heals", "need heals on fc" } },
        { "bg_flag_down", { "flag down at %place", "flag dropped %place" } },
        { "bg_flag_back", { "ret!", "flag back" } },
        { "bg_inc_fr", { "%n inc fr", "need help fr" } },
        { "bg_cap", { "cap %score", "cap! %score" } },
        { "bg_time", { "%min min left, %score, last cap wins", "%min min, %score" } },
        { "bg_omw", { "omw", "on my way" } },
        { "bg_stance", { "%stance, %n home", "%stance (%n home)" } },
        { "bg_scout", { "%n inc %place", "%n coming %place" } },
        { "bg_efc_past", { "efc past gy", "efc passed gy" } },
        { "bg_mid_push", { "mid is stuck, home push mid", "push mid, home come", "all in mid, home too" } },
    };
    return texts[static_cast<size_t>(kind)];
}

// Marks a line as telling the team something: when it is said it becomes an observation of the side (BT-483).
void Observe(Pending& line, PvpObs::Kind kind, Place place, uint32 count)
{
    line.observe = true;
    line.obsKind = kind;
    line.obsPlace = place == Place::NONE ? PvpObs::PLACE_UNKNOWN : static_cast<uint8>(place);
    line.obsCount = static_cast<uint8>(std::min<uint32>(count, PvpObs::MAX_REPORTED_COUNT));
}

// True when a line was queued. `forced` skips the tier's chance (a leader always says its stance); the limiter and the
// tier's delay still apply.
bool Enqueue(Record& r, TeamPlan& team, Member const& speaker, Call kind, uint32 nowMs, Pending base, bool forced = false)
{
    if (!CallsEnabled() || !speaker.bot || !speaker.alive)
        return false;

    ++r.callSeq;
    // Expert bots call everything, good 70 percent, casual 30 (and late); the chance is a fixed roll per event.
    BgPlan::CallDraw const draw = BgPlan::DrawCall(speaker.guid, r.callSeq, speaker.tier);
    if (!draw.pass && !forced)
        return false;

    if (!team.limiter.Allow(speaker.guid, kind, nowMs))
        return false;

    base.guid = speaker.guid;
    base.kind = kind;
    base.seq = r.callSeq;
    base.dueMs = nowMs + draw.delayMs;
    r.pending.push_back(base);
    ++g_pendingTotal;
    return true;
}

// The nearest alive bot of a team to a point, optionally limited to some roles. A heedStamp (the time a human's line was
// received) leaves out the casual bots that ignore that line.
Member* NearestBot(TeamPlan& team, Pos const& to, bool homeAndMidOnly, uint32 heedStamp = 0)
{
    Member* best = nullptr;
    float bestDist = 0.0f;
    for (auto& entry : team.members)
    {
        Member& m = entry.second;
        if (!m.bot || !m.alive || !m.present)
            continue;
        if (homeAndMidOnly && m.role != Role::HOME && m.role != Role::MID)
            continue;
        if (heedStamp && !BgCallout::Heeds(m.tier, m.guid, heedStamp))
            continue;

        float const d = Dist2d(m.x, m.y, to.x, to.y);
        if (!best || d < bestDist || (d == bestDist && m.guid < best->guid))
        {
            best = &m;
            bestDist = d;
        }
    }
    return best;
}

// The nearest alive expert bot within sight of a point (an expert says what it saw).
Member* NearestExpert(TeamPlan& team, Pos const& to, float sightYards)
{
    Member* best = nullptr;
    float bestDist = sightYards;
    for (auto& entry : team.members)
    {
        Member& m = entry.second;
        if (!m.bot || !m.alive || !m.present || m.tier != Tier::EXPERT)
            continue;

        float const d = Dist2d(m.x, m.y, to.x, to.y);
        if (d <= bestDist && (!best || d < bestDist || m.guid < best->guid))
        {
            best = &m;
            bestDist = d;
        }
    }
    return best;
}

Member* FindMember(TeamPlan& team, uint32 guid)
{
    auto const itr = team.members.find(guid);
    return itr == team.members.end() ? nullptr : &itr->second;
}

// ---------------------------------------------------------------------------------------------------------------------
// The leader (BT-481): the plan's stance in the words of a call-out, and the turn the leader takes at each refresh.
// ---------------------------------------------------------------------------------------------------------------------
char const* StanceWord(BgLeader::Stance stance)
{
    switch (stance)
    {
        case BgLeader::Stance::OFFENSE: return "all out";
        case BgLeader::Stance::DEFENSE: return "defense";
        case BgLeader::Stance::ALL_IN: return "all in, last cap wins";
        case BgLeader::Stance::TURTLE: return "turtle";
        default: break;
    }
    return "balanced";
}

// A refresh's roll call of one side for the election: every member of the side, humans included (they are never eligible).
std::vector<BgLeader::Candidate> LeaderCandidates(TeamPlan const& team)
{
    std::vector<BgLeader::Candidate> list;
    list.reserve(team.members.size());
    for (auto const& entry : team.members)
    {
        Member const& m = entry.second;
        list.push_back({ m.guid, m.tier, m.level, m.bot, m.alive, m.present });
    }
    return list;
}

// The leader of side t confirms the stance of the rule's plan (or applies a decision layer's answer) and, after a stance
// change, sets the drift of the casual bots. Returns true when the stance changed after the gates opened.
bool LeaderTurn(Record& r, PvpMatch::State& match, uint32 t, TeamPlan& team, uint32 nowMs)
{
    BgLeader::LeaderState& leader = match.Side(t).leader;
    BgLeader::FlagPlanState const rule{ team.plan, team.situation };
    if (!PvpConfig::LeaderEnabled())
    {
        team.leader = 0;
        team.stance = BgLeader::StanceOf(rule);
        team.stanceCallDue = false;
        return false;
    }
    // BT-502: the answers of the decision layer are judged inside the tick, against the list this refresh just published.
    BgLeader::TickResult const tick = BgLeader::TickWith(leader, LeaderCandidates(team), BgLeader::StanceOf(rule),
                                                         BgLeader::StanceOffer(rule), nowMs, r.started,
                                                         [&](BgLeader::LeaderState& state) { AiPvpDecision::Receive(match, t, state, nowMs); });
    team.leader = tick.leader;
    team.stance = tick.stance;
    team.stanceCallDue = tick.callDue;

    if (tick.stance != BgLeader::StanceOf(rule))
    {
        BgLeader::FlagPlanState const applied = BgLeader::Apply(tick.stance, rule);
        team.plan = applied.plan;
        team.situation = applied.situation;
    }

    if (tick.change != BgLeader::Change::NONE || tick.stanceChanged)
        SF_LOG_DEBUG("playerbots", "[BGWSG-LEAD] inst=%u gen=%u %s leader=%u change=%u stance=%s epoch=%u version=%u",
                     r.instanceId, match.Key().generation, t == uint32(TEAM_ALLIANCE) ? "A" : "H", tick.leader,
                     uint32(tick.change), BgLeader::StanceName(tick.stance), leader.election.epoch, leader.stanceVersion);

    if (!tick.stanceChanged || !r.started)
        return false;

    for (auto& entry : team.members)
    {
        Member& m = entry.second;
        m.keepRole = m.bot && m.guid != tick.leader ? BgLeader::RoleKeepRefreshes(m.tier, m.guid, leader.stanceSeq) : 0;
    }
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// What the decision layer is told about the match (BT-502): game context only, every number from the record the plan keeps.
// ---------------------------------------------------------------------------------------------------------------------
// Both flag maps end at three captures.
constexpr uint32 CAPTURES_TO_WIN = 3;

char const* FlagStateName(uint8 state)
{
    switch (state)
    {
        case FLAG_ON_BASE: return "home";
        case FLAG_WAIT_RESPAWN: return "respawning";
        case FLAG_ON_PLAYER: return "carried";
        case FLAG_ON_GROUND: return "dropped";
        default: break;
    }
    return "unknown";
}

void DescribeMatch(AwareAiBgState& state, Record const& r, uint32 t, int32 leftMs)
{
    TeamPlan const& team = r.team[t];
    TeamPlan const& enemy = r.team[1 - t];

    state.Map = r.map && r.map->mapId == TpLogic::TP_MAP_ID ? "tp" : "wsg";
    state.ScoreOurs = team.score;
    state.ScoreTheirs = enemy.score;
    state.ScoreLimit = CAPTURES_TO_WIN;
    state.TimeLeftS = leftMs > 0 ? leftMs / 1000 : 0;
    state.Nodes = { std::string("our_flag:") + FlagStateName(team.ourFlag), std::string("their_flag:") + FlagStateName(team.theirFlag) };

    // Our bots by job (alive ones): the shape of the team.
    uint32 perRole[ROLE_COUNT] = {};
    uint32 alive = 0;
    for (auto const& entry : team.members)
    {
        Member const& m = entry.second;
        if (!m.bot || !m.alive)
            continue;
        ++alive;
        if (m.role != Role::NONE)
            ++perRole[static_cast<std::size_t>(m.role)];
    }
    for (Role role : { Role::TAKER, Role::BACKUP, Role::ESCORT, Role::HOME, Role::MID })
        if (uint32 const n = perRole[static_cast<std::size_t>(role)])
            state.FriendlyGroups.push_back(std::string(RoleName(role)) + ":" + std::to_string(n));

    // What the plan sees at our flag room, and what call-outs add to it (the plan's own reconciliation).
    if (uint32 const base = PvpObs::ReconciledEnemies(team.enemiesNearStand, team.reportedBase))
        state.ObservedPressure.push_back("our_flag_room:" + std::to_string(base));

    state.Facts.push_back("our_bots_alive:" + std::to_string(alive) + "/" + std::to_string(team.size));
    state.Facts.push_back("enemy_players:" + std::to_string(enemy.size));
    if (team.fcDebuff || team.efcDebuff)
        state.Facts.push_back("carrier_debuff_stacks:ours=" + std::to_string(team.fcDebuff) + ",theirs=" + std::to_string(team.efcDebuff));
    if (team.stalemateHunt || team.stalemateRun)
        state.Facts.push_back(std::string("stalemate:") + (team.stalemateRun ? "run" : "hunt"));
    if (team.efcSeen)
        state.Facts.push_back("enemy_carrier_seen:yes");
}

// A change of this is a flag event (a pickup, a drop, a return) or a capture.
uint64 FlagEventSignature(TeamPlan const& team, TeamPlan const& enemy)
{
    return uint64(team.ourFlag) | uint64(team.theirFlag) << 8 | uint64(team.score & 0xFF) << 16 | uint64(enemy.score & 0xFF) << 24;
}

// ---------------------------------------------------------------------------------------------------------------------
// The record.
// ---------------------------------------------------------------------------------------------------------------------
Record& RecordOf(uint32 instanceId, uint32 nowMs)
{
    // Old records of matches that ended without the leave hook are dropped when a new one starts.
    return g_table.Of(instanceId, nowMs, [](Record const& old)
    {
        g_pendingTotal -= std::min<uint32>(g_pendingTotal.load(), uint32(old.pending.size()));
    });
}

// The stacks of the two carrier debuffs of a player, as one level (WsgLogic::DebuffLevel).
uint32 DebuffLevelOf(Player* p)
{
    Aura const* const focused = p->GetAura(FOCUSED_ASSAULT_SPELL);
    Aura const* const brutal = p->GetAura(BRUTAL_ASSAULT_SPELL);
    return DebuffLevel(focused ? focused->GetStackAmount() : 0u, brutal ? brutal->GetStackAmount() : 0u);
}

void ReadMember(Member& m, Player* p)
{
    m.alive = p->IsAlive();
    m.present = true;
    m.level = p->getLevel();
    m.x = p->GetPositionX();
    m.y = p->GetPositionY();
    m.z = p->GetPositionZ();
    m.hpPercent = p->GetHealthPct();
    m.inCombat = p->IsInCombat();
}

void InitMember(Member& m, Player* p)
{
    m.guid = p->GetGUIDLow();
    m.bot = p->GetSession() && p->GetSession()->IsBot();
    m.cls = p->getClass();
    m.healer = PlayerBotSpec::IsHeal(p, true);
    m.tank = PlayerBotSpec::IsTank(p, true);
    m.melee = PlayerBotSpec::IsMelee(p, true);
    m.feral = m.cls == CLS_DRUID && BotGetSpecialization(p) == SPEC_DRUID_FERAL;
    m.tier = m.bot ? BotSkill::TierOf(p) : Tier::EXPERT;
}

void MixOf(TeamPlan& team, TeamMix& mix)
{
    mix.size = team.size;
    mix.humans = team.humans;
    mix.hasHealer = team.hasHealer;
    mix.lowBracket = team.lowBracket;
}

// Rebuilds the roster, the roles, the groups and the events from the battleground. One call per record per
// REFRESH_MS, on the map thread of the match (the callers are the bots' own ticks).
void Refresh(Record& r, Battleground* bg, uint32 nowMs, uint8 strategyAlliance, uint8 strategyHorde)
{
    r.map = MapOf(bg->GetMapId());
    MapData const& map = *r.map;
    r.lastRefreshMs = nowMs ? nowMs : 1;
    uint32 const elapsed = bg->GetElapsedTime();
    BattlegroundStatus const status = bg->GetStatus();
    if (status == STATUS_IN_PROGRESS && !r.started)
    {
        r.started = true;
        r.gatesOpenElapsedMs = elapsed;
    }
    r.playMs = r.started ? elapsed - r.gatesOpenElapsedMs : 0;
    int32 const left = BgFlagMap::LeftMs(elapsed, map.clock);

    // BT-480: the shared PvP match state of this match (created here, on the map thread; ended by the leave path or swept).
    PvpMatch::Handle const match = PvpMatch::Touch(bg->GetInstanceID(), bg->GetMapId(), elapsed, status == STATUS_WAIT_LEAVE, nowMs);
    bool stanceChangedNow = false;

    // BT-502: a finished match awaits no answer; its asks are summed up and their outcome reported once.
    if (status == STATUS_WAIT_LEAVE)
        for (uint32 t = 0; t < 2; ++t)
        {
            uint32 const ours = bg->GetTeamScore(t);
            uint32 const theirs = bg->GetTeamScore(1 - t);
            AiPvpDecision::MatchOver(*match, t, ours > theirs ? 1 : ours < theirs ? -1 : 0, nowMs);
        }

    // BT-482: the humans' call-outs since the last refresh, put into the stores of their sides.
    for (PvpMatch::State::Ingested const& in : match->IngestInbox(nowMs, PvpConfig::ReaderLimits()))
        r.team[in.side].inbox.push_back(in.obs);

    for (TeamPlan& team : r.team)
    {
        for (auto& entry : team.members)
        {
            entry.second.present = false;
            entry.second.hunter = false;
            entry.second.ownGroundResponder = false;
            entry.second.theirGroundResponder = false;
            entry.second.healerFollow = false;
        }
        team.size = 0;
        team.humans = 0;
        team.hasHealer = false;
        team.lowBracket = false;
    }

    // The flags are read before the roster so that only the two carriers pay for an aura lookup (BT-472).
    BgFlagMap::FlagView flags;
    BgFlagView::Fill(bg, flags);

    // --- roster: one walk over the map's players ---
    uint8 maxLevel[2] = { 0, 0 };
    for (auto const& ref : bg->GetBgMap()->GetPlayers())
    {
        Player* p = ref.GetSource();
        if (!p || !p->IsInWorld() || p->GetBattlegroundId() != bg->GetInstanceID() || p->IsGameMaster())
            continue;

        uint32 const t = TeamIndexOf(p);
        TeamPlan& team = r.team[t];
        auto found = team.members.find(p->GetGUIDLow());
        if (found == team.members.end())
        {
            found = team.members.emplace(p->GetGUIDLow(), Member()).first;
            InitMember(found->second, p);
        }

        Member& m = found->second;
        bool const wasAlive = m.alive;
        bool const seenBefore = m.seen;
        if (wasAlive)
            m.aliveAt = { m.x, m.y, m.z };
        ReadMember(m, p);
        m.seen = true;
        if (seenBefore && status == STATUS_IN_PROGRESS)
            TpPlan::NoteLifeChange(r, map, team, t, m, wasAlive, nowMs);
        m.debuff = (p->GetGUID() == flags.picker[0] || p->GetGUID() == flags.picker[1]) ? DebuffLevelOf(p) : 0;
        ++team.size;
        if (!m.bot)
            ++team.humans;
        if (m.healer)
            team.hasHealer = true;
        maxLevel[t] = std::max(maxLevel[t], m.level);
    }

    // Members that left the match are forgotten.
    for (TeamPlan& team : r.team)
    {
        for (auto itr = team.members.begin(); itr != team.members.end();)
        {
            if (!itr->second.present)
                itr = team.members.erase(itr);
            else
                ++itr;
        }
    }

    for (uint32 t = 0; t < 2; ++t)
        r.team[t].lowBracket = maxLevel[t] > 0 && maxLevel[t] < LOW_BRACKET_BELOW_LEVEL;
    r.humanPresent = r.team[0].humans + r.team[1].humans > 0;

    if (status != STATUS_IN_PROGRESS && status != STATUS_WAIT_JOIN)
        return;

    // --- flags and score ---
    bool changed = false;
    uint64 signature = 1469598103934665603ull;
    auto const mixIn = [&signature](uint64 value) { signature = (signature ^ value) * 1099511628211ull; };

    for (uint32 t = 0; t < 2; ++t)
    {
        TeamPlan& team = r.team[t];
        uint32 const other = 1 - t;
        team.ourFlag = flags.state[t];
        team.theirFlag = flags.state[other];
        team.efcFull = flags.picker[t];
        team.fcFull = flags.picker[other];
        team.score = bg->GetTeamScore(t);

        // Strategy numbers (WSBotStrategy) come from the caller; the harness may pin them.
        uint8 const strategy = t == uint32(TEAM_ALLIANCE) ? strategyAlliance : strategyHorde;
        Plan const plan = BgPlan::PlanFromStrategy(strategy);
        if (plan != team.rulePlan)
        {
            team.rulePlan = plan;
            changed = true;
        }
        team.plan = plan;   // the leader's turn may replace it below (BT-481)
    }

    r.bothHeldMs = BothHeldMs(r.team[0].ourFlag, r.team[0].theirFlag, nowMs, r.bothHeldStartMs);

    // --- roles, per team ---
    for (uint32 t = 0; t < 2; ++t)
    {
        TeamPlan& team = r.team[t];
        uint32 const other = 1 - t;
        TeamPlan& enemy = r.team[other];

        team.situation = SituationOf(team.score, enemy.score, left);
        stanceChangedNow = LeaderTurn(r, *match, t, team, nowMs) || stanceChangedNow;

        std::vector<Candidate> candidates;
        for (auto& entry : team.members)
        {
            Member& m = entry.second;
            if (!m.bot || !m.alive)
                continue;

            Candidate c;
            c.guidLow = m.guid;
            c.cls = m.cls;
            c.healer = m.healer;
            c.tank = m.tank;
            c.melee = m.melee;
            c.feral = m.feral;
            c.level = m.level;
            c.tier = m.tier;
            c.previous = m.role;
            c.carrying = Low(team.fcFull) == m.guid;
            candidates.push_back(c);
        }

        TeamMix mix;
        MixOf(team, mix);
        team.counts = CountsFor(team.plan, team.situation, mix);
        // BT-481: a casual bot may keep its old role for a refresh after a stance change, except the carrier, a flag taker and
        // the first home guard (the base is never left empty by a late bot).
        uint32 homeNow = 0;
        for (auto const& entry : team.members)
            if (entry.second.bot && entry.second.alive && entry.second.role == Role::HOME)
                ++homeNow;
        for (Assignment const& a : Assign(candidates, team.counts))
        {
            Member* m = FindMember(team, a.guidLow);
            if (!m)
                continue;
            if (m->role != a.role)
            {
                bool const critical = Low(team.fcFull) == m->guid || m->role == Role::TAKER || a.role == Role::TAKER ||
                                      (a.role == Role::HOME && homeNow == 0);
                if (m->role != Role::NONE && BgLeader::MayKeepRole(m->keepRole, critical))
                    continue;
                if (m->role == Role::HOME && homeNow)
                    --homeNow;
                if (a.role == Role::HOME)
                    ++homeNow;
                if (m->role != Role::NONE)
                    changed = true;
                m->role = a.role;
            }
        }
        for (auto& entry : team.members)
            if (entry.second.keepRole)
                --entry.second.keepRole;

        // Index within the role, by GUID, among the alive bots.
        for (Role role : { Role::TAKER, Role::BACKUP, Role::ESCORT, Role::HOME, Role::MID })
        {
            std::vector<uint32> guids;
            for (auto& entry : team.members)
                if (entry.second.bot && entry.second.alive && entry.second.role == role)
                    guids.push_back(entry.first);
            std::sort(guids.begin(), guids.end());
            for (uint32 i = 0; i < guids.size(); ++i)
                team.members[guids[i]].roleIndex = i;
        }

        // The enemy carrier of our flag (a member of the enemy team).
        team.efcSeen = false;
        team.efcDebuff = 0;
        if (team.efcFull)
        {
            Member* efc = FindMember(enemy, Low(team.efcFull));
            if (efc && efc->alive)
            {
                team.efcSeen = true;
                team.efcPos = { efc->x, efc->y, efc->z };
                team.efcDebuff = efc->debuff;
            }
        }

        // Our own carrier of their flag.
        Member* carrierMember = FindMember(team, Low(team.fcFull));
        team.fcDebuff = carrierMember && carrierMember->alive ? carrierMember->debuff : 0;
    }

    // --- groups: hunters, dropped flag responders, the strike group, enemies at the stand ---
    for (uint32 t = 0; t < 2; ++t)
    {
        TeamPlan& team = r.team[t];
        uint32 const other = 1 - t;
        Side const& own = map.sides[t];
        Side const& foe = map.sides[other];

        // Is the enemy carrier under attack: some of ours (humans too) stand on it.
        team.efcPressed = false;
        if (team.efcSeen)
        {
            uint32 onIt = 0;
            for (auto const& entry : team.members)
                if (entry.second.alive && Dist2d(entry.second.x, entry.second.y, team.efcPos.x, team.efcPos.y) <= EFC_PRESSURE_RADIUS)
                    ++onIt;
            team.efcPressed = onIt >= EFC_PRESSURE_MIN;
        }
        team.stalemateHunt = StalemateHunts(r.bothHeldMs, team.fcDebuff, team.efcDebuff) && r.bothHeldMs > 0;
        team.stalemateRun = StalemateRuns(r.bothHeldMs, team.fcDebuff, team.efcPressed) && r.bothHeldMs > 0;

        if (team.efcSeen)
        {
            std::vector<std::pair<uint32, float>> list;
            for (auto& entry : team.members)
            {
                Member& m = entry.second;
                if (!m.bot || !m.alive || m.role == Role::NONE || Low(team.fcFull) == m.guid)
                    continue;
                float const toStand = Dist2d(team.efcPos.x, team.efcPos.y, own.standWalk.x, own.standWalk.y);
                if (m.role == Role::HOME && toStand > HOME_CHASE_LIMIT)
                    continue;
                list.emplace_back(m.guid, Dist2d(m.x, m.y, team.efcPos.x, team.efcPos.y));
            }
            for (uint32 guid : NearestN(list, EFC_HUNTERS))
                team.members[guid].hunter = true;
        }

        Pos ground;
        if (team.ourFlag == FLAG_ON_GROUND && DroppedFlagPos(bg, t, ground))
        {
            std::vector<std::pair<uint32, float>> list;
            for (auto& entry : team.members)
            {
                Member& m = entry.second;
                float const d = Dist2d(m.x, m.y, ground.x, ground.y);
                if (m.bot && m.alive && Low(team.fcFull) != m.guid && d <= DROP_RESPONDER_RANGE)
                    list.emplace_back(m.guid, d);
            }
            for (uint32 guid : NearestN(list, DROP_RESPONDERS))
                team.members[guid].ownGroundResponder = true;
        }

        if (team.theirFlag == FLAG_ON_GROUND && DroppedFlagPos(bg, other, ground))
        {
            std::vector<std::pair<uint32, float>> strike;
            std::vector<std::pair<uint32, float>> anyone;
            for (auto& entry : team.members)
            {
                Member& m = entry.second;
                if (!m.bot || !m.alive || Low(team.fcFull) == m.guid)
                    continue;
                float const d = Dist2d(m.x, m.y, ground.x, ground.y);
                anyone.emplace_back(m.guid, d);
                if (IsStrikeRole(m.role))
                    strike.emplace_back(m.guid, d);
            }
            std::vector<uint32> responders = NearestN(strike, DROP_RESPONDERS);
            if (responders.empty())
                responders = NearestN(anyone, 1);
            for (uint32 guid : responders)
                team.members[guid].theirGroundResponder = true;
        }

        // Owner rule 2026-11-17: the healers follow our flag carrier. The nearest two go (a home guard healer only when the
        // carrier is already close to the base), whatever their role: a healer that holds mid or the strike group's
        // gathering spot while the carrier runs home leaves the carrier without heals, which is what the first live
        // match showed (no healer was seen with any of ten carriers).
        if (team.theirFlag == FLAG_ON_PLAYER)
        {
            Member* carrierMember = FindMember(team, Low(team.fcFull));
            if (carrierMember && carrierMember->alive)
            {
                std::vector<HealerCandidate> healers;
                for (auto& entry : team.members)
                {
                    Member& m = entry.second;
                    if (!m.bot || !m.alive || !m.healer || m.role == Role::NONE || m.guid == carrierMember->guid)
                        continue;
                    healers.push_back({ m.guid, m.role == Role::HOME, Dist2d(m.x, m.y, carrierMember->x, carrierMember->y) });
                }
                for (uint32 guid : PickHealerFollowers(healers))
                    team.members[guid].healerFollow = true;
            }
        }

        // The strike group walks to the enemy tunnel exit and goes in together: it is "gathered" when three of them stand
        // within 25 yd of it (fewer when fewer are alive) or when it waited too long.
        uint32 strikeAlive = 0;
        uint32 strikeNear = 0;
        for (auto& entry : team.members)
        {
            Member& m = entry.second;
            if (!m.bot || !m.alive || !IsStrikeRole(m.role))
                continue;
            ++strikeAlive;
            if (Dist2d(m.x, m.y, foe.gather.x, foe.gather.y) <= GATHER_RADIUS)
                ++strikeNear;
        }
        uint32 const strikeSlots = team.counts[Role::TAKER] + team.counts[Role::BACKUP] + team.counts[Role::ESCORT];
        uint32 const need = std::min<uint32>(GATHER_MIN_MEMBERS, strikeAlive);
        if (!team.strikeGathered)
        {
            if (r.started && !team.gatherSinceMs)
                team.gatherSinceMs = nowMs ? nowMs : 1;
            if (need > 0 && strikeNear >= need)
                team.strikeGathered = true;
            else if (team.gatherSinceMs && nowMs - team.gatherSinceMs >= GATHER_GIVE_UP_MS)
                team.strikeGathered = true;
        }
        else if (strikeAlive * 2 < strikeSlots)
        {
            // Half the strike group is down: the survivors wait for the respawns at the exit.
            team.strikeGathered = false;
            team.gatherSinceMs = nowMs ? nowMs : 1;
        }

        // Enemies at our flag room.
        team.enemiesNearStand = 0;
        for (auto& entry : r.team[other].members)
        {
            Member& m = entry.second;
            if (m.alive && Dist2d(m.x, m.y, own.standWalk.x, own.standWalk.y) <= INCOMING_RADIUS)
                ++team.enemiesNearStand;
        }
    }

    // BT-482: the enemies call-outs report at our base, weighted by how fresh and how trusted the lines are; the wipe reply of
    // Twin Peaks counts them with the enemies it sees (a report never lowers what it sees).
    for (uint32 t = 0; t < 2; ++t)
    {
        PvpObs::SideStore& store = match->Side(t).observations;
        store.Sweep(nowMs);
        uint32 reported = 0;
        for (Place place : { Place::FLAG_ROOM, Place::DOOR, Place::ROOF, Place::RAMP })
            reported = std::max(reported, PvpObs::ReportedEnemies(store.items, static_cast<uint8>(place), nowMs));
        r.team[t].reportedBase = reported;
    }

    // --- Twin Peaks (BT-474): where the enemy carrier is on its run, the floater, the wiped attacks (states G and H) ---
    if (map.mapId == TpLogic::TP_MAP_ID)
        TpPlan::Refresh(r, map, nowMs, status == STATUS_IN_PROGRESS, match.get());

    // --- human calls: one answer each ---
    for (uint32 t = 0; t < 2; ++t)
    {
        TeamPlan& team = r.team[t];
        if (team.callActive && nowMs - team.callExpiresMs < 0x80000000u && nowMs >= team.callExpiresMs)
        {
            team.callActive = false;
            for (auto& entry : team.members)
                entry.second.answerCall = false;
            changed = true;
        }

        // BT-482: a human's line is information, never an order. Enemies coming, help and the enemy carrier each get one answer
        // from the nearest bot that heeds the line (a casual bot may not); the other kinds only live in the observation store.
        for (PvpObs::Observation const& call : team.inbox)
        {
            if (call.kind != PvpObs::Kind::INCOMING && call.kind != PvpObs::Kind::HELP &&
                call.kind != PvpObs::Kind::ENEMY_CARRIER)
                continue;

            Pos where;
            if (call.place != PvpObs::PLACE_UNKNOWN)
                where = PlacePosition(map, t, static_cast<Place>(call.place));
            else if (call.kind == PvpObs::Kind::ENEMY_CARRIER)
                continue;   // the bots know where the carrier is
            else
            {
                // "help" with no place: here, where the sender stands (read from the roster, never guessed).
                Member* sender = FindMember(team, call.senderLow);
                if (!sender || !sender->alive)
                    continue;
                where = { sender->x, sender->y, sender->z };
            }

            Member* responder = NearestBot(team, where, true, call.receivedMs);
            if (!responder)
                responder = NearestBot(team, where, false, call.receivedMs);
            if (!responder)
                continue;

            for (auto& entry : team.members)
                entry.second.answerCall = false;
            responder->answerCall = true;
            team.callActive = true;
            team.callPos = where;
            team.callExpiresMs = nowMs + HUMAN_CALL_MS;
            changed = true;
            if (RepliesEnabled())
            {
                Pending text;
                Enqueue(r, team, *responder, Call::ANSWER, nowMs, text);
            }
        }
        team.inbox.clear();
    }

    // --- events and call-outs (the first refresh only records the baseline) ---
    bool const live = r.baseline && status == STATUS_IN_PROGRESS;
    for (uint32 t = 0; t < 2; ++t)
    {
        TeamPlan& team = r.team[t];
        TeamPlan& enemy = r.team[1 - t];
        Side const& own = map.sides[t];
        Member* fc = FindMember(team, Low(team.fcFull));
        Pos const stand = own.standWalk;

        if (team.theirFlag == FLAG_ON_PLAYER && team.prevTheir != FLAG_ON_PLAYER)
        {
            team.fcLowCalled = false;
            team.previousFc = Low(team.fcFull);
        }

        if (live)
        {
            Pending text;

            // BT-481: the leader says what the team plays (at the gates, at every change, after a handover).
            if (team.stanceCallDue)
            {
                Member* leader = FindMember(team, team.leader);
                if (leader)
                {
                    Pending line;
                    line.stance = StanceWord(team.stance);
                    line.number = std::to_string(team.counts[Role::HOME]);
                    if (Enqueue(r, team, *leader, Call::STANCE, nowMs, line, true))
                        BgLeader::CallCommitted(match->Side(t).leader);
                }
            }

            // BT-694: the middle fight is locked, the idle home guards are called in. The leader says it (it always calls its plan);
            // with no leader the first guard that goes does.
            if (team.midPushCallDue)
            {
                Member* speaker = FindMember(team, team.leader);
                bool const forced = speaker && speaker->bot && speaker->alive;
                if (!forced)
                {
                    speaker = nullptr;
                    for (auto& entry : team.members)
                        if (entry.second.midJoin && entry.second.bot && entry.second.alive &&
                            (!speaker || entry.second.guid < speaker->guid))
                            speaker = &entry.second;
                }
                if (speaker)
                    Enqueue(r, team, *speaker, Call::MID_PUSH, nowMs, Pending(), forced);
            }

            if (team.ourFlag == FLAG_ON_PLAYER && team.efcSeen)
            {
                Place const place = PlaceOf(map, team.efcPos);
                if (team.prevOur != FLAG_ON_PLAYER)
                {
                    // The first HOME or MID bot that sees the carrier says where it is.
                    Member* speaker = NearestBot(team, team.efcPos, true);
                    if (!speaker)
                        speaker = NearestBot(team, team.efcPos, false);
                    if (speaker)
                    {
                        Member* efc = FindMember(enemy, Low(team.efcFull));
                        text.place = PlaceWord(place);
                        text.cls = efc ? ClassWord(efc->cls) : "guy";
                        Observe(text, PvpObs::Kind::ENEMY_CARRIER, place, 0);
                        Enqueue(r, team, *speaker, Call::EFC_TAKEN, nowMs, text);
                    }
                    team.lastEfcPlace = place;
                }
                else if (place != Place::NONE && place != team.lastEfcPlace)
                {
                    // One MID bot says when the carrier changes zone (at most once per 10 s, the limiter).
                    Member* speaker = NearestBot(team, team.efcPos, true);
                    if (speaker)
                    {
                        text.place = PlaceWord(place);
                        Observe(text, PvpObs::Kind::ENEMY_CARRIER, place, 0);
                        Enqueue(r, team, *speaker, Call::EFC_MOVED, nowMs, text);
                    }
                    team.lastEfcPlace = place;
                }
            }

            if (team.theirFlag == FLAG_ON_PLAYER && team.prevTheir != FLAG_ON_PLAYER && fc)
                Enqueue(r, team, *fc, Call::FLAG_GOT, nowMs, text);

            if (team.theirFlag == FLAG_ON_PLAYER && fc && !team.fcLowCalled && fc->hpPercent < FC_LOW_PERCENT)
            {
                team.fcLowCalled = true;
                Member* speaker = fc->bot ? fc : NearestBot(team, { fc->x, fc->y, fc->z }, false);
                if (speaker)
                    Enqueue(r, team, *speaker, Call::FC_LOW, nowMs, text);
            }

            Pos ground;
            if (team.ourFlag == FLAG_ON_GROUND && team.prevOur != FLAG_ON_GROUND && DroppedFlagPos(bg, t, ground))
            {
                if (Member* speaker = NearestBot(team, ground, false))
                {
                    text.place = PlaceWord(PlaceOf(map, ground));
                    Enqueue(r, team, *speaker, Call::FLAG_DOWN, nowMs, text);
                }
            }
            else if (team.theirFlag == FLAG_ON_GROUND && team.prevTheir != FLAG_ON_GROUND && DroppedFlagPos(bg, 1 - t, ground))
            {
                if (Member* speaker = NearestBot(team, ground, false))
                {
                    text.place = PlaceWord(PlaceOf(map, ground));
                    Enqueue(r, team, *speaker, Call::FLAG_DOWN, nowMs, text);
                }
            }

            if (team.ourFlag == FLAG_ON_BASE && team.prevOur == FLAG_ON_GROUND)
                if (Member* speaker = NearestBot(team, stand, false))
                    Enqueue(r, team, *speaker, Call::FLAG_BACK, nowMs, text);

            if (team.score > team.prevScore)
            {
                Member* speaker = FindMember(team, team.previousFc);
                if (!speaker || !speaker->bot || !speaker->alive)
                    speaker = NearestBot(team, stand, false);
                if (speaker)
                {
                    text.score = std::to_string(team.score) + "-" + std::to_string(enemy.score);
                    Enqueue(r, team, *speaker, Call::CAPTURE, nowMs, text);
                }
            }

            if (team.enemiesNearStand >= INCOMING_MIN_ENEMIES &&
                (!team.lastIncomingMs || nowMs - team.lastIncomingMs >= INCOMING_REPEAT_MS))
            {
                if (Member* speaker = NearestBot(team, stand, true))
                {
                    team.lastIncomingMs = nowMs ? nowMs : 1;
                    text.number = std::to_string(team.enemiesNearStand);
                    Observe(text, PvpObs::Kind::INCOMING, Place::FLAG_ROOM, team.enemiesNearStand);
                    Enqueue(r, team, *speaker, Call::INCOMING, nowMs, text);
                }
            }

            // BT-483: an expert bot that sees an enemy group on our half of the map says where it is, and, on Twin Peaks, that the
            // enemy carrier passed its own respawn point. Only an expert within sight speaks (what it says is what it saw).
            {
                Side const& foe = map.sides[1 - t];
                std::vector<BgScout::Enemy> seen;
                for (auto const& entry : enemy.members)
                {
                    Member const& m = entry.second;
                    if (!m.alive || !BgScout::OnOurHalf(m.x, m.y, own.standWalk.x, own.standWalk.y, foe.standWalk.x, foe.standWalk.y))
                        continue;
                    Place const zone = PlaceOf(map, { m.x, m.y, m.z });
                    seen.push_back({ m.x, m.y, zone == Place::NONE ? uint8(0xFF) : static_cast<uint8>(zone) });
                }

                BgScout::Zone zone;
                if (BgScout::BestZone(seen, static_cast<uint8>(Place::FLAG_ROOM), zone) &&
                    BgScout::RepeatDue(team.lastScoutMs[zone.place % 16], nowMs))
                {
                    if (Member* scout = NearestExpert(team, { zone.x, zone.y, 0.0f }, BgScout::SCOUT_SIGHT_YARDS))
                    {
                        Pending line;
                        line.place = PlaceWord(static_cast<Place>(zone.place));
                        line.number = std::to_string(zone.count);
                        Observe(line, PvpObs::Kind::INCOMING, static_cast<Place>(zone.place), zone.count);
                        if (Enqueue(r, team, *scout, Call::SCOUT, nowMs, line))
                            team.lastScoutMs[zone.place % 16] = nowMs ? nowMs : 1;
                    }
                }

                if (team.ourFlag == FLAG_ON_PLAYER && team.efcSeen && team.efcPhase == TpLogic::EfcPhase::PAST &&
                    team.prevEfcPhase != TpLogic::EfcPhase::PAST)
                    if (Member* scout = NearestExpert(team, team.efcPos, BgScout::SCOUT_SIGHT_YARDS))
                        Enqueue(r, team, *scout, Call::EFC_PAST, nowMs, Pending());
            }

            if (!team.timeCalled && left <= int32(TIME_CALL_LEFT_MS) && left > 0)
            {
                // The leader says the time and the score; with no leader one expert bot does.
                Member* speaker = FindMember(team, team.leader);
                if (speaker && (!speaker->bot || !speaker->alive))
                    speaker = nullptr;
                if (!speaker)
                {
                    for (auto& entry : team.members)
                    {
                        Member& m = entry.second;
                        if (m.bot && m.alive && m.tier == Tier::EXPERT &&
                            (!speaker || BotSkillMath::Mix(m.guid) < BotSkillMath::Mix(speaker->guid)))
                            speaker = &m;
                    }
                }
                team.timeCalled = true;
                if (speaker)
                {
                    text.minutes = std::to_string(std::max<int32>(1, (left + 59999) / 60000));
                    text.score = std::to_string(team.score) + "-" + std::to_string(enemy.score);
                    Enqueue(r, team, *speaker, Call::TIME_SCORE, nowMs, text);
                }
            }
        }

        team.midPushCallDue = false;
        team.prevEfcPhase = team.efcPhase;
        team.prevOur = team.ourFlag;
        team.prevTheir = team.theirFlag;
        team.prevScore = team.score;
        if (team.ourFlag != FLAG_ON_PLAYER)
            team.lastEfcPlace = Place::NONE;
    }

    // Lines whose speaker died, left or never got a turn are dropped.
    {
        size_t const before = r.pending.size();
        r.pending.erase(std::remove_if(r.pending.begin(), r.pending.end(),
            [&](Pending const& p)
            {
                for (TeamPlan& team : r.team)
                {
                    Member* speaker = FindMember(team, p.guid);
                    if (speaker)
                        return !speaker->alive || int32(nowMs - p.dueMs) > int32(PENDING_MAX_AGE_MS);
                }
                return true;
            }), r.pending.end());
        g_pendingTotal -= std::min<uint32>(g_pendingTotal.load(), uint32(before - r.pending.size()));
    }

    // --- a new epoch when the plan state changed: flags, carriers, score, situation, plan, the strike group, a call ---
    for (uint32 t = 0; t < 2; ++t)
    {
        TeamPlan const& team = r.team[t];
        mixIn(team.ourFlag);
        mixIn(team.theirFlag);
        mixIn(Low(team.efcFull));
        mixIn(Low(team.fcFull));
        mixIn(team.score);
        mixIn(uint64(team.situation));
        mixIn(uint64(team.plan));
        mixIn(team.strikeGathered ? 1 : 0);
        mixIn(team.callActive ? 1 : 0);
        mixIn(team.stalemateHunt ? 1 : 0);
        mixIn(team.wipeGUntilMs ? 1 : 0);
        mixIn(team.wipeHUntilMs ? 1 : 0);
        mixIn(uint64(team.efcPhase));
        mixIn(team.stalemateRun ? 1 : 0);
        uint64 followers = 0;
        uint64 midJoiners = 0;
        for (auto const& entry : team.members)
        {
            if (entry.second.healerFollow)
                followers += BotSkillMath::Mix(entry.first);
            if (entry.second.midJoin)
                midJoiners += BotSkillMath::Mix(entry.first);
        }
        mixIn(followers);
        // BT-694: the guards called into the middle fight (or sent home again) decide again at once.
        mixIn(midJoiners);
        // BT-695: a new lane avoidance, a bot that stood at the lane post and the escort decide again at once.
        mixIn(team.avoidSerial);
        mixIn(uint64(team.midLock.avoidLane));
        mixIn(team.escort.guard);
        uint64 detoured = 0;
        for (auto const& entry : team.members)
            if (entry.second.viaLane)
                detoured += BotSkillMath::Mix(entry.first);
        mixIn(detoured);
    }
    if (signature != r.signature)
    {
        if (r.baseline)
            changed = true;
        r.signature = signature;
    }

    // BT-502: the plan above is published and plays on; each side's leader may now ask the decision layer about it.
    for (uint32 t = 0; t < 2; ++t)
        AiPvpDecision::Ask(match, t, match->Side(t).leader, FlagEventSignature(r.team[t], r.team[1 - t]), nowMs,
                           r.started && status == STATUS_IN_PROGRESS,
                           [&](AwareAiBgState& state) { DescribeMatch(state, r, t, left); });

    if (r.baseline && changed)
    {
        r.BumpEpoch(nowMs);
        if (stanceChangedNow)
            r.stanceEpoch = r.epoch;
        for (uint32 t = 0; t < 2; ++t)
        {
            TeamPlan const& team = r.team[t];
            SF_LOG_DEBUG("playerbots",
                         "[BGWSG] inst=%u %s plan=%u sit=%s phase=%s counts=T%u/B%u/E%u/H%u/M%u size=%u humans=%u score=%u-%u left=%d epoch=%u map=%u debuff=%u/%u",
                         r.instanceId, t == uint32(TEAM_ALLIANCE) ? "A" : "H", uint32(team.plan), SituationName(team.situation),
                         PhaseName(PhaseOf(team.ourFlag, team.theirFlag)), team.counts[Role::TAKER], team.counts[Role::BACKUP],
                         team.counts[Role::ESCORT], team.counts[Role::HOME], team.counts[Role::MID], team.size, team.humans,
                         team.score, r.team[1 - t].score, left, r.epoch, map.mapId, team.fcDebuff, team.efcDebuff);
        }
    }
    r.baseline = true;
}

// A bot's member entry, refreshing the record first when it is due or the bot is new to it.
Member* Prepare(Record& r, Battleground* bg, Player* bot, uint32 nowMs, uint8 strategyAlliance, uint8 strategyHorde)
{
    bool refreshed = false;
    if (!r.lastRefreshMs || nowMs - r.lastRefreshMs >= REFRESH_MS)
    {
        Refresh(r, bg, nowMs, strategyAlliance, strategyHorde);
        refreshed = true;
    }

    TeamPlan& team = r.team[TeamIndexOf(bot)];
    Member* me = FindMember(team, bot->GetGUIDLow());
    if (!me && !refreshed)
    {
        Refresh(r, bg, nowMs, strategyAlliance, strategyHorde);
        me = FindMember(team, bot->GetGUIDLow());
    }
    return me;
}

// Positions the options resolve against, read once per decision.
struct View
{
    Battleground* bg = nullptr;
    MapData const* map = nullptr;
    Player* bot = nullptr;
    Member* me = nullptr;
    TeamPlan* team = nullptr;
    TeamPlan* enemy = nullptr;
    uint32 t = 0;
    bool efc = false;
    Pos efcPos = { 0.0f, 0.0f, 0.0f };
    bool fc = false;
    Pos fcPos = { 0.0f, 0.0f, 0.0f };
};

// Live positions of the two carriers (the roster's are up to two seconds old).
void ReadCarriers(View& v)
{
    if (v.team->efcFull)
    {
        if (Player* efc = ObjectAccessor::GetPlayer(*v.bot, v.team->efcFull))
            if (efc->IsAlive())
            {
                v.efc = true;
                v.efcPos = PosOf(efc);
            }
    }
    if (v.team->fcFull)
    {
        if (Player* fc = ObjectAccessor::GetPlayer(*v.bot, v.team->fcFull))
            if (fc->IsAlive())
            {
                v.fc = true;
                v.fcPos = PosOf(fc);
            }
    }
}

Pos Jitter(Player* bot, Pos const& origin, float radius)
{
    if (radius <= 0.0f)
        return origin;

    float rx, ry, rz;
    bot->GetRandomPoint(Position(origin.x, origin.y, origin.z, 0.0f), radius, rx, ry, rz);
    if (std::fabs(rz - origin.z) > MAX_JITTER_Z)
        return origin;
    return { rx, ry, rz };
}

bool PowerupPos(View const& v, Pos& out)
{
    // Own side: Restoration, and Berserk for everyone but a healer (as the old buff code). Which object is the own one
    // is the map's (BgFlagView::PowerupsOf).
    BgFlagView::Powerups const ups = BgFlagView::PowerupsOf(v.bg, v.t);
    uint32 const regen = ups.regen;
    uint32 const berserk = ups.berserk;
    float best = 0.0f;
    bool found = false;
    for (uint32 type : { regen, berserk })
    {
        if (type == berserk && v.me->healer)
            continue;
        GameObject* go = v.bg->GetBGObject(type);
        if (!go || !go->isSpawned())
            continue;
        float const d = Dist2d(v.me->x, v.me->y, go->GetPositionX(), go->GetPositionY());
        if (!found || d < best)
        {
            found = true;
            best = d;
            out = PosOf(go);
        }
    }
    return found;
}

// The position an option leads to; false when it has none right now (the next option is tried).
bool Resolve(Kind kind, View const& v, Pos& out, uint64& follow)
{
    Side const& own = v.map->sides[v.t];
    Side const& foe = v.map->sides[1 - v.t];
    Pos const& midPos = v.map->mid;
    follow = 0;

    switch (kind)
    {
        case Kind::CARRY_HOME:
            out = own.standWalk;
            return true;
        case Kind::CARRY_HIDE:
        {
            // A fixed post per carrier, not a new roll at every refresh (the old code dithered between three).
            uint32 usable[3];
            uint32 n = 0;
            for (uint32 i = 0; i < 3; ++i)
                if (own.hideUsable[i])
                    usable[n++] = i;
            out = own.hide[usable[BotSkillMath::Mix(v.me->guid) % n]];
            return true;
        }
        case Kind::RETURN_OWN_FLAG:
            return DroppedFlagPos(v.bg, v.t, out);
        case Kind::PICKUP_DROPPED_FLAG:
            return DroppedFlagPos(v.bg, 1 - v.t, out);
        case Kind::BLOCK_STEAL:
        case Kind::TAKE_ENEMY_FLAG:
            out = foe.standWalk;
            return true;
        case Kind::GATHER_STRIKE:
            out = Jitter(v.bot, foe.gather, EXIT_RADIUS);
            return true;
        case Kind::ANSWER_CALL:
            if (!v.team->callActive)
                return false;
            out = v.team->callPos;
            return true;
        case Kind::HUNT_EFC:
            if (!v.efc)
                return false;
            out = v.efcPos;
            return true;
        case Kind::CUT_EFC_LANE:
            if (!v.efc)
                return false;
            out = Towards(v.efcPos, foe.standWalk, CUT_LANE_YARDS);
            return true;
        case Kind::ESCORT_CARRIER:
        case Kind::HEAL_CARRIER:
            if (!v.fc)
                return false;
            out = v.fcPos;
            follow = v.team->fcFull;
            return true;
        case Kind::COVER_CARRIER_REAR:
            // Only while the carrier is in the valley: a point behind it, towards the enemy base.
            if (!v.fc || Dist2d(v.fcPos, midPos) > 150.0f)
                return false;
            out = Towards(v.fcPos, foe.standWalk, COVER_REAR_YARDS);
            return true;
        case Kind::HOLD_HOME_POST:
            out = Jitter(v.bot, own.post[v.me->roleIndex % 3], POST_RADIUS);
            return true;
        case Kind::HOLD_HOME_LOOSE:
            out = Jitter(v.bot, own.standWalk, LOOSE_HOME_RADIUS);
            return true;
        case Kind::HOLD_MID:
        {
            // Twin Peaks (BT-474): MID holds the two lane posts, 2 at the cut post and 1 at the cover post of 3.
            if (v.map->mapId == TpLogic::TP_MAP_ID)
            {
                out = Jitter(v.bot, TpPlan::MidPostOf(*v.team, *v.me, v.t), LANE_POST_RADIUS);
                return true;
            }
            uint32 const lane = v.me->roleIndex % 3;
            if (lane == 0)
                out = Jitter(v.bot, midPos, MID_RADIUS);
            else if (lane == 1)
                out = Jitter(v.bot, own.tunnelExit, 15.0f);
            else
                out = Jitter(v.bot, Towards(midPos, foe.gather, MID_ENEMY_SIDE_YARDS), 15.0f);
            return true;
        }
        case Kind::HOLD_ENEMY_EXIT:
            // Twin Peaks (BT-474): ahead 2-0, MID holds the cut post of the enemy carrier's lane near our own base. It never
            // goes to the enemy graveyard: that is its only respawn point, and the owner rule forbids camping it.
            out = v.map->mapId == TpLogic::TP_MAP_ID ? Jitter(v.bot, TpLogic::LanePostsOf(v.t).cut, LANE_POST_RADIUS)
                                                      : Jitter(v.bot, foe.gather, EXIT_RADIUS);
            return true;
        case Kind::REGROUP_LANE:
            // Twin Peaks state G: the wiped group gathers on the own outbound lane at the cut post (the Alliance's is the
            // lane H berserk, the Horde's the lane A one), wider than a post so it does not stack on one spot.
            out = Jitter(v.bot, TpLogic::LanePostsOf(v.t).cut, REGROUP_RADIUS);
            return true;
        case Kind::JOIN_MID_FIGHT:
            // Twin Peaks (BT-694): the middle of our own fighters, already kept out of the enemy graveyard zone (TpPlan::Refresh).
            out = v.team->midJoinPos;
            return true;
        case Kind::VIA_LANE:
        {
            // Twin Peaks (BT-695): the post on the lane the fight is not on (the far half of it, on the enemy's side of the valley).
            std::uint8_t const lane = TpMidLock::OtherLane(v.team->midLock.avoidLane);
            if (lane == 0 || v.map->mapId != TpLogic::TP_MAP_ID)
                return false;
            out = TpMidLock::ViaPost(lane, v.t == uint32(TEAM_ALLIANCE), TpLogic::LanePostsOf(0), TpLogic::LanePostsOf(1));
            return true;
        }
        case Kind::ESCORT_TAKER:
        {
            // Twin Peaks (BT-695): the taker the plan named when the lock was released by a kill; the roster position is at most a
            // refresh old, and the objective follows it (IsDynamic).
            auto const taker = v.team->members.find(v.team->escort.taker);
            if (taker == v.team->members.end() || !taker->second.alive)
                return false;
            out = { taker->second.x, taker->second.y, taker->second.z };
            return true;
        }
        case Kind::TAKE_POWERUP:
            return PowerupPos(v, out);
    }
    return false;
}

bool IsDynamic(Kind kind)
{
    return kind == Kind::HUNT_EFC || kind == Kind::CUT_EFC_LANE || kind == Kind::ESCORT_CARRIER ||
           kind == Kind::COVER_CARRIER_REAR || kind == Kind::HEAL_CARRIER || kind == Kind::JOIN_MID_FIGHT ||
           kind == Kind::ESCORT_TAKER;
}

void MakeView(View& v, Record& r, Battleground* bg, Player* bot, Member* me)
{
    v.bg = bg;
    v.map = r.map ? r.map : MapOf(bg->GetMapId());
    v.bot = bot;
    v.me = me;
    v.t = TeamIndexOf(bot);
    v.team = &r.team[v.t];
    v.enemy = &r.team[1 - v.t];
    ReadCarriers(v);
}

// The decision line of an objective. With a human in the match it is an info line, at most one per 10 s per bot and only
// when the objective or the role changed (so the next live run can explain the time to the first pickup and each carrier's
// death without turning the logger to debug); in a match of bots alone it stays the old debug line.
void LogObjective(Record const& r, View const& v, Member& me, Option const& option, Pos const& target, uint32 nowMs)
{
    Player* bot = v.bot;
    if (!r.humanPresent)
    {
        if (me.lastLogKind != option.kind || !me.lastLogMs || nowMs - me.lastLogMs >= LOG_REPEAT_MS)
        {
            me.lastLogKind = option.kind;
            me.lastLogMs = nowMs ? nowMs : 1;
            SF_LOG_DEBUG("playerbots",
                         "[BGWSG] %s role=%s idx=%u tier=%s phase=%s -> %s (%.0f,%.0f,%.0f) score=%d map=%u",
                         bot->GetName().c_str(), RoleName(me.role), me.roleIndex, TierWord(me.tier),
                         PhaseName(PhaseOf(v.team->ourFlag, v.team->theirFlag)), KindName(option.kind), target.x, target.y,
                         target.z, option.score, v.map->mapId);
        }
        return;
    }

    uint32 const signature = (uint32(option.kind) << 8) | uint32(me.role);
    if (!me.logGate[0].Due(signature, nowMs))
        return;

    // The carrier of our team (a flag we hold) and how far this bot is from it: the healer and escort distances.
    char fcText[64] = "-";
    if (v.fc && v.team->fcFull && Low(v.team->fcFull) != me.guid)
    {
        Player* fc = ObjectAccessor::GetPlayer(*bot, v.team->fcFull);
        std::snprintf(fcText, sizeof(fcText), "%s@%.0f", fc ? fc->GetName().c_str() : "?",
                      Dist2d(bot->GetPositionX(), bot->GetPositionY(), v.fcPos.x, v.fcPos.y));
    }
    SF_LOG_INFO("playerbots",
                "[BGWSG] t=%us %s role=%s idx=%u tier=%s phase=%s hp=%.0f -> %s (%.0f,%.0f,%.0f) score=%d healer=%d fc=%s map=%u",
                r.playMs / 1000, bot->GetName().c_str(), RoleName(me.role), me.roleIndex, TierWord(me.tier),
                PhaseName(PhaseOf(v.team->ourFlag, v.team->theirFlag)), bot->GetHealthPct(), KindName(option.kind),
                target.x, target.y, target.z, option.score, me.healerFollow ? 1 : 0, fcText, v.map->mapId);
}

bool CommonGuards(Battleground* bg, Player* bot)
{
    return bg && bot && MapOf(bg->GetMapId()) && bot->IsAlive();
}
}  // namespace

namespace WsgPlan
{
bool Enabled(Battleground* bg)
{
    return bg && MapOf(bg->GetMapId());
}

bool Decide(Battleground* bg, Player* bot, uint8 strategyAlliance, uint8 strategyHorde, bool carrying, Objective& out)
{
    if (!CommonGuards(bg, bot))
        return false;

    uint32 const now = getMSTime();
    std::lock_guard<std::mutex> guard(g_lock);
    Record& r = RecordOf(bg->GetInstanceID(), now);
    Member* me = Prepare(r, bg, bot, now, strategyAlliance, strategyHorde);
    if (!me || me->role == Role::NONE)
        return false;

    // The bot just picked up the flag: the roster has not seen it yet, read it now.
    if (carrying && Low(r.team[TeamIndexOf(bot)].fcFull) != me->guid)
    {
        Refresh(r, bg, now, strategyAlliance, strategyHorde);
        me = FindMember(r.team[TeamIndexOf(bot)], bot->GetGUIDLow());
        if (!me || me->role == Role::NONE)
            return false;
    }

    // Set first: a failed decision must not make the bot re-decide at every tick.
    me->seenEpoch = r.epoch;

    View v;
    MakeView(v, r, bg, bot, me);
    Side const& own = v.map->sides[v.t];
    TeamPlan const& team = *v.team;

    DecisionContext c;
    c.role = me->role;
    c.tier = me->tier;
    c.hasFlag = carrying || Low(team.fcFull) == me->guid;
    c.ourFlag = team.ourFlag;
    c.theirFlag = team.theirFlag;
    c.situation = team.situation;
    c.leftMs = BgFlagMap::LeftMs(bg->GetElapsedTime(), v.map->clock);
    c.bothHeldMs = r.bothHeldMs;
    // The bot's own aura is read live when it carries (the roster is up to two seconds old).
    c.ownDebuff = carrying ? DebuffLevelOf(bot) : team.fcDebuff;
    c.enemyDebuff = team.efcDebuff;
    c.efcPressed = team.efcPressed;
    c.playMs = r.playMs;
    c.roleIndex = me->roleIndex;
    c.efcKnown = v.efc;
    if (v.efc)
    {
        c.efcToOurStand = Dist2d(v.efcPos, own.standWalk);
        c.botToEfc = Dist2d(me->x, me->y, v.efcPos.x, v.efcPos.y);
    }
    c.efcHunter = me->hunter;
    c.ownGroundResponder = me->ownGroundResponder;
    c.theirGroundResponder = me->theirGroundResponder;
    c.strikeGathered = team.strikeGathered;
    c.answerCall = me->answerCall && team.callActive;
    c.healerFollower = me->healerFollow;
    c.midJoin = me->midJoin;
    c.viaLane = me->viaLane;
    c.midEscort = me->midEscort;
    if (v.map->mapId == TpLogic::TP_MAP_ID)
        TpPlan::FillContext(c, team, *me, v.efc);
    Pos ground;
    if (team.ourFlag == FLAG_ON_GROUND && DroppedFlagPos(bg, v.t, ground))
        c.botToOwnGround = Dist2d(me->x, me->y, ground.x, ground.y);
    Pos powerup;
    c.powerupAvailable = PowerupPos(v, powerup);

    std::vector<Option> const options = Options(c);
    for (Option const& option : options)
    {
        Pos target;
        uint64 follow = 0;
        if (!Resolve(option.kind, v, target, follow))
            continue;

        // The graveyard rule: a bot that stayed at the enemy graveyard too long does not choose a spot in it again.
        if (me->mustLeaveGy && !c.hasFlag && Dist2d(target, TpPlan::EnemyRespawnPoint(r, *v.map, v.t)) <= GY_ZONE_RADIUS + 10.0f)
            continue;

        out.x = target.x;
        out.y = target.y;
        out.z = target.z;
        out.kind = option.kind;
        out.role = me->role;
        out.followGuid = follow;
        me->kind = option.kind;
        me->dynamic = IsDynamic(option.kind);

        LogObjective(r, v, *me, option, target, now);
        return true;
    }
    return false;
}

bool PlanChanged(Battleground* bg, Player* bot, uint8 strategyAlliance, uint8 strategyHorde)
{
    if (!CommonGuards(bg, bot))
        return false;

    uint32 const now = getMSTime();
    std::lock_guard<std::mutex> guard(g_lock);
    Record& r = RecordOf(bg->GetInstanceID(), now);
    Member* me = Prepare(r, bg, bot, now, strategyAlliance, strategyHorde);
    if (!me || me->role == Role::NONE || me->seenEpoch == r.epoch)
        return false;

    // Core: follow the plan state when it changes - late for the casual bots, and a stance change of the leader one refresh
    // later still (BT-481); the flag carrier is never late.
    uint32 lag = AdoptLagMs(me->tier);
    if (r.epoch == r.stanceEpoch && Low(r.team[TeamIndexOf(bot)].fcFull) != me->guid)
        lag += BgLeader::StanceExtraLagMs(me->tier);
    return now - r.epochChangedMs >= lag;
}

uint32 StartSpot(Battleground* bg, Player* bot)
{
    constexpr uint32 SPOT_TUNNEL = 0;
    constexpr uint32 SPOT_GRAVEYARD = 1;
    constexpr uint32 SPOT_SPAWN = 2;
    if (!bg || !bot || !MapOf(bg->GetMapId()))
        return SPOT_SPAWN;

    uint32 const now = getMSTime();
    std::lock_guard<std::mutex> guard(g_lock);
    Record& r = RecordOf(bg->GetInstanceID(), now);
    // The start spot is chosen during the preparation, before the strategies are asked: the stored plan numbers are used.
    TeamPlan& team = r.team[TeamIndexOf(bot)];
    Member* me = FindMember(team, bot->GetGUIDLow());
    if (!me || !r.lastRefreshMs || now - r.lastRefreshMs >= REFRESH_MS)
    {
        Refresh(r, bg, now, uint8(r.team[0].plan), uint8(r.team[1].plan));
        me = FindMember(team, bot->GetGUIDLow());
    }
    if (!me)
        return SPOT_SPAWN;

    // The strike group leaves by the tunnel door; mid splits over both doors; the home guards wait in the room.
    if (IsStrikeRole(me->role))
        return SPOT_TUNNEL;
    if (me->role == Role::MID)
        return me->roleIndex % 2 == 0 ? SPOT_TUNNEL : SPOT_GRAVEYARD;
    return SPOT_SPAWN;
}

bool IsEfcHunter(Battleground* bg, Player* bot)
{
    if (!CommonGuards(bg, bot))
        return false;

    std::lock_guard<std::mutex> guard(g_lock);
    auto const record = g_records.find(bg->GetInstanceID());
    if (record == g_records.end())
        return false;

    TeamPlan& team = record->second.team[TeamIndexOf(bot)];
    Member* me = FindMember(team, bot->GetGUIDLow());
    return me && me->hunter && team.efcSeen && me->alive;
}

bool DetourDue(Battleground* bg, Player* bot)
{
    // The map first: this runs for every battleground bot at every trigger check.
    if (!bg || !bot || bg->GetMapId() != TpLogic::TP_MAP_ID || !CommonGuards(bg, bot))
        return false;

    std::lock_guard<std::mutex> guard(g_lock);
    auto const record = g_records.find(bg->GetInstanceID());
    if (record == g_records.end())
        return false;

    Member* me = FindMember(record->second.team[TeamIndexOf(bot)], bot->GetGUIDLow());
    return me && TpMidLock::DetourOverridesCombat(me->viaLane, me->alive, bot->GetHealthPct());
}

bool StrikeRunDue(Battleground* bg, Player* bot)
{
    if (!CommonGuards(bg, bot) || !bot->IsInCombat())
        return false;

    std::lock_guard<std::mutex> guard(g_lock);
    auto const record = g_records.find(bg->GetInstanceID());
    if (record == g_records.end())
        return false;

    uint32 const t = TeamIndexOf(bot);
    Member* me = FindMember(record->second.team[t], bot->GetGUIDLow());
    if (!me)
        return false;

    Pos const stand = MapOf(bg->GetMapId())->sides[1 - t].standWalk;
    uint32 meleeAttackers = 0;
    for (Unit* attacker : bot->getAttackers())
        if (attacker && attacker->IsInWorld() && attacker->IsAlive() && attacker->GetVictim() == bot &&
            attacker->IsWithinMeleeRange(bot))
            ++meleeAttackers;

    float const distance = Dist2d(bot->GetPositionX(), bot->GetPositionY(), stand.x, stand.y);
    bool const due = StrikeRunOverridesCombat(me->role, me->kind == Kind::TAKE_ENEMY_FLAG,
        bot->IsAlive(), bot->GetHealthPct(), distance,
        bot->HasUnitState(UNIT_STATE_ROOT | UNIT_STATE_CONTROLLED), meleeAttackers, me->tier);
    if (due && record->second.strikeRunLogged.insert(bot->GetGUIDLow()).second)
        SF_LOG_DEBUG("playerbots", "[BGWSG] %s strike run overrides combat role=%s hp=%.1f stand=%.1f melee=%u",
                     bot->GetName().c_str(), RoleName(me->role), bot->GetHealthPct(), distance, meleeAttackers);
    return due;
}

bool MayTakeEnemyBaseFlag(Battleground* bg, Player* bot)
{
    if (!CommonGuards(bg, bot))
        return true;

    std::lock_guard<std::mutex> guard(g_lock);
    auto const record = g_records.find(bg->GetInstanceID());
    if (record == g_records.end())
        return true;

    uint32 const t = TeamIndexOf(bot);
    TeamPlan& team = record->second.team[t];
    Member* me = FindMember(team, bot->GetGUIDLow());
    if (!me || me->role == Role::NONE)
        return true;

    // The flag takers take it; casual bots walk in and take it themselves (on purpose).
    if (me->role == Role::TAKER || me->role == Role::BACKUP || me->tier == Tier::CASUAL)
        return true;

    // Escorts and the others take it only when no taker or backup is about.
    Pos const stand = MapOf(bg->GetMapId())->sides[1 - t].standWalk;
    for (auto& entry : team.members)
    {
        Member& m = entry.second;
        if (!m.bot || !m.alive || m.guid == me->guid)
            continue;
        if ((m.role == Role::TAKER || m.role == Role::BACKUP) &&
            Dist2d(m.x, m.y, stand.x, stand.y) <= float(TAKER_NEAR_YARDS))
            return false;
    }
    return true;
}

float EscortDistance(Battleground* bg, Player* bot)
{
    if (!CommonGuards(bg, bot))
        return 0.0f;

    std::lock_guard<std::mutex> guard(g_lock);
    auto const record = g_records.find(bg->GetInstanceID());
    if (record == g_records.end())
        return 0.0f;

    Member* me = FindMember(record->second.team[TeamIndexOf(bot)], bot->GetGUIDLow());
    if (!me)
        return 0.0f;

    // A following healer trails a little farther back than the melee escorts (heal reach, out of the swings).
    if (me->healerFollow)
        return HealerFollowYards(me->tier);

    if (!IsStrikeRole(me->role))
        return 0.0f;

    return BotSkillMath::Uses(me->tier, Tier::GOOD) ? ESCORT_DISTANCE : CASUAL_ESCORT_DISTANCE;
}

bool SurroundingsOf(Battleground* bg, Player* carrier, Surroundings& out)
{
    if (!CommonGuards(bg, carrier))
        return false;

    std::lock_guard<std::mutex> guard(g_lock);
    auto const record = g_records.find(bg->GetInstanceID());
    if (record == g_records.end())
        return false;

    uint32 const t = TeamIndexOf(carrier);
    float const cx = carrier->GetPositionX();
    float const cy = carrier->GetPositionY();
    float const cz = carrier->GetPositionZ();
    auto const within = [&](Member const& m, float yards)
    {
        float const dx = m.x - cx;
        float const dy = m.y - cy;
        float const dz = m.z - cz;
        return dx * dx + dy * dy + dz * dz <= yards * yards;
    };

    out = Surroundings();
    for (auto const& entry : record->second.team[1 - t].members)
        if (entry.second.alive && within(entry.second, CARRIER_ALONE_RADIUS))
            ++out.enemiesNear;

    for (auto const& entry : record->second.team[t].members)
    {
        Member const& m = entry.second;
        if (!m.alive || m.guid == carrier->GetGUIDLow())
            continue;
        // A healer beside the carrier heals, it does not peel: only fighters count as an escort.
        if (m.bot && !m.healer && within(m, ESCORT_DISTANCE))
            ++out.escortsNear;
        if (m.healerFollow)
        {
            ++out.healerFollowers;
            float const d = std::sqrt((m.x - cx) * (m.x - cx) + (m.y - cy) * (m.y - cy) + (m.z - cz) * (m.z - cz));
            if (out.nearestHealer < 0.0f || d < out.nearestHealer)
                out.nearestHealer = d;
        }
    }
    return true;
}

bool DecisionLogDue(Battleground* bg, Player* bot, uint32 channel, uint32 signature, uint32& playSeconds)
{
    if (!CommonGuards(bg, bot) || channel > 1)
        return false;

    uint32 const now = getMSTime();
    std::lock_guard<std::mutex> guard(g_lock);
    auto const record = g_records.find(bg->GetInstanceID());
    if (record == g_records.end() || !record->second.humanPresent)
        return false;

    Member* me = FindMember(record->second.team[TeamIndexOf(bot)], bot->GetGUIDLow());
    if (!me || !me->logGate[channel].Due(signature, now))
        return false;

    playSeconds = record->second.playMs / 1000;
    return true;
}

bool IsHealerFollower(Battleground* bg, Player* bot)
{
    if (!CommonGuards(bg, bot))
        return false;

    std::lock_guard<std::mutex> guard(g_lock);
    auto const record = g_records.find(bg->GetInstanceID());
    if (record == g_records.end())
        return false;

    Member* me = FindMember(record->second.team[TeamIndexOf(bot)], bot->GetGUIDLow());
    return me && me->healerFollow && me->alive;
}

Unit* PeelTarget(Battleground* bg, Player* bot, Unit* carrier)
{
    if (!CommonGuards(bg, bot) || !carrier || carrier == bot)
        return nullptr;

    Role role = Role::NONE;
    Tier tier = Tier::CASUAL;
    bool healer = false;
    {
        std::lock_guard<std::mutex> guard(g_lock);
        auto const record = g_records.find(bg->GetInstanceID());
        if (record == g_records.end())
            return nullptr;
        Member* me = FindMember(record->second.team[TeamIndexOf(bot)], bot->GetGUIDLow());
        if (!me)
            return nullptr;
        role = me->role;
        tier = me->tier;
        healer = me->healer;
    }

    // Good rule: the escorts peel what is hitting the carrier. A healer in the escort slot heals instead.
    if (healer || !IsStrikeRole(role) || !BotSkillMath::Uses(tier, Tier::GOOD))
        return nullptr;

    Unit* best = nullptr;
    float bestDist = 0.0f;
    for (Unit* attacker : carrier->getAttackers())
    {
        if (!attacker || !attacker->IsInWorld() || !attacker->IsAlive() || attacker->GetMapId() != bot->GetMapId())
            continue;

        float const dist = bot->GetDistance(attacker);
        if (dist > PEEL_RANGE || (best && dist >= bestDist) || !bot->IsValidAttackTarget(attacker) ||
            !bot->CanSeeOrDetect(attacker) || !bot->IsWithinLOSInMap(attacker))
            continue;

        best = attacker;
        bestDist = dist;
    }
    return best;
}

bool RefreshDynamic(Battleground* bg, Player* bot, Objective& out)
{
    if (!CommonGuards(bg, bot))
        return false;

    uint32 const now = getMSTime();
    std::lock_guard<std::mutex> guard(g_lock);
    auto const record = g_records.find(bg->GetInstanceID());
    if (record == g_records.end())
        return false;

    Record& r = record->second;
    Member* me = FindMember(r.team[TeamIndexOf(bot)], bot->GetGUIDLow());
    if (!me || !me->dynamic || (me->lastDynamicMs && now - me->lastDynamicMs < 1500))
        return false;

    me->lastDynamicMs = now ? now : 1;
    View v;
    MakeView(v, r, bg, bot, me);
    Pos target;
    uint64 follow = 0;
    if (!Resolve(me->kind, v, target, follow))
        return false;

    out.x = target.x;
    out.y = target.y;
    out.z = target.z;
    out.kind = me->kind;
    out.role = me->role;
    out.followGuid = follow;
    return true;
}

float SpeedBuffRange(Battleground* bg, Player* bot)
{
    if (!CommonGuards(bg, bot))
        return 0.0f;

    std::lock_guard<std::mutex> guard(g_lock);
    auto const record = g_records.find(bg->GetInstanceID());
    if (record == g_records.end())
        return 0.0f;

    Record& r = record->second;
    TeamPlan& team = r.team[TeamIndexOf(bot)];
    Member* me = FindMember(team, bot->GetGUIDLow());
    if (!me || !IsStrikeRole(me->role) || !team.lowBracket || !r.started || r.playMs >= LOW_BRACKET_SPEED_MS)
        return 0.0f;

    return LOW_BRACKET_SPEED_RANGE;
}

uint32 HealPriority(Player* healer, Player* target)
{
    if (!healer || !target || !healer->InBattleground() || !MapOf(healer->GetMapId()))
        return 0;

    Battleground* bg = healer->GetBattleground();
    if (!bg || !BotSkill::Uses(healer, Tier::GOOD))
        return 0;

    // Good rule: the carrier of the healer's team (the holder of the other team's flag) is healed first.
    int32 const other = TeamIndexOf(healer) == uint32(TEAM_ALLIANCE) ? int32(TEAM_HORDE) : int32(TEAM_ALLIANCE);
    return bg->GetFlagPickerGUID(other) == target->GetGUID() ? HEAL_PRIORITY_POINTS : 0;
}

bool GraveyardHop(Battleground* bg, Player* bot, Objective const& objective, bool carrying, Objective& hop)
{
    if (!CommonGuards(bg, bot))
        return false;

    Side const& own = MapOf(bg->GetMapId())->sides[TeamIndexOf(bot)];
    // The live position: the roster's is up to two seconds old.
    float const bx = bot->GetPositionX();
    float const by = bot->GetPositionY();
    if (!NeedsGraveyardExitHop(Dist2d(bx, by, own.mainGraveyard.x, own.mainGraveyard.y),
                               Dist2d(bx, by, objective.x, objective.y),
                               Dist2d(objective.x, objective.y, own.tunnelExit.x, own.tunnelExit.y), carrying))
        return false;

    Pos const exit = Jitter(bot, own.tunnelExit, GY_EXIT_JITTER);
    hop = objective;
    hop.x = exit.x;
    hop.y = exit.y;
    hop.z = exit.z;
    return true;
}

bool MustLeaveGraveyard(Battleground* bg, Player* bot)
{
    if (!CommonGuards(bg, bot))
        return false;

    uint32 const now = getMSTime();
    std::lock_guard<std::mutex> guard(g_lock);
    auto const record = g_records.find(bg->GetInstanceID());
    if (record == g_records.end())
        return false;

    uint32 const t = TeamIndexOf(bot);
    Record& r = record->second;
    TeamPlan& team = r.team[t];
    Member* me = FindMember(team, bot->GetGUIDLow());
    if (!me)
        return false;

    Pos const gy = TpPlan::EnemyRespawnPoint(record->second, *MapOf(bg->GetMapId()), t);
    bool const inside = Dist2d(bot->GetPositionX(), bot->GetPositionY(), gy.x, gy.y) <= GY_ZONE_RADIUS;
    uint32 const dwell = me->dwell.Update(inside, now);
    bool const efcOnLane = team.efcSeen && Dist2d(team.efcPos, gy) <= EFC_LANE_RADIUS;
    bool const must = GyMustLeave(dwell, efcOnLane);
    me->mustLeaveGy = must;
    if (!must)
        return false;

    // Ask for a new objective now and then, not at every tick.
    if (me->gyResetMs && now - me->gyResetMs < 5000)
        return false;
    me->gyResetMs = now ? now : 1;
    SF_LOG_DEBUG("playerbots", "[BGWSG] %s stayed %u ms at the enemy graveyard: new objective", bot->GetName().c_str(), dwell);
    return true;
}

bool DeliverCalls(PlayerbotAI* botAI)
{
    if (!g_pendingTotal.load(std::memory_order_relaxed) || !botAI)
        return false;

    Player* bot = botAI->GetBot();
    if (!bot || !bot->InBattleground() || !MapOf(bot->GetMapId()))
        return false;

    uint32 const now = getMSTime();
    Pending call;
    uint32 const instance = bot->GetBattlegroundId();
    uint32 const t = TeamIndexOf(bot);
    {
        std::lock_guard<std::mutex> guard(g_lock);
        auto const record = g_records.find(instance);
        if (record == g_records.end())
            return false;

        Record& r = record->second;
        auto itr = std::find_if(r.pending.begin(), r.pending.end(),
                                [&](Pending const& p) { return p.guid == bot->GetGUIDLow(); });
        if (itr == r.pending.end() || int32(now - itr->dueMs) < 0)
            return false;

        if (!r.team[t].limiter.Allow(itr->guid, itr->kind, now))
        {
            // Too soon after another line of the team: try again, or drop the line when it got stale.
            if (now - itr->dueMs > STALE_CALL_MS)
            {
                r.pending.erase(itr);
                --g_pendingTotal;
            }
            return false;
        }

        call = *itr;
        r.pending.erase(itr);
        --g_pendingTotal;
    }

    CallText const& text = TextOf(call.kind);
    std::string const fallback = text.lines[(call.seq + call.guid) % text.lines.size()];
    std::map<std::string, std::string> placeholders = {
        { "%place", call.place }, { "%class", call.cls }, { "%score", call.score },
        { "%n", call.number }, { "%min", call.minutes }, { "%stance", call.stance }
    };
    bool const sent = BotVoice::Say(botAI, text.key, fallback, placeholders, BotVoice::Channel::BATTLEGROUND,
                                    BotVoice::Priority::NOTICE, 100, nullptr, true);
    if (!sent)
        return false;

    std::lock_guard<std::mutex> guard(g_lock);
    auto const record = g_records.find(instance);
    if (record != g_records.end())
        record->second.team[t].limiter.Commit(call.guid, call.kind, now);

    // BT-483: what the line said is what the side now knows (from a bot's call, with the weight of one).
    if (call.observe)
        if (PvpMatch::Handle const match = PvpMatch::Find(instance))
        {
            PvpObs::Observation seen;
            seen.kind = call.obsKind;
            seen.provenance = PvpObs::Provenance::BOT_CALL;
            seen.place = call.obsPlace;
            seen.count = call.obsCount;
            seen.senderLow = call.guid;
            match->Side(t).observations.Add(seen, now, PvpConfig::ReaderLimits());
        }
    SF_LOG_DEBUG("playerbots", "[BGWSG-CALL] %s says %s (%s)", bot->GetName().c_str(), text.key, fallback.c_str());
    return true;
}

void Forget(uint32 instanceId)
{
    std::lock_guard<std::mutex> guard(g_lock);
    auto const record = g_records.find(instanceId);
    if (record == g_records.end())
        return;

    uint32 const pending = uint32(record->second.pending.size());
    g_pendingTotal -= std::min(pending, g_pendingTotal.load());
    g_records.erase(record);
}
}  // namespace WsgPlan
