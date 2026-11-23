/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE). See PvpDefense.h for what this is.
 */

#include "PvpDefense.h"

#include "BotSkill.h"
#include "Group.h"
#include "Item.h"
#include "OneVsOne.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotSpec.h"
#include "Playerbots.h"
#include "PvpAwareness.h"
#include "PvpConfig.h"
#include "PvpRotationValues.h"
#include "SpellAuras.h"
#include "SpellHistory.h"
#include "SpellInfo.h"
#include "Timer.h"

#include <mutex>
#include <unordered_map>

namespace PvpDefenseGlue
{
namespace
{
    constexpr std::uint32_t SAMPLE_GAP_MS = 100;
    constexpr std::uint32_t SAMPLE_KEEP_MS = 4000;
    constexpr std::uint32_t LOSS_WINDOW_MS = 3000;
    // A go that ended this long ago and starts again is a new go (the mistake roll is drawn again).
    constexpr std::uint32_t NEW_GO_AFTER_MS = 6000;
    // The teammates counted for the kill window: within this distance of the bot.
    constexpr float TEAM_RANGE = 40.0f;
    // The enemies looked at for the healer and the follow-up: within this distance.
    constexpr float ENEMY_RANGE = 40.0f;
    constexpr std::uint32_t DEFAULT_STUN_MS = 3000;
    // A bot below this health or hit by anyone gets the full look; a healthy bot that nobody hits is not looked at.
    constexpr float QUIET_HP = 95.0f;

    struct Track
    {
        std::vector<PvpDefense::HealthSample> samples;
        std::uint32_t lastDefensiveMs = 0;
        std::uint32_t goSeenMs = 0;       // the last time a go was on
        std::uint32_t goKey = 1;
        std::uint32_t controlSince = 0;   // since when the bot has been held (0: not)
        std::uint32_t wantSince = 0;      // since when a defensive has been wanted (0: not)
    };

    std::mutex g_lock;
    std::unordered_map<std::uint32_t, Track> g_tracks;

    bool PlayerControlled(Unit* unit)
    {
        return unit && unit->GetCharmerOrOwnerPlayerOrPlayerItself() != nullptr;
    }

    // What hits the bot, as far as the defensive that answers it is concerned.
    struct Threat
    {
        std::uint32_t attackers = 0;
        bool melee = false;
        bool caster = false;
    };

    Threat ThreatOn(Player* bot)
    {
        Threat t;
        for (Unit* attacker : bot->getAttackers())
        {
            if (!attacker || !attacker->IsAlive() || !PlayerControlled(attacker))
                continue;
            ++t.attackers;
            Player* player = attacker->ToPlayer();
            if (!player)
            {
                t.melee = true;  // a pet bites
                continue;
            }
            // A hunter's arrows are physical but not melee: neither Evasion nor Cloak of Shadows answers them.
            OneVsOne::Role const role = OneVsOne::RoleOfClass(player->getClass(), PlayerBotSpec::IsRanged(player, true),
                                                            PlayerBotSpec::IsTank(player, true), true);
            if (role == OneVsOne::Role::MELEE)
                t.melee = true;
            else if (player->getClass() != CLASS_HUNTER)
                t.caster = true;
        }
        return t;
    }

    Track& TrackOf(Player* bot) { return g_tracks[bot->GetGUIDLow()]; }

    // The health samples of the bot; the newest is now.
    void Sample(Track& t, Player* bot, std::uint32_t nowMs)
    {
        if (t.samples.empty() || getMSTimeDiff(t.samples.back().atMs, nowMs) >= SAMPLE_GAP_MS)
            t.samples.push_back({ nowMs, bot->GetHealthPct() });
        else
            t.samples.back().hpPct = bot->GetHealthPct();
        std::size_t drop = 0;
        while (drop < t.samples.size() && getMSTimeDiff(t.samples[drop].atMs, nowMs) > SAMPLE_KEEP_MS)
            ++drop;
        if (drop)
            t.samples.erase(t.samples.begin(), t.samples.begin() + static_cast<std::ptrdiff_t>(drop));
    }

    // The own spells of a kind the bot knows, as candidates.
    bool Ready(PlayerbotAI* botAI, Player* bot, std::uint32_t spellId)
    {
        return bot->HasSpell(spellId) && !bot->HasSpellCooldown(spellId) && !bot->HasAura(spellId) &&
               botAI->CanCastSpell(spellId, bot, true);
    }

