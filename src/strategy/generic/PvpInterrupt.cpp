/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE). See PvpInterrupt.h for what this is.
 */

#include "PvpInterrupt.h"

#include "BotSkill.h"
#include "Group.h"
#include "InterruptGate.h"
#include "Log.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotSpec.h"
#include "Playerbots.h"
#include "PvpAwareness.h"
#include "PvpConfig.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "Timer.h"

#include <mutex>
#include <unordered_map>

namespace PvpInterruptGlue
{
namespace
{
    // A cast the bot has not looked at for this long is a new cast when it shows up again (as MopDungeon::InterruptGate).
    constexpr std::uint32_t FORGET_MS = 1500;
    // The area control of a caster this close to the bot reaches it (Howl of Terror, Psychic Scream, Shockwave).
    constexpr float AREA_CONTROL_YARDS = 12.0f;
    // One log line per cast and bot.
    constexpr std::size_t TRACK_MAX = 256;
    constexpr std::uint32_t TRACK_IDLE_MS = 10000;

    struct Track
    {
        Spell const* cast = nullptr;
        std::uint32_t firstSeenMs = 0;
        std::uint32_t lastSeenMs = 0;
        std::uint32_t castKey = 0;
        bool logged = false;
    };

    std::mutex g_trackLock;
    std::unordered_map<std::uint64_t, Track> g_tracks;  // (bot low guid << 32) | caster low guid

    std::uint64_t TrackKey(Player* bot, Unit* caster)
    {
        return (std::uint64_t(bot->GetGUIDLow()) << 32) | std::uint32_t(caster->GetGUID());
    }

    bool HasEffectOrAura(SpellInfo const* info, SpellEffects effect, AuraType aura)
    {
        return info->HasEffect(effect) || info->HasAura(aura);
    }

