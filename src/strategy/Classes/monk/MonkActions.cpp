/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 *
 * #194 (S8): the one Monk action that needs a hand-written isUseful() (CastUpliftOnPartyAction), so it
 * can't be a one-line macro in MonkActions.h like every other Monk action. Everything else stays in
 * the header.
 */

#include "MonkActions.h"

#include "BotSkill.h"
#include "DpsRotationMath.h"
#include "Group.h"
#include "HealerGuardrails.h"
#include "HealerTriggers.h"
#include "Playerbots.h"
#include "Spell.h"
#include "SpellInfo.h"

namespace
{
// Class sheet, mistweaver monk (BT-135, docs/class-sheets/monk-mistweaver.md). Spell ids and costs are from the 5.4.8 data.
constexpr uint32 SPELL_SURGING_MIST = 116694;  // level 32
constexpr uint32 SPELL_SOOTHING_MIST = 115175;
constexpr uint8 UPLIFT_CHI_COST = 2;
constexpr uint8 THUNDER_FOCUS_TEA_CHI_COST = 1;
constexpr uint8 ENVELOPING_MIST_CHI_COST = 3;
// Uplift heals only the allies that carry the bot's Renewing Mist, so a lone mist is a waste of 2 Chi.
constexpr uint32 UPLIFT_MIN_RENEWING_MISTS = 2;
// Spinning Crane Kick (spell 107270) hits and, with Teachings of the Monastery, heals everything within 8 yards; the heal
// effect (117640) reaches 10. At least two injured allies (the monk counts) must stand in the smaller radius.
constexpr float SPINNING_CRANE_KICK_RADIUS = 8.0f;
constexpr uint32 SPINNING_CRANE_KICK_MIN_INJURED = 2;

// The lowest member of the group is below the low health threshold: for a Mistweaver a heal comes before any dispel.
bool DetoxYieldsToHeals(Player* bot, AiObjectContext* context)
{
    return BotGetSpecialization(bot) == Specializations::SPEC_MONK_MISTWEAVER &&
           AI_VALUE2(uint8, "health", "party member to heal") < sPlayerbotAIConfig->lowHealth;
}
}  // namespace

uint32 CountOwnRenewingMists(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Group* group = bot->GetGroup();
    if (!group)
        return botAI->GetAura("renewing mist", bot, true) ? 1 : 0;

    uint32 count = 0;
    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* member = gref->GetSource();
        if (member && member->IsAlive() && member->IsInWorld() && member->GetMapId() == bot->GetMapId() &&
            botAI->GetAura("renewing mist", member, true))
            ++count;
    }

    return count;
}

bool DetoxCurePoisonTrigger::IsActive() { return !DetoxYieldsToHeals(bot, context) && NeedCureTrigger::IsActive(); }

bool DetoxCureDiseaseTrigger::IsActive() { return !DetoxYieldsToHeals(bot, context) && NeedCureTrigger::IsActive(); }

bool DetoxCureMagicTrigger::IsActive() { return !DetoxYieldsToHeals(bot, context) && NeedCureTrigger::IsActive(); }

bool DetoxPartyMemberCurePoisonTrigger::IsActive()
{
    return !DetoxYieldsToHeals(bot, context) && PartyMemberNeedCureTrigger::IsActive();
}

bool DetoxPartyMemberCureDiseaseTrigger::IsActive()
{
    return !DetoxYieldsToHeals(bot, context) && PartyMemberNeedCureTrigger::IsActive();
}

bool DetoxPartyMemberCureMagicTrigger::IsActive()
{
    return !DetoxYieldsToHeals(bot, context) && PartyMemberNeedCureTrigger::IsActive();
}

bool CastUpliftOnPartyAction::isUseful()
{
    // Class sheet, mistweaver monk (BT-135): also needs two of the bot's Renewing Mists out (it only heals those).
    return HealPartyMemberAction::isUseful() && AI_VALUE2(uint8, "chi", "self target") >= UPLIFT_CHI_COST &&
           CountOwnRenewingMists(botAI) >= UPLIFT_MIN_RENEWING_MISTS;
}

