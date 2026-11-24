/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE). See WsgCarrier.h for what this is.
 */

#include "WsgCarrier.h"

// DatabaseEnv.h must precede the battleground headers (see BattleGroundTactics.cpp).
#include "DatabaseEnv.h"

#include "AbPlan.h"
#include "BattlegroundEY.h"
#include "BgFlagView.h"
#include "EyPlanLogic.h"
#include "BotSkill.h"
#include "Battleground.h"
#include "GameObject.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotSpec.h"
#include "Map.h"
#include "MotionMaster.h"
#include "Playerbots.h"
#include "Timer.h"
#include "WsgPlan.h"

#include <mutex>
#include <unordered_map>

using namespace WsgLogic;

namespace
{
// Inside this distance of its own flag on the stand the carrier is "home": it keeps running to capture (the old rule's
// 25 yd exception, kept).
constexpr float NEAR_OWN_CAPTURE_YARDS = 25.0f;
// The decision is cached for this long so the two callers of one tick (the path attacker and the hold check) agree and
// the roster is read once.
constexpr uint32 DECISION_CACHE_MS = 200;
// A carrier that was not looked at for this long starts a new fight episode (the old hold window's rule).
constexpr uint32 EPISODE_GAP_MS = 10000;
// The helpers are tried at most this often per bot.
constexpr uint32 HELPER_GAP_MS = 1000;
// A carry that was not seen for this long ended (a drop, a death or a capture) and the next flag is a new carry.
constexpr uint32 CARRY_GAP_MS = 5000;

// Class lists of spell ids, verified against Spell.dbc names (2026-11-17). Order is the order they are tried in.
struct SpellList
{
    uint8 cls;
    uint32 spells[4];
};

// Cooldowns that keep the flag. Left out on purpose: Divine Shield 642, Ice Block 45438 and Hand of Protection 1022 (they
// drop the flag, wiki; the rotation guard already blocks Divine Shield), Cloak of Shadows 31224 and Dispersion 47585
// (immunity and dispel effects: whether they drop the flag is to check live).
constexpr SpellList DEFENSIVES[] = {
    { CLASS_WARRIOR, { 871, 118038, 55694, 0 } },    // Shield Wall, Die by the Sword, Enraged Regeneration
    { CLASS_PALADIN, { 498, 0, 0, 0 } },             // Divine Protection
    { CLASS_HUNTER, { 19263, 0, 0, 0 } },            // Deterrence
    { CLASS_ROGUE, { 5277, 0, 0, 0 } },              // Evasion
    { CLASS_PRIEST, { 19236, 0, 0, 0 } },            // Desperate Prayer
    { CLASS_DEATH_KNIGHT, { 48792, 48707, 0, 0 } },  // Icebound Fortitude, Anti-Magic Shell
    { CLASS_SHAMAN, { 108271, 0, 0, 0 } },           // Astral Shift
    { CLASS_MAGE, { 11426, 0, 0, 0 } },              // Ice Barrier
    { CLASS_WARLOCK, { 104773, 0, 0, 0 } },          // Unending Resolve
    { CLASS_MONK, { 115203, 0, 0, 0 } },             // Fortifying Brew
    { CLASS_DRUID, { 22812, 61336, 0, 0 } },         // Barkskin, Survival Instincts
};

// Run speed. Aspect of the Cheetah is left out: a hunter that is struck is dazed (a slow), the opposite of the job.
constexpr SpellList SPEED[] = {
    { CLASS_ROGUE, { 2983, 108212, 0, 0 } },         // Sprint, Burst of Speed
    { CLASS_PALADIN, { 85499, 0, 0, 0 } },           // Speed of Light
    { CLASS_SHAMAN, { 58875, 0, 0, 0 } },            // Spirit Walk
    { CLASS_DEATH_KNIGHT, { 96268, 0, 0, 0 } },      // Death's Advance
    { CLASS_DRUID, { 1850, 0, 0, 0 } },              // Dash
};

// Roots and snares off the carrier.
constexpr SpellList FREEDOM[] = {
    { CLASS_PALADIN, { 1044, 0, 0, 0 } },            // Hand of Freedom
    { CLASS_SHAMAN, { 58875, 0, 0, 0 } },            // Spirit Walk
    { CLASS_ROGUE, { 108212, 0, 0, 0 } },            // Burst of Speed
    { CLASS_DRUID, { 1850, 0, 0, 0 } },              // Dash
};

struct CarrierState
{
    uint32 lastSeenMs = 0;
    uint32 lastHitMs = 0;
    uint32 lastHp = 0;
    uint32 decidedMs = 0;
    bool decided = false;
    CarrierFacts facts;
    CarrierOption decision = { CarrierChoice::RUN, 0, "" };
    uint32 helperMs = 0;
    // The carry: when it was first seen with the flag and how many carries this bot has had (for the log).
    uint32 carryStartMs = 0;
    uint32 carrySeenMs = 0;
    uint32 carryIndex = 0;
};

std::mutex g_lock;
std::unordered_map<uint32, CarrierState> g_states;

// The maps the carrier rule plays: Warsong Gulch and Twin Peaks (the two-flag plan) and, since BT-583, Eye of the Storm.
bool CarrierMap(Battleground* bg)
{
    return bg && (WsgPlan::Enabled(bg) || bg->GetMapId() == EyLogic::MAP_ID);
}

// Within 25 yd of its own flag object while that flag is on its stand: the capture is a few steps away. Eye of the Storm: within
// 25 yd of the trigger of a base its team holds.
bool NearOwnCapture(Player* bot, Battleground* bg)
{
    if (bg->GetMapId() == EyLogic::MAP_ID)
    {
        BattlegroundEY* const ey = static_cast<BattlegroundEY*>(bg);
        uint32 const team = bot->GetBGTeam();
        for (uint8 b = 0; b < EyLogic::BASE_COUNT; ++b)
            if (ey->GetPointState(b) == EY_POINT_UNDER_CONTROL && ey->GetPointOwnedByTeam(b) == team &&
                bot->GetDistance(EyLogic::TRIGGER_POS[b].x, EyLogic::TRIGGER_POS[b].y, EyLogic::TRIGGER_POS[b].z) <= NEAR_OWN_CAPTURE_YARDS)
                return true;
        return false;
    }
    for (uint8 carried = TEAM_ALLIANCE; carried <= TEAM_HORDE; ++carried)
    {
        if (bot->GetGUID() != bg->GetFlagPickerGUID(carried))
            continue;

        // The picker of the Alliance flag is a Horde player, who captures at the Horde flag.
        GameObject* const ownFlag = BgFlagView::StandObject(bg, carried == TEAM_ALLIANCE ? uint32(TEAM_HORDE) : uint32(TEAM_ALLIANCE));
        return ownFlag && ownFlag->isSpawned() && bot->GetDistance(ownFlag) <= NEAR_OWN_CAPTURE_YARDS;
    }
    return false;
}

// Gathers every fact of the carrier decision. The state (the hit window) is updated under the lock.
CarrierFacts GatherFacts(Player* bot, Battleground* bg, CarrierState& state, uint32 nowMs)
{
    CarrierFacts f;
    f.tier = BotSkill::TierOf(bot);
    f.nearOwnCapture = NearOwnCapture(bot, bg);
    f.attackers = uint32(bot->getAttackers().size());
    f.hpPercent = bot->GetHealthPct();
    f.rooted = bot->HasUnitState(UNIT_STATE_ROOT | UNIT_STATE_CONTROLLED);
    f.slowed = bot->GetSpeedRate(MOVE_RUN) < CARRIER_SLOWED_RATE;

    if (WsgPlan::Enabled(bg))
    {
        WsgPlan::Surroundings around;
        if (WsgPlan::SurroundingsOf(bg, bot, around))
        {
            f.enemiesNear = around.enemiesNear;
            f.escortsNear = around.escortsNear;
        }
    }
    else
        AbPlan::SurroundingsOf(bg, bot, f.enemiesNear, f.escortsNear);

    // A new episode opens with the attack that brought the carrier here; afterwards only a health drop is a hit.
    uint32 const hp = bot->GetHealth();
    if (!state.lastSeenMs || getMSTimeDiff(state.lastSeenMs, nowMs) > EPISODE_GAP_MS || hp < state.lastHp)
        state.lastHitMs = nowMs;
    state.lastSeenMs = nowMs;
    state.lastHp = hp;
    f.hitRecently = getMSTimeDiff(state.lastHitMs, nowMs) <= CARRIER_NO_HIT_MS;
    return f;
}

bool TryCast(PlayerbotAI* botAI, Player* bot, SpellList const* lists, size_t count)
{
    for (size_t i = 0; i < count; ++i)
    {
        if (lists[i].cls != bot->getClass())
            continue;

        for (uint32 spell : lists[i].spells)
        {
            if (!spell || !bot->HasSpell(spell) || bot->HasSpellCooldown(spell) || !botAI->CanCastSpell(spell, bot))
                continue;
            if (botAI->CastSpell(spell, bot))
                return true;
        }
    }
    return false;
}
// The decision line of the carrier (info level, owner 2026-11-17): written when the choice or its reason changed or a new
// carry began, at most every 10 s per bot, and only in a match with a human (WsgPlan::DecisionLogDue).
void LogDecision(Player* bot, Battleground* bg, CarrierState const& state, uint32 nowMs)
{
    if (!WsgPlan::Enabled(bg))
        return;   // the decision line is the two-flag plan's (its log gate lives in its record)
    CarrierFacts const& f = state.facts;
    char const* const why = state.decision.why;
    uint32 const signature = (uint32(state.decision.choice) << 24) | (uint32(why[0]) << 16) | (uint32(why[1]) << 8) |
                             (state.carryIndex & 0xFF);
    uint32 playSeconds = 0;
    if (!WsgPlan::DecisionLogDue(bg, bot, 1, signature, playSeconds))
        return;

    WsgPlan::Surroundings around;
    WsgPlan::SurroundingsOf(bg, bot, around);
    SF_LOG_INFO("playerbots",
                "[BGWSG] t=%us carrier %s class=%u lvl=%u tier=%s carry=%us -> %s why=%s attackers=%u enemies=%u escorts=%u "
                "healers=%u nearest=%.0f hp=%.0f rooted=%d slowed=%d own-stand=%d pos=(%.0f,%.0f,%.0f) map=%u",
                playSeconds, bot->GetName().c_str(), uint32(bot->getClass()), uint32(bot->getLevel()),
                f.tier == Tier::EXPERT ? "expert" : f.tier == Tier::GOOD ? "good" : "casual",
                getMSTimeDiff(state.carryStartMs, nowMs) / 1000, CarrierChoiceName(state.decision.choice), why, f.attackers,
                f.enemiesNear, f.escortsNear, around.healerFollowers, around.nearestHealer, f.hpPercent, f.rooted ? 1 : 0,
                f.slowed ? 1 : 0, f.nearOwnCapture ? 1 : 0, bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(),
                bg->GetMapId());
}

// The carrier's current decision, at most DECISION_CACHE_MS old. The caller holds g_lock.
CarrierState& Evaluate(Player* bot, Battleground* bg, uint32 nowMs)
{
    CarrierState& state = g_states[bot->GetGUIDLow()];
    if (state.decided && getMSTimeDiff(state.decidedMs, nowMs) < DECISION_CACHE_MS)
        return state;

    if (!state.carrySeenMs || getMSTimeDiff(state.carrySeenMs, nowMs) > CARRY_GAP_MS)
    {
        state.carryStartMs = nowMs;
        ++state.carryIndex;
    }
    state.carrySeenMs = nowMs;

    state.facts = GatherFacts(bot, bg, state, nowMs);
    state.decision = ChooseCarrier(state.facts);
    state.decidedMs = nowMs;
    state.decided = true;
    LogDecision(bot, bg, state, nowMs);
    return state;
}
}  // namespace