    void Sweep(std::uint32_t nowMs)
    {
        for (auto it = g_tracks.begin(); it != g_tracks.end();)
            it = getMSTimeDiff(it->second.lastSeenMs, nowMs) > TRACK_IDLE_MS ? g_tracks.erase(it) : std::next(it);
    }
}  // namespace

PvpInterrupt::CastShape ShapeOf(Spell* cast)
{
    SpellInfo const* info = cast->GetSpellInfo();
    PvpInterrupt::CastShape s;
    s.spellId = info->Id;
    s.mechanicMask = info->GetAllEffectsMechanicMask();
    s.positive = info->IsPositive();
    s.heal = HasEffectOrAura(info, SPELL_EFFECT_HEAL, SPELL_AURA_PERIODIC_HEAL) || info->HasEffect(SPELL_EFFECT_HEAL_PCT) ||
             info->HasEffect(SPELL_EFFECT_HEAL_MAX_HEALTH);
    s.damage = HasEffectOrAura(info, SPELL_EFFECT_SCHOOL_DAMAGE, SPELL_AURA_PERIODIC_DAMAGE);
    s.channeled = info->IsChanneled();
    s.castMs = s.channeled ? 0u : static_cast<std::uint32_t>(std::max(cast->GetCastTime(), 0));
    return s;
}

bool Governs(Player* bot, Unit* caster)
{
    if (!PvpConfig::InterruptRulesEnabled() || !bot || !caster || caster == bot)
        return false;
    // A player, a player's pet or totem: never a monster (the dungeon interrupts keep their own turn order).
    if (!caster->GetCharmerOrOwnerPlayerOrPlayerItself())
        return false;
    return bot->IsHostileTo(caster);
}

Evaluation Evaluate(Player* bot, Unit* caster)
{
    Evaluation out;
    if (!Governs(bot, caster))
        return out;
    out.governed = true;

    Spell* cast = MopDungeon::CurrentInterruptCast(caster);
    if (!cast || !cast->GetSpellInfo())
        return out;  // nothing to stop: governed, and the decision holds

    std::uint32_t const now = getMSTime();
    std::uint32_t castKey;
    std::uint32_t since;
    bool firstLook = false;
    {
        std::lock_guard<std::mutex> lock(g_trackLock);
        if (g_tracks.size() > TRACK_MAX)
            Sweep(now);
        Track& t = g_tracks[TrackKey(bot, caster)];
        if (t.cast != cast || !t.firstSeenMs || getMSTimeDiff(t.lastSeenMs, now) > FORGET_MS)
        {
            t = Track();
            t.cast = cast;
            t.firstSeenMs = now;
            t.castKey = BotSkillMath::Mix(std::uint32_t(caster->GetGUID()) ^ std::uint32_t(reinterpret_cast<std::uintptr_t>(cast) >> 4) ^ now);
            firstLook = true;
        }
        t.lastSeenMs = now;
        castKey = t.castKey;
        since = getMSTimeDiff(t.firstSeenMs, now);
    }

    SpellInfo const* info = cast->GetSpellInfo();
    PvpInterrupt::CastFacts f;
    f.tier = BotSkill::TierOf(bot);
    f.botGuid = bot->GetGUIDLow();
    f.castKey = castKey;
    f.shape = ShapeOf(cast);
    f.castSchool = info->GetSchoolMask();
    f.casterClass = caster->getClass();

    Player* casterPlayer = caster->ToPlayer();
    f.casterIsHealer = casterPlayer && PlayerBotSpec::IsHeal(casterPlayer, true);
    f.ownTarget = bot->GetVictim() == caster || bot->GetTarget() == caster->GetGUID();

    Unit* target = cast->m_targets.GetUnitTarget();
    bool const area = (!target || target == caster) && caster->GetExactDist(bot) <= AREA_CONTROL_YARDS;
    f.onUs = target == bot || (area && f.shape.mechanicMask != 0);
    f.onOurSide = f.onUs || (target && target != caster && bot->IsFriendlyTo(target));
    if (target && target != caster && f.onOurSide)
    {
        Player* targetPlayer = target->ToPlayer();
        f.onOurHealer = targetPlayer && PlayerBotSpec::IsHeal(targetPlayer, true);
        f.allyHp = target->GetHealthPct();
    }
    else if (f.onUs)
        f.allyHp = bot->GetHealthPct();

    Unit* healed = target ? target : caster;
    f.healTargetHp = healed->GetHealthPct();
    f.healOnCasterItself = healed == caster;
    f.healTargetIsKillTarget = bot->GetVictim() == healed || bot->GetTarget() == healed->GetGUID();

    if (std::shared_ptr<PvpAware::Book> const book = PvpAwareness::ViewOf(bot))
        f.schoolAlreadyLocked = book->SchoolLocked(caster->GetGUID(), f.castSchool, now);

    out.decision = PvpInterrupt::Decide(f);
    // The order among the group's kickers is counted only for a cast worth a kick (it scans the group).
    if (out.decision.wants && bot->GetGroup())
    {
        f.rank = MopDungeon::InterrupterRank(bot, caster);
        out.decision = PvpInterrupt::Decide(f);
    }
    out.sinceFirstLookMs = since;
    out.due = PvpInterrupt::Due(out.decision, since);

    if (firstLook)
        SF_LOG_DEBUG("playerbots",
                     "[PVPINT] %s caster=%s spell=%u class=%s why=%s score=%d at=%u rank=%u tier=%u school=%u onus=%d side=%d hp=%.0f",
                     bot->GetName().c_str(), caster->GetName().c_str(), info->Id, PvpInterrupt::ClassName(out.decision.cls),
                     out.decision.why, out.decision.score, out.decision.kickAtMs, f.rank, std::uint32_t(f.tier), f.castSchool,
                     int(f.onUs), int(f.onOurSide), f.healTargetHp);
    return out;
}

bool Wants(Player* bot, Unit* caster)
{
    Evaluation const e = Evaluate(bot, caster);
    return !e.governed || e.decision.wants;
}

bool Allow(Player* bot, Unit* caster)
{
    Evaluation const e = Evaluate(bot, caster);
    return !e.governed || e.due;
}
}  // namespace PvpInterruptGlue