    bool DefensiveRunning(Player* bot)
    {
        for (PvpSpells::Kind kind : { PvpSpells::Kind::DEFENSIVE_TEMPO, PvpSpells::Kind::DEFENSIVE_IMMUNITY })
            for (PvpSpells::Entry const& e : PvpSpells::RowsFor(bot->getClass(), kind))
                if (bot->HasAura(e.spellId))
                    return true;
        return false;
    }

    // An enemy attacker whose class holds a silence the bot's book did not see used: the next one may come any moment.
    bool SilenceExpected(Player* bot)
    {
        std::shared_ptr<PvpAware::Book> const book = PvpAwareness::ViewOf(bot);
        std::uint32_t const now = getMSTime();
        for (Unit* attacker : bot->getAttackers())
        {
            Player* player = attacker && attacker->IsAlive() ? attacker->ToPlayer() : nullptr;
            if (!player)
                continue;
            for (PvpSpells::Entry const& e : PvpSpells::InterruptsOf(player->getClass()))
                if (e.kind == PvpSpells::Kind::SILENCE && (!book || PvpAware::AssumeReady(book->CooldownOf(player->GetGUID(), e.spellId, now))))
                    return true;
        }
        return false;
    }

    std::uint32_t KindOfCategory(PvpSpells::DrCategory c) { return PvpSpells::CcKindOfCategory(c); }

    // The control on the bot: the kinds it carries, the hard kind with the longest time left, and that time.
    void ReadControl(Player* bot, std::uint32_t& kinds, std::uint32_t& worstKind, std::uint32_t& remainingMs)
    {
        kinds = 0;
        worstKind = 0;
        remainingMs = 0;
        std::uint32_t worstHard = 0;
        std::uint32_t worstMoving = 0;
        for (auto const& entry : bot->GetAppliedAuras())
        {
            Aura const* aura = entry.second->GetBase();
            SpellInfo const* info = aura ? aura->GetSpellInfo() : nullptr;
            if (!info || entry.second->IsPositive())
                continue;
            std::uint32_t const k = PvpSpells::CcKindsOfMechanics(info->GetAllEffectsMechanicMask());
            if (!(k & (PvpSpells::CC_HARD | PvpSpells::CC_ROOT | PvpSpells::CC_SNARE)))
                continue;
            kinds |= k;
            std::uint32_t const left = aura->GetDuration() > 0 ? static_cast<std::uint32_t>(aura->GetDuration()) : 0;
            if ((k & PvpSpells::CC_HARD) && left >= worstHard)
            {
                worstHard = left;
                // One bit: the first hard kind of the spell.
                std::uint32_t const hard = k & PvpSpells::CC_HARD;
                worstKind = hard & (~hard + 1);
            }
            else if (left >= worstMoving)
                worstMoving = left;
        }
        // A stun without an aura we can read (the core's own unit state): assume a few seconds.
        if (!(kinds & PvpSpells::CC_HARD) && bot->HasUnitState(UNIT_STATE_STUNNED | UNIT_STATE_FLEEING | UNIT_STATE_CONFUSED))
        {
            kinds |= bot->HasUnitState(UNIT_STATE_STUNNED) ? PvpSpells::CC_STUN
                     : bot->HasUnitState(UNIT_STATE_FLEEING) ? PvpSpells::CC_FEAR : PvpSpells::CC_DISORIENT;
            worstKind = kinds & PvpSpells::CC_HARD;
            worstHard = DEFAULT_STUN_MS;
        }
        remainingMs = worstKind ? worstHard : worstMoving;
    }

    bool TeammateLow(Player* bot)
    {
        Group* group = bot->GetGroup();
        if (!group)
            return false;
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (member && member != bot && member->IsAlive() && member->IsInWorld() && member->GetMap() == bot->GetMap() &&
                member->GetExactDist(bot) <= TEAM_RANGE && member->GetHealthPct() <= PvpDefense::TEAMMATE_LOW_HP)
                return true;
        }
        return false;
    }

    // Enemy healers of the bot near it, one of them held by a hard control.
    bool EnemyHealerHeld(PlayerbotAI* botAI, Player* bot)
    {
        GuidVector const near = botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest enemy players")->Get();
        for (BotGuid const guid : near)
        {
            Unit* unit = botAI->GetUnit(guid);
            Player* player = unit ? unit->ToPlayer() : nullptr;
            if (player && player->IsAlive() && bot->GetExactDist(player) <= ENEMY_RANGE && PlayerBotSpec::IsHeal(player, true) &&
                IsCcValue::IsCrowdControlled(player))
                return true;
        }
        return false;
    }

