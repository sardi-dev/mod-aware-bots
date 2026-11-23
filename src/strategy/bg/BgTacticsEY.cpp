/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

// Eye of the Storm (map 566, BATTLEGROUND_EY) bot tactics - BT-580, docs/bg-sheets/eye-of-the-storm.md (owner approved
// 2026-11-18). The old case of BattleGroundTactics.cpp (random roles, a 20 percent run at any enemy, a walk to the enemy
// start room when every base was held) is replaced by the playbook's rules without a team record: squads of 3 and one pinned
// defender per near base, a take group of 3 or more, the hold state with 3 bases, the flag in the middle for one taker and no
// chase. The rules are in EyLogic::Choose (Utils/EyPlanLogic.h, pure and tested). The flag interactions themselves (the 8 s
// capture cast at the centre stand, a click on a dropped flag) stay in BGTactics::atFlag.

#include "BattleGroundTactics.h"

// DatabaseEnv.h must precede the battleground headers (see BattleGroundTactics.cpp).
#include "DatabaseEnv.h"

#include "AbPlan.h"
#include "BotSkill.h"
#include "Battleground.h"
#include "BattlegroundEY.h"
#include "EyFlagLogic.h"
#include "EyPlanLogic.h"
#include "GameObject.h"
#include "WsgCarrier.h"
#include "ObjectAccessor.h"
#include "Playerbots.h"
#include "PlayerbotSpec.h"
#include "PositionValue.h"
#include "WorldSession.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace
{
// Where a carrier with no base to score at keeps clear of the enemy: its own side of the middle (the old case's spots).
Position const EY_RETREAT_HORDE = {1885.529f, 1532.157f, 1200.635f, 0.0f};
Position const EY_RETREAT_ALLIANCE = {2452.253f, 1602.356f, 1203.617f, 0.0f};

// A post stands 3 to 12 yd from the base's capture trigger (inside the 70 yd circle by a wide margin); the rest of the middle
// group stands off the centre stand beyond the 25 yd of the pickup cast, so only the taker casts.
constexpr float EY_POST_RING_MIN = 3.0f;
constexpr float EY_POST_RING_MAX = 12.0f;
constexpr float EY_STAGE_RING_MIN = 26.0f;
constexpr float EY_STAGE_RING_MAX = 30.0f;
constexpr float EY_TWO_PI = 6.2831853f;

// The carrier chooses its base again at this interval (a base can fall while it runs, sheet section 6).
constexpr uint32 EY_CARRIER_RECHECK_MS = 2000;

// ---- The team roster of the old case ---------------------------------------------------------------------------------
// One pass over the battleground's players at most every 2 s per match, shared by the bots behind one mutex (the Battle for
// Gilneas pattern). A human takes a slot and is never commanded.
struct EyRoster
{
    uint32 refreshedMs = 0;
    std::vector<EyLogic::Member> bots[2];
};

constexpr uint32 EY_ROSTER_REFRESH_MS = 2000;
constexpr uint32 EY_ROSTER_MAX_AGE_MS = 10 * 60 * 1000;
std::mutex s_eyRosterLock;
std::map<uint32, EyRoster> s_eyRoster;

void EyReadRoster(Battleground* bg, EyRoster& roster, uint32 nowMs)
{
    roster.refreshedMs = nowMs ? nowMs : 1;
    roster.bots[0].clear();
    roster.bots[1].clear();
    for (auto const& ref : bg->GetBgMap()->GetPlayers())
    {
        Player* p = ref.GetSource();
        if (!p || !p->IsInWorld() || p->GetBattlegroundId() != bg->GetInstanceID() || p->IsGameMaster())
            continue;
        if (!p->GetSession() || !p->GetSession()->IsBot())
            continue;

        EyLogic::Member m;
        m.guidLow = p->GetGUIDLow();
        m.cls = p->getClass();
        m.level = p->getLevel();
        m.healer = PlayerBotSpec::IsHeal(p, true);
        m.tank = PlayerBotSpec::IsTank(p, true);
        m.feral = m.cls == BgPlan::CLS_DRUID && BotGetSpecialization(p) == SPEC_DRUID_FERAL;
        m.tier = BotSkill::TierOf(p);
        roster.bots[p->GetBGTeam() == ALLIANCE ? uint32(TEAM_ALLIANCE) : uint32(TEAM_HORDE)].push_back(m);
    }
}

std::vector<EyLogic::Member> EyTeamOf(Battleground* bg, Player* bot)
{
    uint32 const nowMs = getMSTime();
    uint32 const t = bot->GetBGTeam() == ALLIANCE ? uint32(TEAM_ALLIANCE) : uint32(TEAM_HORDE);
    std::lock_guard<std::mutex> guard(s_eyRosterLock);

    for (auto itr = s_eyRoster.begin(); itr != s_eyRoster.end();)
        itr = (nowMs - itr->second.refreshedMs > EY_ROSTER_MAX_AGE_MS) ? s_eyRoster.erase(itr) : std::next(itr);

    EyRoster& roster = s_eyRoster[bg->GetInstanceID()];
    auto const has = [&]() {
        for (EyLogic::Member const& m : roster.bots[t])
            if (m.guidLow == bot->GetGUIDLow())
                return true;
        return false;
    };
    if (!roster.refreshedMs || nowMs - roster.refreshedMs >= EY_ROSTER_REFRESH_MS || !has())
        EyReadRoster(bg, roster, nowMs);
    return roster.bots[t];
}

// ---- The match as the old case reads it -----------------------------------------------------------------------------------
// GetPointOwnedByTeam returns a team id (ALLIANCE or HORDE), not a TeamId index; the bar is the core's own
// (BattlegroundEY::GetPointBarStatus, the read-only hook of the core patches).
EyLogic::World EyReadWorld(Battleground* bg, Player* bot, TeamId team)
{
    BattlegroundEY* const ey = static_cast<BattlegroundEY*>(bg);
    EyLogic::World w;
    w.team = team == TEAM_ALLIANCE ? EyLogic::TEAM_IDX_ALLIANCE : EyLogic::TEAM_IDX_HORDE;
    for (uint8 b = 0; b < EyLogic::BASE_COUNT; ++b)
    {
        w.bar[b] = ey->GetPointBarStatus(b);
        if (ey->GetPointState(b) != EY_POINT_UNDER_CONTROL)
            continue;
        uint32 const owner = ey->GetPointOwnedByTeam(b);
        w.owner[b] = owner == ALLIANCE ? EyLogic::TEAM_IDX_ALLIANCE : owner == HORDE ? EyLogic::TEAM_IDX_HORDE : EyLogic::TEAM_NONE;
    }
    w.flagState = ey->GetFlagState();
    if (bot && ey->IsFlagPickedup())
    {
        if (Player* carrier = ObjectAccessor::FindPlayer(ey->GetFlagPickerGUID()))
        {
            bool const ours = carrier->GetBGTeam() == bot->GetBGTeam();
            w.teamCarries = ours;
            w.enemyCarries = !ours;
        }
    }
    return w;
}

// A point 3 to 12 yd around a base's trigger for a post (fixed per bot and rank, so a bot at its post does not wander when the
// objective is chosen again), a ring around the centre stand for the rest of the middle group.
void EyPostPoint(uint8 base, uint32 guidLow, uint32 rank, float& x, float& y, float& z)
{
    float const angle = EY_TWO_PI * float(BotSkillMath::Mix(guidLow) % 360) / 360.0f + 0.9f * float(rank) + 0.7f * float(base);
    float const t = float(BotSkillMath::Mix(guidLow + 17u) % 100) / 100.0f;
    float const r = EY_POST_RING_MIN + (EY_POST_RING_MAX - EY_POST_RING_MIN) * t;
    x = EyLogic::TRIGGER_POS[base].x + r * std::cos(angle);
    y = EyLogic::TRIGGER_POS[base].y + r * std::sin(angle);
    z = EyLogic::TRIGGER_POS[base].z;
}

void EyStagePoint(uint32 guidLow, uint32 rank, float& x, float& y, float& z)
{
    float const angle = EY_TWO_PI * float(BotSkillMath::Mix(guidLow) % 360) / 360.0f + 0.9f * float(rank);
    float const t = float(BotSkillMath::Mix(guidLow + 29u) % 100) / 100.0f;
    float const r = EY_STAGE_RING_MIN + (EY_STAGE_RING_MAX - EY_STAGE_RING_MIN) * t;
    x = EyLogic::CENTRE_FLAG.x + r * std::cos(angle);
    y = EyLogic::CENTRE_FLAG.y + r * std::sin(angle);
    z = EyLogic::CENTRE_FLAG.z;
}

// ---- Match evidence ---------------------------------------------------------------------------------------------------
// "[BGEY] base-change" on every observed owner change and one "[BGEY] state" line per instance every 30 s, written by whichever
// bot of the instance runs first (the core's point states are the truth, the bot only reports). Process-wide with a mutex: an
// instance's map update can run on any MapUpdate.Threads worker.
char const* const EY_BASE_NAMES[EyLogic::BASE_COUNT] = { "fel-reaver", "blood-elf", "draenei-ruins", "mage-tower" };

struct EyWatch
{
    uint32 lastLogMs = 0;
    uint8 owner[EyLogic::BASE_COUNT] = { 0xFE, 0xFE, 0xFE, 0xFE };   // 0xFE: not seen yet
    uint32 changes = 0;
};
std::mutex s_eyWatchLock;
std::map<uint32, EyWatch> s_eyWatch;

char const* EyOwnerName(uint8 owner)
{
    return owner == EyLogic::TEAM_IDX_ALLIANCE ? "alliance" : owner == EyLogic::TEAM_IDX_HORDE ? "horde" : "neutral";
}

void EyObserve(Battleground* bg)
{
    if (bg->GetStatus() != STATUS_IN_PROGRESS)
        return;

    EyLogic::World const w = EyReadWorld(bg, nullptr, TEAM_ALLIANCE);
    std::lock_guard<std::mutex> guard(s_eyWatchLock);
    EyWatch& watch = s_eyWatch[bg->GetInstanceID()];
    for (uint8 b = 0; b < EyLogic::BASE_COUNT; ++b)
    {
        uint8 const now = w.owner[b] == EyLogic::TEAM_NONE ? uint8(2) : w.owner[b];   // 0 Alliance, 1 Horde, 2 neutral
        if (watch.owner[b] == now)
            continue;
        if (watch.owner[b] != 0xFE)
        {
            ++watch.changes;
            SF_LOG_DEBUG("playerbots", "[BGEY] base-change inst=%u base=%s %s->%s bar=%d score=%u-%u", bg->GetInstanceID(),
                         EY_BASE_NAMES[b], EyOwnerName(watch.owner[b] == 2 ? EyLogic::TEAM_NONE : watch.owner[b]),
                         EyOwnerName(w.owner[b]), w.bar[b], bg->GetTeamScore(TEAM_ALLIANCE), bg->GetTeamScore(TEAM_HORDE));
        }
        watch.owner[b] = now;
    }

    uint32 const nowMs = getMSTime();
    if (watch.lastLogMs && getMSTimeDiff(watch.lastLogMs, nowMs) < 30000)
        return;
    watch.lastLogMs = nowMs;
    SF_LOG_DEBUG("playerbots", "[BGEY] state inst=%u score=%u-%u changes=%u flag=%u fr=%s/%d be=%s/%d dr=%s/%d mt=%s/%d",
                 bg->GetInstanceID(), bg->GetTeamScore(TEAM_ALLIANCE), bg->GetTeamScore(TEAM_HORDE), watch.changes,
                 uint32(w.flagState), EyOwnerName(w.owner[0]), w.bar[0], EyOwnerName(w.owner[1]), w.bar[1],
                 EyOwnerName(w.owner[2]), w.bar[2], EyOwnerName(w.owner[3]), w.bar[3]);
}

// ---- The flag layer (BT-583, sheet sections 6 and 16) ---------------------------------------------------------------------
// The duty of a free bot for the state of the flag, from the plan's record (AbPlan::GetEyeView, FreeRank): computed at most once a
// second per bot, kept so that the objective is chosen again only when the duty changes.
constexpr uint32 EY_DUTY_RECHECK_MS = 1000;
constexpr float EY_CARRIER_TURN_ROUND_FRACTION = 0.4f;   // a carrier past 60 percent of its way does not turn for another base

struct EyDuty
{
    EyFlag::Duty duty = EyFlag::Duty::NONE;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    uint8 base = EyLogic::BASE_NONE;
    uint32 rank = 0;
    uint32 checkedMs = 0;
};

std::mutex s_eyDutyLock;
std::unordered_map<uint32, EyDuty> s_eyDuty;

struct EyCarry
{
    uint8 base = EyLogic::BASE_NONE;
    float startDistance = 0.0f;
};

std::mutex s_eyCarrierLock;
std::unordered_map<uint32, EyCarry> s_eyCarry;    // bot guid -> the base its carrier runs to
std::unordered_map<uint32, uint32> s_eyCarrierCheckMs;   // bot guid -> when the carrier last chose its base

float EyDist2d(float ax, float ay, float bx, float by)
{
    return EyLogic::Dist2d(ax, ay, bx, by);
}

// The base an enemy carrier runs to: the nearest base of its team (not ours) to it.
uint8 EyTargetBase(EyLogic::World const& w, float x, float y)
{
    uint8 best = EyLogic::BASE_NONE;
    float bestDist = 0.0f;
    for (uint8 b = 0; b < EyLogic::BASE_COUNT; ++b)
    {
        if (w.owner[b] == EyLogic::TEAM_NONE || w.owner[b] == w.team)
            continue;
        float const d = EyDist2d(x, y, EyLogic::TRIGGER_POS[b].x, EyLogic::TRIGGER_POS[b].y);
        if (best == EyLogic::BASE_NONE || d < bestDist)
        {
            best = b;
            bestDist = d;
        }
    }
    return best;
}

// The duty of this bot now. NONE when the node plan decides.
EyDuty EyComputeDuty(Battleground* bg, Player* bot, EyLogic::World const& w)
{
    EyDuty out;
    AbPlan::EyeView view;
    if (!AbPlan::GetEyeView(bg, bot, view) || !view.free)
        return out;

    BattlegroundEY* const ey = static_cast<BattlegroundEY*>(bg);
    BotSkillMath::Tier const tier = BotSkill::TierOf(bot);
    EyFlag::DutyInput in;
    in.flagState = w.flagState;
    in.weCarry = w.teamCarries;
    in.theyCarry = w.enemyCarries;
    in.isHealer = PlayerBotSpec::IsHeal(bot, true);
    in.free = true;
    in.yields = view.yields;

    EyFlag::ActionInput action;
    action.ourBases = view.ourBases;
    action.theirBases = view.theirBases;
    action.enemiesNearStand = view.enemiesStand25;
    action.oddsAtStand = view.oddsStand40;
    action.tier = tier;
    // The decision layer's answer to D15 that still stands replaces the rule's pickup action.
    in.action = view.flagAction >= 0 ? static_cast<EyFlag::Action>(view.flagAction) : EyFlag::RuleAction(action);

    Player* carrier = ey->IsFlagPickedup() ? ObjectAccessor::FindPlayer(ey->GetFlagPickerGUID()) : nullptr;
    uint32 rank = 99;
    float tx = EyLogic::CENTRE_FLAG.x, ty = EyLogic::CENTRE_FLAG.y, tz = EyLogic::CENTRE_FLAG.z;

    if (w.flagState == EyFlag::FLAG_ON_GROUND)
    {
        GameObject* const dropped = ObjectAccessor::GetGameObject(*bot, ey->GetDroppedFlagGUID());
        if (!dropped)
            return out;
        tx = dropped->GetPositionX();
        ty = dropped->GetPositionY();
        tz = dropped->GetPositionZ();
        if (AbPlan::FreeRank(bg, bot, tx, ty, false, false, rank))
            in.rankDropped = rank;
    }
    else if (w.flagState == EyFlag::FLAG_ON_PLAYER && carrier && carrier != bot)
    {
        tx = carrier->GetPositionX();
        ty = carrier->GetPositionY();
        tz = carrier->GetPositionZ();
        if (w.teamCarries)
        {
            if (AbPlan::FreeRank(bg, bot, tx, ty, false, false, rank))
                in.rankCarrier = rank;
            if (in.isHealer && AbPlan::FreeRank(bg, bot, tx, ty, true, false, rank))
                in.rankHealer = rank;
        }
        else if (w.enemyCarries)
        {
            if (AbPlan::FreeRank(bg, bot, tx, ty, false, false, rank))
                in.rankEfc = rank;
            uint8 const target = EyTargetBase(w, tx, ty);
            EyFlag::EfcInput efc;
            efc.theirBases = view.theirBases;
            efc.distanceToEfc = bot->GetDistance(tx, ty, tz);
            efc.haveTarget = target != EyLogic::BASE_NONE;
            if (efc.haveTarget)
            {
                efc.targetFresh = view.heldForMs[target] < EyFlag::FRESH_BASE_MS || !EyLogic::Hardened(view.bar[target], w.owner[target]);
                efc.targetGuards = view.enemies[target];
                // The expert nearest to the target base is the one that may go in and neutralise it (expert rule).
                uint32 expertRank = 99;
                if (tier == BotSkillMath::Tier::EXPERT &&
                    AbPlan::FreeRank(bg, bot, EyLogic::TRIGGER_POS[target].x, EyLogic::TRIGGER_POS[target].y, false, true, expertRank) &&
                    expertRank == 0)
                {
                    efc.expertAvailable = true;
                    in.neutralizer = true;
                }
                out.base = target;
            }
            in.efc = view.efcAction >= 0 ? static_cast<EyFlag::EfcAction>(view.efcAction) : EyFlag::EfcActions(efc).front().action;
        }
    }
    else
    {
        if (AbPlan::FreeRank(bg, bot, tx, ty, false, false, rank))
            in.rankStand = rank;
    }

    out.duty = EyFlag::DutyFor(in);
    out.rank = std::min(in.rankStand, 99u);
    switch (out.duty)
    {
        case EyFlag::Duty::NONE:
            break;
        case EyFlag::Duty::TAKE_FLAG:
            out.x = EyLogic::CENTRE_FLAG.x;
            out.y = EyLogic::CENTRE_FLAG.y;
            out.z = EyLogic::CENTRE_FLAG.z;
            break;
        case EyFlag::Duty::STAGE:
            EyStagePoint(bot->GetGUIDLow(), out.rank, out.x, out.y, out.z);
            break;
        case EyFlag::Duty::ESCORT:
        case EyFlag::Duty::HEAL_CARRIER:
        case EyFlag::Duty::HUNT:
        case EyFlag::Duty::CLICK_DROPPED:
            out.x = tx;
            out.y = ty;
            out.z = tz;
            break;
        case EyFlag::Duty::CUT:
        case EyFlag::Duty::NEUTRALIZE:
            if (out.base >= EyLogic::BASE_COUNT)
            {
                out.duty = EyFlag::Duty::NONE;
                break;
            }
            EyPostPoint(out.base, bot->GetGUIDLow(), 0, out.x, out.y, out.z);
            break;
    }
    return out;
}

// The duty of the bot, at most once a second; `fresh` forces a computation.
EyDuty EyDutyOf(Battleground* bg, Player* bot, EyLogic::World const& w, bool fresh)
{
    uint32 const nowMs = getMSTime();
    {
        std::lock_guard<std::mutex> guard(s_eyDutyLock);
        auto const itr = s_eyDuty.find(bot->GetGUIDLow());
        if (!fresh && itr != s_eyDuty.end() && getMSTimeDiff(itr->second.checkedMs, nowMs) < EY_DUTY_RECHECK_MS)
            return itr->second;
    }
    EyDuty duty = EyComputeDuty(bg, bot, w);
    duty.checkedMs = nowMs ? nowMs : 1;
    std::lock_guard<std::mutex> guard(s_eyDutyLock);
    s_eyDuty[bot->GetGUIDLow()] = duty;
    return duty;
}

// The carrier's base (decision point D16): the sheet's score over the bases its team holds, from the plan's record. BASE_NONE when
// the plan cannot say.
uint8 EyCarrierBase(Battleground* bg, Player* bot, EyLogic::World const& w, bool& listed)
{
    listed = false;
    AbPlan::EyeView view;
    if (!AbPlan::GetEyeView(bg, bot, view))
        return EyLogic::BASE_NONE;

    EyFlag::BaseFact facts[EyLogic::BASE_COUNT];
    unsigned enemiesNear = 0;
    uint32 escorts = 0;
    AbPlan::SurroundingsOf(bg, bot, enemiesNear, escorts);
    for (uint8 b = 0; b < EyLogic::BASE_COUNT; ++b)
    {
        EyFlag::BaseFact& f = facts[b];
        f.owned = w.owner[b] == w.team;
        f.distance = bot->GetDistance(EyLogic::TRIGGER_POS[b].x, EyLogic::TRIGGER_POS[b].y, EyLogic::TRIGGER_POS[b].z);
        bool const inside = EyLogic::InCircle(b, bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ());
        f.guards = view.friends[b] > (inside ? 1u : 0u) ? view.friends[b] - (inside ? 1u : 0u) : 0u;
        f.enemies = view.enemies[b];
        f.margin = EyLogic::Margin(view.bar[b], w.team);
        f.nearOwnStart = EyLogic::IsNearBase(w.team, b);
    }
    std::vector<EyFlag::DestOption> const list = EyFlag::Destinations(facts, escorts > 0, enemiesNear);
    uint8 best = EyFlag::RuleDestination(list);
    listed = !list.empty();
    // The decision layer's answer to D16 that still stands (a base we hold) replaces the rule's base.
    if (view.carrierBase >= 0 && view.carrierBase < EyLogic::BASE_COUNT && w.owner[view.carrierBase] == w.team)
        best = static_cast<uint8>(view.carrierBase);
    return best;
}
}  // namespace

