/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE). See PvpHolyPaladin.h for what this is.
 */

#include "PvpHolyPaladin.h"

#include "BotSkill.h"
#include "Log.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotSpec.h"
#include "Playerbots.h"
#include "PvpAllyFacts.h"
#include "PvpBurst.h"
#include "PvpConfig.h"
#include "PvpHolyPaladinPolicy.h"
#include "PvpTeam.h"
#include "PvpUnitFacts.h"

#include <vector>

namespace PvpHolyPaladinGlue
{
namespace
{
using namespace PvpHolyPaladin;

constexpr char const* LOG_CATEGORY = "playerbots";

bool Active(Player* bot)
{
    return PvpConfig::TeamPlayEnabled() && bot && bot->IsAlive() && bot->getClass() == CLASS_PALADIN;
}

bool Ready(Player* bot, uint32 spellId)
{
    return bot->HasSpell(spellId) && !bot->HasSpellCooldown(spellId);
}

Unit* CurrentTarget(PlayerbotAI* botAI)
{
    return botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
}

// The melee players that are hitting this ally.
std::int32_t MeleeAttackers(Player* bot, Unit* ally)
{
    std::int32_t count = 0;
    for (Unit* attacker : ally->getAttackers())
    {
        Player* enemy = attacker ? attacker->ToPlayer() : nullptr;
        if (enemy && enemy->IsAlive() && bot->IsValidAttackTarget(enemy) && PlayerBotSpec::IsMelee(enemy))
            ++count;
    }
    return count;
}

struct Choice
{
    Player* ally = nullptr;
    std::int32_t score = 0;
};

Choice BestSacrifice(PlayerbotAI* botAI, Player* bot, std::vector<Player*> const& allies)
{
    Choice best;
    if (!Ready(bot, SPELL_HAND_OF_SACRIFICE))
        return best;
    std::vector<PvpAllyFacts::ControlCast> const casts = PvpAllyFacts::IncomingControl(botAI, bot, allies, HEAL_RANGE_YD);
    for (Player* ally : allies)
    {
        bool burst = false;
        SacrificeInputs in;
        in.ready = true;
        in.allyIsSelf = ally == bot;
        in.allyHasSacrifice = ally->HasAura(SPELL_HAND_OF_SACRIFICE);
        in.playerAttackers = PvpAllyFacts::PlayerAttackers(bot, ally, burst);
        in.allyHealthPercent = ally->GetHealthPct();
        in.paladinHealthPercent = bot->GetHealthPct();
        for (PvpAllyFacts::ControlCast const& cast : casts)
            if (cast.aimed == ally)
                in.controlLeftMs = std::max(in.controlLeftMs, std::uint32_t(cast.leftMs));
        SacrificeVerdict const verdict = HandOfSacrifice(in, BotSkill::TierOf(bot));
        if (verdict.cast && (!best.ally || verdict.score > best.score))
            best = { ally, verdict.score };
    }
    return best;
}

Choice BestProtection(Player* bot, std::vector<Player*> const& allies)
{
    Choice best;
    if (!Ready(bot, SPELL_HAND_OF_PROTECTION))
        return best;
    for (Player* ally : allies)
    {
        if (ally == bot)
            continue;
        bool burst = false;
        PvpAllyFacts::PlayerAttackers(bot, ally, burst);
        ProtectionInputs in;
        in.ready = true;
        in.allyHasProtection = ally->HasAura(SPELL_HAND_OF_PROTECTION);
        in.allyIsMeleeDamage = PlayerBotSpec::IsMelee(ally) && !PlayerBotSpec::IsHeal(ally);
        in.allyStunned = ally->HasUnitState(UNIT_STATE_STUNNED);
        in.meleeAttackers = MeleeAttackers(bot, ally);
        in.enemyBurstSeen = burst;
        in.allyHealthPercent = ally->GetHealthPct();
        ProtectionVerdict const verdict = HandOfProtection(in, BotSkill::TierOf(bot));
        if (verdict.cast && (!best.ally || verdict.score > best.score))
            best = { ally, verdict.score };
    }
    return best;
}

// The most pressed ally that is not the paladin (the beacon cannot go on its caster), by players on him and then by health.
Player* FocusedAlly(Player* bot, std::vector<Player*> const& allies, std::int32_t& attackers)
{
    Player* best = nullptr;
    attackers = 0;
    for (Player* ally : allies)
    {
        if (ally == bot)
            continue;
        bool burst = false;
        std::int32_t const count = PvpAllyFacts::PlayerAttackers(bot, ally, burst);
        if (count <= 0)
            continue;
        if (!best || count > attackers || (count == attackers && ally->GetHealthPct() < best->GetHealthPct()))
        {
            best = ally;
            attackers = count;
        }
    }
    return best;
}

BeaconReason BeaconDecision(Player* bot, std::vector<Player*> const& allies, Player*& focus)
{
    BeaconInputs in;
    in.known = bot->HasSpell(SPELL_BEACON_OF_LIGHT);
    focus = FocusedAlly(bot, allies, in.candidateAttackers);
    if (!focus)
        return Beacon(in, BotSkill::TierOf(bot));
    in.candidateHealthPercent = focus->GetHealthPct();
    for (Player* ally : allies)
        if (ally != bot && ally->HasAura(SPELL_BEACON_OF_LIGHT, bot->GetGUID()))
        {
            in.beaconUp = true;
            bool burst = false;
            in.beaconHolderAttackers = PvpAllyFacts::PlayerAttackers(bot, ally, burst);
            in.candidateIsHolder = ally == focus;
        }
    return Beacon(in, BotSkill::TierOf(bot));
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
    Tier const tier = BotSkill::TierOf(bot);

    switch (rule)
    {
        case Rule::DENOUNCE:
        {
            Unit* target = CurrentTarget(botAI);
            DenounceInputs in;
            in.known = bot->HasSpell(SPELL_DENOUNCE);
            in.targetIsPlayer = PvpBurstGlue::TargetIsEnemyPlayer(botAI, bot, target);
            if (!in.targetIsPlayer)
                return false;
            in.targetDistance = bot->GetExactDist(target);
            in.moving = bot->isMoving();
            in.lowestAllyPercent = PvpAllyFacts::LowestPercent(allies);
            in.targetBreakableCc = PvpUnitFacts::HasBreakableCc(target);
            in.enemyKickNotSpent = PvpTeamGlue::EnemyStopperNotSpent(botAI, bot);
            return Denounce(in, tier) == DenounceReason::DOWNTIME;
        }
        case Rule::SACRIFICE:
            return BestSacrifice(botAI, bot, allies).ally != nullptr;
        case Rule::PROTECTION:
            return BestProtection(bot, allies).ally != nullptr;
        case Rule::AURA:
        {
            AuraInputs in;
            in.ready = Ready(bot, SPELL_DEVOTION_AURA);
            in.silenced = PvpAllyFacts::Silenced(bot);
            in.lowestAllyPercent = PvpAllyFacts::LowestPercent(allies);
            return DevotionAuraNow(in, tier) == AuraReason::PRESS;
        }
        case Rule::BEACON:
        {
            Player* focus = nullptr;
            return BeaconDue(BeaconDecision(bot, allies, focus)) && focus != nullptr;
        }
    }
    return false;
}

bool Cast(PlayerbotAI* botAI, Player* bot, Rule rule)
{
    if (!Due(botAI, bot, rule))
        return false;
    std::vector<Player*> const& allies = PvpAllyFacts::ReachableCached(botAI, bot, HEAL_RANGE_YD);
    char const* what = "";
    Unit* aimed = nullptr;
    uint32 spell = 0;
    switch (rule)
    {
        case Rule::DENOUNCE:
            what = "DENOUNCE";
            spell = SPELL_DENOUNCE;
            aimed = CurrentTarget(botAI);
            break;
        case Rule::SACRIFICE:
            what = "HAND_OF_SACRIFICE";
            spell = SPELL_HAND_OF_SACRIFICE;
            aimed = BestSacrifice(botAI, bot, allies).ally;
            break;
        case Rule::PROTECTION:
            what = "HAND_OF_PROTECTION";
            spell = SPELL_HAND_OF_PROTECTION;
            aimed = BestProtection(bot, allies).ally;
            break;
        case Rule::AURA:
            what = "DEVOTION_AURA";
            spell = SPELL_DEVOTION_AURA;
            aimed = bot;
            break;
        case Rule::BEACON:
        {
            what = "BEACON_OF_LIGHT";
            spell = SPELL_BEACON_OF_LIGHT;
            Player* focus = nullptr;
            BeaconDecision(bot, allies, focus);
            aimed = focus;
            break;
        }
    }
    if (!spell || !aimed || !botAI->CastSpell(spell, aimed))
        return false;
    SF_LOG_DEBUG(LOG_CATEGORY, "[PVPPALADIN] bot=%u tier=%u kind=%s target=%u", bot->GetGUIDLow(), uint32(BotSkill::TierOf(bot)), what,
        aimed->GetGUIDLow());
    return true;
}
}  // namespace PvpHolyPaladinGlue
