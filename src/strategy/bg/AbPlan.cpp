/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE). See AbPlan.h for what this is.
 */

#include "AbPlan.h"

// DatabaseEnv.h must precede the battleground headers (see BattleGroundTactics.cpp).
#include "DatabaseEnv.h"

#include "AiPvpDecision.h"
#include "BgCalloutPolicy.h"
#include "BgLeaderAdapters.h"
#include "BgScoutPolicy.h"
#include "BotSkill.h"
#include "BotVoice.h"
#include "Battleground.h"
#include "BattlegroundAB.h"
#include "BattlegroundBFG.h"
#include "BattlegroundEY.h"
#include "BgNodeMap.h"
#include "Config.h"
#include "EyFlagLogic.h"
#include "EyPlanLogic.h"
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
#include "Spell.h"
#include "SpellInfo.h"
#include "Timer.h"
#include "WorldSession.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace Ab = AbLogic;
using Tier = BotSkillMath::Tier;

namespace
{
// ---------------------------------------------------------------------------------------------------------------------
// Map data and tuning. The node, buff and graveyard positions are the core's own (AbPlanLogic.h NODE_POS); everything
// here is a distance, a radius or a time of the playbook.
// ---------------------------------------------------------------------------------------------------------------------
// The maps this plan plays: Arathi Basin and Battle for Gilneas (BgNodeMap.h). BT-551 made the plan generic over the node map; the
// second map is switched on with its rules (BT-552).
// Spell 21651, the capture banner cast (BattlegroundAB.cpp:398 checks a click within 10 yd).
constexpr uint32 CAPTURE_SPELL_ID = 21651;
// A caster stands within this distance of the banner it works on.
constexpr float CAST_NODE_RADIUS = 12.0f;
// Player counts near a banner ignore players this far above or below it (the Lumber Mill plateau is 56 yd above the roads).
constexpr float NEAR_MAX_DZ = 20.0f;
constexpr float NEAR_40 = 40.0f;
constexpr float NEAR_60 = Ab::RAID_STAY_CLEAR_YARDS;
// Posts: a ring around the banner. The ground under a post must lie within POST_MAX_DZ of the banner, else the post
// is the banner itself (an edge of the Lumber Mill plateau, a wall of the Gold Mine pit).
constexpr float POST_RING_MIN = 3.0f;
constexpr float POST_RING_MAX = Ab::POST_RADIUS;
constexpr float POST_MAX_DZ = 5.0f;
constexpr float POST_PROBE_LIFT = 2.0f;
// Eye of the Storm: alive bots needed to send a group at the fourth base (6 guards at three bases and 4 free, sheet section 6).
constexpr uint32 EYE_FOURTH_MIN_ALIVE = 10;
// The carrier decision's surroundings (the Warsong numbers, WsgPlanLogic.h CARRIER_ALONE_RADIUS and ESCORT_DISTANCE): the enemies
// that count as near and the fighters that count as an escort.
constexpr float CARRIER_ENEMY_RADIUS = 35.0f;
constexpr float CARRIER_ESCORT_RADIUS = 12.0f;
constexpr float CAPTURE_STANDOFF = 3.0f;       // the bot stands this far from the banner to click it (not on the pole)
constexpr float STAGE_RING_MIN = 12.0f;
constexpr float STAGE_RING_SPAN = 8.0f;
constexpr float LOOSE_RING_MIN = 10.0f;        // a casual guard's post: anywhere inside the wider leash
constexpr float LOOSE_RING_SPAN = 12.0f;
constexpr float AT_POST_YARDS = 6.0f;
constexpr float TWO_PI = 6.2831853f;
constexpr float NODE_ANGLE_STEP = 0.7f;

// Capper hunting (G7): reach of the guards and the bots near the banner, and the healer's head start in percent points.
constexpr float CAPPER_REACH = 40.0f;
constexpr uint32 CAPTURE_HEAL_POINTS = 25;
constexpr uint32 CARRIER_HEAL_POINTS = 30;
constexpr uint32 ATTACKED_HEAL_POINTS = 10;
constexpr float HEAL_NEAR_BANNER = 25.0f;

constexpr uint32 LOG_REPEAT_MS = 30000;
constexpr uint32 STALE_CALL_MS = 4000;
constexpr uint32 PENDING_MAX_AGE_MS = 15000;
constexpr uint32 INCOMING_REPEAT_MS = 20000;
constexpr uint32 OMW_REPEAT_MS = 20000;
// A "help" with no node is read at the node this close to where the sender stands (the same reach as the 60 yard count).
constexpr float HELP_NODE_RADIUS = 60.0f;
constexpr uint32 OPENING_CALL_MS = 3000;
constexpr uint32 SCORE_CALL_LOW = 1000;

struct NodeInfo
{
    int bar = EyLogic::BAR_START;   // Eye of the Storm: the capture bar (BattlegroundEY::GetPointBarStatus)
    uint8 state = 0;
    uint8 prev = 0;
    uint32 changedMs = 0;
    bool seen = false;
};

struct Capper
{
    uint64 guid = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    uint8 node = Ab::NODE_NONE;
};

struct Member
{
    uint32 guid = 0;
    uint64 full = 0;
    bool bot = false;
    bool alive = false;
    bool present = false;
    bool seenByEnemy = false;
    uint8 cls = 0;
    uint8 level = 0;
    bool healer = false;
    bool tank = false;
    bool melee = false;
    bool feral = false;
    Tier tier = Tier::GOOD;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float hpPercent = 100.0f;
    bool casting = false;
    uint8 castNode = Ab::NODE_NONE;

    bool assigned = false;
    Ab::Slot slot;
    uint32 groupIndex = 0;
    uint32 groupSize = 1;

    // Marks set at each refresh.
    bool restoreResponder = false;
    uint8 restoreNode = Ab::NODE_NONE;
    bool incResponder = false;
    uint8 incNode = Ab::NODE_NONE;
    bool supportHuman = false;
    uint8 supportNode = Ab::NODE_NONE;
    uint8 raidNode = Ab::NODE_NONE;
    bool raidStay = false;

    // What the bot chose last.
    uint32 seenEpoch = 0;
    Ab::Kind kind = Ab::Kind::HOLD_QUIET;
    uint8 optNode = Ab::NODE_NONE;
    bool hold = false;
    bool hasTarget = false;
    float tx = 0.0f;
    float ty = 0.0f;
    float tz = 0.0f;
    uint32 lastLogMs = 0;
    Ab::Kind lastLogKind = Ab::Kind::HOLD_QUIET;

    // BT-481: refreshes a casual bot still keeps its old slot after a stance change.
    uint32 keepSlot = 0;

    // BT-552 (Battle for Gilneas): the rally node of a losing group, the enemy node of the ghost pair.
    uint8 retreatNode = Ab::NODE_NONE;
    uint8 ghostNode = Ab::NODE_NONE;
};

// The retreat of a losing group and the ghost pair of a team (one of each at a time).
struct FightState
{
    bool retreating = false;
    uint8 from = Ab::NODE_NONE;
    uint8 rally = Ab::NODE_NONE;
    uint32 retreatSinceMs = 0;
    uint32 retreatCooldownUntilMs = 0;
    bool retreatCallDue = false;
    uint32 retreatSpeaker = 0;

    uint8 reinforceNode = Ab::NODE_NONE;   // D13 answer "reinforce": the reserve answers this node as if it were called

    bool ghosting = false;
    uint8 ghostTarget = Ab::NODE_NONE;
    uint32 ghostSinceMs = 0;
    bool ghostCallDue = false;
    uint32 ghostSpeaker = 0;
};

struct TeamPlan
{
    Ab::Map const* map = &BgNodeMap::ARATHI;
    BgPlan::Plan plan = BgPlan::Plan::BALANCED;          // the plan the team plays: the rule's, or a decision layer's stance (BT-503)
    Ab::Regime regime = Ab::Regime::EVEN;
    BgPlan::Plan rulePlan = BgPlan::Plan::BALANCED;      // the deterministic plan and regime: what the rules say, the hysteresis' memory
    Ab::Regime ruleRegime = Ab::Regime::EVEN;
    uint32 leader = 0;                       // BT-481: the leader bot of this side (guid low), 0 for none
    BgLeader::Stance stance = BgLeader::Stance::BALANCED;
    bool stanceCallDue = false;
    std::unordered_map<uint32, Member> members;
    uint32 score = 0;
    uint32 prevScore = 0;
    unsigned occupied = 0;
    unsigned prevOccupied = 0;
    Ab::NodeSituation sit[Ab::NODE_COUNT];
    Ab::NodeSituation prevSit[Ab::NODE_COUNT];
    Ab::NodeSituation sitState[Ab::NODE_COUNT];   // the situation from the node state alone (the plan epoch reads this one)
    bool hardened[Ab::NODE_COUNT];                // Eye of the Storm: our bar at the node is far from the enemy's reach
    uint32 captureAgeMs[Ab::NODE_COUNT];
    uint32 enemyNear40[Ab::NODE_COUNT];
    uint32 enemyNear50[Ab::NODE_COUNT];
    uint32 enemyNear60[Ab::NODE_COUNT];
    uint32 friendNear40[Ab::NODE_COUNT];
    uint8 assaultNode[2];                  // [0] good bots, [1] expert bots (the flip limiter applies to them)
    uint8 prevAssaultNode[2];
    uint8 nearestNode = Ab::NODE_NONE;
    uint8 closeOutNode = Ab::NODE_NONE;
    uint8 stageNode = BgNodeMap::ARATHI.hub;
    bool gathered[Ab::NODE_COUNT];
    uint32 gatherSinceMs[Ab::NODE_COUNT];
    bool humanActive = false;
    uint8 humanNode = Ab::NODE_NONE;
    Ab::CallVerb humanVerb = Ab::CallVerb::INC;
    uint32 humanExpiresMs = 0;
    uint32 humanReceivedMs = 0;      // BT-482: when the line was read (the roll of the bots that may ignore it)
    uint32 humanCount = 0;           // the number the human said, 0 for none
    std::vector<PvpObs::Observation> inbox;   // BT-482: the human calls read since the last refresh
    std::vector<Capper> cappers;
    std::vector<Ab::Slot> slots;
    Ab::CallLimiter limiter;
    bool lowBracket = false;
    bool hasHealer = false;
    uint32 size = 0;
    uint32 humans = 0;
    uint32 lastIncomingMs[Ab::NODE_COUNT];
    uint32 lastOmwMs[Ab::NODE_COUNT];
    bool openingCalled = false;
    bool fourthCalled = false;
    bool lowScoreCalled = false;
    bool highScoreCalled = false;
    uint32 lastStateLogMs = 0;
    FightState fight;
    // BT-585 (Eye of the Storm): the decision layer's answers to D15 to D17 that still stand, -1 for none.
    int flagAction = -1;
    int efcAction = -1;
    int carrierBase = -1;

