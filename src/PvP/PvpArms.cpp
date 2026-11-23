/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE). See PvpArms.h for what this is.
 */

#include "PvpArms.h"

#include "Action.h"
#include "BotSkill.h"
#include "LastSpellCastValue.h"
#include "Log.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotSpec.h"
#include "Playerbots.h"
#include "PvpArmsPolicy.h"
#include "PvpAwareness.h"
#include "PvpBurst.h"
#include "PvpConfig.h"
#include "PvpTeam.h"
#include "PvpUnitFacts.h"
#include "Timer.h"

#include <mutex>
#include <unordered_map>
#include <vector>

namespace PvpArmsGlue
{
namespace
{
using namespace PvpArms;

constexpr char const* LOG_CATEGORY = "playerbots";
constexpr time_t GAP_CLOSER_FRESH_SECONDS = 3;
constexpr float CONE_HALF_ANGLE = float(M_PI) / 2.0f;
constexpr std::size_t CLOCK_MAX = 256;

std::mutex g_lock;
std::unordered_map<std::uint32_t, std::uint32_t> g_windowSince;

bool Active(Player* bot)
{
    return PvpConfig::BurstRulesEnabled() && bot && bot->IsAlive() && bot->getClass() == CLASS_WARRIOR;
}

Unit* CurrentTarget(PlayerbotAI* botAI)
{
    return botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
}

bool Ready(Player* bot, uint32 spellId)
{
    return bot->HasSpell(spellId) && !bot->HasSpellCooldown(spellId);
}

// How long the burst window has been open for this bot (0 when closed); the pool of rage is given up after GO_POOL_TIMEOUT_MS.
std::uint32_t WindowOpenMs(Player* bot, bool open, std::uint32_t now)
{
    std::lock_guard<std::mutex> guard(g_lock);
    if (!open)
    {
        g_windowSince.erase(bot->GetGUIDLow());
        return 0;
    }
    if (g_windowSince.size() > CLOCK_MAX)
        g_windowSince.clear();
    auto const it = g_windowSince.find(bot->GetGUIDLow());
    if (it == g_windowSince.end())
    {
        g_windowSince[bot->GetGUIDLow()] = now ? now : 1;
        return 0;
    }
    return getMSTimeDiff(it->second, now);
}

bool GapCloserJustLanded(PlayerbotAI* botAI)
{
    LastSpellCast& last = botAI->GetAiObjectContext()->GetValue<LastSpellCast&>("last spell cast")->Get();
    bool const closer = last.id == SPELL_CHARGE || last.id == 145048 || last.id == SPELL_HEROIC_LEAP || last.id == 54506;
    return closer && last.timer != 0 && time(nullptr) - last.timer <= GAP_CLOSER_FRESH_SECONDS;
}

StunInputs StunFacts(PlayerbotAI* botAI, Player* bot, Unit* target)
{
    StunInputs in;
    in.targetIsPlayer = PvpBurstGlue::TargetIsEnemyPlayer(botAI, bot, target);
    if (!in.targetIsPlayer)
        return in;
    std::uint32_t const now = getMSTime();
    in.windowOpen = PvpBurstGlue::Open(botAI, bot, target);
    in.afterGapCloser = GapCloserJustLanded(botAI);
    in.targetHeld = PvpUnitFacts::HasBreakableCc(target) || PvpUnitFacts::IsHeldStill(target) ||
        target->HasAuraType(SPELL_AURA_MOD_STUN);
    if (BotSkill::TierOf(bot) == BotSkillMath::Tier::EXPERT)
        if (std::shared_ptr<PvpAware::Book> const book = PvpAwareness::ViewOf(bot))
            in.stunDrStep = book->DrStep(target->GetGUID(), PvpSpells::DrCategory::STUN, now);
    in.stormBoltReady = Ready(bot, SPELL_STORM_BOLT);
    in.shockwaveReady = Ready(bot, SPELL_SHOCKWAVE);
    in.targetDistance = bot->GetExactDist(target);
    in.targetInFront = bot->isInFront(target, CONE_HALF_ANGLE);
    GuidVector const near = botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest enemy players")->Get();
    for (BotGuid const guid : near)
    {
        Unit* unit = botAI->GetUnit(guid);
        Player* enemy = unit ? unit->ToPlayer() : nullptr;
        if (enemy && enemy->IsAlive() && bot->IsValidAttackTarget(enemy) && bot->GetExactDist(enemy) <= SHOCKWAVE_RANGE_YD &&
            bot->isInFront(enemy, CONE_HALF_ANGLE))
            ++in.enemiesInCone;
    }
    return in;
}
}  // namespace

bool GoHeld(PlayerbotAI* botAI, Player* bot, Action* action)
{
    if (!action || !Active(bot))
        return false;
    std::string const name = action->getName();
    if (name != "recklessness" && name != "skull banner" && name != "avatar")
        return false;
    Unit* target = CurrentTarget(botAI);
    GoInputs in;
    in.targetIsPlayer = PvpBurstGlue::TargetIsEnemyPlayer(botAI, bot, target);
    if (!in.targetIsPlayer)
        return false;
    in.windowOpen = PvpBurstGlue::Open(botAI, bot, target);
    in.rage = uint32(bot->GetPower(POWER_RAGE) / 10);
    in.windowOpenMs = WindowOpenMs(bot, in.windowOpen, getMSTime());
    return !GoAllowed(Go(in, BotSkill::TierOf(bot)));
}

bool StunDue(PlayerbotAI* botAI, Player* bot)
{
    if (!Active(bot))
        return false;
    return StunChain(StunFacts(botAI, bot, CurrentTarget(botAI)), BotSkill::TierOf(bot)).cast;
}

bool StunCast(PlayerbotAI* botAI, Player* bot)
{
    if (!Active(bot))
        return false;
    Unit* target = CurrentTarget(botAI);
    StunVerdict const verdict = StunChain(StunFacts(botAI, bot, target), BotSkill::TierOf(bot));
    if (!verdict.cast || !target || !botAI->CastSpell(verdict.spellId, target))
        return false;
    SF_LOG_DEBUG(LOG_CATEGORY, "[PVPARMS] bot=%u tier=%u kind=STUN spell=%u target=%u score=%d", bot->GetGUIDLow(),
        uint32(BotSkill::TierOf(bot)), verdict.spellId, target->GetGUIDLow(), verdict.score);
    return true;
}

bool DisarmDue(PlayerbotAI* botAI, Player* bot)
{
    if (!Active(bot))
        return false;
    Unit* target = CurrentTarget(botAI);
    if (!PvpBurstGlue::TargetIsEnemyPlayer(botAI, bot, target))
        return false;
    DisarmInputs in;
    in.ready = Ready(bot, SPELL_DISARM) || Ready(bot, SPELL_DISARM_ARMS);
    in.targetIsMeleePlayer = PlayerBotSpec::IsMelee(target->ToPlayer());
    in.alreadyDisarmed = target->HasAuraType(SPELL_AURA_MOD_DISARM);
    in.targetDistance = bot->GetExactDist(target);
    in.targetStunned = target->HasUnitState(UNIT_STATE_STUNNED);
    in.targetBursting = PvpTeamGlue::HasBurstAura(target);
    in.windowOpen = PvpBurstGlue::Open(botAI, bot, target);
    return PvpArms::DisarmDue(Disarm(in, BotSkill::TierOf(bot)));
}

void Forget(uint32 guidLow)
{
    std::lock_guard<std::mutex> guard(g_lock);
    g_windowSince.erase(guidLow);
}
}  // namespace PvpArmsGlue