// The objective of a bot (BT-580, BT-582, BT-583): the carrier runs to the base its team holds that scores best (D16), a free bot
// follows the duty the flag gives it (the taker, the cover, the escort, the hunters, the dropped flag), every other bot plays the
// node plan (AbPlan.cpp over the Eye of the Storm row of BgNodeMap.h), and when the plan cannot say (no record yet, the bot has no
// slot) the old case: the same answer for every bot from the base owners, the bars and the roster, with no random role.
bool BGTactics::selectObjectiveEY(Battleground* bg)
{
    if (!bg || bg->GetMapId() != EyLogic::MAP_ID)
        return false;

    TeamId const team = BgTeamIdOf(bot);
    EyLogic::World const w = EyReadWorld(bg, bot, team);

    EyLogic::Me me;
    me.guidLow = bot->GetGUIDLow();
    me.x = bot->GetPositionX();
    me.y = bot->GetPositionY();
    me.level = bot->getLevel();
    me.carrying = bot->HasAura(BG_EY_NETHERSTORM_FLAG_SPELL);

    PositionMap& posMap = context->GetValue<PositionMap&>("position")->Get();
    PositionInfo pos = posMap["bg objective"];
    auto const setObjective = [&](float x, float y, float z)
    {
        pos.Set(x, y, z, bot->GetMapId());
        posMap["bg objective"] = pos;
        return true;
    };

    if (me.carrying)
    {
        // D16: the base of the plan's score; none held: keep clear on the own side. The base is chosen again every 2 s (eyTick).
        bool listed = false;
        uint8 base = EyCarrierBase(bg, bot, w, listed);
        if (base >= EyLogic::BASE_COUNT && !listed)
            base = EyLogic::NearestOwnedBase(w, me.x, me.y);
        if (base < EyLogic::BASE_COUNT)
        {
            {
                std::lock_guard<std::mutex> guard(s_eyCarrierLock);
                EyCarry& carry = s_eyCarry[bot->GetGUIDLow()];
                if (carry.base != base)
                {
                    carry.base = base;
                    carry.startDistance = bot->GetDistance(EyLogic::TRIGGER_POS[base].x, EyLogic::TRIGGER_POS[base].y,
                                                           EyLogic::TRIGGER_POS[base].z);
                }
            }
            // The trigger itself: the carrier scores only inside the capture range (eyTick).
            return setObjective(EyLogic::TRIGGER_POS[base].x, EyLogic::TRIGGER_POS[base].y, EyLogic::TRIGGER_POS[base].z);
        }
        Position const& spot = team == TEAM_ALLIANCE ? EY_RETREAT_ALLIANCE : EY_RETREAT_HORDE;
        float x, y, z;
        bot->GetRandomPoint(spot, 5.0f, x, y, z);
        return setObjective(x, y, z);
    }

    if (AbPlan::Enabled(bg))
    {
        EyDuty const duty = EyDutyOf(bg, bot, w, true);
        if (duty.duty != EyFlag::Duty::NONE)
            return setObjective(duty.x, duty.y, duty.z);

        AbPlan::Objective objective;
        if (AbPlan::Decide(bg, bot, GetBotStrategyForTeam(bg, TEAM_ALLIANCE), GetBotStrategyForTeam(bg, TEAM_HORDE), objective))
            return setObjective(objective.x, objective.y, objective.z);
    }

    std::vector<EyLogic::Member> const roster = EyTeamOf(bg, bot);
    BgPlan::Plan const plan = BgPlan::PlanFromStrategy(GetBotStrategyForTeam(bg, team));
    EyLogic::Choice const choice = EyLogic::Choose(w, plan, roster, me);

    float x = 0.0f, y = 0.0f, z = 0.0f;
    switch (choice.what)
    {
        case EyLogic::What::CARRY:
        case EyLogic::What::RETREAT:
        case EyLogic::What::NOTHING:
            return false;
        case EyLogic::What::POST:
        case EyLogic::What::TAKE:
            EyPostPoint(choice.base, bot->GetGUIDLow(), choice.rank, x, y, z);
            break;
        case EyLogic::What::FLAG:
            x = EyLogic::CENTRE_FLAG.x;
            y = EyLogic::CENTRE_FLAG.y;
            z = EyLogic::CENTRE_FLAG.z;
            break;
        case EyLogic::What::STAGE:
            EyStagePoint(bot->GetGUIDLow(), choice.rank, x, y, z);
            break;
    }
    return setObjective(x, y, z);
}