    TeamPlan()
    {
        for (uint8 n = 0; n < Ab::NODE_COUNT; ++n)
        {
            sit[n] = Ab::NodeSituation::NEUTRAL;
            prevSit[n] = Ab::NodeSituation::NEUTRAL;
            sitState[n] = Ab::NodeSituation::NEUTRAL;
            hardened[n] = true;
            captureAgeMs[n] = 0;
            enemyNear40[n] = enemyNear50[n] = enemyNear60[n] = friendNear40[n] = 0;
            gathered[n] = false;
            gatherSinceMs[n] = 0;
            lastIncomingMs[n] = 0;
            lastOmwMs[n] = 0;
        }
        assaultNode[0] = assaultNode[1] = Ab::NODE_NONE;
        prevAssaultNode[0] = prevAssaultNode[1] = Ab::NODE_NONE;
    }
};

struct Pending
{
    uint32 guid = 0;
    Ab::Call kind = Ab::Call::INCOMING;
    uint32 dueMs = 0;
    uint32 seq = 0;
    std::string node;
    std::string number;
    std::string score;
    std::string stance;
    bool behind = false;
    // BT-483: what the line tells the team, kept as an observation of the side when it is said.
    bool observe = false;
    PvpObs::Kind obsKind = PvpObs::Kind::INCOMING;
    uint8 obsPlace = PvpObs::PLACE_UNKNOWN;
    uint8 obsCount = 0;
};

struct Record : BgPlan::RecordBase
{
    Ab::Map const* map = &BgNodeMap::ARATHI;
    uint32 stanceEpoch = 0;   // BT-481: the epoch a stance change made; a casual bot adopts that one a refresh later
    NodeInfo nodes[Ab::NODE_COUNT];
    bool nodeChanged[Ab::NODE_COUNT] = { false, false, false, false, false };
    Ab::FlipLimiter flips;
    std::vector<Pending> pending;
    TeamPlan team[2];
};

BgPlan::RecordTable<Record> g_table;
std::mutex& g_lock = g_table.lock;
std::unordered_map<uint32, Record>& g_records = g_table.records;
std::atomic<uint32> g_pendingTotal{ 0 };

// AiPlayerbot.BattlegroundChat.Enable / .Replies, read once like the Warsong Gulch plan does (the same settings).
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

float Dist2d(float ax, float ay, float bx, float by) { return Ab::Dist2d(ax, ay, bx, by); }

bool NearNode(Ab::Map const& map, Member const& m, uint8 node, float radius)
{
    // Eye of the Storm: a node's players are those inside the core's own 70 yd circle (3D), whatever radius is asked.
    if (Ab::IsPresence(map))
        return Ab::InCircle(node, m.x, m.y, m.z, map);
    return node < map.nodeCount && Dist2d(m.x, m.y, map.pos[node].x, map.pos[node].y) <= radius &&
           std::fabs(m.z - map.pos[node].z) <= NEAR_MAX_DZ;
}

// The score at which the leader says the second score line: the core's own warning (200 below the limit).
uint32 ScoreCallHigh(Ab::Map const& map)
{
    return map.maxScore - 200;
}

// The battleground's node state (0 neutral, 1 and 2 contested, 3 and 4 occupied), through the core's read-only getter.
uint8 NodeStatusOf(Battleground* bg, uint8 node)
{
    switch (bg->GetMapId())
    {
        case 529: return static_cast<BattlegroundAB*>(bg)->GetNodeStatus(node);
        case 761: return static_cast<BattlegroundBFG*>(bg)->GetNodeStatus(node);
        case 566:
        {
            // Eye of the Storm has no contest states: neutral (0), held by the Alliance (3) or by the Horde (4).
            BattlegroundEY* const ey = static_cast<BattlegroundEY*>(bg);
            if (ey->GetPointState(node) != EY_POINT_UNDER_CONTROL)
                return Ab::STATE_NEUTRAL;
            return ey->GetPointOwnedByTeam(node) == ALLIANCE ? uint8(3) : uint8(4);
        }
        default: break;
    }
    return Ab::STATE_NEUTRAL;
}

char const* TierWord(Tier tier)
{
    return tier == Tier::EXPERT ? "expert" : tier == Tier::GOOD ? "good" : "casual";
}

Member* FindMember(TeamPlan& team, uint32 guid)
{
    auto const itr = team.members.find(guid);
    return itr == team.members.end() ? nullptr : &itr->second;
}

// A free bot of the plan: the middle group (an assault bot with no node) and the reserve. A guard, a raider, a squad that is taking a
// base and a bot in a retreat or a ghost pair are somewhere else for a reason.
bool IsFreeMember(Member const& m)
{
    return m.bot && m.alive && m.assigned && m.retreatNode == Ab::NODE_NONE && m.ghostNode == Ab::NODE_NONE &&
           (m.slot.job == Ab::Job::MOBILE || (m.slot.job == Ab::Job::ASSAULT && m.slot.node == Ab::NODE_NONE));
}

// ---------------------------------------------------------------------------------------------------------------------
// Call-outs. The in-code lines are the fallback when the text table has no row of the name
// (sql/.../2026_11_12_03.sql adds them).
// ---------------------------------------------------------------------------------------------------------------------
struct CallText
{
    std::string key;
    std::vector<char const*> lines;
};

// The lines of one map: the rows of the text table are `<prefix>opening`, `<prefix>inc`, ... (sql/.../2026_11_12_03.sql adds the
// Arathi Basin ones, 2026_11_19_01.sql the Battle for Gilneas ones). Rows 0 to 8 follow AbLogic::Call up to the answer. Row 9
// is the "behind" variant of the score line and row 10 the stance line, so the enum value of STANCE (9) is not its row: both are
// picked by name below.
std::vector<CallText> BuildTexts(Ab::Map const& map)
{
    std::string const prefix = map.voicePrefix;
    bool const bfg = map.kind == BgNodeMap::Kind::GILNEAS;
    bool const eye = map.kind == BgNodeMap::Kind::EYE;
    return {
        { prefix + "opening", { "%node group", "going %node", "%node crew here" } },
        { prefix + "inc", { "inc %node %num", "%node %num inc", "inc %node need help" } },
        { prefix + "assaulted", { "%node assaulted, restore", "%node under attack", "they are capping %node" } },
        { prefix + "omw", { "omw %node", "on my way %node" } },
        { prefix + "back", { "%node back", "got %node", "%node is ours" } },
        { prefix + "lost", { "lost %node", "%node gone", "they took %node" } },
        { prefix + "fourth", eye ? std::vector<char const*>{ "take %node, 4 bases and the flag end it", "we need the fourth", "one more base and we win" } :
                              bfg ? std::vector<char const*>{ "take %node, 3 nodes ends it", "we need the third", "one more node and we win" } :
                                    std::vector<char const*>{ "take %node, 5 nodes ends it", "we need a 4th", "one more node and we win" } },
        { prefix + "score_ahead", { "%score, we are ahead", "%score ahead, hold what we have" } },
        { prefix + "answer", { "omw", "coming" } },
        { prefix + "score_behind", { "%score, flip a node", "%score behind, they have %num" } },
        { prefix + "stance", { "%stance", "%stance, stay tight" } },
        { prefix + "retreat", { "falling back to %node", "back to %node", "%node, regroup" } },
        { prefix + "ghost", { "ghost %node", "inking %node", "sneaking to %node" } },
    };
}

CallText const& TextOf(Pending const& p, Ab::Map const& map)
{
    static std::vector<CallText> const arathi = BuildTexts(BgNodeMap::ARATHI);
    static std::vector<CallText> const gilneas = BuildTexts(BgNodeMap::GILNEAS);
    static std::vector<CallText> const eye = BuildTexts(BgNodeMap::EYE);
    std::vector<CallText> const& texts = map.kind == BgNodeMap::Kind::EYE ? eye : map.kind == BgNodeMap::Kind::GILNEAS ? gilneas : arathi;
    static size_t const behindIndex = 9;
    static size_t const stanceIndex = 10;
    if (p.kind == Ab::Call::SCORE && p.behind)
        return texts[behindIndex];
    if (p.kind == Ab::Call::STANCE)
        return texts[stanceIndex];
    if (p.kind == Ab::Call::RETREAT)
        return texts[11];
    if (p.kind == Ab::Call::GHOST)
        return texts[12];
    size_t const index = static_cast<size_t>(p.kind);
    return texts[index < behindIndex ? index : 0];
}

// Marks a line as telling the team something: when it is said it becomes an observation of the side (BT-483).
void Observe(Pending& line, PvpObs::Kind kind, uint8 node, uint32 count)
{
    line.observe = true;
    line.obsKind = kind;
    line.obsPlace = node;
    line.obsCount = static_cast<uint8>(std::min<uint32>(count, PvpObs::MAX_REPORTED_COUNT));
}

// True when a line was queued. `forced` skips the tier's chance (a leader always says its stance); the limiter and the
// tier's delay still apply.
bool Enqueue(Record& r, TeamPlan& team, Member const& speaker, Ab::Call kind, uint32 nowMs, Pending base, bool forced = false)
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

// Who says the team-wide lines (the time, the score, the fifth node): the leader, and with no leader the expert bot with the
// lowest GUID hash (BT-481; before it was the expert alone). nullptr when neither exists.
Member* AnnouncerOf(TeamPlan& team)
{
    auto const leader = team.members.find(team.leader);
    if (team.leader && leader != team.members.end() && leader->second.bot && leader->second.alive)
        return &leader->second;

    Member* speaker = nullptr;
    for (auto& entry : team.members)
    {
        Member& m = entry.second;
        if (m.bot && m.alive && m.tier == Tier::EXPERT &&
            (!speaker || BotSkillMath::Mix(m.guid) < BotSkillMath::Mix(speaker->guid)))
            speaker = &m;
    }
    return speaker;
}

// The nearest alive bot of a team to a node banner; mode 0 any, 1 only guards of that node, 2 only the reserve. A heedStamp
// (the time a human's line was read) leaves out the casual bots that ignore that line.
Member* NearestBot(TeamPlan& team, uint8 node, int mode, uint32 heedStamp = 0)
{
    Member* best = nullptr;
    float bestDist = 0.0f;
    for (auto& entry : team.members)
    {
        Member& m = entry.second;
        if (!m.bot || !m.alive || !m.present)
            continue;
        if (mode == 1 && !(m.assigned && m.slot.job == Ab::Job::GUARD && m.slot.node == node))
            continue;
        if (mode == 2 && !(m.assigned && m.slot.job == Ab::Job::MOBILE))
            continue;
        if (heedStamp && !BgCallout::Heeds(m.tier, m.guid, heedStamp))
            continue;

        float const d = Ab::DistToNode(node, m.x, m.y, *team.map);
        if (!best || d < bestDist || (d == bestDist && m.guid < best->guid))
        {
            best = &m;
            bestDist = d;
        }
    }
    return best;
}

// The nearest alive expert bot within sight of a node's banner (an expert says what it saw), nullptr when there is none.
Member* NearestExpert(TeamPlan& team, uint8 node)
{
    Member* best = nullptr;
    float bestDist = BgScout::SCOUT_SIGHT_YARDS;
    for (auto& entry : team.members)
    {
        Member& m = entry.second;
        if (!m.bot || !m.alive || !m.present || m.tier != Tier::EXPERT)
            continue;

        float const d = Ab::DistToNode(node, m.x, m.y, *team.map);
        if (d <= bestDist && (!best || d < bestDist || m.guid < best->guid))
        {
            best = &m;
            bestDist = d;
        }
    }
    return best;
}

Member* SpeakerFor(TeamPlan& team, uint8 node)
{
    Member* speaker = NearestBot(team, node, 1);
    return speaker ? speaker : NearestBot(team, node, 0);
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
        case BgLeader::Stance::HOLD: return "hold what we have";
        case BgLeader::Stance::PUSH: return "push, flip a node";
        case BgLeader::Stance::LAST_NODE: return "hold the last node";
        case BgLeader::Stance::PANIC: return "panic, retake";
        default: break;
    }
    return "balanced";
}

// The roll call of one side for the election: every member, humans included (they are never eligible).
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