bool CastUpliftAction::isUseful()
{
    return CastHealingSpellAction::isUseful() && AI_VALUE2(uint8, "chi", "self target") >= UPLIFT_CHI_COST &&
           CountOwnRenewingMists(botAI) >= UPLIFT_MIN_RENEWING_MISTS;
}

// Thunder Focus Tea costs 1 Chi and its Uplift 2, so the three are needed together.
bool CastThunderFocusTeaAction::isUseful()
{
    return CastBuffSpellAction::isUseful() &&
           AI_VALUE2(uint8, "chi", "self target") >= THUNDER_FOCUS_TEA_CHI_COST + UPLIFT_CHI_COST &&
           CountOwnRenewingMists(botAI) >= UPLIFT_MIN_RENEWING_MISTS;
}

// Soothing Mist is the heal of levels 16 to 31 only: from Surging Mist (32) the direct heals are Surging, Renewing and
// Enveloping Mist. A new channel does not start while one runs (every cast is refused during it, PlayerbotAI::CanCastSpell).
bool CastSoothingMistOnPartyAction::isUseful()
{
    if (bot->HasSpell(SPELL_SURGING_MIST) || bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
        return false;

    return HealPartyMemberAction::isUseful();
}

// The channel ends when its target is at or above the almost full health threshold or gone, or when another member is
// below the critical threshold (the shared "cancel channel" action then frees the bot for a faster heal).
bool SoothingMistChannelCheckTrigger::IsActive()
{
    Spell* channel = bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
    if (!channel || channel->m_spellInfo->Id != SPELL_SOOTHING_MIST)
        return false;

    Unit* target = channel->m_targets.GetUnitTarget();
    if (!target || !target->IsAlive() || target->GetHealthPct() >= float(sPlayerbotAIConfig->almostFullHealth))
        return true;

    Unit* lowest = AI_VALUE(Unit*, "party member to heal");
    return lowest && lowest != target && lowest->GetHealthPct() < float(sPlayerbotAIConfig->criticalHealth);
}

// Upkeep on the main tank follows the same mana rule as the priests' (PriestTriggers.cpp): it never drains a healer that is
// saving mana. Renewing Mist has an 8 second cooldown, so the cooldown is read before the target is.
bool RenewingMistOnMainTankTrigger::IsActive()
{
    uint32 const spellId = AI_VALUE2(uint32, "spell id", "renewing mist");
    if (!spellId || !bot->HasSpell(spellId) || bot->HasSpellCooldown(spellId))
        return false;

    // BT-157: the tank guardrail (more than 30 percent mana); Renewing Mist is the monk's first pre-pull spell (core).
    if (!HealerUpkeep::TankUpkeepAllowed(botAI, BotSkillMath::Tier::CASUAL))
        return false;

    return BuffOnMainTankTrigger::IsActive();
}

// "good" tier: Enveloping Mist (3 Chi, no mana, 2 second cast) on a main tank below the medium health threshold that does
// not carry the bot's own Enveloping Mist yet (the trigger base reads the aura).
bool EnvelopingMistOnMainTankTrigger::IsActive()
{
    if (!BotSkill::Uses(bot, BotSkillMath::Tier::GOOD))
        return false;

    uint32 const spellId = AI_VALUE2(uint32, "spell id", "enveloping mist");
    if (!spellId || !bot->HasSpell(spellId) || AI_VALUE2(uint8, "chi", "self target") < ENVELOPING_MIST_CHI_COST)
        return false;

    // BT-157: before the pull the tank is at full health, so the health rule is for the fight only; the Chi rule above
    // stays. Enveloping Mist is the monk's second pre-pull spell and an expert line, as a pull is the one time 3 Chi are
    // spare.
    Unit* tank = GetTarget();
    if (!tank || !tank->IsAlive())
        return false;

    bool const inFight = bot->IsInCombat() || tank->IsInCombat();
    if (inFight && tank->GetHealthPct() >= float(sPlayerbotAIConfig->mediumHealth))
        return false;

    if (!inFight && !HealerUpkeep::TankUpkeepAllowed(botAI, BotSkillMath::Tier::EXPERT))
        return false;

    return BuffOnMainTankTrigger::IsActive();
}

// The main tank below the critical health threshold, with no Life Cocoon on him (any monk's, the base trigger reads the aura).
bool LifeCocoonOnMainTankTrigger::IsActive()
{
    uint32 const spellId = AI_VALUE2(uint32, "spell id", "life cocoon");
    if (!spellId || !bot->HasSpell(spellId) || bot->HasSpellCooldown(spellId))
        return false;

    Unit* tank = GetTarget();
    if (!tank || !tank->IsAlive() || tank->GetHealthPct() >= float(sPlayerbotAIConfig->criticalHealth))
        return false;

    return BuffOnMainTankTrigger::IsActive();
}

bool CastSpinningCraneKickHealAction::isUseful()
{
    if (!BotSkill::Uses(bot, BotSkillMath::Tier::GOOD) || !CastSpellAction::isUseful())
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    uint32 injured = 0;
    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* member = gref->GetSource();
        if (member && member->IsAlive() && member->IsInWorld() && member->GetMapId() == bot->GetMapId() &&
            member->GetHealthPct() < float(sPlayerbotAIConfig->almostFullHealth) &&
            bot->IsWithinDistInMap(member, SPINNING_CRANE_KICK_RADIUS))
            ++injured;
    }

    return injured >= SPINNING_CRANE_KICK_MIN_INJURED;
}