namespace WsgCarrier
{
bool SuppressTarget(Player* bot)
{
    if (!bot || !bot->IsAlive())
        return false;
    Battleground* bg = bot->GetBattleground();
    if (!WsgPlan::Enabled(bg) || !BgFlagView::CarriesFlag(bot))
        return false;
    if (!bg->GetBgMap())
        return true;

    uint32 enemies = 0, fighters = 0;
    for (auto const& ref : bg->GetBgMap()->GetPlayers())
    {
        Player* other = ref.GetSource();
        if (!other || other == bot || !other->IsInWorld() || !other->IsAlive() || other->IsGameMaster() ||
            other->GetBattlegroundId() != bg->GetInstanceID())
            continue;
        if (other->GetBGTeam() != bot->GetBGTeam())
        {
            if (bot->GetDistance(other) <= CARRIER_ALONE_RADIUS)
                ++enemies;
        }
        else if (!PlayerBotSpec::IsHeal(other, true) && bot->GetDistance(other) <= ESCORT_DISTANCE)
            ++fighters;
    }
    return !CarrierMayFight(uint32(bot->getAttackers().size()), enemies, fighters, bot->GetHealthPct(), BotSkill::TierOf(bot));
}

void DropTarget(PlayerbotAI* botAI, Player* bot)
{
    if (!botAI || !SuppressTarget(bot))
        return;
    Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    if (target)
    {
        static std::unordered_map<uint32, uint32> lastLog;
        std::lock_guard<std::mutex> guard(g_lock);
        uint32 const now = getMSTime();
        uint32& last = lastLog[bot->GetGUIDLow()];
        if (!last || getMSTimeDiff(last, now) >= 10000)
        {
            SF_LOG_DEBUG("playerbots", "[BGWSG] carrier %s drops offensive target %s", bot->GetName().c_str(), target->GetName().c_str());
            last = now ? now : 1;
        }
        botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Set(nullptr);
        bot->SetTarget(BotGuid::Empty);
        bot->SetSelection(BotGuid());
    }
    if (target || bot->GetVictim())
    {
        bot->AttackStop();
        bot->InterruptSpell(CURRENT_AUTOREPEAT_SPELL);
    }
    if (bot->GetMotionMaster()->GetCurrentMovementGeneratorType() == CHASE_MOTION_TYPE &&
        bot->GetMotionMaster()->GetMotionSlotType(MOTION_SLOT_CONTROLLED) == NULL_MOTION_TYPE)
    {
        bot->GetMotionMaster()->Clear(false);
        bot->StopMoving();
    }
}

bool Fights(Player* bot, Battleground* bg)
{
    if (!bot || !bg || !CarrierMap(bg) || !bot->IsAlive() || !BgFlagView::CarriesFlag(bot))
        return false;

    if (WsgPlan::Enabled(bg))
        return !SuppressTarget(bot);
    std::lock_guard<std::mutex> guard(g_lock);
    return CarrierFights(Evaluate(bot, bg, getMSTime()).decision.choice);
}

void UseHelpers(PlayerbotAI* botAI, Player* bot, Battleground* bg)
{
    if (!botAI || !bot || !bg || !CarrierMap(bg) || !bot->IsAlive() || !BgFlagView::CarriesFlag(bot) || bot->GetVehicle())
        return;

    // Called every tick of a carrier: this also writes the first decision line of a carry, at the pickup.
    uint32 const now = getMSTime();
    CarrierFacts facts;
    {
        std::lock_guard<std::mutex> guard(g_lock);
        CarrierState& state = Evaluate(bot, bg, now);
        if (state.helperMs && getMSTimeDiff(state.helperMs, now) < HELPER_GAP_MS)
            return;
        state.helperMs = now ? now : 1;
        facts = state.facts;
    }

    // Nobody on the carrier and nobody near: the next cooldown is kept for when it counts.
    if (facts.attackers == 0 && facts.enemiesNear == 0)
        return;

    // Rooted or slowed with somebody on it: free first, then the defensive, then speed.
    if (CarrierWantsFreedom(facts.rooted, facts.slowed, facts.attackers) &&
        TryCast(botAI, bot, FREEDOM, std::size(FREEDOM)))
        return;

    if (CarrierWantsDefensive(facts.tier, facts.hpPercent, facts.attackers) &&
        TryCast(botAI, bot, DEFENSIVES, std::size(DEFENSIVES)))
        return;

    bool const sped = bot->GetSpeedRate(MOVE_RUN) > 1.2f;
    if (CarrierWantsSpeed(facts.attackers, facts.enemiesNear, sped, facts.rooted, facts.nearOwnCapture))
        TryCast(botAI, bot, SPEED, std::size(SPEED));
}
}  // namespace WsgCarrier