// The leader of side t confirms the stance of the rule's plan and regime (or applies a decision layer's answer: BT-503) and, after
// a stance change, sets the drift of the casual bots. Returns true when the stance changed after the gates opened.
bool LeaderTurn(Record& r, PvpMatch::State& match, uint32 t, TeamPlan& team, uint32 nowMs)
{
    BgLeader::LeaderState& leader = match.Side(t).leader;
    BgLeader::AbPlanState const rule{ team.rulePlan, team.ruleRegime };
    if (!PvpConfig::LeaderEnabled())
    {
        team.leader = 0;
        team.stance = BgLeader::StanceOf(rule.plan, rule.regime);
        team.stanceCallDue = false;
        return false;
    }
    BgLeader::AbStanceFacts const facts{ team.occupied, r.team[1 - t].occupied };
    // The answers of the decision layer are judged inside the tick, against the list this refresh just published.
    BgLeader::TickResult const tick = BgLeader::TickWith(leader, LeaderCandidates(team), BgLeader::StanceOf(rule.plan, rule.regime),
                                                         BgLeader::StanceOffer(rule.plan, rule.regime, facts), nowMs, r.started,
                                                         [&](BgLeader::LeaderState& state) { AiPvpDecision::Receive(match, t, state, nowMs); });
    team.leader = tick.leader;
    team.stance = tick.stance;
    team.stanceCallDue = tick.callDue;

    if (tick.stance != BgLeader::StanceOf(rule.plan, rule.regime))
    {
        BgLeader::AbPlanState const applied = BgLeader::Apply(tick.stance, rule);
        team.plan = applied.plan;
        team.regime = applied.regime;
    }

    if (tick.change != BgLeader::Change::NONE || tick.stanceChanged)
        SF_LOG_DEBUG("playerbots", "[BGAB-LEAD] inst=%u gen=%u %s leader=%u change=%u stance=%s epoch=%u version=%u",
                     r.instanceId, match.Key().generation, t == uint32(TEAM_ALLIANCE) ? "A" : "H", tick.leader,
                     uint32(tick.change), BgLeader::StanceName(tick.stance), leader.election.epoch, leader.stanceVersion);

    if (!tick.stanceChanged || !r.started)
        return false;

    for (auto& entry : team.members)
    {
        Member& m = entry.second;
        m.keepSlot = m.bot && m.guid != tick.leader ? BgLeader::RoleKeepRefreshes(m.tier, m.guid, leader.stanceSeq) : 0;
    }
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// What the decision layer is told about the match (BT-503): game context only, every number from the record the plan keeps.
// ---------------------------------------------------------------------------------------------------------------------
// The longest eta worth telling the model (a team that scores nothing has none).
constexpr int32 ETA_REPORT_CAP_S = 3600;

void DescribeMatch(AwareAiBgState& state, Record const& r, PvpMatch::State& match, uint32 t, uint32 nowMs)
{
    TeamPlan const& team = r.team[t];
    TeamPlan const& enemy = r.team[1 - t];

    state.Map = r.map->mapName;
    state.ScoreOurs = team.score;
    state.ScoreTheirs = enemy.score;
    state.ScoreLimit = r.map->maxScore;
    for (uint8 n = 0; n < r.map->nodeCount; ++n)
        state.Nodes.push_back(std::string(BgLeader::NodeName(n, *r.map)) + ":" + Ab::SituationName(team.sit[n]));

    // Our bots by job, the guards by node: where the team stands.
    uint32 guards[Ab::NODE_COUNT] = {};
    uint32 jobs[4] = {};
    uint32 alive = 0;
    for (auto const& entry : team.members)
    {
        Member const& m = entry.second;
        if (!m.bot || !m.alive || !m.assigned)
            continue;
        ++alive;
        ++jobs[uint8(m.slot.job)];
        if (m.slot.job == Ab::Job::GUARD && m.slot.node < r.map->nodeCount)
            ++guards[m.slot.node];
    }
    for (uint8 n = 0; n < r.map->nodeCount; ++n)
        if (guards[n])
            state.FriendlyGroups.push_back(std::string("guards_") + BgLeader::NodeName(n, *r.map) + ":" + std::to_string(guards[n]));
    for (Ab::Job job : { Ab::Job::ASSAULT, Ab::Job::RAIDER, Ab::Job::MOBILE })
        if (jobs[uint8(job)])
            state.FriendlyGroups.push_back(std::string(Ab::JobName(job)) + ":" + std::to_string(jobs[uint8(job)]));

    // What call-outs reported (the side's observations: humans' lines and our scouts), weighed by the store.
    PvpObs::SideStore const& store = match.Side(t).observations;
    for (uint8 n = 0; n < r.map->nodeCount; ++n)
        if (uint32 const reported = PvpObs::ReportedEnemies(store.items, n, nowMs))
            state.ObservedPressure.push_back(std::string(BgLeader::NodeName(n, *r.map)) + ":" + std::to_string(reported));

    state.Facts.push_back(std::string("race_regime:") + Ab::RegimeName(team.ruleRegime));
    state.Facts.push_back("nodes_held:" + std::to_string(team.occupied) + " against " + std::to_string(enemy.occupied));
    state.Facts.push_back("eta_to_win_s:ours=" + std::to_string(std::min(ETA_REPORT_CAP_S, Ab::EtaSeconds(team.score, team.occupied, *r.map))) +
                          ",theirs=" + std::to_string(std::min(ETA_REPORT_CAP_S, Ab::EtaSeconds(enemy.score, enemy.occupied, *r.map))));
    state.Facts.push_back("our_bots_alive:" + std::to_string(alive) + "/" + std::to_string(team.size));
}

// A change of this is a node event: a node was clicked, contested, captured or lost.
uint64 NodeEventSignature(TeamPlan const& team)
{
    uint64 h = 1469598103934665603ull;
    for (uint8 n = 0; n < team.map->nodeCount; ++n)
        h = (h ^ uint64(team.sit[n])) * 1099511628211ull;
    return h;
}

// ---------------------------------------------------------------------------------------------------------------------
// The record.
// ---------------------------------------------------------------------------------------------------------------------
Record& RecordOf(uint32 instanceId, uint32 nowMs, Ab::Map const& map)
{
    // Old records of matches that ended without the leave hook are dropped when a new one starts.
    Record& r = g_table.Of(instanceId, nowMs, [](Record const& old)
    {
        g_pendingTotal -= std::min<uint32>(g_pendingTotal.load(), uint32(old.pending.size()));
    });
    if (r.map != &map)
    {
        // A new record: the map decides how many nodes, which ticks and which words.
        r.map = &map;
        for (TeamPlan& team : r.team)
        {
            team.map = &map;
            team.stageNode = map.hub;
        }
    }
    return r;
}

void ReadMember(Member& m, Player* p, Ab::Map const& map)
{
    m.alive = p->IsAlive();
    m.present = true;
    m.level = p->getLevel();
    m.x = p->GetPositionX();
    m.y = p->GetPositionY();
    m.z = p->GetPositionZ();
    m.hpPercent = p->GetHealthPct();

    // The capture is an 8 second cast: a player in the middle of it is easy to see.
    m.casting = false;
    m.castNode = Ab::NODE_NONE;
    if (m.alive)
    {
        Spell* spell = p->GetCurrentSpell(CURRENT_GENERIC_SPELL);
        if (!spell)
            spell = p->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
        if (spell && spell->m_spellInfo && spell->m_spellInfo->Id == CAPTURE_SPELL_ID)
        {
            m.casting = true;
            m.castNode = Ab::NodeNear(m.x, m.y, CAST_NODE_RADIUS, map);
        }
    }
}

void InitMember(Member& m, Player* p)
{
    m.guid = p->GetGUIDLow();
    m.full = p->GetGUID();
    m.bot = p->GetSession() && p->GetSession()->IsBot();
    m.cls = p->getClass();
    m.healer = PlayerBotSpec::IsHeal(p, true);
    m.tank = PlayerBotSpec::IsTank(p, true);
    m.melee = PlayerBotSpec::IsMelee(p, true);
    m.feral = m.cls == BgPlan::CLS_DRUID && BotGetSpecialization(p) == SPEC_DRUID_FERAL;
    m.tier = m.bot ? BotSkill::TierOf(p) : Tier::EXPERT;
}

uint8 TeamOfIndex(uint32 t) { return static_cast<uint8>(t); }

// Reads the roster: one walk over the map's players.
void ReadRoster(Record& r, Battleground* bg)
{
    for (TeamPlan& team : r.team)
    {
        for (auto& entry : team.members)
            entry.second.present = false;
        team.size = 0;
        team.humans = 0;
        team.hasHealer = false;
        team.lowBracket = false;
    }

    uint8 maxLevel[2] = { 0, 0 };
    std::vector<Player*> players[2];
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
        ReadMember(m, p, *r.map);
        players[t].push_back(p);
        ++team.size;
        if (!m.bot)
            ++team.humans;
        if (m.healer)
            team.hasHealer = true;
        maxLevel[t] = std::max(maxLevel[t], m.level);
    }

    // Cache visibility once for this refresh, using the same roster for every node.
    for (uint32 t = 0; t < 2; ++t)
        for (Player* enemy : players[t])
        {
            Member& member = r.team[t].members.at(enemy->GetGUIDLow());
            member.seenByEnemy = Ab::SeenByTeam(member.alive, players[1 - t],
                [](Player* observer) { return observer->IsAlive(); },
                [enemy](Player* observer) { return Ab::WithinSight(enemy->GetPositionX(), enemy->GetPositionY(), enemy->GetPositionZ(),
                                                                  observer->GetPositionX(), observer->GetPositionY(), observer->GetPositionZ()); },
                [enemy](Player* observer) { return observer->CanSeeOrDetect(enemy); });
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
        r.team[t].lowBracket = maxLevel[t] > 0 && maxLevel[t] < Ab::LOW_BRACKET_BELOW_LEVEL;
}

// The side (0 Alliance, 1 Horde) a node state belongs to, -1 for neutral.
int SideOfState(uint8 state)
{
    return state == 0 || state > Ab::STATE_MAX ? -1 : int((state - 1) % 2);
}

// Reads the node states and notes when each changed and what it was before (the core keeps the previous state private).
void ReadNodes(Record& r, Battleground* bg, uint32 nowMs, bool& changed)
{
    for (uint8 n = 0; n < r.map->nodeCount; ++n)
    {
        NodeInfo& info = r.nodes[n];
        uint8 const state = NodeStatusOf(bg, n);
        r.nodeChanged[n] = false;
        if (Ab::IsPresence(*r.map))
            info.bar = static_cast<BattlegroundEY*>(bg)->GetPointBarStatus(n);
        if (!info.seen)
        {
            info.seen = true;
            info.state = state;
            info.prev = Ab::STATE_NEUTRAL;
            info.changedMs = nowMs;
            continue;
        }
        if (state == info.state)
            continue;

        // The flip limiter counts a change of side at a node (contested or held by the other team).
        int const before = SideOfState(info.state);
        int const after = SideOfState(state);
        if (before >= 0 && ((after >= 0 && before != after) || (after < 0 && Ab::IsPresence(*r.map))))
            r.flips.Record(n, nowMs);   // a neutralised base counts as a flip where there is no contest (Eye of the Storm)

        info.prev = info.state;
        info.state = state;
        info.changedMs = nowMs;
        r.nodeChanged[n] = true;
        changed = true;
    }
}

// The situation of each node for each team, the score and the occupied nodes.
void ReadSituations(Record& r, Battleground* bg, uint32 nowMs)
{
    for (uint32 t = 0; t < 2; ++t)
    {
        TeamPlan& team = r.team[t];
        team.prevScore = team.score;
        team.prevOccupied = team.occupied;
        team.score = bg->GetTeamScore(t);
        team.occupied = 0;
        for (uint8 n = 0; n < r.map->nodeCount; ++n)
        {
            team.prevSit[n] = team.sit[n];
            NodeInfo const& info = r.nodes[n];
            team.sit[n] = Ab::ClassifyFor(*r.map, info.state, info.prev, TeamOfIndex(t));
            team.sitState[n] = team.sit[n];
            team.hardened[n] = !Ab::IsPresence(*r.map) || !Ab::IsOccupiedBy(info.state, TeamOfIndex(t)) ||
                               EyLogic::Hardened(info.bar, TeamOfIndex(t));
            team.captureAgeMs[n] = team.sit[n] == Ab::NodeSituation::CAPPING ? nowMs - info.changedMs : 0;
            if (Ab::IsOccupiedBy(info.state, TeamOfIndex(t)))
                ++team.occupied;
        }
    }
}

// Players near each of our banners: seen enemies (for odds and calls) and our own.
void CountNear(Record& r, PvpMatch::State& match, uint32 nowMs)
{
    for (uint32 t = 0; t < 2; ++t)
    {
        TeamPlan& team = r.team[t];
        TeamPlan& enemy = r.team[1 - t];
        team.cappers.clear();
        auto const isSeen = [](Member const& e) { return e.seenByEnemy; };

        for (uint8 n = 0; n < r.map->nodeCount; ++n)
        {
            team.enemyNear40[n] = team.enemyNear50[n] = team.enemyNear60[n] = team.friendNear40[n] = 0;
            for (auto const& entry : enemy.members)
            {
                Member const& e = entry.second;
                if (!e.alive || !isSeen(e))
                    continue;
                if (NearNode(*r.map, e, n, NEAR_40))
                    ++team.enemyNear40[n];
                if (NearNode(*r.map, e, n, Ab::INC_RADIUS))
                    ++team.enemyNear50[n];
                if (NearNode(*r.map, e, n, NEAR_60))
                    ++team.enemyNear60[n];
            }
            for (auto const& entry : team.members)
                if (entry.second.alive && NearNode(*r.map, entry.second, n, NEAR_40))
                    ++team.friendNear40[n];

            // What the call-outs of the side say is there (a human's line, a scout's), never below what the team sees.
            {
                uint32 const reported = PvpObs::ReportedEnemies(match.Side(t).observations.items, n, nowMs);
                team.enemyNear40[n] = PvpObs::ReconciledEnemies(team.enemyNear40[n], reported);
                team.enemyNear50[n] = PvpObs::ReconciledEnemies(team.enemyNear50[n], reported);
                team.enemyNear60[n] = PvpObs::ReconciledEnemies(team.enemyNear60[n], reported);
            }
        }

        // Enemy players casting the capture at one of the banners we hold.
        for (auto const& entry : enemy.members)
        {
            Member const& e = entry.second;
            if (e.casting && e.castNode < r.map->nodeCount && Ab::IsHeld(team.sit[e.castNode]) && isSeen(e))
                team.cappers.push_back({ e.full, e.x, e.y, e.z, e.castNode });
            // Eye of the Storm (BT-585): there is no cast; the capper is a seen enemy standing in the circle of a base we hold, the
            // player whose presence moves the bar.
            else if (Ab::IsPresence(*r.map) && e.alive && isSeen(e))
                for (uint8 n = 0; n < r.map->nodeCount; ++n)
                    if (Ab::IsHeld(team.sit[n]) && Ab::InCircle(n, e.x, e.y, e.z, *r.map))
                    {
                        team.cappers.push_back({ e.full, e.x, e.y, e.z, n });
                        break;
                    }
        }
    }
}

// The jobs: slots for the size and state of the team, assigned to the alive bots (a bot keeps its slot unless an event
// changes it; a bot in the middle of the capture cast keeps it for sure).
void AssignJobs(Record& r, uint32 t, uint32 nowMs, bool& changed)
{
    TeamPlan& team = r.team[t];

    Ab::SlotInput in;
    in.team = TeamOfIndex(t);
    in.plan = team.plan;
    in.regime = team.regime;
    in.size = team.size;
    in.humans = team.humans;
    for (uint8 n = 0; n < r.map->nodeCount; ++n)
    {
        in.sit[n] = team.sit[n];
        in.enemyNear50[n] = team.enemyNear50[n];
        in.hardened[n] = team.hardened[n];
    }
    in.lowBracket = team.lowBracket;
    in.hasHealer = team.hasHealer;
    in.playMs = r.playMs;
    in.nearerContested = Ab::NearerContested(in.team, r.instanceId);
    in.map = r.map;
    for (auto const& entry : team.members)
    {
        Member const& m = entry.second;
        BgPlan::CandidateBase c;
        c.cls = m.cls;
        c.feral = m.feral;
        if (m.bot && m.alive && BgPlan::IsStealthClass(c))
            ++in.stealthBots;
    }
    team.slots = Ab::BuildSlots(in);

    std::vector<Ab::Candidate> candidates;
    for (auto& entry : team.members)
    {
        Member& m = entry.second;
        if (!m.bot || !m.alive)
            continue;

        Ab::Candidate c;
        c.guidLow = m.guid;
        c.cls = m.cls;
        c.healer = m.healer;
        c.tank = m.tank;
        c.melee = m.melee;
        c.feral = m.feral;
        c.level = m.level;
        c.tier = m.tier;
        c.x = m.x;
        c.y = m.y;
        c.hasPrevious = m.assigned;
        c.previous = m.slot;
        c.pinned = m.casting && m.assigned;
        candidates.push_back(c);
    }

    // BT-481: a casual bot may keep its old slot for a refresh after a stance change, except a bot in the middle of the
    // capture cast and a slot that would leave a node without any guard (the node is never left empty by a late bot).
    uint32 guardsAt[Ab::NODE_COUNT] = { 0, 0, 0, 0, 0 };
    uint32 guardsPlanned[Ab::NODE_COUNT] = { 0, 0, 0, 0, 0 };
    for (Ab::Slot const& slot : team.slots)
        if (slot.job == Ab::Job::GUARD && slot.node < r.map->nodeCount)
            ++guardsPlanned[slot.node];
    for (auto const& entry : team.members)
    {
        Member const& m = entry.second;
        if (m.bot && m.alive && m.assigned && m.slot.job == Ab::Job::GUARD && m.slot.node < r.map->nodeCount)
            ++guardsAt[m.slot.node];
    }
    for (Ab::Assignment const& a : Ab::Assign(candidates, team.slots, in.team, *r.map))
    {
        Member* m = FindMember(team, a.guidLow);
        if (!m)
            continue;
        if (!m->assigned || m->slot != a.slot)
        {
            bool const emptiesNode = a.slot.job == Ab::Job::GUARD && a.slot.node < r.map->nodeCount && !guardsAt[a.slot.node];
            bool const surplus = r.map->kind == BgNodeMap::Kind::ARATHI && m->assigned && m->slot.job == Ab::Job::GUARD && m->slot.node < r.map->nodeCount &&
                Ab::SurplusGuard(m->slot, guardsAt[m->slot.node], guardsPlanned[m->slot.node]);
            if (m->assigned && !surplus && BgLeader::MayKeepRole(m->keepSlot, m->casting || emptiesNode))
                continue;
            if (m->assigned && m->slot.job == Ab::Job::GUARD && m->slot.node < r.map->nodeCount && guardsAt[m->slot.node])
                --guardsAt[m->slot.node];
            if (a.slot.job == Ab::Job::GUARD && a.slot.node < r.map->nodeCount)
                ++guardsAt[a.slot.node];
            if (m->assigned)
                changed = true;
            m->slot = a.slot;
            m->assigned = true;
        }
    }
    for (auto& entry : team.members)
        if (entry.second.keepSlot)
            --entry.second.keepSlot;

    // The index of a bot among those with the same job and node, by GUID, for spreading the posts.
    for (uint8 job = 0; job < 4; ++job)
    {
        for (uint8 node = 0; node <= r.map->nodeCount; ++node)
        {
            uint8 const nodeValue = node == Ab::NODE_COUNT ? Ab::NODE_NONE : node;
            std::vector<uint32> guids;
            for (auto& entry : team.members)
            {
                Member const& m = entry.second;
                if (m.bot && m.alive && m.assigned && uint8(m.slot.job) == job && m.slot.node == nodeValue)
                    guids.push_back(entry.first);
            }
            std::sort(guids.begin(), guids.end());
            for (uint32 i = 0; i < guids.size(); ++i)
            {
                team.members[guids[i]].groupIndex = i;
                team.members[guids[i]].groupSize = uint32(guids.size());
            }
        }
    }
    (void)nowMs;
}

// The node the assault group goes for (swing value per second of effort, the odds near the banner, the flip limiter for
// experts), the node a casual group goes for, the fifth node, and the reserve's staging node. A human's call overrides.
void ChooseTargets(Record& r, PvpMatch::State& match, uint32 t, uint32 nowMs)
{
    TeamPlan& team = r.team[t];
    TeamPlan& enemy = r.team[1 - t];

    // Where the assault group is: the centroid of its alive bots, else the Blacksmith.
    float cx = 0.0f, cy = 0.0f;
    uint32 groupSize = 0;
    uint32 levelSum = 0;
    for (auto const& entry : team.members)
    {
        Member const& m = entry.second;
        if (m.bot && m.alive && m.assigned && m.slot.job == Ab::Job::ASSAULT)
        {
            cx += m.x;
            cy += m.y;
            levelSum += m.level;
            ++groupSize;
        }
    }
    if (groupSize)
    {
        cx /= groupSize;
        cy /= groupSize;
    }
    else if (Ab::IsPresence(*r.map))
    {
        cx = r.map->centre.x;   // no hub: the middle of the map, where the flag stands
        cy = r.map->centre.y;
    }
    else
    {
        cx = r.map->pos[r.map->hub].x;
        cy = r.map->pos[r.map->hub].y;
    }
    uint8 const avgLevel = groupSize ? uint8(levelSum / groupSize) : uint8(Ab::MOUNT_LEVEL);

    std::vector<Ab::TargetInput> inputs;
    for (uint8 n = 0; n < r.map->nodeCount; ++n)
    {
        Ab::TargetInput in;
        in.node = n;
        in.sit = team.sit[n];
        in.enemyNear40 = team.enemyNear40[n];
        in.friendNear40 = team.friendNear40[n];
        in.etaS = Ab::TravelMs(Ab::DistToNode(n, cx, cy, *r.map), avgLevel) / 1000.0f;
        in.flipBlocked = r.flips.Blocked(n, nowMs, int(team.friendNear40[n]) - int(team.enemyNear40[n]));
        inputs.push_back(in);
    }

    unsigned const ours = team.occupied;
    unsigned const theirs = enemy.occupied;
    std::vector<Ab::TargetScore> good = Ab::ScoreTargets(inputs, ours, theirs, groupSize, Tier::GOOD, *r.map);

    // BT-482: a human's "take <node>" or "<node> lost" is one more reason to go there for the good bots, worth less as the
    // line ages; it never overrides a clearly better target (it did, when the swing was not negative). Experts keep their own
    // numbers and casual bots go to the nearest node.
    if (team.humanActive && team.humanNode < r.map->nodeCount && team.humanVerb != Ab::CallVerb::INC &&
        nowMs - team.humanExpiresMs >= 0x80000000u)
    {
        uint32 const left = std::min<uint32>(team.humanExpiresMs - nowMs, Ab::HUMAN_CALL_MS);
        BgCallout::ApplyClaimBonus(good, team.humanNode, PvpObs::HUMAN_WEIGHT_PERCENT * left / Ab::HUMAN_CALL_MS);
    }
    std::vector<Ab::TargetScore> const expert = Ab::ScoreTargets(inputs, ours, theirs, groupSize, Tier::EXPERT, *r.map);
    team.assaultNode[0] = Ab::StickyTarget(good, team.prevAssaultNode[0]);
    team.assaultNode[1] = Ab::StickyTarget(expert, team.prevAssaultNode[1]);
    team.nearestNode = Ab::NearestTarget(inputs, *r.map);
    team.closeOutNode = Ab::CloseOutNode(team.sit, *r.map);

    // BT-503: the targets the group can click now are the decision layer's list (D9); its answer that still stands replaces the
    // rule's pick, only while that node is still on the list (it can be clicked and the flip limiter lets the experts at it).
    // Casual bots keep the nearest node.
    // The fifth node ends the match: it is the target whatever anybody thinks, so there is nothing to ask then.
    uint8 const idBase = r.map->kind == BgNodeMap::Kind::EYE ? BgLeader::EYE_NODE_ID_BASE :
                         r.map->kind == BgNodeMap::Kind::GILNEAS ? BgLeader::BFG_NODE_ID_BASE : uint8(0);
    bool const fifthNode = (team.regime == Ab::Regime::CLOSE_OUT || team.regime == Ab::Regime::PANIC) && team.closeOutNode < r.map->nodeCount;
    AiPvp::Book& book = match.Side(t).ai.book;
    if (fifthNode)
        AiPvp::Withdraw(book, BgLeader::DecisionPoint::D9_NODE_TARGET);
    else
    {
        AiPvp::Publish(book, BgLeader::NodeTargetOffer(expert, idBase), { "group_size:" + std::to_string(groupSize) }, false, nowMs);
        if (std::optional<uint8> const forced = AiPvp::Override(book, BgLeader::DecisionPoint::D9_NODE_TARGET, nowMs))
            if (*forced >= idBase && *forced - idBase < r.map->nodeCount)
                team.assaultNode[0] = team.assaultNode[1] = uint8(*forced - idBase);
    }

    // The fifth node ends the match: it is the target whatever the swing table says.
    if (fifthNode)
        team.assaultNode[0] = team.assaultNode[1] = team.closeOutNode;

    team.prevAssaultNode[0] = team.assaultNode[0];
    team.prevAssaultNode[1] = team.assaultNode[1];

    // The reserve stages at the Blacksmith (the shortest walks to the others); with the centre lost, at our home node, else
    // at the nearest node we hold.
    uint8 stage = r.map->hub;
    if (!Ab::IsHeld(team.sit[r.map->hub]))
    {
        uint8 const home = Ab::HomeNode(TeamOfIndex(t), *r.map);
        if (Ab::IsHeld(team.sit[home]))
            stage = home;
        else
            for (uint8 n = 0; n < r.map->nodeCount; ++n)
                if (Ab::IsHeld(team.sit[n]))
                {
                    stage = n;
                    break;
                }
    }
    // Eye of the Storm (no hub): the reserve stages at the base we hold nearest the middle, with all but one held at the one
    // nearest the enemy's start room (their dead walk from there); none held: our first near base.
    if (Ab::IsPresence(*r.map))
    {
        stage = Ab::HomeNode(TeamOfIndex(t), *r.map);
        float best = 0.0f;
        bool found = false;
        bool const hold = team.regime == Ab::Regime::HOLD;
        Ab::Pt const target = hold ? r.map->start[Ab::OtherTeam(TeamOfIndex(t)) & 1] : r.map->centre;
        for (uint8 n = 0; n < r.map->nodeCount; ++n)
        {
            if (!Ab::IsHeld(team.sit[n]))
                continue;
            float const d = Dist2d(r.map->pos[n].x, r.map->pos[n].y, target.x, target.y);
            if (!found || d < best)
            {
                found = true;
                best = d;
                stage = n;
            }
        }
    }
    // Battle for Gilneas, all three held: the reserve stands at the node next to the enemy's start room, where their dead come out.
    if (r.map->kind == BgNodeMap::Kind::GILNEAS && team.regime == Ab::Regime::HOLD)
        stage = Ab::HomeNode(Ab::OtherTeam(TeamOfIndex(t)), *r.map);

    // BT-553: the decision layer may move the reserve (D14): the rule's node first, the other nodes we can use after it.
    if (r.map->kind == BgNodeMap::Kind::GILNEAS)
    {
        BgLeader::StageFacts sf;
        sf.ruleNode = stage;
        sf.hubNode = r.map->hub;
        sf.farNode = Ab::HomeNode(Ab::OtherTeam(TeamOfIndex(t)), *r.map);
        sf.ownNearNode = Ab::HomeNode(TeamOfIndex(t), *r.map);
        sf.hubUsable = Ab::IsHeld(team.sit[sf.hubNode]);
        sf.ownNearUsable = Ab::IsHeld(team.sit[sf.ownNearNode]);
        sf.farUsable = team.occupied >= 2;
        BgLeader::Offer const offer = BgLeader::StageOffer(sf);
        AiPvp::Publish(book, offer, { "stage_rule:" + std::string(BgLeader::NodeName(stage, *r.map)) }, true, nowMs);
        if (std::optional<uint8> const id = AiPvp::Override(book, BgLeader::DecisionPoint::D14_BFG_STAGE, nowMs))
            stage = BgLeader::StageNodeOf(static_cast<BgLeader::BfgStage>(*id), sf);
    }
    team.stageNode = stage;
}

// The opening groups wait for each other 40 yd short of their node; the group is gathered when three stand there (or
// all of a small group) or after the wait.
void UpdateGather(Record& r, uint32 t, uint32 nowMs)
{
    TeamPlan& team = r.team[t];
    for (uint8 n = 0; n < r.map->nodeCount; ++n)
    {
        uint32 alive = 0;
        uint32 nearCount = 0;
        for (auto const& entry : team.members)
        {
            Member const& m = entry.second;
            if (!(m.bot && m.alive && m.assigned && m.slot.job == Ab::Job::ASSAULT && m.slot.node == n))
                continue;
            ++alive;
            if (Dist2d(m.x, m.y, r.map->pos[n].x, r.map->pos[n].y) <= Ab::GatherDistance(*r.map) + Ab::GATHER_RADIUS)
                ++nearCount;
        }

        if (!alive)
        {
            team.gathered[n] = false;
            team.gatherSinceMs[n] = 0;
            continue;
        }
        if (nearCount && !team.gatherSinceMs[n])
            team.gatherSinceMs[n] = nowMs ? nowMs : 1;
        uint32 const waited = team.gatherSinceMs[n] ? nowMs - team.gatherSinceMs[n] : 0;
        if (Ab::GroupGathered(alive, nearCount, waited))
            team.gathered[n] = true;
    }
}

// BT-552 (Battle for Gilneas): the fights, the retreat and the ghost (sheet section 6). A group that loses a local fight falls back
// to the nearest node we hold, where the dead come out in the next wave, and pushes again together; two expert stealth bots ink
// the enemy's near node while the main fight goes badly. Good rules (retreat) and expert rules (ghost): the option lists of the
// bots carry the tiers (AbLogic::Options). Only what the team has seen counts (CountNear).
constexpr uint32 RETREAT_COOLDOWN_MS = 30000;   // after a retreat ends, the next one waits this long (no loop of retreats)
constexpr uint32 GHOST_MAX_MS = 120000;

bool IsStealthMember(Member const& m)
{
    BgPlan::CandidateBase c;
    c.cls = m.cls;
    c.feral = m.feral;
    return BgPlan::IsStealthClass(c);
}

void EvaluateFights(Record& r, PvpMatch::State& match, uint32 t, uint32 nowMs, bool& changed)
{
    if (r.map->kind == BgNodeMap::Kind::ARATHI)
        return;

    TeamPlan& team = r.team[t];
    FightState& f = team.fight;
    Ab::Map const& map = *r.map;
    uint8 const us = TeamOfIndex(t);
    // The enemy node the ghost pair goes for: Gilneas, the enemy's near node; Eye of the Storm, the one of the enemy's two near
    // bases with fewer seen defenders (the pair needs a base with none seen).
    uint8 enemyNear = Ab::HomeNode(Ab::OtherTeam(us), map);
    if (Ab::IsPresence(map))
    {
        uint8 const a = map.nearNodes[Ab::OtherTeam(us) & 1][0];
        uint8 const b = map.nearNodes[Ab::OtherTeam(us) & 1][1];
        enemyNear = team.enemyNear40[b] < team.enemyNear40[a] ? b : a;
    }
    AiPvp::Book& book = match.Side(t).ai.book;
    f.reinforceNode = Ab::NODE_NONE;

    // --- the retreat that runs: over when the group stands together at the rally node, or after the time limit
    if (f.retreating)
    {
        uint32 atRally = 0;
        for (auto const& entry : team.members)
            if (entry.second.alive && NearNode(map, entry.second, f.rally, 25.0f))
                ++atRally;
        if (Ab::RetreatOver(atRally, nowMs - f.retreatSinceMs) || !Ab::IsHeld(team.sit[f.rally]))
        {
            f.retreating = false;
            f.retreatCooldownUntilMs = nowMs + RETREAT_COOLDOWN_MS;
            for (auto& entry : team.members)
                entry.second.retreatNode = Ab::NODE_NONE;
            changed = true;
        }
        else
        {
            AiPvp::Withdraw(book, BgLeader::DecisionPoint::D13_BFG_FIGHT);
            return;
        }
    }

    // --- the ghost that runs: over when its node is ours or contested by us, after the time limit, or when the pair is gone
    if (f.ghosting)
    {
        bool pairAlive = false;
        for (auto const& entry : team.members)
            if (entry.second.ghostNode != Ab::NODE_NONE && entry.second.alive)
                pairAlive = true;
        bool const targetOpen = Ab::IsTargetable(team.sit[f.ghostTarget]);
        if (!pairAlive || !targetOpen || nowMs - f.ghostSinceMs >= GHOST_MAX_MS)
        {
            f.ghosting = false;
            for (auto& entry : team.members)
                entry.second.ghostNode = Ab::NODE_NONE;
            changed = true;
        }
    }

    if (nowMs < f.retreatCooldownUntilMs)
    {
        AiPvp::Withdraw(book, BgLeader::DecisionPoint::D13_BFG_FIGHT);
        return;
    }

    // --- a fight that is going badly: friends and seen enemies within 40 yd of a banner (the node with the worst odds)
    int worstOdds = 0;
    int worstNode = -1;
    for (uint8 n = 0; n < map.nodeCount; ++n)
    {
        unsigned const friends = team.friendNear40[n];
        unsigned const enemies = team.enemyNear40[n];
        if (!friends || enemies < 2 || Ab::CappingNearFlip(team.sit[n], team.captureAgeMs[n]))
            continue;
        int const odds = Ab::FightOdds(friends, enemies);
        if (odds < 0 && (worstNode < 0 || odds < worstOdds))
        {
            worstOdds = odds;
            worstNode = n;
        }
    }
    if (worstNode < 0)
    {
        AiPvp::Withdraw(book, BgLeader::DecisionPoint::D13_BFG_FIGHT);
        return;
    }

    uint8 const n = uint8(worstNode);
    bool healerAlive = false;
    uint32 group = 0;
    uint32 reserveNear = 0;
    for (auto const& entry : team.members)
    {
        Member const& m = entry.second;
        if (m.alive && NearNode(map, m, n, Ab::FIGHT_RADIUS))
            healerAlive = healerAlive || m.healer;
        if (m.bot && m.alive && m.assigned && m.slot.job != Ab::Job::GUARD && m.slot.job != Ab::Job::RAIDER && !m.casting &&
            NearNode(map, m, n, Ab::RETREAT_REACH))
            ++group;
        if (m.bot && m.alive && m.assigned && m.slot.job == Ab::Job::MOBILE && !NearNode(map, m, n, Ab::RETREAT_REACH))
            ++reserveNear;
    }
    if (!group)
    {
        AiPvp::Withdraw(book, BgLeader::DecisionPoint::D13_BFG_FIGHT);
        return;
    }

    // The two expert stealth bots of the ghost, if there are two.
    std::vector<uint32> stealth;
    for (auto const& entry : team.members)
    {
        Member const& m = entry.second;
        if (m.bot && m.alive && m.assigned && !m.casting && m.tier == Tier::EXPERT && IsStealthMember(m) && m.slot.job != Ab::Job::GUARD)
            stealth.push_back(m.guid);
    }
    uint8 const rally = Ab::RallyNode(team.sit, n, map.pos[n].x, map.pos[n].y, map);

    BgLeader::FightFacts facts;
    facts.odds = worstOdds;
    facts.healerAlive = healerAlive;
    facts.rallyAvailable = rally != Ab::NODE_NONE;
    facts.ghostWanted = !f.ghosting && Ab::IsTargetable(team.sit[enemyNear]) &&
                        Ab::GhostWanted(worstOdds, healerAlive, unsigned(stealth.size()), team.enemyNear40[enemyNear]);
    facts.reinforceAvailable = reserveNear > 0;
    BgLeader::Offer const offer = BgLeader::FightOffer(facts);
    std::vector<std::string> ask = { "fight_node:" + std::string(BgLeader::NodeName(n, map)), "odds:" + std::to_string(worstOdds),
                                     std::string("healer_alive:") + (healerAlive ? "yes" : "no") };
    AiPvp::Publish(book, offer, ask, true, nowMs);

    // The rule's answer is the first of the list; a decision layer's answer that still stands replaces it.
    BgLeader::BfgFight action = static_cast<BgLeader::BfgFight>(offer.Fallback());
    if (std::optional<uint8> const id = AiPvp::Override(book, BgLeader::DecisionPoint::D13_BFG_FIGHT, nowMs))
        action = static_cast<BgLeader::BfgFight>(*id);

    switch (action)
    {
        case BgLeader::BfgFight::GHOST:
        {
            if (stealth.size() < Ab::GHOST_PAIR)
                break;
            std::sort(stealth.begin(), stealth.end(), [](uint32 a, uint32 b)
            {
                uint32 const ha = BotSkillMath::Mix(a);
                uint32 const hb = BotSkillMath::Mix(b);
                return ha != hb ? ha < hb : a < b;
            });
            for (uint32 i = 0; i < Ab::GHOST_PAIR; ++i)
                FindMember(team, stealth[i])->ghostNode = enemyNear;
            f.ghosting = true;
            f.ghostTarget = enemyNear;
            f.ghostSinceMs = nowMs ? nowMs : 1;
            f.ghostCallDue = true;
            f.ghostSpeaker = stealth[0];
            changed = true;   // the group fights on while the pair inks; the retreat is not started
            break;
        }
        case BgLeader::BfgFight::RETREAT:
        {
            if (rally == Ab::NODE_NONE)
                break;
            f.retreating = true;
            f.from = n;
            f.rally = rally;
            f.retreatSinceMs = nowMs ? nowMs : 1;
            f.retreatCallDue = true;
            f.retreatSpeaker = 0;
            for (auto& entry : team.members)
            {
                Member& m = entry.second;
                if (m.bot && m.alive && m.assigned && m.slot.job != Ab::Job::GUARD && m.slot.job != Ab::Job::RAIDER && !m.casting &&
                    NearNode(map, m, n, Ab::RETREAT_REACH))
                {
                    m.retreatNode = rally;
                    if (!f.retreatSpeaker || BotSkillMath::Mix(m.guid) < BotSkillMath::Mix(f.retreatSpeaker))
                        f.retreatSpeaker = m.guid;
                }
            }
            changed = true;
            break;
        }
        case BgLeader::BfgFight::REINFORCE:
            f.reinforceNode = n;
            break;
        case BgLeader::BfgFight::FIGHT_ON:
            break;
    }
}

// Raiders: the enemy node with the fewest defenders; a raider that is capturing stays only when nobody is near.
void ChooseRaids(Record& r, uint32 t)
{
    TeamPlan& team = r.team[t];
    for (auto& entry : team.members)
    {
        Member& m = entry.second;
        m.raidNode = Ab::NODE_NONE;
        m.raidStay = false;
        if (!(m.bot && m.alive && m.assigned && m.slot.job == Ab::Job::RAIDER))
            continue;

        std::vector<Ab::RaidInput> inputs;
        for (uint8 n = 0; n < r.map->nodeCount; ++n)
            inputs.push_back({ n, team.sit[n], team.enemyNear40[n] });
        m.raidNode = Ab::PickRaidNode(inputs, TeamOfIndex(t), m.tier, *r.map);

        // The raider that is capturing at a node (its cast is done): hold there or leave.
        uint8 const capped = m.optNode;
        if (m.kind == Ab::Kind::RAID && capped < r.map->nodeCount && team.sit[capped] == Ab::NodeSituation::CAPPING)
        {
            m.raidNode = capped;
            m.raidStay = team.enemyNear60[capped] == 0;
        }
    }
}

// Who answers what: a node under assault is restored (the guards of the node and the nearest reserve bots that can arrive
// before the contest ends); a node with enough enemies near its banner gets the reserve ("inc"); a human who captures
// gets two bots beside him. A bot answers one thing only.
void ChooseResponders(Record& r, uint32 t, uint32 nowMs, bool& changed)
{
    TeamPlan& team = r.team[t];
    // The answers of the last refresh: a bot that answered a node keeps its place unless another is clearly nearer.
    std::unordered_map<uint32, uint8> lastAnswer;
    for (auto& entry : team.members)
    {
        Member& m = entry.second;
        if (m.restoreResponder)
            lastAnswer[m.guid] = m.restoreNode;
        else if (m.incResponder)
            lastAnswer[m.guid] = m.incNode;
    }
    for (auto& entry : team.members)
    {
        Member& m = entry.second;
        m.restoreResponder = false;
        m.restoreNode = Ab::NODE_NONE;
        m.incResponder = false;
        m.incNode = Ab::NODE_NONE;
        m.supportHuman = false;
        m.supportNode = Ab::NODE_NONE;
    }

    uint32 assaultAlive = 0;
    for (auto const& entry : team.members)
        if (entry.second.bot && entry.second.alive && entry.second.assigned && entry.second.slot.job == Ab::Job::ASSAULT)
            ++assaultAlive;

    std::vector<uint32> used;
    auto const pool = [&](uint8 node)
    {
        std::vector<Ab::ResponderCandidate> all;
        for (auto const& entry : team.members)
        {
            Member const& m = entry.second;
            if (!m.bot || !m.alive || !m.assigned || std::find(used.begin(), used.end(), m.guid) != used.end())
                continue;
            if (m.slot.job == Ab::Job::RAIDER || m.casting)
                continue;
            uint32 eta = Ab::TravelMs(Ab::DistToNode(node, m.x, m.y, *r.map), m.level);
            auto const before = lastAnswer.find(m.guid);
            if (before != lastAnswer.end() && before->second == node)
                eta = eta > Ab::RESPONDER_STICKY_MS ? eta - Ab::RESPONDER_STICKY_MS : 0u;
            all.push_back({ m.guid, m.slot.job, m.slot.node, eta });
        }
        return all;
    };

    // 1. Restores first: the larger the swing, the earlier.
    std::vector<std::pair<float, uint8>> assaulted;
    for (uint8 n = 0; n < r.map->nodeCount; ++n)
        if (team.sit[n] == Ab::NodeSituation::UNDER_ASSAULT)
            assaulted.emplace_back(-Ab::SwingValue(team.occupied, r.team[1 - t].occupied, Ab::SwingKind::DENY, *r.map), n);
    std::sort(assaulted.begin(), assaulted.end());

    for (auto const& pair : assaulted)
    {
        uint8 const node = pair.second;
        uint32 const since = nowMs - r.nodes[node].changedMs;
        uint32 const remaining = Ab::ContestRemainingMs(since);
        uint32 const byContest = remaining > Ab::CAST_TIME_MS ? remaining - Ab::CAST_TIME_MS : 0u;
        uint32 const byDeadline = since < Ab::RESTORE_DEADLINE_MS ? Ab::RESTORE_DEADLINE_MS - since : 0u;
        // No contest timer on a presence map: a neutralised base is retaken for as long as the deadline allows.
        uint32 const maxEta = Ab::IsPresence(*r.map) ? byDeadline : std::min(byContest, byDeadline);
        for (uint32 guid : Ab::ChooseResponders(pool(node), node, Ab::ResponderCount(team.enemyNear50[node]), maxEta, assaultAlive))
        {
            if (Member* m = FindMember(team, guid))
            {
                m->restoreResponder = true;
                m->restoreNode = node;
                used.push_back(guid);
            }
        }
    }

    // 2. "inc": enemies near a banner we hold or capture.
    std::vector<std::pair<uint32, uint8>> pressed;
    for (uint8 n = 0; n < r.map->nodeCount; ++n)
        if ((team.sit[n] == Ab::NodeSituation::OURS || team.sit[n] == Ab::NodeSituation::CAPPING) &&
            team.enemyNear50[n] >= Ab::IncThreshold(team.lowBracket))
            pressed.emplace_back(team.enemyNear50[n], n);
    // BT-553: a decision layer's "reinforce" answer for a fight that goes slightly badly sends the reserve as if the node were called.
    if (team.fight.reinforceNode < r.map->nodeCount &&
        std::none_of(pressed.begin(), pressed.end(), [&](std::pair<uint32, uint8> const& p) { return p.second == team.fight.reinforceNode; }))
        pressed.emplace_back(std::max<uint32>(team.enemyNear50[team.fight.reinforceNode], Ab::IncThreshold(team.lowBracket)),
                             team.fight.reinforceNode);
    std::sort(pressed.begin(), pressed.end(), [](std::pair<uint32, uint8> const& a, std::pair<uint32, uint8> const& b)
    {
        return a.first != b.first ? a.first > b.first : a.second < b.second;
    });

    for (auto const& pair : pressed)
    {
        uint8 const node = pair.second;
        std::vector<uint32> const picks = Ab::ChooseResponders(pool(node), node, Ab::ResponderCount(pair.first),
                                                              Ab::RESPONDER_MAX_ETA_MS, assaultAlive);
        for (uint32 guid : picks)
        {
            Member* m = FindMember(team, guid);
            if (!m || m->slot.job == Ab::Job::GUARD)
                continue;   // the node's own guards fight where they stand
            m->incResponder = true;
            m->incNode = node;
            used.push_back(guid);
        }
        if (!picks.empty() && (!team.lastOmwMs[node] || nowMs - team.lastOmwMs[node] >= OMW_REPEAT_MS))
        {
            team.lastOmwMs[node] = nowMs ? nowMs : 1;
            for (uint32 guid : picks)
            {
                Member* m = FindMember(team, guid);
                if (m && m->incResponder && m->incNode == node)
                {
                    Pending text;
                    text.node = Ab::NodeWord(node, *r.map);
                    Enqueue(r, team, *m, Ab::Call::ON_MY_WAY, nowMs, text);
                    break;
                }
            }
        }
    }

    // 2b. A human's "inc <node>" or "help <node>" (valid for a while): the two nearest non-guard bots go.
    if (team.humanActive && team.humanVerb == Ab::CallVerb::INC && team.humanNode < r.map->nodeCount)
    {
        std::vector<std::pair<uint32, float>> list;
        for (auto const& entry : team.members)
        {
            Member const& m = entry.second;
            if (m.bot && m.alive && m.assigned && m.slot.job != Ab::Job::GUARD && m.slot.job != Ab::Job::RAIDER &&
                BgCallout::Heeds(m.tier, m.guid, team.humanReceivedMs) &&
                std::find(used.begin(), used.end(), m.guid) == used.end())
                list.emplace_back(m.guid, Ab::DistToNode(team.humanNode, m.x, m.y, *r.map));
        }
        // A human who said "1" asks for one bot, any other line (or none) for the usual two.
        std::size_t const wanted = team.humanCount == 1 ? 1 : Ab::RESPONDERS;
        for (uint32 guid : BgPlan::NearestN(list, wanted))
            if (Member* m = FindMember(team, guid))
            {
                m->incResponder = true;
                m->incNode = team.humanNode;
                used.push_back(guid);
            }
    }

    // 3. A human who is casting the capture: the two nearest non-guard bots go and stand beside him.
    for (auto const& entry : team.members)
    {
        Member const& human = entry.second;
        if (human.bot || !human.alive || !human.casting || human.castNode >= r.map->nodeCount)
            continue;
        if (!Ab::CanClickNode(r.nodes[human.castNode].state, TeamOfIndex(t)))
            continue;

        std::vector<std::pair<uint32, float>> list;
        for (auto const& other : team.members)
        {
            Member const& m = other.second;
            if (!m.bot || !m.alive || !m.assigned || m.slot.job == Ab::Job::GUARD || m.slot.job == Ab::Job::RAIDER ||
                std::find(used.begin(), used.end(), m.guid) != used.end())
                continue;
            list.emplace_back(m.guid, Dist2d(m.x, m.y, human.x, human.y));
        }
        for (uint32 guid : BgPlan::NearestN(list, Ab::SUPPORTERS))
        {
            Member* m = FindMember(team, guid);
            if (m)
            {
                m->supportHuman = true;
                m->supportNode = human.castNode;
                used.push_back(guid);
            }
        }
    }
    (void)changed;
}

// A human's calls (BT-482): information, never an order. "inc" and "help" make one answer from the nearest bot that heeds the
// line (a casual bot may not) and two responders (one when the human said "1"); "take" and "lost" weigh on the targets of the
// good bots (ChooseTargets). Valid for HUMAN_CALL_MS.
void ReadHumanCalls(Record& r, uint32 t, uint32 nowMs, bool& changed)
{
    TeamPlan& team = r.team[t];
    if (team.humanActive && nowMs >= team.humanExpiresMs && nowMs - team.humanExpiresMs < 0x80000000u)
    {
        team.humanActive = false;
        changed = true;
    }

    for (PvpObs::Observation const& call : team.inbox)
    {
        Ab::CallVerb verb = Ab::CallVerb::INC;
        switch (call.kind)
        {
            case PvpObs::Kind::INCOMING:
            case PvpObs::Kind::HELP:
                verb = Ab::CallVerb::INC;
                break;
            case PvpObs::Kind::CLAIM:
                verb = Ab::CallVerb::TAKE;
                break;
            case PvpObs::Kind::LOST:
                verb = Ab::CallVerb::LOST;
                break;
            case PvpObs::Kind::CLEAR:
                // "<node> clear": nobody is there, so an earlier "inc" for it stops steering the responders.
                if (team.humanActive && team.humanVerb == Ab::CallVerb::INC && team.humanNode == call.place)
                {
                    team.humanActive = false;
                    changed = true;
                }
                continue;
            default:
                continue;
        }

        uint8 node = call.place;
        if (node == PvpObs::PLACE_UNKNOWN)
        {
            // "help" with no node: the node next to where the sender stands, read from the roster (never guessed).
            Member* sender = FindMember(team, call.senderLow);
            if (call.kind != PvpObs::Kind::HELP || !sender || !sender->alive)
                continue;
            node = Ab::NodeNear(sender->x, sender->y, HELP_NODE_RADIUS, *r.map);
        }
        if (node >= r.map->nodeCount)
            continue;

        team.humanActive = true;
        team.humanNode = node;
        team.humanVerb = verb;
        team.humanExpiresMs = nowMs + Ab::HUMAN_CALL_MS;
        team.humanReceivedMs = call.receivedMs;
        team.humanCount = call.count;
        changed = true;

        if (RepliesEnabled() && verb == Ab::CallVerb::INC)
        {
            Member* speaker = NearestBot(team, node, 2, call.receivedMs);
            if (!speaker)
                speaker = NearestBot(team, node, 0, call.receivedMs);
            if (speaker)
            {
                Pending text;
                text.node = Ab::NodeWord(node, *r.map);
                Enqueue(r, team, *speaker, Ab::Call::ANSWER, nowMs, text);
            }
        }
    }
    team.inbox.clear();
}

// The events and the call-outs of one team (the first refresh only records the baseline).
void EmitCalls(Record& r, PvpMatch::State& match, uint32 t, uint32 nowMs)
{
    TeamPlan& team = r.team[t];
    TeamPlan& enemy = r.team[1 - t];

    // BT-481: the leader says what the team plays (at the gates, at every change, after a handover).
    if (team.stanceCallDue)
    {
        Member* leader = FindMember(team, team.leader);
        if (leader)
        {
            Pending line;
            line.stance = StanceWord(team.stance);
            if (Enqueue(r, team, *leader, Ab::Call::STANCE, nowMs, line, true))
                BgLeader::CallCommitted(match.Side(t).leader);
        }
    }

    // BT-552: the survivor of a losing group says where it falls back to; the ghost pair says where it goes (before it starts).
    if (team.fight.retreatCallDue)
    {
        team.fight.retreatCallDue = false;
        if (Member* speaker = FindMember(team, team.fight.retreatSpeaker))
        {
            Pending line;
            line.node = Ab::NodeWord(team.fight.rally, *r.map);
            Enqueue(r, team, *speaker, Ab::Call::RETREAT, nowMs, line);
        }
    }
    if (team.fight.ghostCallDue)
    {
        team.fight.ghostCallDue = false;
        if (Member* speaker = FindMember(team, team.fight.ghostSpeaker))
        {
            Pending line;
            line.node = Ab::NodeWord(team.fight.ghostTarget, *r.map);
            Enqueue(r, team, *speaker, Ab::Call::GHOST, nowMs, line);
        }
    }

    for (uint8 n = 0; n < r.map->nodeCount; ++n)
    {
        Pending text;
        text.node = Ab::NodeWord(n, *r.map);
        Ab::NodeSituation const now = team.sit[n];
        Ab::NodeSituation const before = team.prevSit[n];

        if (now == Ab::NodeSituation::UNDER_ASSAULT && before != Ab::NodeSituation::UNDER_ASSAULT)
        {
            if (Member* speaker = SpeakerFor(team, n))
                Enqueue(r, team, *speaker, Ab::Call::ASSAULTED, nowMs, text);
        }
        else if (now == Ab::NodeSituation::OURS && (before == Ab::NodeSituation::UNDER_ASSAULT || before == Ab::NodeSituation::CAPPING))
        {
            if (Member* speaker = SpeakerFor(team, n))
                Enqueue(r, team, *speaker, Ab::Call::NODE_BACK, nowMs, text);
        }
        else if (now == Ab::NodeSituation::THEIRS && (before == Ab::NodeSituation::OURS || before == Ab::NodeSituation::UNDER_ASSAULT))
        {
            if (Member* speaker = SpeakerFor(team, n))
            {
                Observe(text, PvpObs::Kind::LOST, n, 0);
                Enqueue(r, team, *speaker, Ab::Call::NODE_LOST, nowMs, text);
            }
        }

        if ((now == Ab::NodeSituation::OURS || now == Ab::NodeSituation::CAPPING) &&
            team.enemyNear50[n] >= Ab::IncThreshold(team.lowBracket) &&
            (!team.lastIncomingMs[n] || nowMs - team.lastIncomingMs[n] >= INCOMING_REPEAT_MS))
        {
            if (Member* speaker = SpeakerFor(team, n))
            {
                team.lastIncomingMs[n] = nowMs ? nowMs : 1;
                text.number = std::to_string(team.enemyNear50[n]);
                Observe(text, PvpObs::Kind::INCOMING, n, team.enemyNear50[n]);
                Enqueue(r, team, *speaker, Ab::Call::INCOMING, nowMs, text);
            }
        }
        else if ((now == Ab::NodeSituation::OURS || now == Ab::NodeSituation::CAPPING) &&
                 BgScout::EarlyWarning(team.enemyNear50[n], team.enemyNear60[n], Ab::IncThreshold(team.lowBracket)) &&
                 (!team.lastIncomingMs[n] || nowMs - team.lastIncomingMs[n] >= INCOMING_REPEAT_MS))
        {
            // BT-483: an expert within sight of the banner warns of the group before it is in everybody's range.
            if (Member* scout = NearestExpert(team, n))
            {
                team.lastIncomingMs[n] = nowMs ? nowMs : 1;
                text.number = std::to_string(team.enemyNear60[n]);
                Observe(text, PvpObs::Kind::INCOMING, n, team.enemyNear60[n]);
                Enqueue(r, team, *scout, Ab::Call::INCOMING, nowMs, text);
            }
        }
    }

    // The opening groups say where they go, once, a few seconds after the gates open.
    if (!team.openingCalled && r.started && r.playMs >= OPENING_CALL_MS)
    {
        team.openingCalled = true;
        for (uint8 n = 0; n < r.map->nodeCount; ++n)
        {
            Member* first = nullptr;
            for (auto& entry : team.members)
            {
                Member& m = entry.second;
                if (m.bot && m.alive && m.assigned && m.slot.job == Ab::Job::ASSAULT && m.slot.node == n &&
                    (!first || BotSkillMath::Mix(m.guid) < BotSkillMath::Mix(first->guid)))
                    first = &m;
            }
            if (first)
            {
                Pending text;
                text.node = Ab::NodeWord(n, *r.map);
                Enqueue(r, team, *first, Ab::Call::OPENING_GROUP, nowMs, text);
            }
        }
    }

    // The leader (else one expert bot) names the fifth node when we hold four.
    if (!team.fourthCalled && team.regime == Ab::Regime::CLOSE_OUT && team.closeOutNode < r.map->nodeCount)
    {
        Member* const speaker = AnnouncerOf(team);
        team.fourthCalled = true;
        if (speaker)
        {
            Pending text;
            text.node = Ab::NodeWord(team.closeOutNode, *r.map);
            Enqueue(r, team, *speaker, Ab::Call::NEED_FOURTH, nowMs, text);
        }
    }
    if (team.regime != Ab::Regime::CLOSE_OUT)
        team.fourthCalled = false;

    // The score at 1000 and 1400: the leader (else one expert bot), ahead or behind.
    auto const scoreCall = [&](bool& called, uint32 limit)
    {
        if (called || std::max(team.score, enemy.score) < limit)
            return;
        called = true;
        Member* const speaker = AnnouncerOf(team);
        if (!speaker)
            return;
        Pending text;
        text.score = std::to_string(team.score) + "-" + std::to_string(enemy.score);
        text.number = std::to_string(enemy.occupied);
        text.behind = team.score < enemy.score;
        Enqueue(r, team, *speaker, Ab::Call::SCORE, nowMs, text);
    };
    scoreCall(team.lowScoreCalled, SCORE_CALL_LOW);
    scoreCall(team.highScoreCalled, ScoreCallHigh(*r.map));
}

void LogState(Record& r, uint32 t, uint32 nowMs, bool force)
{
    TeamPlan& team = r.team[t];
    if (!force && team.lastStateLogMs && nowMs - team.lastStateLogMs < LOG_REPEAT_MS)
        return;
    team.lastStateLogMs = nowMs ? nowMs : 1;

    // Guards at each node: those within the leash of the banner.
    uint32 atPost[Ab::NODE_COUNT] = { 0, 0, 0, 0, 0 };
    uint32 jobs[4] = { 0, 0, 0, 0 };
    for (auto const& entry : team.members)
    {
        Member const& m = entry.second;
        if (!m.bot || !m.alive || !m.assigned)
            continue;
        ++jobs[uint8(m.slot.job)];
        if (m.slot.job == Ab::Job::GUARD && m.slot.node < r.map->nodeCount &&
            Dist2d(m.x, m.y, r.map->pos[m.slot.node].x, r.map->pos[m.slot.node].y) <= Ab::GUARD_LEASH)
            ++atPost[m.slot.node];
    }

    std::string nodes;
    std::string posts;
    for (uint8 n = 0; n < r.map->nodeCount; ++n)
    {
        nodes += std::string(n ? " " : "") + Ab::NodeWord(n, *r.map) + ":" + Ab::SituationName(team.sit[n]);
        posts += std::string(n ? "/" : "") + std::to_string(atPost[n]);
    }
    SF_LOG_DEBUG("playerbots",
                 "[%s] inst=%u %s plan=%u regime=%s score=%u-%u occ=%u-%u size=%u humans=%u jobs=G%u/R%u/A%u/M%u nodes=%s "
                 "guards@post=%s epoch=%u",
                 r.map->tag, r.instanceId, t == uint32(TEAM_ALLIANCE) ? "A" : "H", uint32(team.plan), Ab::RegimeName(team.regime),
                 team.score, r.team[1 - t].score, team.occupied, r.team[1 - t].occupied, team.size, team.humans, jobs[0],
                 jobs[1], jobs[2], jobs[3], nodes.c_str(), posts.c_str(), r.epoch);
}

// Rebuilds the roster, the node situations, the jobs, the groups and the events from the battleground. One call per
// record per BgPlan::REFRESH_MS, on the map thread of the match (the callers are the bots' own ticks).
void PublishEyeFlag(Record& r, Battleground* bg, PvpMatch::State& match, uint32 t, uint32 nowMs);

void Refresh(Record& r, Battleground* bg, uint32 nowMs, uint8 strategyAlliance, uint8 strategyHorde)
{
    r.lastRefreshMs = nowMs ? nowMs : 1;
    uint32 const elapsed = bg->GetElapsedTime();
    BattlegroundStatus const status = bg->GetStatus();
    if (status == STATUS_IN_PROGRESS && !r.started)
    {
        r.started = true;
        r.gatesOpenElapsedMs = elapsed;
    }
    r.playMs = r.started ? elapsed - r.gatesOpenElapsedMs : 0;

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
    for (uint32 t = 0; t < 2; ++t)
        match->Side(t).observations.Sweep(nowMs);

    ReadRoster(r, bg);
    if (status != STATUS_IN_PROGRESS && status != STATUS_WAIT_JOIN)
        return;

    bool changed = false;
    ReadNodes(r, bg, nowMs, changed);
    ReadSituations(r, bg, nowMs);
    CountNear(r, *match, nowMs);

    for (uint32 t = 0; t < 2; ++t)
    {
        TeamPlan& team = r.team[t];
        TeamPlan& enemy = r.team[1 - t];

        uint8 const strategy = t == uint32(TEAM_ALLIANCE) ? strategyAlliance : strategyHorde;
        BgPlan::Plan const plan = BgPlan::PlanFromStrategy(strategy);
        if (plan != team.rulePlan)
        {
            team.rulePlan = plan;
            changed = true;
        }

        Ab::RaceInput race;
        race.ourScore = team.score;
        race.theirScore = enemy.score;
        race.ourNodes = team.occupied;
        race.theirNodes = enemy.occupied;
        race.map = r.map;
        // Eye of the Storm: the fourth base only when the odds there are 0 or better and 4 or more bots are free (sheet section 6);
        // otherwise the team holds what it has.
        if (r.map->kind == BgNodeMap::Kind::EYE)
        {
            uint8 const last = Ab::CloseOutNode(team.sit, *r.map);
            uint32 aliveBots = 0;
            for (auto const& entry : team.members)
                if (entry.second.bot && entry.second.alive)
                    ++aliveBots;
            race.closeOutOk = last < r.map->nodeCount &&
                              int(team.friendNear40[last]) - int(team.enemyNear40[last]) >= 0 && aliveBots >= EYE_FOURTH_MIN_ALIVE;
        }
        Ab::Regime const regime = Ab::ChooseRegime(race, team.ruleRegime);
        if (regime != team.ruleRegime)
        {
            team.ruleRegime = regime;
            changed = true;
        }
        // The leader's turn may replace these two with a decision layer's stance (BT-503); the rule's own stay above.
        team.plan = team.rulePlan;
        team.regime = team.ruleRegime;

        stanceChangedNow = LeaderTurn(r, *match, t, team, nowMs) || stanceChangedNow;

        AssignJobs(r, t, nowMs, changed);
        UpdateGather(r, t, nowMs);
        ChooseRaids(r, t);
        EvaluateFights(r, *match, t, nowMs, changed);
        ReadHumanCalls(r, t, nowMs, changed);
        ChooseResponders(r, t, nowMs, changed);
        ChooseTargets(r, *match, t, nowMs);
        PublishEyeFlag(r, bg, *match, t, nowMs);
    }

    // A node that changed state, a death, a call: the bots choose again.
    for (uint8 n = 0; n < r.map->nodeCount; ++n)
        if (r.nodeChanged[n])
            changed = true;

    // --- events and call-outs (the first refresh only records the baseline) ---
    bool const live = r.baseline && status == STATUS_IN_PROGRESS;
    if (live)
        for (uint32 t = 0; t < 2; ++t)
            EmitCalls(r, *match, t, nowMs);

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

    // A new epoch when the plan state changed: a node, a regime, a plan, a job, a call. The opening only records.
    uint64 signature = 1469598103934665603ull;
    auto const mixIn = [&signature](uint64 value) { signature = (signature ^ value) * 1099511628211ull; };
    for (uint32 t = 0; t < 2; ++t)
    {
        TeamPlan const& team = r.team[t];
        mixIn(uint64(team.regime));
        mixIn(uint64(team.plan));
        mixIn(team.humanActive ? 1 + team.humanNode : 0);
        mixIn(team.fight.retreating ? 1 + team.fight.rally : 0);
        mixIn(team.fight.ghosting ? 1 + team.fight.ghostTarget : 0);
        mixIn(team.assaultNode[0]);
        mixIn(team.assaultNode[1]);
        for (uint8 n = 0; n < r.map->nodeCount; ++n)
        {
            mixIn(uint64(team.sitState[n]));
            mixIn(r.map->kind == BgNodeMap::Kind::ARATHI && team.sit[n] == Ab::NodeSituation::CAPPING &&
                  team.captureAgeMs[n] >= Ab::StayAfterCastMs(*r.map) ? 1 : 0);
            mixIn(team.hardened[n] ? 1 : 0);
            mixIn(team.gathered[n] ? 1 : 0);
        }
        for (auto const& entry : team.members)
        {
            Member const& m = entry.second;
            mixIn(m.restoreResponder ? 1 + m.restoreNode : 0);
            mixIn(m.incResponder ? 1 + m.incNode : 0);
            mixIn(m.supportHuman ? 1 + m.supportNode : 0);
        }
    }
    if (signature != r.signature)
    {
        if (r.baseline)
            changed = true;
        r.signature = signature;
    }

    // BT-502: the plan above is published and plays on; each side's leader may now ask the decision layer about it.
    for (uint32 t = 0; t < 2; ++t)
        AiPvpDecision::Ask(match, t, match->Side(t).leader, NodeEventSignature(r.team[t]), nowMs, r.started && status == STATUS_IN_PROGRESS,
                           [&](AwareAiBgState& state) { DescribeMatch(state, r, *match, t, nowMs); });

    if (r.baseline && changed)
    {
        r.BumpEpoch(nowMs);
        if (stanceChangedNow)
            r.stanceEpoch = r.epoch;
        for (uint32 t = 0; t < 2; ++t)
            LogState(r, t, nowMs, true);
    }
    else if (r.baseline)
    {
        for (uint32 t = 0; t < 2; ++t)
            LogState(r, t, nowMs, false);
    }
    r.baseline = true;
}

// A bot's member entry, refreshing the record first when it is due or the bot is new to it.
Member* Prepare(Record& r, Battleground* bg, Player* bot, uint32 nowMs, uint8 strategyAlliance, uint8 strategyHorde)
{
    bool refreshed = false;
    if (r.RefreshDue(nowMs))
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

// ---------------------------------------------------------------------------------------------------------------------
// Positions.
// ---------------------------------------------------------------------------------------------------------------------
struct Spot
{
    float x;
    float y;
    float z;
};

// A point on a ring around a banner, on solid ground at about the banner's height. The Lumber Mill plateau and the Gold
// Mine pit have edges: a ring point whose ground is far from the banner's height falls back to the banner itself. A bot
// that fell off the plateau is 56 yd from where it should be (sheet section 2; to check live).
Spot RingPoint(Player* bot, Ab::Map const& map, uint8 node, float angle, float radius)
{
    Ab::Pt const banner = map.pos[node];
    Spot spot = { banner.x, banner.y, banner.z };
    float const x = banner.x + radius * std::cos(angle);
    float const y = banner.y + radius * std::sin(angle);
    if (Ab::NearExitPortal(x, y, map))
        return spot;

    float const ground = BotGetMapWaterOrGroundLevel(bot, x, y, banner.z + POST_PROBE_LIFT);
    if (ground > INVALID_HEIGHT && std::fabs(ground - banner.z) <= POST_MAX_DZ)
        spot = { x, y, ground };
    return spot;
}

float PostAngle(Member const& me, uint8 node)
{
    uint32 const count = std::max<uint32>(1, me.groupSize);
    return TWO_PI * (float(me.groupIndex) + 0.5f) / float(count) + NODE_ANGLE_STEP * float(node);
}

float RingRadius(Member const& me, float minRadius, float span)
{
    return minRadius + span * float(BotSkillMath::Mix(me.guid) % 100) / 100.0f;
}

// The gather point of an opening group: GATHER_DISTANCE from the banner on the line to the team's base.
Spot GatherPoint(Ab::Map const& map, uint32 team, uint8 node)
{
    // Eye of the Storm: the squad waits outside the 70 yd circle (inside it every bot already moves the bar), 100 yd out.
    float const gatherDistance = Ab::GatherDistance(map);
    Ab::Pt const banner = map.pos[node];
    // The start rooms of the map (Arathi Basin: WorldSafeLocs 890 and 889).
    float const dx = map.start[team & 1].x - banner.x;
    float const dy = map.start[team & 1].y - banner.y;
    float const len = std::sqrt(dx * dx + dy * dy);
    if (len < 1.0f)
        return { banner.x, banner.y, banner.z };

    float const f = gatherDistance / len;
    return { banner.x + dx * f, banner.y + dy * f, banner.z };
}

bool IsHoldKind(Ab::Kind kind)
{
    return kind == Ab::Kind::HOLD_POST || kind == Ab::Kind::HOLD_LOOSE || kind == Ab::Kind::HOLD_CAPTURE ||
           kind == Ab::Kind::STAGE || kind == Ab::Kind::RETREAT;
}

// The position an option leads to; false when it has none right now (the next option is tried).
bool Resolve(Ab::Option const& option, Ab::Map const& map, Player* bot, Member const& me, uint32 teamIdx, Spot& out)
{
    switch (option.kind)
    {
        case Ab::Kind::RESTORE:
        case Ab::Kind::GHOST:
        case Ab::Kind::TAKE:
        case Ab::Kind::CLOSE_OUT:
        case Ab::Kind::RAID:
        case Ab::Kind::REINFORCE:
        case Ab::Kind::SUPPORT_HUMAN:
        {
            if (option.node >= map.nodeCount)
                return false;
            // Stand a few yards from the banner (a cast from the pole itself is moved off it by the capture code); the
            // side differs per bot.
            float const angle = TWO_PI * float(BotSkillMath::Mix(me.guid) % 360) / 360.0f;
            out = RingPoint(bot, map, option.node, angle, CAPTURE_STANDOFF);
            return true;
        }
        case Ab::Kind::HOLD_CAPTURE:
        case Ab::Kind::HOLD_POST:
        {
            if (option.node >= map.nodeCount)
                return false;
            out = RingPoint(bot, map, option.node, PostAngle(me, option.node),
                            POST_RING_MIN + (POST_RING_MAX - POST_RING_MIN) * float(me.groupIndex % 3) / 3.0f);
            return true;
        }
        case Ab::Kind::HOLD_LOOSE:
        {
            if (option.node >= map.nodeCount)
                return false;
            out = RingPoint(bot, map, option.node, PostAngle(me, option.node), RingRadius(me, LOOSE_RING_MIN, LOOSE_RING_SPAN));
            return true;
        }
        case Ab::Kind::GATHER:
        {
            if (option.node >= map.nodeCount)
                return false;
            Spot const gather = GatherPoint(map, teamIdx, option.node);
            float const angle = TWO_PI * float(BotSkillMath::Mix(me.guid) % 360) / 360.0f;
            out = { gather.x + 4.0f * std::cos(angle), gather.y + 4.0f * std::sin(angle), gather.z };
            return true;
        }
        case Ab::Kind::RETREAT:
        case Ab::Kind::STAGE:
        {
            if (option.node >= map.nodeCount)
                return false;
            out = RingPoint(bot, map, option.node, PostAngle(me, option.node), RingRadius(me, STAGE_RING_MIN, STAGE_RING_SPAN));
            return true;
        }
        case Ab::Kind::HOLD_QUIET:
            return false;
    }
    return false;
}

// The map this plan plays for a battleground, nullptr for any other: Arathi Basin and, since BT-552, Battle for Gilneas.
Ab::Map const* PlayedMap(uint32 mapId)
{
    return BgNodeMap::ByMapId(mapId);
}

bool CommonGuards(Battleground* bg, Player* bot)
{
    return bg && bot && PlayedMap(bg->GetMapId()) && bot->IsAlive();
}

Ab::DecisionContext MakeContext(Record const& r, TeamPlan const& team, Member const& me)
{
    Ab::DecisionContext c;
    c.map = r.map;
    c.job = me.slot.job;
    c.slotNode = me.slot.node;
    c.tier = me.tier;
    c.regime = team.regime;
    for (uint8 n = 0; n < r.map->nodeCount; ++n)
    {
        c.sit[n] = team.sit[n];
        c.captureAgeMs[n] = team.captureAgeMs[n];
    }
    c.restoreResponder = me.restoreResponder;
    c.restoreNode = me.restoreNode;
    c.incResponder = me.incResponder;
    c.incNode = me.incNode;
    c.supportHuman = me.supportHuman;
    c.supportNode = me.supportNode;
    c.assaultNode = team.assaultNode[me.tier == Tier::EXPERT ? 1 : 0];
    c.nearestNode = team.nearestNode;
    c.closeOutNode = team.closeOutNode;
    c.raidNode = me.raidNode;
    c.raidStay = me.raidStay;
    c.stageNode = team.stageNode;
    c.retreatNode = me.retreatNode;
    c.ghostNode = me.ghostNode;
    c.groupGathered = me.slot.node < r.map->nodeCount ? team.gathered[me.slot.node] : true;
    // The lone defender of Battle for Gilneas does not wait for a group at the gather point.
    uint8 const side = &team == &r.team[1] ? Ab::TEAM_IDX_HORDE : Ab::TEAM_IDX_ALLIANCE;
    if (r.map->kind == BgNodeMap::Kind::GILNEAS && me.slot.node == Ab::HomeNode(side, *r.map))
        c.groupGathered = true;
    (void)r;
    return c;
}
// The enemies the team has seen near the centre stand of Eye of the Storm (a friend within sight) and the odds there.
void StandFacts(Record const& r, uint32 t, uint32& enemies25, int& odds40)
{
    TeamPlan const& team = r.team[t];
    Ab::Pt const stand = r.map->centre;
    unsigned friends40 = 0;
    for (auto const& entry : team.members)
        if (entry.second.alive && Dist2d(entry.second.x, entry.second.y, stand.x, stand.y) <= 40.0f)
            ++friends40;
    unsigned enemies40 = 0;
    enemies25 = 0;
    for (auto const& entry : r.team[1 - t].members)
    {
        Member const& e = entry.second;
        if (!e.alive)
            continue;
        float const d = Dist2d(e.x, e.y, stand.x, stand.y);
        if (d > 40.0f)
            continue;
        if (!e.seenByEnemy)
            continue;
        ++enemies40;
        if (d <= 25.0f)
            ++enemies25;
    }
    odds40 = int(friends40) - int(enemies40);
}

// BT-585 (sheet section 16): the flag decisions of Eye of the Storm as lists for the decision layer. D15 while the flag waits on its
// stand, D16 while our carrier runs, D17 while the enemy carries; each list's first option is the rule, an answer that stands
// replaces it (the flag layer of BgTacticsEY.cpp reads it through EyeView).
void PublishEyeFlag(Record& r, Battleground* bg, PvpMatch::State& match, uint32 t, uint32 nowMs)
{
    TeamPlan& team = r.team[t];
    AiPvp::Book& book = match.Side(t).ai.book;
    team.flagAction = team.efcAction = team.carrierBase = -1;
    if (!Ab::IsPresence(*r.map) || bg->GetStatus() != STATUS_IN_PROGRESS)
    {
        AiPvp::Withdraw(book, BgLeader::DecisionPoint::D15_EYE_FLAG);
        AiPvp::Withdraw(book, BgLeader::DecisionPoint::D16_EYE_CARRIER);
        AiPvp::Withdraw(book, BgLeader::DecisionPoint::D17_EYE_EFC);
        return;
    }

    BattlegroundEY* const ey = static_cast<BattlegroundEY*>(bg);
    uint8 const state = ey->GetFlagState();
    Member* carrier = nullptr;
    uint32 carrierSide = 2;
    if (state == EyFlag::FLAG_ON_PLAYER)
    {
        uint64 const picker = ey->GetFlagPickerGUID();
        for (uint32 side = 0; side < 2 && !carrier; ++side)
            for (auto& entry : r.team[side].members)
                if (entry.second.full == picker)
                {
                    carrier = &entry.second;
                    carrierSide = side;
                    break;
                }
    }

    bool const standOpen = state == EyFlag::FLAG_ON_BASE || state == EyFlag::FLAG_WAIT_RESPAWN;
    bool const ours = carrier && carrierSide == t;
    bool const theirs = carrier && carrierSide != t;

    if (standOpen)
    {
        EyFlag::ActionInput in;
        in.ourBases = team.occupied;
        in.theirBases = r.team[1 - t].occupied;
        uint32 enemies25 = 0;
        StandFacts(r, t, enemies25, in.oddsAtStand);
        in.enemiesNearStand = enemies25;
        in.tier = Tier::GOOD;
        AiPvp::Publish(book, BgLeader::EyeFlagOffer(in), { "flag_state:" + std::to_string(unsigned(state)),
                       "enemies_at_stand:" + std::to_string(enemies25), "odds_at_stand:" + std::to_string(in.oddsAtStand) }, true, nowMs);
        if (std::optional<uint8> const id = AiPvp::Override(book, BgLeader::DecisionPoint::D15_EYE_FLAG, nowMs))
            team.flagAction = *id;
    }
    else
        AiPvp::Withdraw(book, BgLeader::DecisionPoint::D15_EYE_FLAG);

    if (ours)
    {
        EyFlag::BaseFact facts[EyFlag::BASE_COUNT];
        for (uint8 n = 0; n < r.map->nodeCount && n < EyFlag::BASE_COUNT; ++n)
        {
            EyFlag::BaseFact& f = facts[n];
            f.owned = Ab::IsOccupiedBy(r.nodes[n].state, TeamOfIndex(t));
            f.distance = std::sqrt((carrier->x - r.map->pos[n].x) * (carrier->x - r.map->pos[n].x) +
                                   (carrier->y - r.map->pos[n].y) * (carrier->y - r.map->pos[n].y) +
                                   (carrier->z - r.map->pos[n].z) * (carrier->z - r.map->pos[n].z));
            bool const inside = Ab::InCircle(n, carrier->x, carrier->y, carrier->z, *r.map);
            f.guards = team.friendNear40[n] > (inside ? 1u : 0u) ? team.friendNear40[n] - (inside ? 1u : 0u) : 0u;
            f.enemies = team.enemyNear40[n];
            f.margin = EyLogic::Margin(r.nodes[n].bar, TeamOfIndex(t));
            f.nearOwnStart = Ab::IsNearNode(TeamOfIndex(t), n, *r.map);
        }
        unsigned enemiesNear = 0;
        for (auto const& entry : r.team[1 - t].members)
            if (entry.second.alive && Dist2d(entry.second.x, entry.second.y, carrier->x, carrier->y) <= CARRIER_ENEMY_RADIUS)
                ++enemiesNear;
        uint32 escorts = 0;
        for (auto const& entry : team.members)
            if (entry.second.alive && entry.second.guid != carrier->guid && entry.second.bot && !entry.second.healer &&
                Dist2d(entry.second.x, entry.second.y, carrier->x, carrier->y) <= CARRIER_ESCORT_RADIUS)
                ++escorts;
        std::vector<EyFlag::DestOption> const list = EyFlag::Destinations(facts, escorts > 0, enemiesNear);
        if (!list.empty())
        {
            AiPvp::Publish(book, BgLeader::EyeCarrierOffer(list), { "enemies_near_carrier:" + std::to_string(enemiesNear) }, true, nowMs);
            if (std::optional<uint8> const id = AiPvp::Override(book, BgLeader::DecisionPoint::D16_EYE_CARRIER, nowMs))
                if (*id < EyFlag::BASE_COUNT && facts[*id].owned)
                    team.carrierBase = *id;
        }
        else
            AiPvp::Withdraw(book, BgLeader::DecisionPoint::D16_EYE_CARRIER);
    }
    else
        AiPvp::Withdraw(book, BgLeader::DecisionPoint::D16_EYE_CARRIER);

    if (theirs)
    {
        EyFlag::EfcInput in;
        in.theirBases = r.team[1 - t].occupied;
        float nearest = 0.0f;
        bool any = false;
        for (auto const& entry : team.members)
        {
            Member const& m = entry.second;
            if (!IsFreeMember(m))
                continue;
            float const d = Dist2d(m.x, m.y, carrier->x, carrier->y);
            if (!any || d < nearest)
            {
                any = true;
                nearest = d;
            }
        }
        in.distanceToEfc = nearest;
        AiPvp::Publish(book, BgLeader::EyeEfcOffer(in), { "their_bases:" + std::to_string(in.theirBases) }, true, nowMs);
        if (std::optional<uint8> const id = AiPvp::Override(book, BgLeader::DecisionPoint::D17_EYE_EFC, nowMs))
            team.efcAction = *id;
    }
    else
        AiPvp::Withdraw(book, BgLeader::DecisionPoint::D17_EYE_EFC);
}
}  // namespace

namespace AbPlan
{
bool Enabled(Battleground* bg)
{
    return bg && PlayedMap(bg->GetMapId());
}

bool Decide(Battleground* bg, Player* bot, uint8 strategyAlliance, uint8 strategyHorde, Objective& out)
{
    if (!CommonGuards(bg, bot))
        return false;

    uint32 const now = getMSTime();
    std::lock_guard<std::mutex> guard(g_lock);
    Record& r = RecordOf(bg->GetInstanceID(), now, *PlayedMap(bg->GetMapId()));
    Member* me = Prepare(r, bg, bot, now, strategyAlliance, strategyHorde);
    if (!me || !me->assigned)
        return false;

    // Set first: a failed decision must not make the bot re-decide at every tick.
    me->seenEpoch = r.epoch;

    uint32 const t = TeamIndexOf(bot);
    TeamPlan const& team = r.team[t];
    Ab::DecisionContext const c = MakeContext(r, team, *me);

    std::vector<Ab::Option> const options = Ab::Options(c);
    for (Ab::Option const& option : options)
    {
        Spot target;
        if (!Resolve(option, *r.map, bot, *me, t, target))
            continue;

        out.x = target.x;
        out.y = target.y;
        out.z = target.z;
        out.kind = option.kind;
        out.job = me->slot.job;
        out.node = option.node;
        out.hold = IsHoldKind(option.kind);
        me->kind = option.kind;
        me->optNode = option.node;
        me->hold = out.hold;
        me->hasTarget = true;
        me->tx = target.x;
        me->ty = target.y;
        me->tz = target.z;

        if (me->lastLogKind != option.kind || !me->lastLogMs || now - me->lastLogMs >= LOG_REPEAT_MS)
        {
            me->lastLogKind = option.kind;
            me->lastLogMs = now ? now : 1;
            SF_LOG_DEBUG("playerbots", "[BGAB] %s job=%s node=%s tier=%s regime=%s -> %s %s (%.0f,%.0f,%.0f) score=%d options=%u",
                         bot->GetName().c_str(), Ab::JobName(me->slot.job), Ab::NodeWord(me->slot.node, *r.map), TierWord(me->tier),
                         Ab::RegimeName(team.regime), Ab::KindName(option.kind), Ab::NodeWord(option.node, *r.map), target.x, target.y,
                         target.z, option.score, uint32(options.size()));
        }
        return true;
    }

    me->hasTarget = false;
    me->hold = false;
    return false;
}

bool PlanChanged(Battleground* bg, Player* bot, uint8 strategyAlliance, uint8 strategyHorde)
{
    if (!CommonGuards(bg, bot))
        return false;

    uint32 const now = getMSTime();
    std::lock_guard<std::mutex> guard(g_lock);
    Record& r = RecordOf(bg->GetInstanceID(), now, *PlayedMap(bg->GetMapId()));
    Member* me = Prepare(r, bg, bot, now, strategyAlliance, strategyHorde);
    if (!me || !me->assigned || me->seenEpoch == r.epoch)
        return false;

    // A bot in the middle of the capture cast is never moved (sheet section 4).
    if (me->casting)
        return false;

    // Core: follow the plan state when it changes - late for the casual bots, and a stance change of the leader one refresh
    // later still (BT-481).
    uint32 const lag = BgPlan::AdoptLagMs(me->tier) + (r.epoch == r.stanceEpoch ? BgLeader::StanceExtraLagMs(me->tier) : 0u);
    return now - r.epochChangedMs >= lag;
}

bool AtPost(Battleground* bg, Player* bot)
{
    if (!CommonGuards(bg, bot))
        return false;

    std::lock_guard<std::mutex> guard(g_lock);
    auto const record = g_records.find(bg->GetInstanceID());
    if (record == g_records.end())
        return false;

    Member* me = FindMember(record->second.team[TeamIndexOf(bot)], bot->GetGUIDLow());
    if (!me || !me->assigned || !me->hold || !me->hasTarget)
        return false;

    return Dist2d(bot->GetPositionX(), bot->GetPositionY(), me->tx, me->ty) <= AT_POST_YARDS;
}

bool StaysWhileFriendCasts(Battleground* bg, Player* bot)
{
    if (!CommonGuards(bg, bot))
        return false;

    std::lock_guard<std::mutex> guard(g_lock);
    auto const record = g_records.find(bg->GetInstanceID());
    if (record == g_records.end())
        return false;

    Member* me = FindMember(record->second.team[TeamIndexOf(bot)], bot->GetGUIDLow());
    return me && me->assigned;
}

bool MayCapture(Battleground* bg, Player* bot, uint8 node)
{
    if (!CommonGuards(bg, bot) || node >= PlayedMap(bg->GetMapId())->nodeCount)
        return true;

    uint32 const now = getMSTime();
    std::lock_guard<std::mutex> guard(g_lock);
    auto const record = g_records.find(bg->GetInstanceID());
    if (record == g_records.end())
        return true;

    Record& r = record->second;
    TeamPlan& team = r.team[TeamIndexOf(bot)];
    Member* me = FindMember(team, bot->GetGUIDLow());
    if (!me || !me->assigned)
        return true;

    // A raider never starts a capture while two or more defenders are awake near the banner.
    if (me->slot.job == Ab::Job::RAIDER && team.enemyNear40[node] > Ab::RAID_MAX_DEFENDERS)
        return false;

    // Expert rule: a node that was flipped back twice in three minutes is not re-assaulted unless the odds improved.
    if (me->tier == Tier::EXPERT && team.sit[node] != Ab::NodeSituation::UNDER_ASSAULT &&
        r.flips.Blocked(node, now, int(team.friendNear40[node]) - int(team.enemyNear40[node])))
        return false;

    return true;
}

bool MayUseBuff(Battleground* bg, Player* bot, float buffX, float buffY)
{
    if (!CommonGuards(bg, bot))
        return true;

    std::lock_guard<std::mutex> guard(g_lock);
    auto const record = g_records.find(bg->GetInstanceID());
    if (record == g_records.end())
        return true;

    TeamPlan& team = record->second.team[TeamIndexOf(bot)];
    Member* me = FindMember(team, bot->GetGUIDLow());
    if (!me || !me->assigned || me->slot.job != Ab::Job::GUARD || me->slot.node >= team.map->nodeCount)
        return true;

    // Casual guards stand on the buff spot (on purpose); the others use a buff inside their leash or when the node is quiet.
    if (me->tier == Tier::CASUAL)
        return true;

    uint8 const node = me->slot.node;
    if (Dist2d(buffX, buffY, team.map->pos[node].x, team.map->pos[node].y) <= Ab::GUARD_LEASH)
        return true;

    return team.sit[node] == Ab::NodeSituation::OURS && team.enemyNear50[node] == 0;
}

uint64 CapperGuid(Battleground* bg, Player* bot)
{
    if (!CommonGuards(bg, bot))
        return 0;

    std::lock_guard<std::mutex> guard(g_lock);
    auto const record = g_records.find(bg->GetInstanceID());
    if (record == g_records.end())
        return 0;

    TeamPlan& team = record->second.team[TeamIndexOf(bot)];
    Member* me = FindMember(team, bot->GetGUIDLow());
    if (!me || !me->assigned || me->slot.job == Ab::Job::RAIDER || !BotSkillMath::Uses(me->tier, Tier::GOOD))
        return 0;

    // The capper nearest to the bot, within reach. (The list is the enemies casting at a banner we hold, rebuilt with
    // the record: no scan here.)
    Capper const* best = nullptr;
    float bestDist = 0.0f;
    for (Capper const& c : team.cappers)
    {
        float const d = Dist2d(bot->GetPositionX(), bot->GetPositionY(), c.x, c.y);
        if (d <= CAPPER_REACH && (!best || d < bestDist))
        {
            best = &c;
            bestDist = d;
        }
    }
    return best ? best->guid : 0;
}

bool GetRaid(Battleground* bg, Player* bot, Raid& out)
{
    if (!CommonGuards(bg, bot))
        return false;

    std::lock_guard<std::mutex> guard(g_lock);
    auto const record = g_records.find(bg->GetInstanceID());
    if (record == g_records.end())
        return false;

    uint32 const t = TeamIndexOf(bot);
    TeamPlan& team = record->second.team[t];
    Member* me = FindMember(team, bot->GetGUIDLow());
    // A raider on its raid, or one of the expert pair on its ghost (BT-552): the same stealth approach.
    bool const raiding = me && me->assigned && me->slot.job == Ab::Job::RAIDER && me->kind == Ab::Kind::RAID;
    bool const ghosting = me && me->assigned && me->kind == Ab::Kind::GHOST;
    if (!raiding && !ghosting)
        return false;
    if (me->optNode >= team.map->nodeCount)
        return false;

    out.active = true;
    out.node = me->optNode;
    out.expert = me->tier == Tier::EXPERT;
    out.distance = Dist2d(bot->GetPositionX(), bot->GetPositionY(), team.map->pos[me->optNode].x, team.map->pos[me->optNode].y);
    out.lone = 0;
    if (team.enemyNear40[me->optNode] == 1)
    {
        for (auto const& entry : record->second.team[1 - t].members)
        {
            Member const& e = entry.second;
            if (e.alive && NearNode(*record->second.map, e, me->optNode, NEAR_40))
                out.lone = e.full;
        }
    }
    return true;
}

uint32 HealPriority(Player* healer, Player* target)
{
    if (!healer || !target || !healer->InBattleground() || !PlayedMap(healer->GetMapId()))
        return 0;

    Battleground* bg = healer->GetBattleground();
    if (!bg || !BotSkill::Uses(healer, Tier::GOOD))
        return 0;

    // Eye of the Storm (BT-585): the flag carrier of the healer's team first (a capture is worth 75 to 500 and the carrier takes no extra
    // damage but runs through the enemy), above the pickup cast.
    if (target->HasAura(BG_EY_NETHERSTORM_FLAG_SPELL) && target->GetBGTeam() == healer->GetBGTeam())
        return CARRIER_HEAL_POINTS;

    // The player in the middle of the capture cast first: an 8 second cast that any hit stops.
    Spell* spell = target->GetCurrentSpell(CURRENT_GENERIC_SPELL);
    if (spell && spell->m_spellInfo && spell->m_spellInfo->Id == CAPTURE_SPELL_ID)
        return CAPTURE_HEAL_POINTS;

    // Next, the player under attack next to a banner of the healer's team.
    if (target->getAttackers().empty())
        return 0;

    Ab::Map const& map = *PlayedMap(healer->GetMapId());
    uint8 const team = uint8(TeamIndexOf(healer));
    for (uint8 n = 0; n < map.nodeCount; ++n)
    {
        if (!Ab::IsOccupiedBy(NodeStatusOf(bg, n), team))
            continue;
        if (Dist2d(target->GetPositionX(), target->GetPositionY(), map.pos[n].x, map.pos[n].y) <= HEAL_NEAR_BANNER)
            return ATTACKED_HEAL_POINTS;
    }
    return 0;
}

bool DeliverCalls(PlayerbotAI* botAI)
{
    if (!g_pendingTotal.load(std::memory_order_relaxed) || !botAI)
        return false;

    Player* bot = botAI->GetBot();
    if (!bot || !bot->InBattleground() || !PlayedMap(bot->GetMapId()))
        return false;
    Ab::Map const* const map = PlayedMap(bot->GetMapId());

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

    CallText const& text = TextOf(call, *map);
    std::string const fallback = text.lines[(call.seq + call.guid) % text.lines.size()];
    std::map<std::string, std::string> placeholders = {
        { "%node", call.node }, { "%score", call.score }, { "%num", call.number }, { "%stance", call.stance }
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
    SF_LOG_DEBUG("playerbots", "[%s-CALL] %s says %s (%s)", map->tag, bot->GetName().c_str(), text.key.c_str(), fallback.c_str());
    return true;
}

bool GetEyeView(Battleground* bg, Player* bot, EyeView& out)
{
    if (!CommonGuards(bg, bot) || !Ab::IsPresence(*PlayedMap(bg->GetMapId())))
        return false;

    uint32 const now = getMSTime();
    std::lock_guard<std::mutex> guard(g_lock);
    auto const record = g_records.find(bg->GetInstanceID());
    if (record == g_records.end() || !record->second.baseline)
        return false;

    Record const& r = record->second;
    uint32 const t = TeamIndexOf(bot);
    TeamPlan const& team = r.team[t];
    out = EyeView();
    out.valid = true;
    out.regime = uint8(team.regime);
    out.yields = team.regime == Ab::Regime::CLOSE_OUT || team.regime == Ab::Regime::PANIC;
    out.ourBases = team.occupied;
    out.theirBases = r.team[1 - t].occupied;
    for (uint8 n = 0; n < r.map->nodeCount && n < 4; ++n)
    {
        out.held[n] = Ab::IsOccupiedBy(r.nodes[n].state, TeamOfIndex(t));
        out.bar[n] = r.nodes[n].bar;
        out.friends[n] = team.friendNear40[n];
        out.enemies[n] = team.enemyNear40[n];
        out.heldForMs[n] = now - r.nodes[n].changedMs;
    }
    auto const itr = team.members.find(bot->GetGUIDLow());
    out.free = itr != team.members.end() && IsFreeMember(itr->second);

    // The centre stand: enemies the team has seen (a friend within sight, the Gilneas rule) near it, and the odds there.
    StandFacts(r, t, out.enemiesStand25, out.oddsStand40);
    out.flagAction = team.flagAction;
    out.efcAction = team.efcAction;
    out.carrierBase = team.carrierBase;
    return true;
}

bool FreeRank(Battleground* bg, Player* bot, float x, float y, bool healersOnly, bool expertsOnly, uint32& rank)
{
    if (!CommonGuards(bg, bot) || !Ab::IsPresence(*PlayedMap(bg->GetMapId())))
        return false;

    std::lock_guard<std::mutex> guard(g_lock);
    auto const record = g_records.find(bg->GetInstanceID());
    if (record == g_records.end())
        return false;

    TeamPlan const& team = record->second.team[TeamIndexOf(bot)];
    auto const me = team.members.find(bot->GetGUIDLow());
    auto const eligible = [&](Member const& m)
    {
        return IsFreeMember(m) && (!healersOnly || m.healer) && (!expertsOnly || m.tier == Tier::EXPERT);
    };
    if (me == team.members.end() || !eligible(me->second))
        return false;

    float const mine = Dist2d(me->second.x, me->second.y, x, y);
    rank = 0;
    for (auto const& entry : team.members)
    {
        Member const& m = entry.second;
        if (entry.first == me->first || !eligible(m))
            continue;
        float const d = Dist2d(m.x, m.y, x, y);
        if (d < mine || (d == mine && entry.first < me->first))
            ++rank;
    }
    return true;
}

bool SurroundingsOf(Battleground* bg, Player* carrier, uint32& enemiesNear, uint32& escortsNear)
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
    enemiesNear = 0;
    escortsNear = 0;
    for (auto const& entry : record->second.team[1 - t].members)
        if (entry.second.alive && within(entry.second, CARRIER_ENEMY_RADIUS))
            ++enemiesNear;
    for (auto const& entry : record->second.team[t].members)
    {
        Member const& m = entry.second;
        // A healer beside the carrier heals, it does not peel: only fighters count as an escort.
        if (m.alive && m.guid != carrier->GetGUIDLow() && m.bot && !m.healer && within(m, CARRIER_ESCORT_RADIUS))
            ++escortsNear;
    }
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
}  // namespace AbPlan
