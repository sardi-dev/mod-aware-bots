/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE). See PvpBurst.h for what this is.
 */

#include "PvpBurst.h"

#include "BotSkill.h"
#include "Log.h"
#include "OneVsOne.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "PvpAwareness.h"
#include "PvpConfig.h"
#include "PvpTeam.h"
#include "PvpUnitFacts.h"
#include "Timer.h"

#include <mutex>
#include <unordered_map>

namespace PvpBurstGlue
{
namespace
{
// A fight clock not refreshed for this long starts again (the bot left combat or switched target and came back).
constexpr std::uint32_t CLOCK_GAP_MS = 4000;
constexpr std::size_t CLOCK_MAX = 256;
// An answer this fresh for the same target is reused.
constexpr std::uint32_t CACHE_MS = 150;

struct FightClock
{
    std::uint32_t targetLow = 0;
    std::uint32_t sinceMs = 0;
    std::uint32_t lastMs = 0;
    PvpBurst::Reason lastReason = PvpBurst::Reason::NONE;
    bool lastOpen = false;
    // The answer is shared by every trigger and multiplier asked in the same tick (a bot asks several times per tick).
    std::uint32_t cachedMs = 0;
    std::uint32_t cachedTargetLow = 0;
    PvpBurst::Verdict cached;
};

std::mutex g_lock;
std::unordered_map<std::uint32_t, FightClock> g_clocks;

// How long the bot has fought this target, in milliseconds; also remembers what it answered last, for the log.
std::uint32_t Touch(Player* bot, Player* enemy, std::uint32_t now)
{
    std::lock_guard<std::mutex> guard(g_lock);
    if (g_clocks.size() > CLOCK_MAX)
        for (auto it = g_clocks.begin(); it != g_clocks.end();)
            it = getMSTimeDiff(it->second.lastMs, now) > CLOCK_GAP_MS * 4 ? g_clocks.erase(it) : std::next(it);

    FightClock& clock = g_clocks[bot->GetGUIDLow()];
    bool const fresh = clock.targetLow != enemy->GetGUIDLow() || getMSTimeDiff(clock.lastMs, now) > CLOCK_GAP_MS || !bot->IsInCombat();
    if (fresh)
    {
        clock.targetLow = enemy->GetGUIDLow();
        clock.sinceMs = now;
    }
    clock.lastMs = now;
    return bot->IsInCombat() ? getMSTimeDiff(clock.sinceMs, now) : 0;
}

void LogChange(Player* bot, Player* enemy, PvpBurst::Verdict const& verdict, std::uint32_t fightMs)
{
    std::lock_guard<std::mutex> guard(g_lock);
    FightClock& clock = g_clocks[bot->GetGUIDLow()];
    if (clock.lastOpen == verdict.open && clock.lastReason == verdict.reason)
        return;
    clock.lastOpen = verdict.open;
    clock.lastReason = verdict.reason;
    SF_LOG_DEBUG("playerbots", "[PVPBURST] bot=%u tier=%u target=%u open=%u why=%s score=%d fight=%u", bot->GetGUIDLow(),
        uint32(BotSkill::TierOf(bot)), enemy->GetGUIDLow(), uint32(verdict.open), PvpBurst::ReasonName(verdict.reason), verdict.score,
        fightMs);
}
}  // namespace

bool TargetIsEnemyPlayer(PlayerbotAI* /*botAI*/, Player* bot, Unit* target)
{
    Player* enemy = target ? target->ToPlayer() : nullptr;
    return enemy && enemy != bot && enemy->IsAlive() && bot->IsValidAttackTarget(enemy);
}

PvpBurst::Verdict Window(PlayerbotAI* botAI, Player* bot, Unit* target)
{
    if (!PvpConfig::BurstRulesEnabled() || !TargetIsEnemyPlayer(botAI, bot, target))
        return {};

    Player* enemy = target->ToPlayer();
    std::uint32_t const now = getMSTime();
    {
        std::lock_guard<std::mutex> guard(g_lock);
        auto const it = g_clocks.find(bot->GetGUIDLow());
        if (it != g_clocks.end() && it->second.cachedTargetLow == enemy->GetGUIDLow() && it->second.cachedMs != 0 &&
            getMSTimeDiff(it->second.cachedMs, now) < CACHE_MS)
            return it->second.cached;
    }
    BotSkillMath::Tier const tier = BotSkill::TierOf(bot);

    PvpBurst::Inputs in;
    in.targetIsEnemyPlayer = true;
    in.immune = PvpUnitFacts::IsImmune(enemy);
    in.breakableCc = PvpUnitFacts::HasBreakableCc(enemy);
    in.controlled = !in.breakableCc && PvpUnitFacts::IsHeldStill(enemy);
    in.lone = OneVsOneGlue::LoneEnemy(botAI, bot) == enemy;
    in.targetHpPercent = static_cast<std::int32_t>(enemy->GetHealthPct());
    in.fightMs = Touch(bot, enemy, now);

    // The shared go of its side and a partner's burst (R6); the team state judges the healer lock, never a teammate's book.
    PvpTeamGlue::BurstFacts const team = PvpTeamGlue::BurstFactsFor(botAI, bot, enemy);
    in.teamWindow = team.teamWindow;
    in.partnerBurst = team.partnerBurst;

    // What the bot itself saw of the medallion (its own book, never a teammate's).
    if (std::shared_ptr<PvpAware::Book> const book = PvpAwareness::ViewOf(bot))
        in.trinketKnownDown = book->TrinketReading(enemy->GetGUID(), now).state == PvpAware::State::DOWN;

    PvpBurst::Verdict const verdict = PvpBurst::Decide(in, tier);
    LogChange(bot, enemy, verdict, in.fightMs);
    {
        std::lock_guard<std::mutex> guard(g_lock);
        FightClock& clock = g_clocks[bot->GetGUIDLow()];
        clock.cachedMs = now ? now : 1;
        clock.cachedTargetLow = enemy->GetGUIDLow();
        clock.cached = verdict;
    }
    return verdict;
}

PvpBurst::Verdict WindowOnCurrentTarget(PlayerbotAI* botAI, Player* bot)
{
    return Window(botAI, bot, botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get());
}

bool Open(PlayerbotAI* botAI, Player* bot, Unit* target)
{
    return Window(botAI, bot, target).open;
}

bool OpenOnCurrentTarget(PlayerbotAI* botAI, Player* bot)
{
    return WindowOnCurrentTarget(botAI, bot).open;
}

void Forget(uint32 guidLow)
{
    std::lock_guard<std::mutex> guard(g_lock);
    g_clocks.erase(guidLow);
}
}  // namespace PvpBurstGlue
