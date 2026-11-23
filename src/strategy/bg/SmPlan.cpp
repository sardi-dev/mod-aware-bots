/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE). See SmPlan.h for what this is.
 */

#include "SmPlan.h"

// DatabaseEnv.h must precede the battleground headers (see BattleGroundTactics.cpp).
#include "DatabaseEnv.h"

#include "AiPvpDecision.h"
#include "BgCalloutPolicy.h"
#include "BgLeaderAdapters.h"
#include "BotSkill.h"
#include "BotVoice.h"
#include "Battleground.h"
#include "BattlegroundSM.h"
#include "Creature.h"
#include "Config.h"
#include "Log.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotSpec.h"
#include "Playerbots.h"
#include "PvpConfig.h"
#include "PvpMatchState.h"
#include "Timer.h"
#include "WorldSession.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Sm = SmLogic;

namespace
{
using Tier = BotSkillMath::Tier;

constexpr uint32 LOG_REPEAT_MS = 30000;
// The opening bonus counts while the carts are still near their start (the first 90 s of play).
constexpr uint32 OPENING_WINDOW_MS = 90000;
// A switch stand is walked to within this many yards before the click (the NPC is 1.7 to 1.9 yd from the mesh).
constexpr float ENEMY_AT_SWITCH_RADIUS = 20.0f;
// The call-outs: a line that was not said this long after it was due is dropped, a queued one is dropped after this long.
constexpr uint32 STALE_CALL_MS = 4000;
constexpr uint32 PENDING_MAX_AGE_MS = 15000;
// An "inc" for the same cart is said at most this often; a group of this many enemies at a cart is worth one.
constexpr uint32 INC_REPEAT_MS = 15000;
constexpr uint32 INC_MIN_ENEMIES = 2;
// A switch call follows a click of the holder this long.
constexpr uint32 SWITCH_CALL_WINDOW_MS = 15000;
// A wipe: the main group had this many bodies at the cart a refresh ago and none now; one wipe call every this long.
constexpr uint32 WIPE_MIN_BODIES = 2;
constexpr uint32 WIPE_REPEAT_MS = 30000;

struct Member
{
    uint32 guid = 0;
    uint64 full = 0;
    bool bot = false;
    bool alive = false;
    bool present = false;
    bool stealth = false;
    bool mounted = false;
    uint8 cls = 0;
    uint8 level = 0;
    bool healer = false;
    bool tank = false;
    bool feral = false;
    Tier tier = Tier::GOOD;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    // The plan's answer for this bot.
    uint8 cart = Sm::CART_NONE;
    SmPlan::Job job = SmPlan::Job::NONE;
    uint8 switchTrack = Sm::TRACK_NONE;
    Sm::SwitchAct switchAct = Sm::SwitchAct::NONE;
    uint32 seenEpoch = 0;
    uint32 keepRefreshes = 0;          // BT-481: a casual bot keeps its old cart this many refreshes after a stance change
    uint32 lastLogMs = 0;
};

struct CartInfo
{
    bool live = false;
    uint64 guid = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    uint8 keeper = Sm::TEAM_NONE;
    Sm::Route const* route = nullptr;
    float along = 0.0f;
    float remainingS = 0.0f;
    bool before = false;
    float toJunction = 0.0f;
    uint32 changedMs = 0;               // the keeper last changed
};

enum class Call : uint8
{
    OPENING = 0,
    INC,
    TAKEN,
    LOST,
    SWITCH,
    DOCKED,
    STANCE,
    WIPED
};

struct Pending
{
    uint32 guid = 0;
    Call kind = Call::INC;
    uint32 dueMs = 0;
    uint32 seq = 0;
    std::string node;
    std::string number;
    std::string stance;
    std::string main;
    std::string pair;
    std::string depot;
    // BT-483: what the line tells the team, kept as an observation of the side when it is said.
    bool observe = false;
    PvpObs::Kind obsKind = PvpObs::Kind::INCOMING;
    uint8 obsPlace = PvpObs::PLACE_UNKNOWN;
    uint8 obsCount = 0;
};

struct TeamPlan
{
    std::unordered_map<uint32, Member> members;
    uint32 size = 0;
    uint32 bots = 0;
    bool hasHealer = false;
    BgPlan::Plan rulePlan = BgPlan::Plan::BALANCED;
    BgLeader::Stance ruleStance = BgLeader::Stance::BALANCED;
    BgLeader::Stance stance = BgLeader::Stance::BALANCED;
    uint32 leader = 0;
    bool stanceCallDue = false;
    uint8 mainCart = Sm::CART_NONE;
    std::vector<Sm::CartOption> options;
    int bar[Sm::CART_COUNT] = { Sm::BAR_START, Sm::BAR_START, Sm::BAR_START };
    uint32 friends[Sm::CART_COUNT] = { 0, 0, 0 };
    uint32 enemies[Sm::CART_COUNT] = { 0, 0, 0 };
    uint32 enemiesAtSwitch[Sm::TRACK_COUNT] = { 0, 0 };
    uint32 holder[Sm::TRACK_COUNT] = { 0, 0 };         // the switch holder of each track
    // Call-outs.
    BgPlan::CallLimiter limiter;
    bool openingCalled = false;
    uint8 prevKeeper[Sm::CART_COUNT] = { Sm::TEAM_NONE, Sm::TEAM_NONE, Sm::TEAM_NONE };
    uint32 prevEnemies[Sm::CART_COUNT] = { 0, 0, 0 };
    uint32 lastIncMs[Sm::CART_COUNT] = { 0, 0, 0 };
    uint32 prevMainBodies = 0;
    uint32 lastWipeMs = 0;
    uint32 recentHolder[Sm::TRACK_COUNT] = { 0, 0 };   // the last holder of each track and when it last held the duty
    uint32 recentHolderMs[Sm::TRACK_COUNT] = { 0, 0 };
    uint32 switchSaidMs[Sm::TRACK_COUNT] = { 0, 0 };
};

struct Record : BgPlan::RecordBase
{
    CartInfo carts[Sm::CART_COUNT];
    bool switchOpened[Sm::TRACK_COUNT] = { false, false };
    bool switchSeen[Sm::TRACK_COUNT] = { false, false };
    uint32 switchChangedMs[Sm::TRACK_COUNT] = { 0, 0 };
    uint32 lastRefreshElapsedMs = 0;
    uint32 lastStateLogMs = 0;
    uint32 stanceEpoch = 0;     // BT-481: the epoch a stance change made; a casual bot adopts that one a refresh later
    uint32 docks[2] = { 0, 0 };
    uint32 score[2] = { 0, 0 };
    // Events of this refresh, said by the call-outs and then cleared.
    bool cartFresh[Sm::CART_COUNT] = { false, false, false };
    bool dockEvent[Sm::CART_COUNT] = { false, false, false };
    uint8 dockKeeper[Sm::CART_COUNT] = { Sm::TEAM_NONE, Sm::TEAM_NONE, Sm::TEAM_NONE };
    bool switchEvent[Sm::TRACK_COUNT] = { false, false };
    std::vector<Pending> pending;
    TeamPlan team[2];
};

BgPlan::RecordTable<Record> g_table;
std::mutex& g_lock = g_table.lock;
std::unordered_map<uint32, Record>& g_records = g_table.records;
std::atomic<uint32> g_pendingTotal{ 0 };

uint32 TeamIndexOf(Player* player)
{
    return player->GetBGTeam() == ALLIANCE ? uint32(TEAM_ALLIANCE) : uint32(TEAM_HORDE);
}

char const* TierWord(Tier tier)
{
    return tier == Tier::EXPERT ? "expert" : tier == Tier::GOOD ? "good" : "casual";
}

char const* KeeperWord(uint8 keeper)
{
    return keeper == Sm::TEAM_IDX_ALLIANCE ? "A" : keeper == Sm::TEAM_IDX_HORDE ? "H" : "-";
}

Member* FindMember(TeamPlan& team, uint32 guid)
{
    auto const itr = team.members.find(guid);
    return itr == team.members.end() ? nullptr : &itr->second;
}

// The four routes a cart can ride, built once: [cart][opened]. The waterfall cart has the one.
Sm::Route const& RouteOf(uint8 cart, bool opened)
{
    static Sm::Route const routes[Sm::CART_COUNT][2] = {
        { Sm::BuildRoute(Sm::CART_EAST, false), Sm::BuildRoute(Sm::CART_EAST, true) },
        { Sm::BuildRoute(Sm::CART_WATERFALL, false), Sm::BuildRoute(Sm::CART_WATERFALL, false) },
        { Sm::BuildRoute(Sm::CART_NORTH, false), Sm::BuildRoute(Sm::CART_NORTH, true) }
    };
    return routes[cart][opened ? 1 : 0];
}

// A BgCreatures slot as a creature, or null. Looked up through the map and not Battleground::GetBGCreature, which logs an error on
// every call between a cart being deleted at its depot and the next one being summoned.
Creature* Slot(Battleground* bg, uint32 slot)
{
    if (slot >= bg->BgCreatures.size() || !bg->BgCreatures[slot])
        return nullptr;
    return bg->GetBgMap()->GetCreature(bg->BgCreatures[slot]);
}

// ---------------------------------------------------------------------------------------------------------------------
// The record.
// ---------------------------------------------------------------------------------------------------------------------
Record& RecordOf(uint32 instanceId, uint32 nowMs)
{
    return g_table.Of(instanceId, nowMs, [](Record const&) {});
}

void ReadMember(Member& m, Player* p)
{
    m.alive = p->IsAlive();
    m.present = true;
    m.level = p->getLevel();
    m.x = p->GetPositionX();
    m.y = p->GetPositionY();
    m.z = p->GetPositionZ();
    m.stealth = p->HasAuraType(SPELL_AURA_MOD_STEALTH);
    m.mounted = p->IsMounted();
}

void InitMember(Member& m, Player* p)
{
    m.guid = p->GetGUIDLow();
    m.full = p->GetGUID();
    m.bot = p->GetSession() && p->GetSession()->IsBot();
    m.cls = p->getClass();
    m.healer = PlayerBotSpec::IsHeal(p, true);
    m.tank = PlayerBotSpec::IsTank(p, true);
    m.feral = m.cls == BgPlan::CLS_DRUID && BotGetSpecialization(p) == SPEC_DRUID_FERAL;
    m.tier = m.bot ? BotSkill::TierOf(p) : Tier::EXPERT;
}

void ReadRoster(Record& r, Battleground* bg)
{
    for (TeamPlan& team : r.team)
    {
        for (auto& entry : team.members)
            entry.second.present = false;
        team.size = 0;
        team.bots = 0;
        team.hasHealer = false;
    }

    for (auto const& ref : bg->GetBgMap()->GetPlayers())
    {
        Player* p = ref.GetSource();
        if (!p || !p->IsInWorld() || p->GetBattlegroundId() != bg->GetInstanceID() || p->IsGameMaster())
            continue;

        TeamPlan& team = r.team[TeamIndexOf(p)];
        auto found = team.members.find(p->GetGUIDLow());
        if (found == team.members.end())
        {
            found = team.members.emplace(p->GetGUIDLow(), Member()).first;
            InitMember(found->second, p);
        }

        Member& m = found->second;
        ReadMember(m, p);
        if (m.alive)
        {
            ++team.size;
            if (m.bot)
                ++team.bots;
            if (m.healer)
                team.hasHealer = true;
        }
    }

    for (TeamPlan& team : r.team)
        for (auto itr = team.members.begin(); itr != team.members.end();)
            itr = itr->second.present ? std::next(itr) : team.members.erase(itr);
}

// The carts and the switches as the battleground shows them: the creature, its aura, its place on the rail.
void ReadCarts(Record& r, Battleground* bg, uint32 nowMs, bool& changed)
{
    for (uint8 t = 0; t < Sm::TRACK_COUNT; ++t)
    {
        Creature* npc = Slot(bg, SM_TRACK_SWITCH_EAST + t);
        if (!npc)
            continue;
        bool const opened = npc->HasAura(BG_SM_TRACK_SWITCH_OPENED);
        if (!r.switchSeen[t] || opened != r.switchOpened[t])
        {
            if (r.switchSeen[t])
            {
                r.switchChangedMs[t] = nowMs ? nowMs : 1;
                changed = true;
                r.switchEvent[t] = true;
                SF_LOG_DEBUG("playerbots", "[BGSM] switch inst=%u %s now %s", r.instanceId, t == Sm::TRACK_EAST ? "east" : "north",
                             opened ? "opened" : "closed");
            }
            r.switchSeen[t] = true;
            r.switchOpened[t] = opened;
        }
    }

    for (uint8 c = 0; c < Sm::CART_COUNT; ++c)
    {
        CartInfo& info = r.carts[c];
        Creature* cart = Slot(bg, NPC_SM_MINE_CART_1 + c);
        if (cart && !cart->IsAlive())
            cart = nullptr;

        if (!cart)
        {
            if (info.live)
            {
                changed = true;
                r.dockEvent[c] = true;
                r.dockKeeper[c] = info.keeper;
                if (info.keeper < 2)
                    ++r.docks[info.keeper];
                SF_LOG_DEBUG("playerbots", "[BGSM] cart-gone inst=%u cart=%u keeper=%s score=%u-%u", r.instanceId, uint32(c),
                             KeeperWord(info.keeper), bg->GetTeamScore(TEAM_ALLIANCE), bg->GetTeamScore(TEAM_HORDE));
            }
            info.live = false;
            info.guid = 0;
            continue;
        }

        uint8 keeper = Sm::TEAM_NONE;
        if (cart->HasAura(BG_SM_CONTROL_VISUAL_ALLIANCE))
            keeper = Sm::TEAM_IDX_ALLIANCE;
        else if (cart->HasAura(BG_SM_CONTROL_VISUAL_HORDE))
            keeper = Sm::TEAM_IDX_HORDE;

        bool const fresh = !info.live || info.guid != cart->GetGUID();
        if (fresh)
        {
            changed = true;
            r.cartFresh[c] = true;
            // A new cart: the bar is back at 50 for both teams' models.
            for (TeamPlan& team : r.team)
                team.bar[c] = Sm::BAR_START;
            SF_LOG_DEBUG("playerbots", "[BGSM] cart-new inst=%u cart=%u", r.instanceId, uint32(c));
        }
        if (fresh || keeper != info.keeper)
        {
            info.changedMs = nowMs ? nowMs : 1;
            if (!fresh)
            {
                changed = true;
                SF_LOG_DEBUG("playerbots", "[BGSM] keeper inst=%u cart=%u %s->%s score=%u-%u", r.instanceId, uint32(c),
                             KeeperWord(info.keeper), KeeperWord(keeper), bg->GetTeamScore(TEAM_ALLIANCE),
                             bg->GetTeamScore(TEAM_HORDE));
            }
        }

        info.live = true;
        info.guid = cart->GetGUID();
        info.x = cart->GetPositionX();
        info.y = cart->GetPositionY();
        info.z = cart->GetPositionZ();
        info.keeper = keeper;

        // The route: the state the switch has now while the junction is ahead, afterwards the leg the cart is on.
        uint8 const track = Sm::TrackOf(c);
        bool const openedNow = track != Sm::TRACK_NONE && r.switchOpened[track];
        Sm::Route const& current = RouteOf(c, openedNow);
        info.before = Sm::BeforeJunction(current, info.x, info.y);
        Sm::Route const* route = &current;
        if (track != Sm::TRACK_NONE && !info.before)
        {
            Sm::Route const& other = RouteOf(c, !openedNow);
            if (Sm::Project(other, info.x, info.y).off < Sm::Project(current, info.x, info.y).off)
                route = &other;
        }
        info.route = route;
        info.along = Sm::Project(*route, info.x, info.y).along;
        info.remainingS = Sm::RemainingSeconds(*route, info.x, info.y);
        info.toJunction = info.before ? Sm::SecondsToJunction(*route, info.x, info.y) : 0.0f;
    }
}

// Bodies in the circles and the bar model of every team. A player counts for the first cart of the list within range only, like the core
// (BattlegroundSM.cpp:278), and a stealthed player not at all. Enemies count when the team saw them (a friend within SEEN_RADIUS) or a call-out
// placed them there; the bar is stepped by what the team counts.
void CountCarts(Record& r, PvpMatch::State* match, uint32 nowMs, float dtSeconds)
{
    for (uint32 t = 0; t < 2; ++t)
    {
        TeamPlan& team = r.team[t];
        TeamPlan& enemy = r.team[1 - t];
        for (uint8 c = 0; c < Sm::CART_COUNT; ++c)
            team.friends[c] = team.enemies[c] = 0;
        team.enemiesAtSwitch[0] = team.enemiesAtSwitch[1] = 0;

        for (auto const& entry : enemy.members)
        {
            Member const& e = entry.second;
            if (!e.alive)
                continue;
            bool seen = false;
            for (auto const& other : team.members)
                if (other.second.alive && Sm::SeenFrom(e.x, e.y, other.second.x, other.second.y))
                {
                    seen = true;
                    break;
                }
            if (!seen)
                continue;

            for (uint8 tr = 0; tr < Sm::TRACK_COUNT; ++tr)
                if (Sm::Dist2d(e.x, e.y, Sm::SWITCH_POS[tr].x, Sm::SWITCH_POS[tr].y) <= ENEMY_AT_SWITCH_RADIUS)
                    ++team.enemiesAtSwitch[tr];
            if (e.stealth)
                continue;
            for (uint8 c = 0; c < Sm::CART_COUNT; ++c)
                if (r.carts[c].live && Sm::InCircle({ r.carts[c].x, r.carts[c].y, r.carts[c].z }, e.x, e.y, e.z))
                {
                    ++team.enemies[c];
                    break;
                }
        }

        for (auto const& entry : team.members)
        {
            Member const& m = entry.second;
            if (!m.alive || m.stealth)
                continue;
            for (uint8 c = 0; c < Sm::CART_COUNT; ++c)
                if (r.carts[c].live && Sm::InCircle({ r.carts[c].x, r.carts[c].y, r.carts[c].z }, m.x, m.y, m.z))
                {
                    ++team.friends[c];
                    break;
                }
        }

        for (uint8 c = 0; c < Sm::CART_COUNT; ++c)
        {
            // What the side's call-outs say is there, never below what the team sees (the shared reconcile).
            if (match)
            {
                uint32 const reported = PvpObs::ReportedEnemies(match->Side(t).observations.items, c, nowMs);
                team.enemies[c] = PvpObs::ReconciledEnemies(team.enemies[c], reported);
            }
            if (!r.carts[c].live)
                continue;
            int const alliance = t == uint32(TEAM_ALLIANCE) ? int(team.friends[c]) : int(team.enemies[c]);
            int const horde = t == uint32(TEAM_ALLIANCE) ? int(team.enemies[c]) : int(team.friends[c]);
            team.bar[c] = Sm::ClampToKeeper(Sm::StepBar(team.bar[c], alliance, horde, dtSeconds), r.carts[c].keeper);
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// The leader's turn (BT-481): the stance of the race, called by the elected bot.
// ---------------------------------------------------------------------------------------------------------------------
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


// The leader of side t confirms the stance of the race (or applies a decision layer's answer: BT-502) and, after a change, sets the drift of
// the casual bots. Returns true when the stance changed after the gates opened.
bool LeaderTurn(Record& r, PvpMatch::State& match, uint32 t, TeamPlan& team, uint32 nowMs)
{
    Sm::RaceFacts race;
    race.ourScore = int(r.score[t]);
    race.theirScore = int(r.score[1 - t]);
    team.ruleStance = Sm::ChooseStance(team.rulePlan, race, team.ruleStance);

    if (!PvpConfig::LeaderEnabled())
    {
        team.leader = 0;
        team.stance = team.ruleStance;
        team.stanceCallDue = false;
        return false;
    }

    BgLeader::LeaderState& leader = match.Side(t).leader;
    // The answers of the decision layer are judged inside the tick, against the list this refresh just published.
    BgLeader::TickResult const tick = BgLeader::TickWith(leader, LeaderCandidates(team), team.ruleStance, Sm::StanceOffer(team.ruleStance, race),
                                                         nowMs, r.started,
                                                         [&](BgLeader::LeaderState& state) { AiPvpDecision::Receive(match, t, state, nowMs); });
    team.leader = tick.leader;
    team.stance = tick.stance;
    team.stanceCallDue = tick.callDue;

    if (tick.change != BgLeader::Change::NONE || tick.stanceChanged)
        SF_LOG_DEBUG("playerbots", "[BGSM-LEAD] inst=%u gen=%u %s leader=%u change=%u stance=%s epoch=%u version=%u", r.instanceId,
                     match.Key().generation, t == uint32(TEAM_ALLIANCE) ? "A" : "H", tick.leader, uint32(tick.change),
                     BgLeader::StanceName(tick.stance), leader.election.epoch, leader.stanceVersion);

    if (!tick.stanceChanged || !r.started)
        return false;

    for (auto& entry : team.members)
    {
        Member& m = entry.second;
        m.keepRefreshes = m.bot && m.guid != tick.leader ? BgLeader::RoleKeepRefreshes(m.tier, m.guid, leader.stanceSeq) : 0;
    }
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// The jobs: the cart of every bot and the switch holders.
// ---------------------------------------------------------------------------------------------------------------------
void PublishLists(Record& r, PvpMatch::State& match, uint32 t, uint32 nowMs);

int ClaimBonus(PvpMatch::State* match, uint32 t, uint8 cart, uint32 nowMs)
{
    if (!match)
        return 0;
    int best = 0;
    for (PvpObs::Observation const& o : match->Side(t).observations.items)
        if (o.kind == PvpObs::Kind::CLAIM && o.place == cart && PvpObs::IsFresh(o, nowMs))
            best = std::max(best, int(BgCallout::CLAIM_BONUS_POINTS * PvpObs::ConfidencePercent(o, nowMs) / 100));
    return best;
}

void AssignCarts(Record& r, uint32 t, PvpMatch::State* match, uint32 nowMs, bool& changed)
{
    TeamPlan& team = r.team[t];
    std::array<Sm::CartFacts, Sm::CART_COUNT> facts;
    float bestEta[Sm::CART_COUNT] = { Sm::NEVER_S, Sm::NEVER_S, Sm::NEVER_S };

    std::vector<Sm::Hand> hands;
    std::vector<Member*> who;
    for (auto& entry : team.members)
    {
        Member& m = entry.second;
        if (!m.alive)
        {
            if (m.cart != Sm::CART_NONE || m.job != SmPlan::Job::NONE)
                changed = true;
            m.cart = Sm::CART_NONE;
            m.job = SmPlan::Job::NONE;
            continue;
        }

        Sm::Hand hand;
        hand.guidLow = m.guid;
        hand.tier = m.tier;
        hand.offFit = Sm::OffFitOf(m.cls, m.healer, m.tank, m.feral);
        hand.mainFit = Sm::MainFitOf(m.healer);
        hand.prevCart = m.cart;
        for (uint8 c = 0; c < Sm::CART_COUNT; ++c)
        {
            CartInfo const& cart = r.carts[c];
            if (!cart.live || !cart.route)
                continue;
            Sm::Intercept const hit = Sm::InterceptOf(*cart.route, cart.along, m.x, m.y, m.mounted);
            hand.eta[c] = hit.possible ? hit.seconds : Sm::NEVER_S;
            bestEta[c] = std::min(bestEta[c], hand.eta[c]);
        }
        if (m.bot)
        {
            hands.push_back(hand);
            who.push_back(&m);
        }
    }

    bool const opening = r.playMs < OPENING_WINDOW_MS;
    for (uint8 c = 0; c < Sm::CART_COUNT; ++c)
    {
        CartInfo const& cart = r.carts[c];
        Sm::CartFacts& f = facts[c];
        f.cart = c;
        f.live = cart.live;
        f.keeper = cart.keeper;
        f.bar = team.bar[c];
        f.remainingS = cart.remainingS;
        f.friends = team.friends[c];
        f.enemies = team.enemies[c];
        f.etaS = bestEta[c];
        f.arrivalBonus = opening && cart.along < 120.0f ? Sm::OpeningBonus(uint8(t), c) : 0;
        f.claimBonus = ClaimBonus(match, t, c, nowMs);
    }

    std::vector<Sm::CartOption> const options = Sm::ScoreCarts(facts, uint8(t));
    uint8 const previousMain = team.mainCart;
    team.options = options;
    team.mainCart = Sm::HoldMain(options, team.mainCart);

    // BT-613: the list is published for the decision layer (D18); an answer that still stands and names a cart that scores replaces the
    // rule's main cart, the rule stays the first option and the fallback.
    if (match)
    {
        PublishLists(r, *match, t, nowMs);
        if (std::optional<uint8> const id = AiPvp::Override(match->Side(t).ai.book, BgLeader::DecisionPoint::D18_SM_CART, nowMs))
            for (Sm::CartOption const& o : options)
                if (o.cart == *id && o.score > 0 && o.kind != Sm::OptionKind::SKIP)
                    team.mainCart = *id;
    }

    // No cart is worth a walk (all about to dock, or out of reach): the nearest live one is the fallback.
    uint8 fallback = Sm::CART_NONE;
    for (uint8 c = 0; c < Sm::CART_COUNT; ++c)
        if (r.carts[c].live && (fallback == Sm::CART_NONE || bestEta[c] < bestEta[fallback]))
            fallback = c;

    Sm::Wants const wants = Sm::WantCounts(team.mainCart == Sm::CART_NONE ? options : Sm::WithMainFirst(options, team.mainCart), facts,
                                           uint32(hands.size()), team.stance, team.hasHealer, fallback);
    if (team.mainCart == Sm::CART_NONE)
        team.mainCart = wants.mainCart;
    if (team.mainCart != previousMain)
        changed = true;

    std::vector<uint8> const deal = Sm::Deal(hands, wants);
    for (std::size_t i = 0; i < who.size(); ++i)
    {
        Member& m = *who[i];
        uint8 next = deal[i];
        // A casual bot keeps its old cart a refresh after a stance change, while that cart is still there.
        if (m.keepRefreshes > 0)
        {
            --m.keepRefreshes;
            if (m.cart < Sm::CART_COUNT && r.carts[m.cart].live && next != Sm::CART_NONE)
                next = m.cart;
        }
        SmPlan::Job const job = next == Sm::CART_NONE ? SmPlan::Job::NONE : next == wants.mainCart ? SmPlan::Job::MAIN : SmPlan::Job::OFF;
        if (next != m.cart || job != m.job)
            changed = true;
        m.cart = next;
        m.job = job;
    }
}

// The switch duty of a team: one holder at most, for the track with the bigger gain.
void AssignSwitches(Record& r, uint32 t, PvpMatch::State* match, uint32 nowMs, bool& changed)
{
    TeamPlan& team = r.team[t];
    for (auto& entry : team.members)
    {
        Member& m = entry.second;
        if (m.switchTrack != Sm::TRACK_NONE)
            changed = true;       // a duty that was there is re-decided below; an unchanged duty is set again
        m.switchTrack = Sm::TRACK_NONE;
        m.switchAct = Sm::SwitchAct::NONE;
        if (m.job == SmPlan::Job::SWITCH)
            m.job = m.cart == team.mainCart ? SmPlan::Job::MAIN : m.cart == Sm::CART_NONE ? SmPlan::Job::NONE : SmPlan::Job::OFF;
    }

    uint8 order[Sm::TRACK_COUNT] = { Sm::TRACK_EAST, Sm::TRACK_NORTH };
    if (Sm::PreferredSwitch(Sm::TRACK_NORTH, uint8(t)).gain > Sm::PreferredSwitch(Sm::TRACK_EAST, uint8(t)).gain)
        std::swap(order[0], order[1]);

    bool taken = false;
    bool anyOffer = false;
    for (uint8 track : order)
    {
        uint8 const c = Sm::CartOfTrack(track);
        CartInfo const& cart = r.carts[c];
        Sm::SwitchFacts f;
        f.track = track;
        f.team = uint8(t);
        f.cartLive = cart.live;
        f.beforeJunction = cart.before;
        f.secondsToJunction = cart.toJunction;
        f.opened = r.switchOpened[track];
        f.enemiesAtSwitch = team.enemiesAtSwitch[track];
        if (!cart.live || !cart.before || !Sm::SwitchWrong(f) || taken)
        {
            team.holder[track] = 0;
            continue;
        }

        std::vector<Sm::SwitchCandidate> list;
        for (auto& entry : team.members)
        {
            Member const& m = entry.second;
            if (!m.bot || !m.alive || m.tier == Tier::CASUAL)
                continue;
            Sm::SwitchCandidate cand;
            cand.guid = m.guid;
            cand.tier = m.tier;
            cand.walkS = Sm::WalkSeconds(Sm::Dist2d(m.x, m.y, Sm::SWITCH_STAND[track].x, Sm::SWITCH_STAND[track].y), m.mounted);
            cand.offBot = m.job == SmPlan::Job::OFF;
            cand.onTheTrackCart = m.cart == c;
            list.push_back(cand);
        }

        // The holder of the last refresh keeps the duty while it can still make it (no change of mind every two seconds).
        uint32 holder = 0;
        for (Sm::SwitchCandidate const& cand : list)
            if (cand.guid == team.holder[track] && cand.walkS + Sm::SWITCH_CAST_S + Sm::SWITCH_SLACK_S <= cart.toJunction)
                holder = cand.guid;
        if (!holder)
            holder = Sm::PickSwitchHolder(list, cart.toJunction);
        Member* m = holder ? FindMember(team, holder) : nullptr;
        if (!m)
        {
            team.holder[track] = 0;
            continue;
        }

        for (Sm::SwitchCandidate const& cand : list)
            if (cand.guid == holder)
                f.walkS = cand.walkS;
        f.tier = m->tier;
        Sm::SwitchAct act = Sm::ChooseSwitch(f);
        // BT-613: the list of D19 is published for the decision layer; an answer that still stands replaces the rule's act.
        if (match && (act == Sm::SwitchAct::CLICK_NOW || act == Sm::SwitchAct::WAIT_LAST))
        {
            AiPvp::Book& book = match->Side(t).ai.book;
            anyOffer = true;
            AiPvp::Publish(book, Sm::SwitchOffer(act),
                           { std::string("track:") + (track == Sm::TRACK_EAST ? "east" : "north"),
                             "seconds_to_junction:" + std::to_string(int(cart.toJunction)), "enemies_at_switch:" + std::to_string(f.enemiesAtSwitch),
                             "holder_tier:" + std::string(TierWord(m->tier)) },
                           true, nowMs);
            if (std::optional<uint8> const id = AiPvp::Override(book, BgLeader::DecisionPoint::D19_SM_SWITCH, nowMs))
                if (*id <= static_cast<uint8>(Sm::SwitchAct::LEAVE))
                    act = static_cast<Sm::SwitchAct>(*id);
        }
        if (act == Sm::SwitchAct::NONE || act == Sm::SwitchAct::LEAVE)
        {
            team.holder[track] = 0;
            continue;
        }

        if (team.holder[track] != holder)
            SF_LOG_DEBUG("playerbots", "[BGSM] switch-duty inst=%u %s track=%s holder=%u tier=%s act=%u toJunction=%.0f state=%s", r.instanceId,
                         t == uint32(TEAM_ALLIANCE) ? "A" : "H", track == Sm::TRACK_EAST ? "east" : "north", holder, TierWord(m->tier),
                         uint32(act), cart.toJunction, f.opened ? "opened" : "closed");
        m->switchTrack = track;
        m->switchAct = act;
        m->job = SmPlan::Job::SWITCH;
        team.holder[track] = holder;
        team.recentHolder[track] = holder;
        team.recentHolderMs[track] = nowMs;
        taken = true;
        changed = true;
    }

    if (match && !anyOffer)
        AiPvp::Withdraw(match->Side(t).ai.book, BgLeader::DecisionPoint::D19_SM_SWITCH);
}

// ---------------------------------------------------------------------------------------------------------------------
// Call-outs (sheet section 8). The in-code lines are the fallback when the text table has no row of the name (sql 2026_11_19_02.sql adds
// them, with a variant per personality).
// ---------------------------------------------------------------------------------------------------------------------
bool CallsEnabled()
{
    static bool const enabled = sConfigMgr->GetBoolDefault("AiPlayerbot.BattlegroundChat.Enable", true);
    return enabled;
}

struct CallText
{
    std::string key;
    std::vector<char const*> lines;
};

// Rows in the order of `Call`. Placeholders: %node (a cart or a switch), %num, %stance, %main, %pair, %depot.
std::vector<CallText> const& Texts()
{
    static std::vector<CallText> const texts = {
        { "bg_sm_opening", { "main %main, pair %pair", "%main main group, %pair off", "going %main, off bots %pair" } },
        { "bg_sm_inc", { "inc %node %num", "%node %num inc", "inc %node need help" } },
        { "bg_sm_taken", { "got %node", "%node is ours", "%node tagged" } },
        { "bg_sm_lost", { "lost %node", "%node gone", "they took %node" } },
        { "bg_sm_switch", { "%node set, %depot", "%node done, %depot", "%node %depot" } },
        { "bg_sm_docked", { "%node docked", "%node docked, next cart", "%node in, +150" } },
        { "bg_sm_stance", { "%stance", "%stance, stay tight" } },
        { "bg_sm_wiped", { "wiped at %node, regroup pit", "%node group down, regroup", "lost the %node group, pit" } }
    };
    return texts;
}

char const* StanceWord(BgLeader::Stance stance)
{
    switch (stance)
    {
        case BgLeader::Stance::OFFENSE: return "all out";
        case BgLeader::Stance::DEFENSE: return "hold what we have";
        case BgLeader::Stance::ALL_IN: return "all in, flip carts";
        case BgLeader::Stance::TURTLE: return "turtle, guard the carts";
        default: break;
    }
    return "balanced";
}

// Marks a line as telling the team something: when it is said it becomes an observation of the side (BT-483).
void Observe(Pending& line, PvpObs::Kind kind, uint8 place, uint32 count)
{
    line.observe = true;
    line.obsKind = kind;
    line.obsPlace = place;
    line.obsCount = static_cast<uint8>(std::min<uint32>(count, PvpObs::MAX_REPORTED_COUNT));
}

// True when a line was queued. `forced` skips the tier's chance (a leader always says its stance); the limiter and the tier's delay still apply.
bool Enqueue(Record& r, TeamPlan& team, Member const& speaker, Call kind, uint32 nowMs, Pending base, bool forced = false)
{
    if (!CallsEnabled() || !speaker.bot || !speaker.alive)
        return false;

    ++r.callSeq;
    // Expert bots call everything, good 70 percent, casual 30 (and late); the chance is a fixed roll per event.
    BgPlan::CallDraw const draw = BgPlan::DrawCall(speaker.guid, r.callSeq, speaker.tier);
    if (!draw.pass && !forced)
        return false;

    if (!team.limiter.Allow(speaker.guid, static_cast<uint8>(kind), nowMs))
        return false;

    base.guid = speaker.guid;
    base.kind = kind;
    base.seq = r.callSeq;
    base.dueMs = nowMs + draw.delayMs;
    r.pending.push_back(base);
    ++g_pendingTotal;
    return true;
}

// The alive bot of the team nearest to a point (the one that sees it), nullptr for none.
Member* NearestBot(TeamPlan& team, float x, float y)
{
    Member* best = nullptr;
    float bestDistance = 0.0f;
    for (auto& entry : team.members)
    {
        Member& m = entry.second;
        if (!m.bot || !m.alive)
            continue;
        float const d = Sm::Dist2d(m.x, m.y, x, y);
        if (!best || d < bestDistance || (d == bestDistance && m.guid < best->guid))
        {
            best = &m;
            bestDistance = d;
        }
    }
    return best;
}

std::string PairWords(TeamPlan const& team)
{
    std::string words;
    for (uint8 c = 0; c < Sm::CART_COUNT; ++c)
    {
        if (c == team.mainCart)
            continue;
        bool any = false;
        for (auto const& entry : team.members)
            any = any || (entry.second.bot && entry.second.alive && entry.second.cart == c);
        if (!any)
            continue;
        if (!words.empty())
            words += " and ";
        words += Sm::CartWord(c);
    }
    return words.empty() ? "none" : words;
}

void EmitCalls(Record& r, PvpMatch::State& match, uint32 t, uint32 nowMs)
{
    TeamPlan& team = r.team[t];

    // BT-481: the leader says what the team plays (at the gates, at every change, after a handover).
    if (team.stanceCallDue)
        if (Member* leader = FindMember(team, team.leader))
        {
            Pending line;
            line.stance = StanceWord(team.stance);
            if (Enqueue(r, team, *leader, Call::STANCE, nowMs, line, true))
                BgLeader::CallCommitted(match.Side(t).leader);
        }

    // The opening: where the main group goes and where the off bots are, said once by the leader (or the best bot without one).
    if (!team.openingCalled && r.started && team.mainCart < Sm::CART_COUNT)
    {
        Member* speaker = FindMember(team, team.leader);
        if (!speaker)
            for (auto& entry : team.members)
                if (entry.second.bot && entry.second.alive && (!speaker || entry.second.guid < speaker->guid))
                    speaker = &entry.second;
        if (speaker)
        {
            Pending line;
            line.main = Sm::CartWord(team.mainCart);
            line.pair = PairWords(team);
            team.openingCalled = Enqueue(r, team, *speaker, Call::OPENING, nowMs, line, true);
        }
    }

    for (uint8 c = 0; c < Sm::CART_COUNT; ++c)
    {
        CartInfo const& cart = r.carts[c];
        uint8 const was = team.prevKeeper[c];

        // A new cart starts neutral and is nobody's gain.
        if (r.dockEvent[c] && r.dockKeeper[c] == t)
            if (Member* speaker = NearestBot(team, cart.x, cart.y))
            {
                Pending line;
                line.node = Sm::CartWord(c);
                Enqueue(r, team, *speaker, Call::DOCKED, nowMs, line);
            }

        if (cart.live && !r.cartFresh[c] && cart.keeper != was)
        {
            Member* speaker = NearestBot(team, cart.x, cart.y);
            if (speaker)
            {
                Pending line;
                line.node = Sm::CartWord(c);
                if (cart.keeper == t)
                    Enqueue(r, team, *speaker, Call::TAKEN, nowMs, line);
                else if (was == t)
                    Enqueue(r, team, *speaker, Call::LOST, nowMs, line);
            }
        }

        // A group at one of our carts (or any cart that matters): the bot nearest to it warns, an expert early.
        if (cart.live && team.enemies[c] >= INC_MIN_ENEMIES && team.prevEnemies[c] < INC_MIN_ENEMIES &&
            (!team.lastIncMs[c] || nowMs - team.lastIncMs[c] >= INC_REPEAT_MS))
            if (Member* speaker = NearestBot(team, cart.x, cart.y))
            {
                Pending line;
                line.node = Sm::CartWord(c);
                line.number = std::to_string(team.enemies[c]);
                Observe(line, PvpObs::Kind::INCOMING, c, team.enemies[c]);
                if (Enqueue(r, team, *speaker, Call::INC, nowMs, line))
                    team.lastIncMs[c] = nowMs;
            }
    }

    // The switch is set the way the team wants it: the holder says so (one call per change).
    for (uint8 tr = 0; tr < Sm::TRACK_COUNT; ++tr)
    {
        if (!r.switchEvent[tr] || team.recentHolder[tr] == 0 || nowMs - team.recentHolderMs[tr] > SWITCH_CALL_WINDOW_MS)
            continue;
        Sm::SwitchFacts f;
        f.track = tr;
        f.team = uint8(t);
        f.opened = r.switchOpened[tr];
        if (Sm::SwitchWrong(f))
            continue;
        if (Member* speaker = FindMember(team, team.recentHolder[tr]))
        {
            Pending line;
            line.node = Sm::SwitchWord(tr);
            line.depot = Sm::DepotWord(Sm::DepotOf(Sm::CartOfTrack(tr), r.switchOpened[tr]));
            if (Enqueue(r, team, *speaker, Call::SWITCH, nowMs, line))
                team.recentHolder[tr] = 0;
        }
    }

    // A wipe: the main group stood at the main cart a refresh ago and nobody is there now.
    if (team.mainCart < Sm::CART_COUNT && r.carts[team.mainCart].live)
    {
        uint32 const bodies = team.friends[team.mainCart];
        if (team.prevMainBodies >= WIPE_MIN_BODIES && bodies == 0 && (!team.lastWipeMs || nowMs - team.lastWipeMs >= WIPE_REPEAT_MS))
            if (Member* speaker = NearestBot(team, r.carts[team.mainCart].x, r.carts[team.mainCart].y))
            {
                Pending line;
                line.node = Sm::CartWord(team.mainCart);
                if (Enqueue(r, team, *speaker, Call::WIPED, nowMs, line))
                    team.lastWipeMs = nowMs;
            }
    }
}

// What the next refresh compares with.
void RememberCalls(Record& r)
{
    for (uint32 t = 0; t < 2; ++t)
    {
        TeamPlan& team = r.team[t];
        for (uint8 c = 0; c < Sm::CART_COUNT; ++c)
        {
            team.prevKeeper[c] = r.carts[c].live ? r.carts[c].keeper : uint8(Sm::TEAM_NONE);
            team.prevEnemies[c] = team.enemies[c];
        }
        team.prevMainBodies = team.mainCart < Sm::CART_COUNT ? team.friends[team.mainCart] : 0;
    }
    for (uint8 c = 0; c < Sm::CART_COUNT; ++c)
    {
        r.cartFresh[c] = false;
        r.dockEvent[c] = false;
    }
    for (uint8 tr = 0; tr < Sm::TRACK_COUNT; ++tr)
        r.switchEvent[tr] = false;
}

// The answers of the decision layer for D18 and D19 that still stand, and the lists published for the next ask. The rule's own answer stays
// the first option and the fallback.
void PublishLists(Record& r, PvpMatch::State& match, uint32 t, uint32 nowMs)
{
    TeamPlan& team = r.team[t];
    AiPvp::Book& book = match.Side(t).ai.book;

    BgLeader::Offer const carts = Sm::CartOffer(team.options);
    if (r.started && carts.options.size() >= 2)
    {
        std::vector<std::string> facts;
        for (uint8 c = 0; c < Sm::CART_COUNT; ++c)
            facts.push_back(std::string(Sm::CartWord(c)) + ":" + (r.carts[c].live ? std::string(KeeperWord(r.carts[c].keeper)) : "none") +
                            ",enemies=" + std::to_string(team.enemies[c]) + ",friends=" + std::to_string(team.friends[c]));
        AiPvp::Publish(book, carts, facts, true, nowMs);
    }
    else
        AiPvp::Withdraw(book, BgLeader::DecisionPoint::D18_SM_CART);
}

// ---------------------------------------------------------------------------------------------------------------------
// What the decision layer is told about the match (BT-502): game context only, every number from the record.
// ---------------------------------------------------------------------------------------------------------------------
void DescribeMatch(AwareAiBgState& state, Record const& r, uint32 t)
{
    TeamPlan const& team = r.team[t];
    state.Map = "ssm";
    state.ScoreOurs = r.score[t];
    state.ScoreTheirs = r.score[1 - t];
    state.ScoreLimit = Sm::WIN_SCORE;
    uint32 kept[2] = { 0, 0 };
    for (uint8 c = 0; c < Sm::CART_COUNT; ++c)
    {
        CartInfo const& cart = r.carts[c];
        char const* const who = !cart.live ? "none" : cart.keeper == Sm::TEAM_NONE ? "neutral" : cart.keeper == t ? "ours" : "theirs";
        state.Nodes.push_back(std::string(Sm::CartWord(c)) + ":" + who);
        if (cart.live && cart.keeper < 2)
            ++kept[cart.keeper];
    }
    uint32 groups[Sm::CART_COUNT] = { 0, 0, 0 };
    for (auto const& entry : team.members)
        if (entry.second.bot && entry.second.alive && entry.second.cart < Sm::CART_COUNT)
            ++groups[entry.second.cart];
    for (uint8 c = 0; c < Sm::CART_COUNT; ++c)
        if (groups[c])
            state.FriendlyGroups.push_back(std::string(Sm::CartWord(c)) + ":" + std::to_string(groups[c]));
    for (uint8 c = 0; c < Sm::CART_COUNT; ++c)
        if (team.enemies[c])
            state.ObservedPressure.push_back(std::string(Sm::CartWord(c)) + ":" + std::to_string(team.enemies[c]));
    state.Facts.push_back("main_cart:" + std::string(team.mainCart < Sm::CART_COUNT ? Sm::CartWord(team.mainCart) : "none"));
    state.Facts.push_back("carts_kept:ours=" + std::to_string(kept[t]) + ",theirs=" + std::to_string(kept[1 - t]));
    state.Facts.push_back("eta_to_win_s:ours=" + std::to_string(Sm::EtaSeconds(int(r.score[t]), int(kept[t]))) +
                          ",theirs=" + std::to_string(Sm::EtaSeconds(int(r.score[1 - t]), int(kept[1 - t]))));
    state.Facts.push_back("our_bots_alive:" + std::to_string(team.bots));
}

// A new epoch when this changes: the keepers, the carts alive, the switches, the main carts, the jobs.
uint64 Signature(Record const& r)
{
    uint64 h = 1469598103934665603ull;
    auto const mixIn = [&h](uint64 value) { h = (h ^ value) * 1099511628211ull; };
    for (uint8 c = 0; c < Sm::CART_COUNT; ++c)
    {
        mixIn(r.carts[c].live ? 1 + r.carts[c].keeper : 0);
        mixIn(r.carts[c].before ? 1 : 0);
        mixIn(r.carts[c].guid);
    }
    for (uint8 t = 0; t < Sm::TRACK_COUNT; ++t)
        mixIn(r.switchOpened[t] ? 1 : 0);
    for (TeamPlan const& team : r.team)
    {
        mixIn(uint64(team.stance));
        mixIn(team.mainCart);
        for (auto const& entry : team.members)
        {
            mixIn(entry.second.guid);
            mixIn(uint64(entry.second.cart) * 7 + uint64(entry.second.job) * 3 + entry.second.switchTrack);
        }
    }
    return h;
}

void LogState(Record& r, Battleground* bg, uint32 nowMs, bool force)
{
    if (!force && r.lastStateLogMs && nowMs - r.lastStateLogMs < LOG_REPEAT_MS)
        return;
    r.lastStateLogMs = nowMs ? nowMs : 1;
    char carts[Sm::CART_COUNT][48];
    for (uint8 c = 0; c < Sm::CART_COUNT; ++c)
    {
        CartInfo const& cart = r.carts[c];
        if (!cart.live)
            snprintf(carts[c], sizeof(carts[c]), "-");
        else
            snprintf(carts[c], sizeof(carts[c]), "%s@%.0f,%.0f rem=%.0fs bar=%d/%d", KeeperWord(cart.keeper), cart.x, cart.y,
                     cart.remainingS, r.team[0].bar[c], r.team[1].bar[c]);
    }
    uint32 jobs[2][4] = {};
    for (uint32 t = 0; t < 2; ++t)
        for (auto const& entry : r.team[t].members)
            if (entry.second.bot && entry.second.alive)
                ++jobs[t][uint32(entry.second.job)];
    SF_LOG_DEBUG("playerbots",
                 "[BGSM] state inst=%u score=%u-%u east=%s mid=%s north=%s sw=%s,%s A(main=%s stance=%s main/off/switch=%u/%u/%u) "
                 "H(main=%s stance=%s main/off/switch=%u/%u/%u)",
                 bg->GetInstanceID(), r.score[0], r.score[1], carts[0], carts[1], carts[2], r.switchOpened[0] ? "open" : "closed",
                 r.switchOpened[1] ? "open" : "closed",
                 r.team[0].mainCart < Sm::CART_COUNT ? Sm::CartWord(r.team[0].mainCart) : "-", BgLeader::StanceName(r.team[0].stance),
                 jobs[0][1], jobs[0][2], jobs[0][3],
                 r.team[1].mainCart < Sm::CART_COUNT ? Sm::CartWord(r.team[1].mainCart) : "-", BgLeader::StanceName(r.team[1].stance),
                 jobs[1][1], jobs[1][2], jobs[1][3]);
}

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
    r.score[0] = bg->GetTeamScore(TEAM_ALLIANCE);
    r.score[1] = bg->GetTeamScore(TEAM_HORDE);

    // BT-480: the shared PvP match state of this match (created here, on the map thread; ended by the leave path or swept).
    PvpMatch::Handle const match = PvpMatch::Touch(bg->GetInstanceID(), bg->GetMapId(), elapsed, status == STATUS_WAIT_LEAVE, nowMs);

    // BT-502: a finished match awaits no answer; its asks are summed up and their outcome reported once.
    if (status == STATUS_WAIT_LEAVE)
        for (uint32 t = 0; t < 2; ++t)
            AiPvpDecision::MatchOver(*match, t, r.score[t] > r.score[1 - t] ? 1 : r.score[t] < r.score[1 - t] ? -1 : 0, nowMs);

    // BT-482: the humans' call-outs since the last refresh, put into the stores of their sides (read through the stores).
    match->IngestInbox(nowMs, PvpConfig::ReaderLimits());
    for (uint32 t = 0; t < 2; ++t)
        match->Side(t).observations.Sweep(nowMs);

    ReadRoster(r, bg);
    if (status != STATUS_IN_PROGRESS && status != STATUS_WAIT_JOIN)
        return;

    bool changed = false;
    ReadCarts(r, bg, nowMs, changed);
    float const dt = r.baseline && elapsed > r.lastRefreshElapsedMs ? std::min(4.0f, float(elapsed - r.lastRefreshElapsedMs) / 1000.0f) : 0.0f;
    r.lastRefreshElapsedMs = elapsed;
    CountCarts(r, match.get(), nowMs, status == STATUS_IN_PROGRESS ? dt : 0.0f);

    bool stanceChangedNow = false;
    for (uint32 t = 0; t < 2; ++t)
    {
        TeamPlan& team = r.team[t];
        team.rulePlan = BgPlan::PlanFromStrategy(t == uint32(TEAM_ALLIANCE) ? strategyAlliance : strategyHorde);
        stanceChangedNow = LeaderTurn(r, *match, t, team, nowMs) || stanceChangedNow;
        AssignCarts(r, t, match.get(), nowMs, changed);
        AssignSwitches(r, t, match.get(), nowMs, changed);
    }

    // --- events and call-outs (the first refresh only records the baseline) ---
    if (r.baseline && status == STATUS_IN_PROGRESS)
        for (uint32 t = 0; t < 2; ++t)
            EmitCalls(r, *match, t, nowMs);
    RememberCalls(r);

    // Lines whose speaker died, left or never got a turn are dropped.
    {
        size_t const before = r.pending.size();
        r.pending.erase(std::remove_if(r.pending.begin(), r.pending.end(),
            [&](Pending const& p)
            {
                for (TeamPlan& team : r.team)
                    if (Member* speaker = FindMember(team, p.guid))
                        return !speaker->alive || int32(nowMs - p.dueMs) > int32(PENDING_MAX_AGE_MS);
                return true;
            }), r.pending.end());
        g_pendingTotal -= std::min<uint32>(g_pendingTotal.load(), uint32(before - r.pending.size()));
    }

    uint64 const signature = Signature(r);
    if (signature != r.signature)
    {
        if (r.baseline)
            changed = true;
        r.signature = signature;
    }

    // BT-502: the plan above is published and plays on; each side's leader may now ask the decision layer about it.
    for (uint32 t = 0; t < 2; ++t)
        AiPvpDecision::Ask(match, t, match->Side(t).leader, signature, nowMs, r.started && status == STATUS_IN_PROGRESS,
                           [&](AwareAiBgState& state) { DescribeMatch(state, r, t); });

    if (r.baseline && changed)
    {
        r.BumpEpoch(nowMs);
        if (stanceChangedNow)
            r.stanceEpoch = r.epoch;
        LogState(r, bg, nowMs, true);
    }
    else if (r.baseline)
        LogState(r, bg, nowMs, false);
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

bool CommonGuards(Battleground* bg, Player* bot)
{
    return bg && bot && bg->GetMapId() == Sm::MAP_ID && bot->IsAlive();
}
}  // namespace

namespace SmPlan
{
bool Enabled(Battleground* bg)
{
    return bg && bg->GetMapId() == Sm::MAP_ID;
}

bool Decide(Battleground* bg, Player* bot, uint8 strategyAlliance, uint8 strategyHorde, View& out)
{
    if (!CommonGuards(bg, bot))
        return false;

    uint32 const now = getMSTime();
    std::lock_guard<std::mutex> guard(g_lock);
    Record& r = RecordOf(bg->GetInstanceID(), now);
    Member* me = Prepare(r, bg, bot, now, strategyAlliance, strategyHorde);
    if (!me)
        return false;

    // Set first: a failed decision must not make the bot re-decide at every tick.
    me->seenEpoch = r.epoch;
    if (me->cart >= Sm::CART_COUNT || !r.carts[me->cart].live || !r.carts[me->cart].route)
        return false;

    uint32 const t = TeamIndexOf(bot);
    CartInfo const& cart = r.carts[me->cart];
    out.job = me->job;
    out.cart = me->cart;
    out.cartX = cart.x;
    out.cartY = cart.y;
    out.cartZ = cart.z;
    out.stance = uint8(r.team[t].stance);

    Sm::Intercept const hit = Sm::InterceptOf(*cart.route, cart.along, bot->GetPositionX(), bot->GetPositionY(), bot->IsMounted());
    out.canMeet = hit.possible;
    out.x = hit.possible ? hit.where.x : cart.x;
    out.y = hit.possible ? hit.where.y : cart.y;
    out.z = hit.possible ? hit.where.z : cart.z;

    if (me->switchTrack < Sm::TRACK_COUNT)
    {
        uint8 const track = me->switchTrack;
        CartInfo const& trackCart = r.carts[Sm::CartOfTrack(track)];
        out.switchTrack = track;
        out.switchAct = me->switchAct;
        out.secondsToJunction = trackCart.toJunction;
        out.switchOpened = r.switchOpened[track];
        Sm::SwitchFacts f;
        f.track = track;
        f.team = uint8(t);
        f.opened = out.switchOpened;
        out.switchWrong = Sm::SwitchWrong(f);
    }

    uint32 const nowLog = now ? now : 1;
    if (!me->lastLogMs || nowLog - me->lastLogMs >= LOG_REPEAT_MS)
    {
        me->lastLogMs = nowLog;
        SF_LOG_DEBUG("playerbots", "[BGSM] %s tier=%s job=%u cart=%s meet=%d (%.0f,%.0f,%.0f) switch=%u", bot->GetName().c_str(),
                     TierWord(me->tier), uint32(me->job), Sm::CartWord(me->cart), int(out.canMeet), out.x, out.y, out.z,
                     uint32(me->switchTrack));
    }
    return true;
}

bool PlanChanged(Battleground* bg, Player* bot, uint8 strategyAlliance, uint8 strategyHorde)
{
    if (!CommonGuards(bg, bot))
        return false;

    uint32 const now = getMSTime();
    std::lock_guard<std::mutex> guard(g_lock);
    Record& r = RecordOf(bg->GetInstanceID(), now);
    Member* me = Prepare(r, bg, bot, now, strategyAlliance, strategyHorde);
    if (!me || me->seenEpoch == r.epoch)
        return false;

    // A bot in the middle of a switch click is never moved (the cast is the point of the duty).
    if (bot->IsNonMeleeSpellCasted(false))
        return false;

    // Core: follow the plan state when it changes - late for the casual bots, and a stance change of the leader one refresh later
    // still (BT-481).
    uint32 const lag = BgPlan::AdoptLagMs(me->tier) + (r.epoch == r.stanceEpoch ? BgLeader::StanceExtraLagMs(me->tier) : 0u);
    return now - r.epochChangedMs >= lag;
}

bool CartDistance(Battleground* bg, Player* bot, uint8& cart, float& yards)
{
    if (!CommonGuards(bg, bot))
        return false;

    uint32 const now = getMSTime();
    std::lock_guard<std::mutex> guard(g_lock);
    auto const record = g_records.find(bg->GetInstanceID());
    if (record == g_records.end())
        return false;

    Record const& r = record->second;
    TeamPlan const& team = r.team[TeamIndexOf(bot)];
    auto const found = team.members.find(bot->GetGUIDLow());
    if (found == team.members.end() || found->second.cart >= Sm::CART_COUNT || !r.carts[found->second.cart].live)
        return false;

    (void)now;
    CartInfo const& info = r.carts[found->second.cart];
    cart = found->second.cart;
    yards = Sm::Dist2d(bot->GetPositionX(), bot->GetPositionY(), info.x, info.y);
    return true;
}

uint32 StartSpot(Battleground* bg, Player* bot)
{
    if (!bg || !bot)
        return 0;

    std::lock_guard<std::mutex> guard(g_lock);
    auto const record = g_records.find(bg->GetInstanceID());
    if (record == g_records.end())
        return bot->GetGUIDLow() % 3;

    uint32 below = 0;
    for (auto const& entry : record->second.team[TeamIndexOf(bot)].members)
        if (entry.second.guid < bot->GetGUIDLow())
            ++below;
    return below % 3;
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

// Said by the bot when its turn comes (after the tier's delay); true when a line went out.
bool DeliverCalls(PlayerbotAI* botAI)
{
    if (!g_pendingTotal.load(std::memory_order_relaxed) || !botAI)
        return false;

    Player* bot = botAI->GetBot();
    if (!bot || !bot->InBattleground() || bot->GetMapId() != Sm::MAP_ID)
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
        auto itr = std::find_if(r.pending.begin(), r.pending.end(), [&](Pending const& p) { return p.guid == bot->GetGUIDLow(); });
        if (itr == r.pending.end() || int32(now - itr->dueMs) < 0)
            return false;

        if (!r.team[t].limiter.Allow(itr->guid, static_cast<uint8>(itr->kind), now))
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

    CallText const& text = Texts()[static_cast<size_t>(call.kind)];
    std::string const fallback = text.lines[(call.seq + call.guid) % text.lines.size()];
    std::map<std::string, std::string> placeholders = { { "%node", call.node },   { "%num", call.number }, { "%stance", call.stance },
                                                        { "%main", call.main },   { "%pair", call.pair }, { "%depot", call.depot } };
    bool const sent = BotVoice::Say(botAI, text.key, fallback, placeholders, BotVoice::Channel::BATTLEGROUND, BotVoice::Priority::NOTICE, 100,
                                    nullptr, true);
    if (!sent)
        return false;

    std::lock_guard<std::mutex> guard(g_lock);
    auto const record = g_records.find(instance);
    if (record != g_records.end())
        record->second.team[t].limiter.Commit(call.guid, static_cast<uint8>(call.kind), now);

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
    SF_LOG_DEBUG("playerbots", "[BGSM-CALL] %s says %s (%s)", bot->GetName().c_str(), text.key.c_str(), fallback.c_str());
    return true;
}
}  // namespace SmPlan
