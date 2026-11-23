/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE). See PvpRestoDruid.h for what this is.
 */

#include "PvpRestoDruid.h"

#include "BotSkill.h"
#include "Log.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotSpec.h"
#include "Playerbots.h"
#include "PvpAllyFacts.h"
#include "PvpAwareness.h"
#include "PvpConfig.h"
#include "PvpRestoDruidPolicy.h"
#include "PvpTeam.h"
#include "PvpUnitFacts.h"
#include "ServerFacade.h"
#include "Timer.h"

#include <vector>

namespace PvpRestoDruidGlue
{
namespace
{
using namespace PvpRestoDruid;

constexpr char const* LOG_CATEGORY = "playerbots";

bool Active(Player* bot)
{
    return PvpConfig::TeamPlayEnabled() && bot && bot->IsAlive() && bot->getClass() == CLASS_DRUID;
}

bool Ready(Player* bot, uint32 spellId)
{
    return bot->HasSpell(spellId) && !bot->HasSpellCooldown(spellId);
}

struct Choice
{
    Player* ally = nullptr;
    std::int32_t score = 0;
};

Choice BestWard(Player* bot, std::vector<Player*> const& allies)
{
    Choice best;
    if (!Ready(bot, SPELL_CENARION_WARD))
        return best;
    for (Player* ally : allies)
    {
        bool burst = false;
        WardInputs in;
        in.ready = true;
        in.allyHasWard = ally->HasAura(102352);   // the ward's aura on the ally
        in.allyHealthPercent = ally->GetHealthPct();
        in.playerAttackers = PvpAllyFacts::PlayerAttackers(bot, ally, burst);
        in.druidSilenced = PvpAllyFacts::Silenced(bot);
        WardVerdict const verdict = CenarionWard(in, BotSkill::TierOf(bot));
        if (verdict.cast && (!best.ally || verdict.score > best.score))
            best = { ally, verdict.score };
    }
    return best;
}

Choice BestIronbark(Player* bot, std::vector<Player*> const& allies)
{
    Choice best;
    if (!Ready(bot, SPELL_IRONBARK))
        return best;
    for (Player* ally : allies)
    {
        bool burst = false;
        IronbarkInputs in;
        in.ready = true;
        in.allyHasIronbark = ally->HasAura(SPELL_IRONBARK);
        in.allyHealthPercent = ally->GetHealthPct();
        in.playerAttackers = PvpAllyFacts::PlayerAttackers(bot, ally, burst);
        in.enemyBurstSeen = burst;
        IronbarkVerdict const verdict = Ironbark(in, BotSkill::TierOf(bot));
        if (verdict.cast && (!best.ally || verdict.score > best.score))
            best = { ally, verdict.score };
    }
    return best;
}

struct CycloneChoice
{
    Player* target = nullptr;
    CycloneVerdict verdict;
};

CycloneChoice BestCyclone(PlayerbotAI* botAI, Player* bot, std::vector<Player*> const& allies)
{
    CycloneChoice best;
    if (!bot->HasSpell(SPELL_CYCLONE) || bot->HasSpellCooldown(SPELL_CYCLONE))
        return best;
    std::uint32_t const now = getMSTime();
    std::shared_ptr<PvpAware::Book> const book = PvpAwareness::ViewOf(bot);
    GuidVector const near = botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest enemy players")->Get();
    bool const kickNotSpent = PvpTeamGlue::EnemyStopperNotSpent(botAI, bot);
    for (BotGuid const guid : near)
    {
        Unit* unit = botAI->GetUnit(guid);
        Player* enemy = unit ? unit->ToPlayer() : nullptr;
        if (!enemy || !enemy->IsAlive() || !bot->IsValidAttackTarget(enemy) || !bot->CanSeeOrDetect(enemy) ||
            !bot->IsWithinLOSInMap(enemy))
            continue;
        CycloneInputs in;
        in.known = true;
        in.targetIsPlayer = true;
        in.targetDistance = bot->GetExactDist(enemy);
        in.targetIsHealer = PlayerBotSpec::IsHeal(enemy, true);
        in.targetBursting = PvpTeamGlue::HasBurstAura(enemy);
        for (Player* ally : allies)
            if (ally->getAttackers().count(enemy) > 0)
                ++in.allyHitByTarget;
        in.targetHeld = PvpUnitFacts::HasBreakableCc(enemy) || PvpUnitFacts::IsHeldStill(enemy) || enemy->HasAura(SPELL_CYCLONE) ||
            PvpUnitFacts::IsImmune(enemy);
        in.drStep = book ? book->DrStep(enemy->GetGUID(), PvpSpells::DrCategory::CYCLONE, now) : 0;
        in.swiftnessReady = Ready(bot, SPELL_NATURES_SWIFTNESS);
        in.swiftnessUp = bot->HasAura(SPELL_NATURES_SWIFTNESS);
        in.enemyKickNotSpent = kickNotSpent;
        CycloneVerdict const verdict = BurstCyclone(in, BotSkill::TierOf(bot));
        if ((verdict.cast || verdict.pressSwiftness) && (!best.target || verdict.score > best.verdict.score))
            best = { enemy, verdict };
    }
    return best;
}

// The melee players within reach that are hitting the druid, and the enemies near it that a teammate holds in a breakable control.
EscapeInputs EscapeFacts(PlayerbotAI* botAI, Player* bot, Player*& nearestMelee)
{
    EscapeInputs in;
    in.healthPercent = bot->GetHealthPct();
    in.displacerReady = Ready(bot, SPELL_DISPLACER_BEAST);
    in.typhoonReady = Ready(bot, SPELL_TYPHOON);
    GuidVector const near = botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest enemy players")->Get();
    for (BotGuid const guid : near)
    {
        Unit* unit = botAI->GetUnit(guid);
        Player* enemy = unit ? unit->ToPlayer() : nullptr;
        if (!enemy || !enemy->IsAlive() || !bot->IsValidAttackTarget(enemy) || bot->GetExactDist(enemy) > 15.0f)
            continue;
        if (PvpUnitFacts::HasBreakableCc(enemy))
            ++in.breakableHeldNear;
        if (bot->GetExactDist(enemy) <= MELEE_REACH_YD && bot->getAttackers().count(enemy) > 0 && PlayerBotSpec::IsMelee(enemy))
        {
            ++in.meleeAttackers;
            if (!nearestMelee || bot->GetExactDist(enemy) < bot->GetExactDist(nearestMelee))
                nearestMelee = enemy;
        }
    }
    return in;
}
}  // namespace

bool Due(PlayerbotAI* botAI, Player* bot, Rule rule)
{
    if (!Active(bot))
        return false;
    // BT-699: no line-of-sight work unless a fight against players is on, and the group read once per tick.
    if (!PvpAllyFacts::PvpFightNear(botAI, bot, HEAL_RANGE_YD))
        return false;
    std::vector<Player*> const& allies = PvpAllyFacts::ReachableCached(botAI, bot, HEAL_RANGE_YD);
    if (!PvpAllyFacts::InPvpFight(bot, allies))
        return false;

    switch (rule)
    {
        case Rule::WARD:
            return BestWard(bot, allies).ally != nullptr;
        case Rule::IRONBARK:
            return BestIronbark(bot, allies).ally != nullptr;
        case Rule::SWIFTNESS:
            return BestCyclone(botAI, bot, allies).verdict.pressSwiftness;
        case Rule::CYCLONE:
            return BestCyclone(botAI, bot, allies).verdict.cast;
        case Rule::DISPLACER:
        {
            Player* melee = nullptr;
            EscapeInputs in = EscapeFacts(botAI, bot, melee);
            in.inPvpFight = true;
            return MeleeEscape(in, BotSkill::TierOf(bot)).reason == EscapeReason::DISPLACER;
        }
        case Rule::TYPHOON:
        {
            Player* melee = nullptr;
            EscapeInputs in = EscapeFacts(botAI, bot, melee);
            in.inPvpFight = true;
            // The escape comes first: Typhoon is the peel only while the escape is not there.
            return MeleeEscape(in, BotSkill::TierOf(bot)).reason == EscapeReason::TYPHOON;
        }
    }
    return false;
}

bool Cast(PlayerbotAI* botAI, Player* bot, Rule rule)
{
    if (!Due(botAI, bot, rule))
        return false;
    std::vector<Player*> const& allies = PvpAllyFacts::ReachableCached(botAI, bot, HEAL_RANGE_YD);
    bool done = false;
    char const* what = "";
    uint32 other = 0;
    switch (rule)
    {
        case Rule::WARD:
        {
            what = "CENARION_WARD";
            Player* ally = BestWard(bot, allies).ally;
            other = ally ? ally->GetGUIDLow() : 0;
            done = ally && botAI->CastSpell(SPELL_CENARION_WARD, ally);
            break;
        }
        case Rule::IRONBARK:
        {
            what = "IRONBARK";
            Player* ally = BestIronbark(bot, allies).ally;
            other = ally ? ally->GetGUIDLow() : 0;
            done = ally && botAI->CastSpell(SPELL_IRONBARK, ally);
            break;
        }
        case Rule::SWIFTNESS:
            what = "NATURES_SWIFTNESS";
            done = botAI->CastSpell(SPELL_NATURES_SWIFTNESS, bot);
            break;
        case Rule::CYCLONE:
        {
            what = "CYCLONE";
            CycloneChoice const choice = BestCyclone(botAI, bot, allies);
            other = choice.target ? choice.target->GetGUIDLow() : 0;
            done = choice.target && botAI->CastSpell(SPELL_CYCLONE, choice.target);
            break;
        }
        case Rule::DISPLACER:
        {
            what = "DISPLACER_BEAST";
            Player* melee = nullptr;
            EscapeFacts(botAI, bot, melee);
            if (melee)
                bot->SetOrientation(bot->GetAngle(melee) + M_PI);   // the leap goes forward: turn away from the melee first
            other = melee ? melee->GetGUIDLow() : 0;
            done = botAI->CastSpell(SPELL_DISPLACER_BEAST, bot);
            break;
        }
        case Rule::TYPHOON:
        {
            what = "TYPHOON";
            Player* melee = nullptr;
            EscapeFacts(botAI, bot, melee);
            other = melee ? melee->GetGUIDLow() : 0;
            if (melee)
                sServerFacade->SetFacingTo(bot, melee);   // the cone strikes in front of the druid
            done = melee && botAI->CastSpell(SPELL_TYPHOON, bot);
            break;
        }
    }
    if (done)
        SF_LOG_DEBUG(LOG_CATEGORY, "[PVPDRUID] bot=%u tier=%u kind=%s other=%u", bot->GetGUIDLow(), uint32(BotSkill::TierOf(bot)), what,
            other);
    return done;
}
}  // namespace PvpRestoDruidGlue
