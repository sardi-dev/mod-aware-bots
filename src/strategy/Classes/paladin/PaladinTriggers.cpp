/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#include "PaladinTriggers.h"

#include "PaladinActions.h"
#include "PlayerbotAIConfig.h"
#include "PlayerbotSpec.h"
#include "Playerbots.h"
#include "AiFactory.h"
#include "HealerTriggers.h"
#include "BotSkill.h"
#include "BuffRecastPolicy.h"

namespace
{
// Class sheet, holy paladin (BT-132): same rule and threshold as the priest's dispels (the lowest member of the group is
// below the low health threshold: a heal comes before any dispel).
bool MemberNeedsHealBeforeDispel(AiObjectContext* context)
{
    return AI_VALUE2(uint8, "health", "party member to heal") < sPlayerbotAIConfig->lowHealth;
}

constexpr uint32 SPELL_BLESSING_OF_MIGHT = 19740;
}  // namespace

// A seal is active when none of the 5.4.8 seals is up on the target. For a non-tank, Seal of Insight keeps the seal "up"
// only while mana is at 70% or below, so a Holy paladin with mana to spare gets the trigger again.
// Class sheet, protection paladin (BT-127): a tank keeps Seal of Insight whatever its mana (its melee hits heal), so for
// a tank Insight always counts as up and the trigger never replaces it with Seal of Truth.
bool SealTrigger::IsActive()
{
    Unit* target = GetTarget();
    bool const insightKeepsSealUp = botAI->HasAura("seal of insight", target) &&
                                    (PlayerBotSpec::IsTank(bot) || AI_VALUE2(uint8, "mana", "self target") <= 70);
    // Retribution sheet: return to Truth once fewer than three enemies remain.
    if (AiFactory::GetPlayerSpecTab(bot) == SPEC_PALADIN_RETRIBUTION && botAI->HasAura("seal of righteousness", target) &&
        !context->GetTrigger("medium aoe")->IsActive())
        return true;
    return !botAI->HasAura("seal of justice", target) && !botAI->HasAura("seal of command", target) &&
           !botAI->HasAura("seal of righteousness", target) && !botAI->HasAura("seal of truth", target) &&
           !insightKeepsSealUp;
}

// Retribution sheet: reserve damage Holy Power when a living healer is present.
bool RetWordOfGloryTrigger::IsActive()
{
    if (!bot->GetPower(POWER_HOLY_POWER) || AI_VALUE2(uint8, "health", "party member to heal") >= sPlayerbotAIConfig->lowHealth)
        return false;
    if (AiFactory::GetPlayerSpecTab(bot) != SPEC_PALADIN_RETRIBUTION ||
        AI_VALUE(Unit*, "party member to heal") == bot ||
        AI_VALUE2(uint8, "health", "party member to heal") < sPlayerbotAIConfig->criticalHealth)
        return true;
    Group* group = bot->GetGroup();
    if (group)
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (member && member != bot && member->IsAlive() && member->IsInWorld() &&
                member->FindMap() == bot->FindMap() && PlayerBotSpec::IsHeal(member))
                return false;
        }
    return true;
}

bool RetHurtDevotionTrigger::IsActive()
{
    return BotSkill::Uses(bot, BotSkillMath::Tier::GOOD) && !bot->HasSpellCooldown(31821) &&
           PaladinHurtGroupTrigger::IsActive();
}

bool RetExpertApproachTrigger::IsActive()
{
    return BotSkill::Uses(bot, BotSkillMath::Tier::EXPERT) && bot->HasSpell(20271) && EnemyOutOfMeleeTrigger::IsActive();
}

bool RetBossDevotionTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    Creature* creature = target ? target->ToCreature() : nullptr;
    return BotSkill::Uses(bot, BotSkillMath::Tier::GOOD) && !bot->HasSpellCooldown(31821) && creature && (creature->IsDungeonBoss() || creature->isWorldBoss());
}

bool RetSealTrigger::IsActive()
{
    return BotSkill::Uses(bot, BotSkillMath::Tier::EXPERT) && bot->HasSpell(20154) && !botAI->HasAura("seal of righteousness", bot) && HighAoeTrigger::IsActive();
}

// BT-176: Ernac, the protection paladin of the Deadmines run of 2026-11-13, cast Blessing of Might on someone every 4.3 s
// for the whole run (212 casts). The cause is not proven (class fixes wait for the owner's test): this keeps the
// symptom down and names the member in the log, so the next run can show which one never keeps the buff.
bool BlessingOnPartyTrigger::IsActive()
{
    if (!BuffOnPartyTrigger::IsActive())
        return false;

    Unit* target = GetTarget();
    if (!target)
        return false;

    time_t const now = time(nullptr);
    bool const same = lastTarget == target->GetGUID();
    if (!BuffRecastPolicy::RecastAllowed(same, now - lastAt))
    {
        if (!loggedAt || now - loggedAt >= BuffRecastPolicy::BLESSING_RECAST_SECONDS)
        {
            loggedAt = now;
            SF_LOG_INFO("playerbots", "[BUFF] %s holds back %s on %s (class %u, pet=%u, summon=%u): cast %u s ago, the aura is not found",
                        bot->GetName().c_str(), spell.c_str(), target->GetName().c_str(), uint32(target->getClass()),
                        uint32(target->IsPet() || target->IsHunterPet()), uint32(target->IsSummon()), unsigned(now - lastAt));
        }
        return false;
    }
    lastTarget = target->GetGUID();
    lastAt = now;
    return true;
}