// Windwalker sheet: save Chi for Rising Sun Kick and Fists of Fury. Other specs retain their own rules.
// Windwalker sheet: Kings and Mark provide the same exclusive stat bonus.
bool LegacyOfTheEmperorTrigger::IsActive()
{
    return !botAI->HasAnyAuraOf(bot, "blessing of kings", "mark of the wild", nullptr) && BuffTrigger::IsActive();
}

Value<Unit*>* LegacyOfTheEmperorOnPartyTrigger::GetTargetValue()
{
    return context->GetValue<Unit*>("party member without aura", "legacy of the emperor,blessing of kings,mark of the wild");
}

Value<Unit*>* CastLegacyOfTheEmperorOnPartyAction::GetTargetValue()
{
    return context->GetValue<Unit*>("party member without aura", "legacy of the emperor,blessing of kings,mark of the wild");
}

// Windwalker sheet gap item 9: no Legacy of the White Tiger on a member who has another critical strike buff.
bool LegacyOfTheWhiteTigerTrigger::IsActive()
{
    return !botAI->HasAnyAuraOf(bot, "leader of the pack", "arcane brilliance", "dalaran brilliance", "furious howl",
               "terrifying roar", "fearless roar", "still water", nullptr) &&
           BuffTrigger::IsActive();
}

Value<Unit*>* LegacyOfTheWhiteTigerOnPartyTrigger::GetTargetValue()
{
    return context->GetValue<Unit*>("party member without aura", DpsRotationMath::Monk::CRITICAL_STRIKE_BUFFS);
}

Value<Unit*>* CastLegacyOfTheWhiteTigerOnPartyAction::GetTargetValue()
{
    return context->GetValue<Unit*>("party member without aura", DpsRotationMath::Monk::CRITICAL_STRIKE_BUFFS);
}

Unit* CastTigersLustAction::GetTarget()
{
    if (bot->HasUnitState(UNIT_STATE_ROOT) || bot->HasAuraType(SPELL_AURA_MOD_DECREASE_SPEED))
        return bot;
    Unit* ally = CastSpellAction::GetTarget();
    if (ally)
        return ally;
    Unit* enemy = AI_VALUE(Unit*, "current target");
    return bot->duel && enemy && bot->GetDistance(enemy) > 11.0f ? bot : nullptr;
}

Value<Unit*>* CastTigersLustAction::GetTargetValue()
{
    return context->GetValue<Unit*>("party member snared target");
}

bool CastTigersLustAction::isUseful()
{
    return bot->HasSpell(116841) && GetTarget() && CastSpellAction::isUseful();
}

bool CastDisablePvpAction::isUseful()
{
    return BotSkill::Uses(bot, BotSkillMath::Tier::EXPERT) && bot->HasSpell(116095) &&
           (bot->duel || AI_VALUE(bool, "in pvp instance")) && CastSnareSpellAction::isUseful();
}

