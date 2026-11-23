/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE). See FakeCast.h for what this is.
 */

#include "FakeCast.h"

#include "BotSkill.h"
#include "FakeCastPolicy.h"
#include "Log.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "PvpAwareness.h"
#include "PvpConfig.h"
#include "PvpInterrupt.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "Timer.h"

#include <cmath>
#include <mutex>
#include <unordered_map>

namespace FakeCastGlue
{
namespace
{
    // The enemies looked at for a stopper: within this distance (the longest range of a stopper is 40 yd, Counterspell).
    constexpr float KICKER_SCAN_YARDS = 45.0f;
    constexpr std::size_t TRACK_MAX = 128;
    constexpr std::uint32_t TRACK_IDLE_MS = 30000;
    // A cast that has run this long is not the one the track remembers (a pointer can be reused).
    constexpr std::uint32_t MAX_CAST_AGE_MS = 20000;
    // A cast not looked at for this long is a new cast when it shows up again.
    constexpr std::uint32_t NEW_CAST_GAP_MS = 1500;

    struct Track
    {
        Spell const* cast = nullptr;
        std::uint32_t startMs = 0;
        std::uint32_t castKey = 0;
        FakeCast::Verdict verdict;
        std::uint32_t lastFakeMs = 0;
        std::uint32_t touchedMs = 0;
    };

    std::mutex g_lock;
    std::unordered_map<std::uint32_t, Track> g_tracks;

    // The bot's own cast that a fake could cancel: a generic cast with a cast time, still being prepared.
    Spell* FakeableCast(Player* bot)
    {
        Spell* cast = bot->GetCurrentSpell(CURRENT_GENERIC_SPELL);
        if (!cast || !cast->GetSpellInfo() || cast->getState() != SPELL_STATE_PREPARING || cast->GetCastTime() <= 0 ||
            cast->IsTriggered() || cast->IsAutoRepeat())
            return nullptr;
        return cast;
    }

    float HealTargetHp(Player* bot, Spell* cast)
    {
        Unit* target = cast->m_targets.GetUnitTarget();
        return (target ? target : bot)->GetHealthPct();
    }

    // An enemy player in sight that can stop the bot's cast now, as far as the bot can tell: it has a stopper whose cooldown the
    // bot's book did not see running, is within the stopper's range, has line of sight to the bot, faces it and is not held.
    bool KickerInRange(PlayerbotAI* botAI, Player* bot)
    {
        std::shared_ptr<PvpAware::Book> const book = PvpAwareness::ViewOf(bot);
        std::uint32_t const now = getMSTime();
        GuidVector const near = botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest enemy players")->Get();
        for (BotGuid const guid : near)
        {
            Unit* unit = botAI->GetUnit(guid);
            Player* enemy = unit ? unit->ToPlayer() : nullptr;
            if (!enemy || !enemy->IsAlive() || !bot->IsHostileTo(enemy) || !bot->CanSeeOrDetect(enemy))
                continue;
            float const dist = bot->GetExactDist(enemy);
            if (dist > KICKER_SCAN_YARDS || !enemy->IsWithinLOSInMap(bot) || !enemy->HasInArc(static_cast<float>(M_PI), bot) ||
                enemy->HasUnitState(UNIT_STATE_STUNNED | UNIT_STATE_FLEEING | UNIT_STATE_CONFUSED))
                continue;
            if (book && book->KickReading(enemy->GetGUID(), enemy->getClass(), now).state == PvpAware::State::DOWN)
                continue;
            for (PvpSpells::Entry const& row : PvpSpells::InterruptsOf(enemy->getClass()))
            {
                if (row.cost != 0)
                    continue;  // a stopper of one spec or a talent: not counted without having seen it
                SpellInfo const* info = sSpellMgr->GetSpellInfo(row.spellId);
                float const range = info ? std::max(info->GetMaxRange(false), 5.0f) : 5.0f;
                if (dist <= range + 1.0f)
                    return true;
            }
        }
        return false;
    }