bool BlessingOfKingsOnPartyTrigger::IsActive()
{
    return bot->HasSpell(20217) && BlessingOnPartyTrigger::IsActive();
}

bool BlessingOfMightSecondTrigger::IsActive()
{
    if (!bot->HasSpell(SPELL_BLESSING_OF_MIGHT) || !BlessingOfMightOnPartyTrigger::IsActive())
        return false;

    // The target is the member without Might (the one the action casts on): it needs Kings from someone else, not from
    // this bot, otherwise the bot would overwrite its own Kings.
    Unit* target = GetTarget();
    return target && botAI->GetAura("blessing of kings", target) &&
           !botAI->GetAura("blessing of kings", target, true);
}

bool CleanseCureDiseaseTrigger::IsActive()
{
    return !MemberNeedsHealBeforeDispel(context) && NeedCureTrigger::IsActive();
}

bool CleanseCurePoisonTrigger::IsActive()
{
    return !MemberNeedsHealBeforeDispel(context) && NeedCureTrigger::IsActive();
}

bool CleanseCureMagicTrigger::IsActive()
{
    return !MemberNeedsHealBeforeDispel(context) && NeedCureTrigger::IsActive();
}

bool CleansePartyMemberCureDiseaseTrigger::IsActive()
{
    return !MemberNeedsHealBeforeDispel(context) && PartyMemberNeedCureTrigger::IsActive();
}

bool CleansePartyMemberCurePoisonTrigger::IsActive()
{
    return !MemberNeedsHealBeforeDispel(context) && PartyMemberNeedCureTrigger::IsActive();
}

bool CleansePartyMemberCureMagicTrigger::IsActive()
{
    return !MemberNeedsHealBeforeDispel(context) && PartyMemberNeedCureTrigger::IsActive();
}

bool BlessingTrigger::IsActive()
{
	Unit* target = GetTarget();
	return SpellTrigger::IsActive() && !botAI->HasAnyAuraOf(target, "blessing of kings", "blessing of might", nullptr);
}

bool HandOfFreedomOnPartyTrigger::IsActive()
{
	Unit* target = GetTarget();
	if (!target)
		return false;

	// #163 S8 fix per research (B.4): AC checks 30 yd explicitly here even though MoP's own Hand of
	// Freedom range is 40 yd (5.0.4) - AC is the spec, so this keeps AC's tighter radius rather than
	// relying on "party member snared target"'s own (looser) distance check.
	if (target != bot && bot->GetExactDist2dSq(target->GetPositionX(), target->GetPositionY()) > 30.0f * 30.0f)
		return false;

	return botAI->CanCastSpell("hand of freedom", target) && !botAI->HasAura("hand of freedom", target);
}

// #357 slice 6 (AC PaladinTriggers.cpp:65-67) - 1:1
bool DivineShieldLowHealthTrigger::IsActive()
{
    return botAI->HasAura("divine shield", bot) && AI_VALUE2(uint8, "health", "self target") < 80;
}

// #735 (AC ComboPointsAvailableTrigger::IsActive / EnergyAvailable::IsActive, AC GenericTriggers.cpp:92-94)
bool HolyPowerAvailableTrigger::IsActive() { return int32(bot->GetPower(POWER_HOLY_POWER)) >= amount; }

// #735: 90174 = the Divine Purpose proc (see PaladinTriggers.h)
bool DivinePurposeTrigger::IsActive() { return bot->HasAura(90174); }

// ---- rotation task 6 (#861) ----
bool PaladinSelfHealthTrigger::IsActive() { return AI_VALUE2(uint8, "health", "self target") <= pct; }

bool BastionOfGloryTrigger::IsActive()
{
    Aura* aura = bot->GetAura(114637, bot->GetGUID());
    return aura && aura->GetStackAmount() >= 3;
}

bool PaladinHurtGroupTrigger::IsActive()
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    std::vector<Player*> hurt;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member->IsAlive() && member->GetMapId() == bot->GetMapId() && member->GetHealthPct() < 50.0f &&
            bot->GetDistance(member) <= 40.0f)
            hurt.push_back(member);
    }

    if (radius >= 40.0f)
        return hurt.size() >= 3;

    for (Player* center : hurt)
    {
        uint32 count = 0;
        for (Player* other : hurt)
            if (center == other || center->GetDistance(other) <= radius)
                ++count;
        if (count >= 3)
            return true;
    }
    return false;
}

// BT-157: Beacon of Light (level 39) is the paladin's first pre-pull spell (core), Sacred Shield (a talent) the second (good).
// Both follow the tank guardrail of the other healers; before this they ignored mana altogether.
bool BeaconOfLightOnMainTankTrigger::IsActive()
{
    uint32 const spellId = AI_VALUE2(uint32, "spell id", "beacon of light");
    if (!spellId || !bot->HasSpell(spellId))
        return false;

    return HealerUpkeep::TankUpkeepAllowed(botAI, BotSkillMath::Tier::CASUAL) && BuffOnMainTankTrigger::IsActive();
}

bool SacredShieldOnMainTankTrigger::IsActive()
{
    uint32 const spellId = AI_VALUE2(uint32, "spell id", "sacred shield");
    if (!spellId || !bot->HasSpell(spellId))
        return false;

    return HealerUpkeep::TankUpkeepAllowed(botAI, BotSkillMath::Tier::GOOD) && BuffOnMainTankTrigger::IsActive();
}

// BT-683 to BT-687: see PaladinTriggers.h.
bool PvpHolyPaladinTrigger::IsActive()
{
    return PvpHolyPaladinGlue::Due(botAI, bot, rule);
}