bool CastFlyingSerpentKickPvpAction::isUseful()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    return BotSkill::Uses(bot, BotSkillMath::Tier::EXPERT) && bot->HasSpell(101545) &&
           (bot->duel || AI_VALUE(bool, "in pvp instance")) && target && !bot->IsMounted() &&
           bot->GetDistance(target) >= 11.0f && bot->GetDistance(target) <= 30.0f && CastSpellAction::isUseful();
}

bool CastNimbleBrewPvpAction::isUseful()
{
    return BotSkill::Uses(bot, BotSkillMath::Tier::EXPERT) && bot->HasSpell(137562) &&
           (bot->duel || AI_VALUE(bool, "in pvp instance")) &&
           (bot->HasAuraType(SPELL_AURA_MOD_STUN) || bot->HasAuraType(SPELL_AURA_MOD_ROOT) ||
            bot->HasUnitState(UNIT_STATE_ROOT)) && CastSpellAction::isUseful();
}

bool MistweaverMeleeAction::isUseful()
{
    Unit* target = GetTarget();
    return MeleeAction::isUseful() &&
        HealerGuardrails::MeleeFallbackWorthTrying(target != nullptr,
                                                   target && bot->GetVictim() == target &&
                                                       bot->HasUnitState(UNIT_STATE_MELEE_ATTACKING),
                                                   bot->GetGroup() != nullptr,
                                                   target && bot->IsWithinMeleeRange(target));
}

bool CastJabAction::isUseful()
{
    return (BotGetSpecialization(bot) != Specializations::SPEC_MONK_WINDWALKER ||
            AI_VALUE2(uint8, "chi", "self target") <= 2) && CastMeleeSpellAction::isUseful();
}

bool CastBlackoutKickAction::isUseful()
{
    if (BotGetSpecialization(bot) == Specializations::SPEC_MONK_WINDWALKER)
    {
        uint8 chi = AI_VALUE2(uint8, "chi", "self target");
        if (!bot->HasAura(116768) && chi < bot->GetMaxPower(POWER_CHI) &&
            !(bot->getLevel() < 56 && bot->GetSpellCooldownDelay(113656) > 6))
            return false;
    }
    return CastMeleeSpellAction::isUseful();
}

bool CastRushingJadeWindAction::isUseful()
{
    return (BotGetSpecialization(bot) != Specializations::SPEC_MONK_WINDWALKER ||
            (bot->HasSpell(116847) && AI_VALUE2(uint8, "chi", "self target") <= 2)) &&
           CastSpellAction::isUseful();
}

bool CastSpinningCraneKickAction::isUseful()
{
    return (BotGetSpecialization(bot) != Specializations::SPEC_MONK_WINDWALKER ||
            (bot->HasSpell(101546) && AI_VALUE2(uint8, "chi", "self target") <= 2)) &&
           CastMeleeSpellAction::isUseful();
}

bool TouchOfKarmaTrigger::IsActive()
{
    uint8 attackers = AI_VALUE(uint8, "my attacker count");
    return bot->HasSpell(122470) && (attackers >= 3 || (attackers && bot->GetHealthPct() < 60.0f)) &&
           SpellCanBeCastTrigger::IsActive();
}

bool InvokeXuenTrigger::IsActive()
{
    if (!bot->HasSpell(123904))
        return false;
    if (BotGetSpecialization(bot) == Specializations::SPEC_MONK_WINDWALKER)
    {
        Unit* target = GetTarget();
        Creature* creature = target ? target->ToCreature() : nullptr;
        if ((!creature || (!creature->isElite() && !creature->isWorldBoss())) &&
            AI_VALUE(uint8, "attacker count") < 3)
            return false;
    }
    return SpellCanBeCastTrigger::IsActive();
}

bool CastTigerPalmAction::isUseful()
{
    if (!CastMeleeSpellAction::isUseful())
        return false;

    // #526 (wave 11): Tiger Power 125359 does not stack in 5.4.8 (SpellAuraOptions.dbc StackAmount=1; Wowhead mop-classic
    // tooltip 100787, 2026-10-05: "ignore 30% of enemies' armor for 20 sec"). The old "stacks < 3" test was always true, so
    // Tiger Palm (1 Chi, +0.2) fired at every Chi and starved Blackout Kick / Rising Sun Kick (2 Chi). Refresh upkeep only.
    Aura* power = botAI->GetAura("tiger power", bot);
    return !power || power->GetDuration() < 4000 ||
           (BotGetSpecialization(bot) == Specializations::SPEC_MONK_WINDWALKER && bot->HasAura(118864));
}

