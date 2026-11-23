/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option) any later version.
 */

#include "PriestTriggers.h"
#include "DpsRotationMath.h"

#include "BotSkill.h"
#include "HealerTriggers.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "ThreatManager.h"

namespace
{
// Class sheet, holy priest (BT-126).
constexpr uint32 SPELL_PRAYER_OF_MENDING = 33076;
// Party size from which the shared group heal rule applies (AoeInGroupTrigger), and the hurt members a smaller party needs.
constexpr int32 SHARED_GROUP_HEAL_MEMBERS = 5;
constexpr int32 SMALL_GROUP_HURT_MEMBERS = 3;

// The lowest member of the group is below the low health threshold: a heal comes before any dispel.
bool GroupMemberNeedsHealFirst(AiObjectContext* context)
{
    return AI_VALUE2(uint8, "health", "party member to heal") < sPlayerbotAIConfig->lowHealth;
}
}  // namespace

bool PowerWordFortitudeTrigger::IsActive()
{
    return BuffTrigger::IsActive() && !botAI->HasAura("power word: fortitude", GetTarget());
}

bool InnerFireTrigger::IsActive()
{
    // #790: Spirit of Redemption (27827, 15 sec after death; "can cast any healing spell", Wowhead mop-classic tooltip
    // 27827, 2026-10-12) is a baseline Holy passive in 5.4.8 (20711, level 30), a talent in WotLK. The spirit form has
    // no auras, so this fired every tick of the 15 s and Inner Fire 588 failed with SPELL_FAILED_NOT_SHAPESHIFT (71),
    // which CanCastSpell lets through like AC (wave 35 SM Durand, Gineriana: 68 PREREQ/FAILED, all in the spirit
    // windows right before "T:dead"). Same guard as AC's IsDisallowedShapeshiftForm (NonCombatActions.cpp:27-30).
    if (bot->HasAuraType(SPELL_AURA_SPIRIT_OF_REDEMPTION))
        return false;

    Unit* target = GetTarget();
    return SpellTrigger::IsActive() && !botAI->HasAura(spell, target);
}

bool FearWardOnMainTankTrigger::IsActive()
{
    uint32 const spellId = AI_VALUE2(uint32, "spell id", spell);
    if (!spellId || bot->HasSpellCooldown(spellId))
        return false;

    return BuffOnMainTankTrigger::IsActive();
}

bool DispelMagicTrigger::IsActive()
{
    return !GroupMemberNeedsHealFirst(context) && NeedCureTrigger::IsActive();
}

bool DispelMagicPartyMemberTrigger::IsActive()
{
    return !GroupMemberNeedsHealFirst(context) && PartyMemberNeedCureTrigger::IsActive();
}

bool CureDiseaseTrigger::IsActive() { return !GroupMemberNeedsHealFirst(context) && NeedCureTrigger::IsActive(); }

bool PartyMemberCureDiseaseTrigger::IsActive()
{
    return !GroupMemberNeedsHealFirst(context) && PartyMemberNeedCureTrigger::IsActive();
}

// Class sheet, holy priest (BT-126): see RenewOnMainTankTrigger in PriestTriggers.h. Renew is level 26; before that the
// spell id check keeps the edge silent.
bool RenewOnMainTankTrigger::IsActive()
{
    uint32 const spellId = AI_VALUE2(uint32, "spell id", "renew");
    if (!spellId || !bot->HasSpell(spellId))
        return false;

    // BT-157: the tank's upkeep follows the tank guardrail (more than 30 percent mana) in a fight and before the pull, no
    // longer the 60 percent save-mana line of the top-ups. Renew is the holy priest's first pre-pull spell (core) and the
    // discipline priest's second (good).
    BotSkillMath::Tier const prePullTier = BotGetSpecialization(bot) == Specializations::SPEC_PRIEST_DISCIPLINE ?
        BotSkillMath::Tier::GOOD : BotSkillMath::Tier::CASUAL;
    if (!HealerUpkeep::TankUpkeepAllowed(botAI, prePullTier))
        return false;

    return BuffOnMainTankTrigger::IsActive();
}

bool PrayerOfMendingOnMainTankTrigger::IsActive()
{
    if (!BotSkill::Uses(bot, BotSkillMath::Tier::GOOD) || !bot->HasSpell(SPELL_PRAYER_OF_MENDING) ||
        bot->HasSpellCooldown(SPELL_PRAYER_OF_MENDING))
        return false;

    // BT-157: expert tier before the pull, otherwise as the tank's other upkeep.
    if (!HealerUpkeep::TankUpkeepAllowed(botAI, BotSkillMath::Tier::EXPERT))
        return false;

    return BuffOnMainTankTrigger::IsActive();
}