    void Sweep(std::uint32_t now)
    {
        for (auto it = g_tracks.begin(); it != g_tracks.end();)
            it = getMSTimeDiff(it->second.touchedMs, now) > TRACK_IDLE_MS ? g_tracks.erase(it) : std::next(it);
    }
}  // namespace

bool CancelDue(PlayerbotAI* botAI, Player* bot)
{
    if (!PvpConfig::FakeCastEnabled() || !bot->IsAlive() || BotSkill::TierOf(bot) != BotSkillMath::Tier::EXPERT)
        return false;

    Spell* cast = FakeableCast(bot);
    std::uint32_t const now = getMSTime();
    if (!cast)
    {
        // Between two casts: the next one is a new cast even if the core hands out the same Spell address again.
        std::lock_guard<std::mutex> lock(g_lock);
        auto it = g_tracks.find(bot->GetGUIDLow());
        if (it != g_tracks.end())
            it->second.cast = nullptr;
        return false;
    }

    PvpInterrupt::CastShape const shape = PvpInterruptGlue::ShapeOf(cast);
    float const healHp = HealTargetHp(bot, cast);
    float const selfHp = bot->GetHealthPct();

    // First look at this cast: remember it, then decide once. The scan for a stopper only runs for a cast worth faking.
    bool firstLook = false;
    std::uint32_t castKey = 0;
    std::uint32_t lastFakeMs = 0;
    {
        std::lock_guard<std::mutex> lock(g_lock);
        if (g_tracks.size() > TRACK_MAX)
            Sweep(now);
        Track& t = g_tracks[bot->GetGUIDLow()];
        firstLook = t.cast != cast || getMSTimeDiff(t.touchedMs, now) > NEW_CAST_GAP_MS || getMSTimeDiff(t.startMs, now) > MAX_CAST_AGE_MS;
        t.touchedMs = now;
        if (firstLook)
        {
            t.cast = cast;
            t.startMs = now;
            t.castKey = BotSkillMath::Mix(bot->GetGUIDLow() ^ std::uint32_t(reinterpret_cast<std::uintptr_t>(cast) >> 4) ^ now);
            t.verdict = FakeCast::Verdict{};
        }
        castKey = t.castKey;
        lastFakeMs = t.lastFakeMs;
    }
    if (firstLook && FakeCast::Important(shape, healHp) && !FakeCast::Emergency(shape, healHp, selfHp))
    {
        bool const kicker = KickerInRange(botAI, bot);
        FakeCast::Facts f;
        f.tier = BotSkillMath::Tier::EXPERT;
        f.botGuid = bot->GetGUIDLow();
        f.castKey = castKey;
        f.chancePercent = PvpConfig::FakeCastChancePercent();
        f.shape = shape;
        f.healTargetHp = healHp;
        f.selfHp = selfHp;
        f.kickerInRange = kicker;
        f.sinceLastFakeMs = lastFakeMs ? getMSTimeDiff(lastFakeMs, now) : FakeCast::NO_FAKE_YET;
        FakeCast::Verdict const verdict = FakeCast::Decide(f);
        {
            std::lock_guard<std::mutex> lock(g_lock);
            Track& t = g_tracks[bot->GetGUIDLow()];
            if (t.cast == cast)
                t.verdict = verdict;
        }
        SF_LOG_DEBUG("playerbots", "[FAKECAST] %s spell=%u decision=%s why=%s cancel_at=%u cast_ms=%u kicker=%d heal_hp=%.0f",
                     bot->GetName().c_str(), shape.spellId, verdict.fake ? "fake" : "real", verdict.why, verdict.cancelAtMs,
                     shape.castMs, int(kicker), healHp);
    }

    bool fake;
    {
        std::lock_guard<std::mutex> lock(g_lock);
        Track const& t = g_tracks[bot->GetGUIDLow()];
        fake = FakeCast::CancelDue(t.verdict, getMSTimeDiff(t.startMs, now));
    }
    if (!fake)
        return false;
    // At the moment of the cancel the threat must still be there and the cast not an emergency now.
    return FakeCast::StillWorthCancelling(KickerInRange(botAI, bot), shape, healHp, selfHp);
}

bool Cancel(PlayerbotAI* botAI, Player* bot)
{
    if (!CancelDue(botAI, bot))
        return false;
    Spell* cast = FakeableCast(bot);
    if (!cast)
        return false;

    std::uint32_t const spellId = cast->GetSpellInfo()->Id;
    std::uint32_t const now = getMSTime();
    {
        std::lock_guard<std::mutex> lock(g_lock);
        Track& t = g_tracks[bot->GetGUIDLow()];
        t.lastFakeMs = now;
        t.verdict = FakeCast::Verdict{};  // one cancel per cast
    }
    bot->InterruptSpell(CURRENT_GENERIC_SPELL, false);
    SF_LOG_DEBUG("playerbots", "[FAKECAST] %s spell=%u cancelled", bot->GetName().c_str(), spellId);
    return true;
}
}  // namespace FakeCastGlue