// Nothing is clicked here: a base is taken by standing in its circle, the flag by the cast of atFlag.
bool BGTactics::atObjectiveEY(Battleground* bg)
{
    if (!bg)
        return false;
    EyObserve(bg);
    return false;
}

// Once a second per bot: a carrier uses its defensive, speed and freedom spells while it runs, scores the moment it stands inside the
// capture range of the trigger of a base its team holds (owner, 2026-11-18: 2.5 yd, the core's trigger is 3 yd; the old case called the
// core from up to 13 yd away) and chooses its base again every 2 s (a base can fall while it runs; it never turns for another base
// once past 60 percent of its way while the one it runs to is still ours and standing); any other bot follows its flag duty: the
// objective is chosen again when the duty changes and refreshed while it follows a moving target.
void BGTactics::eyTick(Battleground* bg)
{
    if (!bg || !bot->IsAlive() || bg->GetStatus() != STATUS_IN_PROGRESS)
        return;

    TeamId const team = BgTeamIdOf(bot);
    EyLogic::World const w = EyReadWorld(bg, bot, team);

    if (bot->HasAura(BG_EY_NETHERSTORM_FLAG_SPELL))
    {
        WsgCarrier::UseHelpers(botAI, bot, bg);

        for (uint8 b = 0; b < EyLogic::BASE_COUNT; ++b)
        {
            if (w.owner[b] != w.team)
                continue;
            if (bot->GetDistance(EyLogic::TRIGGER_POS[b].x, EyLogic::TRIGGER_POS[b].y, EyLogic::TRIGGER_POS[b].z) <=
                EyLogic::CAPTURE_RANGE)
            {
                // See the note at the top of BattleGroundTactics.cpp: CMSG_AREATRIGGER is bit-packed here, so go straight to the
                // battleground's handler; the core checks the owner and the carrier itself.
                bg->HandleAreaTrigger(bot, EyLogic::TRIGGER_ID[b]);
                return;
            }
        }

        uint32 const nowMs = getMSTime();
        {
            std::lock_guard<std::mutex> guard(s_eyCarrierLock);
            uint32& last = s_eyCarrierCheckMs[bot->GetGUIDLow()];
            if (last && getMSTimeDiff(last, nowMs) < EY_CARRIER_RECHECK_MS)
                return;
            last = nowMs;
        }

        bool listed = false;
        uint8 best = EyCarrierBase(bg, bot, w, listed);
        if (best >= EyLogic::BASE_COUNT && !listed)
            best = EyLogic::NearestOwnedBase(w, bot->GetPositionX(), bot->GetPositionY());
        EyCarry carry;
        {
            std::lock_guard<std::mutex> guard(s_eyCarrierLock);
            auto const itr = s_eyCarry.find(bot->GetGUIDLow());
            if (itr != s_eyCarry.end())
                carry = itr->second;
        }
        bool turn = best != carry.base;
        if (turn && carry.base < EyLogic::BASE_COUNT && w.owner[carry.base] == w.team && carry.startDistance > 0.0f)
        {
            float const left = bot->GetDistance(EyLogic::TRIGGER_POS[carry.base].x, EyLogic::TRIGGER_POS[carry.base].y,
                                                EyLogic::TRIGGER_POS[carry.base].z);
            if (left <= carry.startDistance * EY_CARRIER_TURN_ROUND_FRACTION)
                turn = false;
        }
        if (turn)
            resetObjective();
        return;
    }

    // The flag duty of a free bot: at most once a second.
    EyDuty previous;
    {
        std::lock_guard<std::mutex> guard(s_eyDutyLock);
        auto const itr = s_eyDuty.find(bot->GetGUIDLow());
        if (itr != s_eyDuty.end())
            previous = itr->second;
    }
    uint32 const nowMs = getMSTime();
    if (previous.checkedMs && getMSTimeDiff(previous.checkedMs, nowMs) < EY_DUTY_RECHECK_MS)
        return;

    EyDuty const duty = EyDutyOf(bg, bot, w, true);
    if (duty.duty != previous.duty || (duty.duty == EyFlag::Duty::CUT && duty.base != previous.base))
    {
        resetObjective();
        return;
    }
    if (EyFlag::IsDynamic(duty.duty))
    {
        PositionMap& posMap = context->GetValue<PositionMap&>("position")->Get();
        PositionInfo pos = posMap["bg objective"];
        pos.Set(duty.x, duty.y, duty.z, bot->GetMapId());
        posMap["bg objective"] = pos;
    }
}