bool CastPurifyingBrewAction::isUseful()
{
    // Wave-75 fix J (#862), MoP rotation row 26817 (brewmaster, prio group 5/1): Purifying Brew also at health <= 40%
    // (self, in combat) whatever the Stagger level, next to the Moderate (26810) / Heavy (26811) Stagger rows.
    return CastBuffSpellAction::isUseful() &&
           (bot->HasAura(124274) || bot->HasAura(124273) ||  // Moderate / Heavy Stagger
            bot->GetHealthPct() <= 40.0f);
}

bool CastShuffleAction::isUseful()
{
    if (!CastMeleeSpellAction::isUseful())
        return false;

    Aura* shuffle = botAI->GetAura("shuffle", bot);
    return !shuffle || shuffle->GetDuration() < 3000;
}

// Class sheet, brewmaster monk (BT-133): "good" tier line, behind Keg Smash in the default list.
bool CastTankChiWaveAction::isUseful()
{
    return BotSkill::Uses(bot, BotSkillMath::Tier::GOOD) && CastMeleeSpellAction::isUseful();
}

// Class sheet, brewmaster monk (BT-133): the channel blocks other casts, so it starts only with Shuffle running and Chi
// below TANK_CRANE_KICK_CHI_BELOW (Shuffle and Guard are never starved of the Chi they need, and the channel then
// generates Chi from three targets).
bool CastTankSpinningCraneKickAction::isUseful()
{
    if (!CastMeleeSpellAction::isUseful())
        return false;

    return botAI->GetAura("shuffle", bot) && AI_VALUE2(uint8, "chi", "self target") < TANK_CRANE_KICK_CHI_BELOW;
}

// Class sheet, brewmaster monk (BT-133): see BrewmasterExpelHarmTrigger in MonkActions.h. The Energy test keeps the edge
// silent while the cast would fail for power.
bool BrewmasterExpelHarmTrigger::IsActive()
{
    if (bot->GetHealthPct() >= float(BREWMASTER_EXPEL_HARM_HEALTH_PCT) ||
        AI_VALUE2(uint8, "energy", "self target") < EXPEL_HARM_ENERGY)
        return false;

    return SpellCanBeCastTrigger::IsActive();
}

// #526 (AC pattern: BoostTrigger::IsActive, GenericTriggers.cpp:364-372 on this fork): the same
// "player target or balance <= 50" gate, with SpellCanBeCastTrigger replacing BuffTrigger's no-aura
// test (see MonkActions.h for why). Wave 10: CanCastSpell does not test the cast's CASTER_AURASTATE
// (result 24 x63), so the trigger itself requires a Tigereye Brew stack (aura 125195, Wowhead tooltip
// 2026-10-05) and no running 116740 buff.
bool TigereyeBrewTrigger::IsActive()
{
    Aura* stacks = bot->GetAura(125195);
    if (!bot->HasSpell(116740) || !stacks || stacks->GetStackAmount() < 10 || bot->HasAura(116740))
        return false;

    if (!SpellCanBeCastTrigger::IsActive())
        return false;

    if (stacks->GetStackAmount() >= 15)
        return true;
    Unit* target = AI_VALUE(Unit*, "current target");
    if (target && target->ToPlayer())
        return true;

    return AI_VALUE(uint8, "balance") <= 50.f;
}