// Class sheet, discipline priest (BT-128): see PowerWordShieldOnMainTankTrigger in PriestTriggers.h. Weakened Soul (6788,
// and its two later rows of the same name) is read by name from any caster.
bool PowerWordShieldOnMainTankTrigger::IsActive()
{
    uint32 const spellId = AI_VALUE2(uint32, "spell id", "power word: shield");
    if (!spellId || !bot->HasSpell(spellId))
        return false;

    // BT-157: the shield is the first pre-pull spell of the discipline priest (core): it lands before the mob does.
    if (!HealerUpkeep::TankUpkeepAllowed(botAI, BotSkillMath::Tier::CASUAL))
        return false;

    if (botAI->HasAura("weakened soul", GetTarget()))
        return false;

    return BuffOnMainTankTrigger::IsActive();
}

bool HolyGroupHealTrigger::IsActive()
{
    if (botAI->GetNearGroupMemberCount() >= SHARED_GROUP_HEAL_MEMBERS)
        return AoeInGroupTrigger::IsActive();

    return AI_VALUE2(uint8, "aoe heal", type) >= SMALL_GROUP_HURT_MEMBERS;
}

bool ShadowformTrigger::IsActive() { return bot->HasSpell(15473) && !botAI->HasAura("shadowform", bot); }

// Shadow priest sheet: avoid name lookups before these spells are learned.
// Shadow sheet line 15: an expert Shadow priest recasts its own DoT on the target in the refresh window, when the
// target lives as long as the caller asks for a new DoT.
bool ShadowPriestSheet::DotRefreshDue(PlayerbotAI* botAI, Unit* target, uint32 spellId, float needLifeTime)
{
    Player* bot = botAI->GetBot();
    if (!target || !target->IsAlive() || BotGetSpecialization(bot) != SPEC_PRIEST_SHADOW ||
        !BotSkill::Uses(bot, BotSkillMath::Tier::EXPERT))
        return false;
    Aura const* dot = target->GetAura(spellId, bot->GetGUID());
    if (!dot || !DpsRotationMath::Priest::RefreshDot(true, dot->GetDuration()))
        return false;
    float const groupDps = botAI->GetAiObjectContext()->GetValue<float>("estimated group dps")->Get();
    return target->GetHealth() / groupDps >= needLifeTime;
}

bool ShadowWordPainTrigger::IsActive()
{
    return bot->HasSpell(589) &&
           (DebuffTrigger::IsActive() || ShadowPriestSheet::DotRefreshDue(botAI, GetTarget(), 589, needLifeTime));
}
bool ShadowWordPainOnAttackerTrigger::IsActive() { return bot->HasSpell(589) && DebuffOnAttackerTrigger::IsActive(); }
bool VampiricTouchTrigger::IsActive()
{
    return bot->HasSpell(34914) &&
           (DebuffTrigger::IsActive() || ShadowPriestSheet::DotRefreshDue(botAI, GetTarget(), 34914, needLifeTime));
}
bool VampiricTouchOnAttackerTrigger::IsActive() { return bot->HasSpell(34914) && DebuffOnAttackerTrigger::IsActive(); }

bool VampiricEmbraceTrigger::IsActive()
{
    if (!bot->HasSpell(15286) || !bot->IsInCombat())
        return false;
    Group* group = bot->GetGroup();
    if (!group)
        return false;
    uint32 hurt = 0;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        if (Player* member = ref->GetSource())
            if (member->IsAlive() && member->GetHealthPct() < 80.0f && bot->GetDistance(member) <= 30.0f && ++hurt >= 2)
                return true;
    return false;
}

bool ShadowPriestConditionTrigger::IsActive()
{
    if (condition == "shadow solo flash heal")
        return !bot->GetGroup() && bot->getLevel() < 24 && bot->HasSpell(2061) && bot->GetHealthPct() < 25.0f;
    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || !bot->IsInCombat())
        return false;
    if (condition == "shadow mind sear")
    {
        AoeTrigger pack(botAI, 5, 10.0f);
        return bot->HasSpell(48045) && pack.IsActive();
    }
    if (condition == "shadowfiend hard fight")
    {
        AoeTrigger pack(botAI, 3, 8.0f);
        Creature* creature = target->ToCreature();
        return bot->HasSpell(34433) && (pack.IsActive() || (creature && creature->isElite()));
    }
    if (condition == "priest high threat" && target->ToCreature())
    {
        Unit* tank = AI_VALUE(Unit*, "main tank");
        if (!tank || tank == bot)
            return false;
        ThreatManager& threat = target->getThreatManager();
        float tankThreat = threat.getThreat(tank);
        return tankThreat > 0.0f && threat.getThreat(bot) >= tankThreat * 0.7f;
    }
    return false;
}

// #812: same rule as ShadowformTrigger, gated on the spell being known (Holy L56, spec257) so non-Holy priests and
// low-level Holy priests never queue it.
// Task 8 / #863 (MoP rotation row 25723): >= 3 group members (the bot included) within 30 yd of the bot below 80% HP.
static bool PriestWantsSanctuary(Player* bot)
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;
    uint32 hurt = 0;
    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* member = gref->GetSource();
        if (member && member->IsAlive() && member->GetHealthPct() < 80.0f && bot->GetDistance(member) <= 30.0f)
            ++hurt;
    }
    return hurt >= 3;
}

