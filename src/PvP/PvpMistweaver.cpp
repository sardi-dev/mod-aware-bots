/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE). See PvpMistweaver.h for what this is.
 */

#include "PvpMistweaver.h"

#include "BotSkill.h"
#include "Group.h"
#include "Log.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "PvpAllyFacts.h"
#include "PvpConfig.h"
#include "PvpMistweaverPolicy.h"
#include "PvpTeam.h"
#include "SpellAuras.h"

#include <vector>

namespace PvpMistweaverGlue
{
namespace
{
using namespace PvpMistweaver;

constexpr char const* LOG_CATEGORY = "playerbots";

bool Active(Player* bot)
{
    return PvpConfig::TeamPlayEnabled() && bot && bot->IsAlive() && bot->getClass() == CLASS_MONK;
}

using PvpAllyFacts::LowestPercent;
using PvpAllyFacts::PlayerAttackers;
using PvpAllyFacts::Silenced;
using PvpAllyFacts::Weakest;

// BT-699: read once per AI tick.
std::vector<Player*> const& Allies(PlayerbotAI* botAI, Player* bot)
{
    return PvpAllyFacts::ReachableCached(botAI, bot, SPHERE_RANGE_YD);
}

bool PvpFight(Player* bot, std::vector<Player*> const& allies)
{
    return PvpAllyFacts::InPvpFight(bot, allies);
}

struct Choice
{
    Player* ally = nullptr;
    std::int32_t score = 0;
};

Choice BestSphere(Player* bot, std::vector<Player*> const& allies)
{
    Choice best;
    if (!bot->HasSpell(SPELL_HEALING_SPHERE))
        return best;
    for (Player* ally : allies)
    {
        bool burst = false;
        SphereInputs in;
        in.known = true;
        in.allyHealthPercent = ally->GetHealthPct();
        in.allyDistance = bot->GetExactDist(ally);
        in.spheresOut = bot->CountAreaTrigger(SPELL_HEALING_SPHERE);
        in.allyAttackedByPlayer = PlayerAttackers(bot, ally, burst) > 0;
        SphereVerdict const verdict = HealingSphere(in, BotSkill::TierOf(bot));
        if (verdict.cast && (!best.ally || verdict.score > best.score))
            best = { ally, verdict.score };
    }
    return best;
}

Choice BestCocoon(Player* bot, std::vector<Player*> const& allies)
{
    Choice best;
    if (!bot->HasSpell(SPELL_LIFE_COCOON) || bot->HasSpellCooldown(SPELL_LIFE_COCOON))
        return best;
    for (Player* ally : allies)
    {
        bool burst = false;
        CocoonInputs in;
        in.ready = true;
        in.allyHasCocoon = ally->HasAura(SPELL_LIFE_COCOON);
        in.allyHealthPercent = ally->GetHealthPct();
        in.playerAttackers = PlayerAttackers(bot, ally, burst);
        in.enemyBurstSeen = burst;
        CocoonVerdict const verdict = LifeCocoon(in, BotSkill::TierOf(bot));
        if (verdict.cast && (!best.ally || verdict.score > best.score))
            best = { ally, verdict.score };
    }
    return best;
}

TftReason TeaReason(PlayerbotAI* botAI, Player* bot, std::vector<Player*> const& allies)
{
    TftInputs in;
    in.known = bot->HasSpell(SPELL_THUNDER_FOCUS_TEA);
    in.ready = in.known && !bot->HasSpellCooldown(SPELL_THUNDER_FOCUS_TEA);
    in.teaUp = bot->HasAura(SPELL_THUNDER_FOCUS_TEA);
    in.lowestAllyPercent = LowestPercent(allies);
    Player* weakest = Weakest(allies);
    bool burst = false;
    in.allyAttackedByPlayer = weakest && PlayerAttackers(bot, weakest, burst) > 0;
    in.enemyKickNotSpent = PvpTeamGlue::EnemyStopperNotSpent(botAI, bot);
    return ThunderFocusTea(in, BotSkill::TierOf(bot));
}

bool TeaStacks(Player* bot, std::uint32_t& stacks)
{
    Aura* aura = bot->GetAura(AURA_MANA_TEA_STACKS);
    stacks = aura ? std::uint32_t(aura->GetStackAmount()) : 0;
    return bot->HasSpell(SPELL_MANA_TEA) && !bot->HasSpellCooldown(SPELL_MANA_TEA);
}

bool LustWanted(PlayerbotAI* botAI, Player* bot, std::vector<Player*> const& allies)
{
    if (!bot->HasSpell(SPELL_TIGERS_LUST) || bot->HasSpellCooldown(SPELL_TIGERS_LUST))
        return false;
    bool const selfImpaired = bot->HasUnitState(UNIT_STATE_ROOT) || bot->HasAuraType(SPELL_AURA_MOD_DECREASE_SPEED);
    bool allyImpaired = false;
    for (Player* ally : allies)
        if (ally != bot && (ally->HasUnitState(UNIT_STATE_ROOT) || ally->HasAuraType(SPELL_AURA_MOD_DECREASE_SPEED)))
            allyImpaired = true;
    (void)botAI;
    return LustDue(TigersLust(PvpFight(bot, allies), selfImpaired, allyImpaired, BotSkill::TierOf(bot)));
}

Player* ImpairedAlly(Player* bot, std::vector<Player*> const& allies)
{
    if (bot->HasUnitState(UNIT_STATE_ROOT) || bot->HasAuraType(SPELL_AURA_MOD_DECREASE_SPEED))
        return bot;
    for (Player* ally : allies)
        if (ally != bot && (ally->HasUnitState(UNIT_STATE_ROOT) || ally->HasAuraType(SPELL_AURA_MOD_DECREASE_SPEED)))
            return ally;
    return nullptr;
}
}  // namespace

bool Due(PlayerbotAI* botAI, Player* bot, Rule rule)
{
    if (!Active(bot))
        return false;
    if (!PvpAllyFacts::PvpFightNear(botAI, bot, SPHERE_RANGE_YD))
        return false;
    std::vector<Player*> const& allies = Allies(botAI, bot);
    if (!PvpFight(bot, allies))
        return false;

    switch (rule)
    {
        case Rule::SPHERE:
            return BestSphere(bot, allies).ally != nullptr;
        case Rule::COCOON:
            return BestCocoon(bot, allies).ally != nullptr;
        case Rule::TEA:
            return TeaReason(botAI, bot, allies) == TftReason::CAST_TEA;
        case Rule::SURGE:
            return bot->HasSpell(SPELL_SURGING_MIST) && TeaReason(botAI, bot, allies) == TftReason::SURGE_NOW;
        case Rule::MANA_TEA:
        {
            std::uint32_t stacks = 0;
            if (!TeaStacks(bot, stacks))
                return false;
            TeaInputs in;
            in.stacks = stacks;
            uint32 const maxMana = bot->GetMaxPower(POWER_MANA);
            in.manaPercent = maxMana ? 100.0f * bot->GetPower(POWER_MANA) / maxMana : 100.0f;
            in.silenced = Silenced(bot);
            in.lowestAllyPercent = LowestPercent(allies);
            bool burst = false;
            in.attackedByPlayer = PlayerAttackers(bot, bot, burst) > 0;
            return TeaDue(ManaTea(in, BotSkill::TierOf(bot)));
        }
        case Rule::LUST:
            return LustWanted(botAI, bot, allies);
    }
    return false;
}

bool Cast(PlayerbotAI* botAI, Player* bot, Rule rule)
{
    if (!Due(botAI, bot, rule))
        return false;
    std::vector<Player*> const& allies = Allies(botAI, bot);
    bool done = false;
    char const* what = "";
    Player* ally = nullptr;
    switch (rule)
    {
        case Rule::SPHERE:
            what = "SPHERE";
            ally = BestSphere(bot, allies).ally;
            done = ally && botAI->CastSpell(SPELL_HEALING_SPHERE, ally);
            break;
        case Rule::COCOON:
            what = "COCOON";
            ally = BestCocoon(bot, allies).ally;
            done = ally && botAI->CastSpell(SPELL_LIFE_COCOON, ally);
            break;
        case Rule::TEA:
            what = "THUNDER_FOCUS_TEA";
            ally = bot;
            done = botAI->CastSpell(SPELL_THUNDER_FOCUS_TEA, bot);
            break;
        case Rule::SURGE:
            what = "SURGING_MIST";
            ally = Weakest(allies);
            done = ally && botAI->CastSpell(SPELL_SURGING_MIST, ally);
            break;
        case Rule::MANA_TEA:
            what = "MANA_TEA";
            ally = bot;
            done = botAI->CastSpell(SPELL_MANA_TEA, bot);
            break;
        case Rule::LUST:
            what = "TIGERS_LUST";
            ally = ImpairedAlly(bot, allies);
            done = ally && botAI->CastSpell(SPELL_TIGERS_LUST, ally);
            break;
    }
    if (done)
        SF_LOG_DEBUG(LOG_CATEGORY, "[PVPMONK] bot=%u tier=%u kind=%s ally=%u", bot->GetGUIDLow(), uint32(BotSkill::TierOf(bot)), what,
            ally ? ally->GetGUIDLow() : 0u);
    return done;
}
}  // namespace PvpMistweaverGlue