// The plan's taker casts the centre flag; the rest of the middle group stays off it (so ten bots do not start ten 8 s casts).
// A dropped flag is an instant click and stays open to everybody (atFlag only asks for the stand).
bool BGTactics::eyMayTakeFlag(Battleground* bg)
{
    if (!bg || bg->GetMapId() != EyLogic::MAP_ID)
        return true;
    TeamId const team = BgTeamIdOf(bot);
    EyLogic::World const w = EyReadWorld(bg, bot, team);
    if (AbPlan::Enabled(bg))
        return EyDutyOf(bg, bot, w, false).duty == EyFlag::Duty::TAKE_FLAG;

    EyLogic::Me me;
    me.guidLow = bot->GetGUIDLow();
    me.x = bot->GetPositionX();
    me.y = bot->GetPositionY();
    me.level = bot->getLevel();
    std::vector<EyLogic::Member> const roster = EyTeamOf(bg, bot);
    BgPlan::Plan const plan = BgPlan::PlanFromStrategy(GetBotStrategyForTeam(bg, team));
    return EyLogic::Choose(w, plan, roster, me).what == EyLogic::What::FLAG;
}

// The bot has a flag duty at the moment (the last one eyTick or the objective chose): it is not at a post of the node plan.
bool BGTactics::eyHasDuty()
{
    std::lock_guard<std::mutex> guard(s_eyDutyLock);
    auto const itr = s_eyDuty.find(bot->GetGUIDLow());
    return itr != s_eyDuty.end() && itr->second.duty != EyFlag::Duty::NONE;
}