    // An enemy the bot's book did not see use all its control: a follow-up that could land after the first effect ends.
    bool FollowUpPossible(PlayerbotAI* botAI, Player* bot, std::uint32_t worstKind)
    {
        std::shared_ptr<PvpAware::Book> const book = PvpAwareness::ViewOf(bot);
        if (!book)
            return false;
        PvpSpells::DrCategory except = PvpSpells::DrCategory::NONE;
        for (std::uint8_t c = 1; c < static_cast<std::uint8_t>(PvpSpells::DrCategory::COUNT); ++c)
            if (KindOfCategory(static_cast<PvpSpells::DrCategory>(c)) == worstKind)
            {
                except = static_cast<PvpSpells::DrCategory>(c);
                break;
            }
        std::uint32_t const now = getMSTime();
        GuidVector const near = botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest enemy players")->Get();
        for (BotGuid const guid : near)
        {
            Unit* unit = botAI->GetUnit(guid);
            Player* player = unit ? unit->ToPlayer() : nullptr;
            if (player && player->IsAlive() && bot->GetExactDist(player) <= ENEMY_RANGE &&
                book->FollowUpControlPossible(player->GetGUID(), player->getClass(), bot->GetGUID(), except, now))
                return true;
        }
        return false;
    }
}  // namespace

Item* ReadyTrinket(Player* bot)
{
    for (uint8 slot : { EQUIPMENT_SLOT_TRINKET1, EQUIPMENT_SLOT_TRINKET2 })
    {
        Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        ItemTemplate const* proto = item ? item->GetTemplate() : nullptr;
        if (!proto)
            continue;
        for (auto const& spell : proto->Spells)
        {
            if (spell.SpellId != PvpSpells::TRINKET_SPELL)
                continue;
            SpellInfo const* info = sSpellMgr->GetSpellInfo(PvpSpells::TRINKET_SPELL);
            if (info && BotSpellHistory(bot)->IsReady(info, item->GetEntry(), false))
                return item;
        }
    }
    return nullptr;
}

bool InPvp(PlayerbotAI* botAI, Player* bot)
{
    if (!PvpConfig::DefensiveRulesEnabled() || !bot || !bot->IsAlive())
        return false;
    return bot->InBattleground() || bot->InArena() || OneVsOneGlue::Owns(botAI, bot);
}

DefensiveVerdict Defensive(PlayerbotAI* botAI, Player* bot, bool due)
{
    DefensiveVerdict out;
    if (!PvpConfig::DefensiveRulesEnabled() || !bot->IsAlive())
        return out;

    std::uint32_t const now = getMSTime();
    Threat const threat = ThreatOn(bot);

    PvpDefense::DefFacts f;
    f.tier = BotSkill::TierOf(bot);
    f.guid = bot->GetGUIDLow();
    f.hpPct = bot->GetHealthPct();
    f.attackers = threat.attackers;
    f.meleeThreat = threat.melee;
    f.casterThreat = threat.caster;
    {
        std::lock_guard<std::mutex> lock(g_lock);
        Track& t = TrackOf(bot);
        Sample(t, bot, now);
        f.lossPct3s = PvpDefense::LossOver(t.samples, now, LOSS_WINDOW_MS);
        if (PvpDefense::GoOn(f))
        {
            if (!t.goSeenMs || getMSTimeDiff(t.goSeenMs, now) > NEW_GO_AFTER_MS)
                t.goKey = BotSkillMath::Mix(f.guid ^ now);
            t.goSeenMs = now;
        }
        f.goKey = t.goKey;
        f.sinceLastDefensiveMs = t.lastDefensiveMs ? getMSTimeDiff(t.lastDefensiveMs, now) : 0xFFFFFFFFu;
    }
    // A healthy bot that nobody hits needs no look; the cheapest exit.
    if (!due && f.hpPct >= QUIET_HP && threat.attackers == 0)
        return out;

    f.defensiveRunning = DefensiveRunning(bot);
    f.silenceExpected = f.tier == BotSkillMath::Tier::EXPERT && SilenceExpected(bot);

    std::vector<PvpDefense::Candidate> candidates;
    for (PvpSpells::Kind kind : { PvpSpells::Kind::DEFENSIVE_TEMPO, PvpSpells::Kind::DEFENSIVE_IMMUNITY })
        for (PvpSpells::Entry const& e : PvpSpells::RowsFor(bot->getClass(), kind))
        {
            PvpDefense::Candidate c;
            c.spellId = e.spellId;
            c.kind = e.kind;
            c.fit = e.fit;
            c.silenceBlocked = e.silenceBlocked;
            c.cooldownMs = e.cooldownMs;
            c.ready = Ready(botAI, bot, e.spellId);
            candidates.push_back(c);
        }

    PvpDefense::DefPick const pick = PvpDefense::Pick(f, candidates, due);
    out.why = pick.why;
    if (pick.use)
    {
        out.use = true;
        out.spellId = candidates[static_cast<std::size_t>(pick.index)].spellId;
    }
    return out;
}