// #526: Touch of Death 115080 tooltip (Wowhead, 2026-10-04): only usable on creatures with equal or less
// health than the monk, or on players at or below 10% health.
bool TouchOfDeathTrigger::IsActive()
{
    Unit* target = GetTarget();
    if (!target || !SpellCanBeCastTrigger::IsActive())
        return false;

    // Wave 10: the trigger stayed active every tick during the 90 s cooldown (cast result "cooldown" x30, action
    // PREREQ/FAILED); gate on the cooldown and on the 3 Chi it costs (tooltip 115080, 2026-10-05) as AC rogue
    // finishers gate on combo points.
    uint32 spellId = AI_VALUE2(uint32, "spell id", "touch of death");
    if (!spellId || bot->HasSpellCooldown(spellId) || AI_VALUE2(uint8, "chi", "self target") < 3)
        return false;

    if (target->GetTypeId() == TypeID::TYPEID_PLAYER)
        return target->GetHealth() * 10 <= target->GetMaxHealth();

    Creature* creature = target->ToCreature();
    return target->GetHealth() <= bot->GetHealth() &&
           ((creature && creature->isElite()) || uint64(target->GetHealth()) * 2 >= bot->GetHealth());
}

// #862 MoP rotation row 27013: Mana Tea needs >= 2 stacks of the stack aura 115867.
bool CastManaTeaAction::isUseful()
{
    if (!CastSpellAction::isUseful())
        return false;

    Aura* stacks = bot->GetAura(115867);
    return stacks && stacks->GetStackAmount() >= 2;
}

// #862 MoP rotation row 26809 / 26916: >= 7 stacks of 128939, buff 115308 not already running.
bool ElusiveBrewTrigger::IsActive()
{
    Aura* stacks = bot->GetAura(128939);
    if (!stacks || stacks->GetStackAmount() < 7 || bot->HasAura(115308))
        return false;

    return SpellCanBeCastTrigger::IsActive();
}

// #862 MoP rotation row 1001 (chi <= 2, all specs) / 26915 (Windwalker: energy <= 30%).
bool ChiBrewTrigger::IsActive()
{
    if (!bot->HasSpell(115399) || AI_VALUE2(uint8, "chi", "self target") > 2)
        return false;

    return SpellCanBeCastTrigger::IsActive();
}

// rot-fix-F: Energizing Brew (MoP rotation row 26914) - Windwalker, Energy <= 75%.
bool EnergizingBrewTrigger::IsActive()
{
    if (BotGetSpecialization(bot) != Specializations::SPEC_MONK_WINDWALKER)
        return false;
    if (!bot->HasSpell(115288) || AI_VALUE2(uint8, "energy", "self target") > 40)
        return false;

    return SpellCanBeCastTrigger::IsActive();
}

// #862 MoP rotation row 26812 / 27006: cast the statue only when the bot has none standing nearby.
bool MonkStatueTrigger::IsActive()
{
    if (!SpellCanBeCastTrigger::IsActive())
        return false;

    // Every statue in range, not just the nearest: another monk's statue can stand closer than the bot's own.
    std::list<Creature*> statues;
    bot->GetCreatureListWithEntryInGrid(statues, statueEntry, STATUE_SEARCH_RANGE);
    for (Creature* statue : statues)
        if (statue->IsAlive() && statue->GetOwnerGUID() == bot->GetGUID())
            return false;

    return true;
}

bool CastSpearHandStrikeOnEnemyCasterAction::isUseful()
{
    Unit* target = GetTarget();
    return target && CastSpellOnEnemyCasterAction::isUseful() && bot->IsWithinMeleeRange(target);
}

// #862 MoP rotation row 1010: not mounted, target 11+ yd away (Roll covers ~20 yd), PvP instances only.
bool CastRollGapAction::isUseful()
{
    if (!AI_VALUE(bool, "in pvp instance") || bot->IsMounted())
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target)
        return false;

    float dist = bot->GetDistance(target);
    return dist >= 11.0f && dist <= 30.0f && CastSpellAction::isUseful();
}

// BT-657 to BT-662: see MonkActions.h. The decisions are in Utils/PvpMistweaverPolicy.h, the facts in PvP/PvpMistweaver.cpp.
bool PvpMistweaverTrigger::IsActive()
{
    return PvpMistweaverGlue::Due(botAI, bot, rule);
}

bool PvpMistweaverAction::isUseful()
{
    return PvpMistweaverGlue::Due(botAI, bot, rule);
}

bool PvpMistweaverAction::Execute(Event /*event*/)
{
    return PvpMistweaverGlue::Cast(botAI, bot, rule);
}