bool ChakraSerenityTrigger::IsActive()
{
    return AI_VALUE2(uint32, "spell id", spell) && !botAI->HasAura("chakra: serenity", bot) &&
           !PriestWantsSanctuary(bot);  // MoP rotation row 25724: Serenity only when Sanctuary is not wanted
}

bool ChakraSanctuaryTrigger::IsActive()
{
    return AI_VALUE2(uint32, "spell id", "chakra: sanctuary") && !botAI->HasAura("chakra: sanctuary", bot) &&
           PriestWantsSanctuary(bot);
}

bool SurgeOfLightProcTrigger::IsActive() { return bot->HasAura(114255); }

bool LightwellCanCastTrigger::IsActive()
{
    constexpr uint32 SPELL_LIGHTWELL = 126135;
    return bot->IsInCombat() && bot->HasSpell(SPELL_LIGHTWELL) && !bot->HasSpellCooldown(SPELL_LIGHTWELL);
}

bool ChakraSanctuaryMissingTrigger::IsActive()
{
    return AI_VALUE2(uint32, "spell id", "chakra: sanctuary") && !botAI->HasAura("chakra: sanctuary", bot);
}

// w75: MoP's 7 yd is conservative; Void Tendrils 108920 roots everything within 8 yd (Wowhead tooltip) and the area
// check (WorldObjectSpellAreaTargetCheck, Spell.cpp:9436) adds the target's object size. The harness mob 32285 stands
// exactly 8.0 yd away, so the old 7 yd gate could never open.
bool VoidTendrilsTargetCloseTrigger::IsActive()
{
    if (!bot->IsInCombat() || !bot->HasSpell(108920) || bot->HasSpellCooldown(108920))
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || !target->IsAlive() || target->IsFriendlyTo(bot) || bot->GetExactDist(target) > 8.0f + target->GetObjectSize())
        return false;

    return (target->GetTypeId() == TypeID::TYPEID_PLAYER) || target->GetVictim() == bot;
}

bool PriestSelfHealthTrigger::IsActive() { return (pct == 45.0f || bot->IsInCombat()) && bot->GetHealthPct() < pct; }

bool ShadowfiendTrigger::IsActive() { return BoostTrigger::IsActive() && !bot->HasSpellCooldown(34433); }

BindingHealTrigger::BindingHealTrigger(PlayerbotAI* botAI)
    : PartyMemberLowHealthTrigger(botAI, "binding heal", sPlayerbotAIConfig->lowHealth, 0)
{
}

bool BindingHealTrigger::IsActive()
{
    return PartyMemberLowHealthTrigger::IsActive() &&
           AI_VALUE2(uint8, "health", "self target") < sPlayerbotAIConfig->mediumHealth;
}

const std::set<uint32> MindSearChannelCheckTrigger::MIND_SEAR_SPELL_IDS = {
    48045,  // Mind Sear Rank 1
    53023   // Mind Sear Rank 2
};

bool MindSearChannelCheckTrigger::IsActive()
{
    Player* bot = botAI->GetBot();

    // Check if the bot is channeling a spell
    if (Spell* spell = bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
    {
        // Only trigger if the spell being channeled is Mind Sear
        if (MIND_SEAR_SPELL_IDS.count(spell->m_spellInfo->Id))
        {
            uint8 attackerCount = AI_VALUE(uint8, "attacker count");
            return attackerCount < minEnemies;
        }
    }

    // Not channeling Mind Sear
    return false;
}

// #735 (AC ComboPointsAvailableTrigger::IsActive, AC GenericTriggers.cpp:94)
bool ShadowOrbsAvailableTrigger::IsActive() { return int32(bot->GetPower(POWER_SHADOW_ORBS)) >= amount; }

bool ShadowMindFlayClipTrigger::IsActive()
{
    // Plain Mind Flay only: Mind Flay (Insanity) 129197 is worth more than a Mind Blast and runs to its end.
    constexpr uint32 SPELL_MIND_FLAY = 15407;
    constexpr uint32 SPELL_MIND_BLAST = 8092;
    Spell* channel = bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
    if (!channel || channel->m_spellInfo->Id != SPELL_MIND_FLAY)
        return false;
    if (!BotSkill::Uses(bot, BotSkillMath::Tier::EXPERT))
        return false;
    Unit* target = AI_VALUE(Unit*, "current target");
    Aura* flay = target ? target->GetAura(SPELL_MIND_FLAY, bot->GetGUID()) : nullptr;
    if (!flay)
        return false;
    bool const blastReady = bot->HasSpell(SPELL_MIND_BLAST) && !bot->HasSpellCooldown(SPELL_MIND_BLAST);
    return DpsRotationMath::Priest::ClipMindFlay(true, blastReady, flay->GetDuration(), flay->GetMaxDuration());
}

// BT-688 to BT-690: see PriestTriggers.h.
bool PvpShadowPriestTrigger::IsActive()
{
    return PvpShadowPriestGlue::Due(botAI, bot, rule);
}