void NoteDefensiveCast(Player* bot)
{
    std::lock_guard<std::mutex> lock(g_lock);
    TrackOf(bot).lastDefensiveMs = getMSTime();
}

BreakVerdict Break(PlayerbotAI* botAI, Player* bot, bool needsToMove)
{
    BreakVerdict out;
    if (!PvpConfig::DefensiveRulesEnabled() || !bot->IsAlive())
        return out;

    PvpDefense::BreakFacts f;
    ReadControl(bot, f.kinds, f.worstKind, f.remainingMs);
    if (!(f.kinds & (PvpSpells::CC_HARD | PvpSpells::CC_ROOT | PvpSpells::CC_SNARE)))
    {
        std::lock_guard<std::mutex> lock(g_lock);
        TrackOf(bot).controlSince = 0;  // free again: the next effect is a new one
        return out;
    }

    std::uint32_t const now = getMSTime();
    f.tier = BotSkill::TierOf(bot);
    f.guid = bot->GetGUIDLow();
    f.hpPct = bot->GetHealthPct();
    {
        std::lock_guard<std::mutex> lock(g_lock);
        Track& t = TrackOf(bot);
        Sample(t, bot, now);
        f.lossPctPerSec = PvpDefense::LossOver(t.samples, now, LOSS_WINDOW_MS) / (static_cast<float>(LOSS_WINDOW_MS) / 1000.0f);
        if (!t.controlSince)
            t.controlSince = now;
        // One roll per effect: the control that began at this moment.
        f.effectKey = BotSkillMath::Mix(f.guid ^ t.controlSince);
    }
    f.isHealer = PlayerBotSpec::IsHeal(bot, true);
    f.teammateLow = TeammateLow(bot);
    f.enemyBurst = f.lossPctPerSec * 3.0f >= PvpDefense::BURST_LOSS_PERCENT || f.teammateLow;
    f.enemyHealerHeld = EnemyHealerHeld(botAI, bot);
    if (Unit* victim = bot->GetVictim())
        f.killTargetHp = victim->GetHealthPct();
    f.followUpPossible = f.tier == BotSkillMath::Tier::EXPERT && FollowUpPossible(botAI, bot, f.worstKind);
    f.needsToMove = needsToMove;

    std::vector<PvpDefense::BreakCandidate> candidates;
    std::vector<std::uint32_t> ids;
    for (PvpSpells::Entry const& e : PvpSpells::RowsFor(bot->getClass(), PvpSpells::Kind::BREAKER))
    {
        PvpDefense::BreakCandidate c;
        c.spellId = e.spellId;
        c.breaks = e.breaks;
        c.cost = e.cost;
        c.ready = bot->HasSpell(e.spellId) && !bot->HasSpellCooldown(e.spellId);
        candidates.push_back(c);
    }
    Item* trinket = ReadyTrinket(bot);
    {
        PvpDefense::BreakCandidate c;
        c.spellId = PvpSpells::TRINKET_SPELL;
        c.breaks = PvpSpells::CC_MEDALLION;
        c.cost = PvpSpells::BREAK_COST_MEDALLION;
        c.ready = trinket != nullptr;
        candidates.push_back(c);
    }

    PvpDefense::BreakPick const pick = PvpDefense::PickBreak(f, candidates);
    out.why = pick.why;
    if (!pick.use)
        return out;
    out.use = true;
    out.spellId = candidates[static_cast<std::size_t>(pick.index)].spellId;
    if (out.spellId == PvpSpells::TRINKET_SPELL)
    {
        out.trinket = true;
        out.item = trinket;
    }
    return out;
}

bool RacialTriggerMayFire(PlayerbotAI* botAI, Player* bot)
{
    return !InPvp(botAI, bot);
}
}  // namespace PvpDefenseGlue
