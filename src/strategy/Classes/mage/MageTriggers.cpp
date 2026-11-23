/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#include "MageTriggers.h"

// 5.4.8 include-order: DynamicObject.h derives from GridObject<DynamicObject> but only includes
// Object.h, and Map.h does NOT pull GridObject.h either - GridObject.h is reached only via Creature.h
// (Creature.h:28) and GameObject.h. So DynamicObject.h does not compile unless one of those came first
// (C7568/C3770 "argument list missing after assumed function template 'GridObject'"). This matches the
// core's own precedent, e.g. ObjectAccessor.cpp includes Creature.h before DynamicObject.h.
#include "BotSkill.h"
#include "Creature.h"
#include "DpsRotationMath.h"
#include "DynamicObject.h"
#include "Group.h"
#include "MageActions.h"
#include "OneVsOne.h"
#include "Player.h"
#include "Playerbots.h"
#include "PvpBurst.h"
#include "PvpFrostMage.h"
#include "PvpTeam.h"
#include "ServerFacade.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "Value.h"

bool ArcaneBrillianceTrigger::IsActive()
{
    return BuffTrigger::IsActive() && !botAI->HasAura("arcane brilliance", GetTarget());
}

bool RuneOfPowerTrigger::IsActive()
{
    return BuffTrigger::IsActive() && !botAI->HasAura("rune of power", GetTarget());
}

// Mage sheets: prefer the armor that helps the spec, including while grouped.
bool MageArmorTrigger::IsActive()
{
    return !bot->HasSpell(7302) && !bot->HasSpell(30482) && !botAI->HasAura("mage armor", bot);
}

bool FrostArmorTrigger::IsActive()
{
    return BotGetSpecialization(bot) != Specializations::SPEC_MAGE_FIRE && bot->HasSpell(7302) &&
        !botAI->HasAura("frost armor", bot);
}

bool MoltenArmorTrigger::IsActive()
{
    return (BotGetSpecialization(bot) == Specializations::SPEC_MAGE_FIRE || !bot->HasSpell(7302)) &&
        bot->HasSpell(30482) && !botAI->HasAura("molten armor", bot);
}

bool FrostNovaOnTargetTrigger::IsActive()
{
    Unit* target = GetTarget();
    if (!target || !target->IsAlive() || !target->IsInWorld())
    {
        return false;
    }
    return botAI->HasAura(spell, target);
}

bool IcyVeinsTrigger::IsActive()
{
    Unit* target = GetTarget();
    if (!target || !target->IsAlive() || !target->IsInWorld())
    {
        return false;
    }
    return !botAI->HasAura("icy veins", target);
}

bool FingersOfFrostSingleTrigger::IsActive()
{
    // AC: "Fingers of Frost "stack" count is always 1. The value is instead stored in the charges." (WotLK 44544.)
    // #790: this 5.4.8 core keeps Fingers of Frost 44544 ("Limit 2 charges", Wowhead mop-classic tooltip 44544,
    // 2026-10-12) as a STACK count - spell_mage.cpp spell_mage_ice_lance / spell_mage_deep_freez consume it with
    // ModStackAmount(-1) - so GetCharges() was always 0 and this "last charge" edge never fired (wave 35: no
    // "T:fingers of frost single" at all). Same test on the stack amount.
    Aura* aura = botAI->GetAura("fingers of frost", bot, false, true, -1);
    return (aura && aura->GetStackAmount() == 1);
}

bool SlowNotOnTargetTrigger::IsActive()
{
    Unit* target = GetTarget();
    if (!target || !target->IsAlive() || !target->IsInWorld())
    {
        return false;
    }
    return !botAI->HasAura(spell, target);
}

bool HeatingUpTrigger::IsActive()
{
    Aura* aura = botAI->GetAura("heating up", bot, false, false, -1);
    return aura != nullptr;
}
// ---------------------------------------------------------------------------
// AC WotLK parity (#357 slice 8) - ported 1:1 from
// modules/mod-playerbots/src/Ai/Class/Mage/MageTriggers.cpp in the AC WotLK tree.
// ---------------------------------------------------------------------------

bool NoManaGemTrigger::IsActive()
{
    // #385: AC's six WotLK gem ids (33312/22044/8008/8007/5513/5514) do not exist as conjured items in
    // 5.4.8; Conjure Mana Gem (759) makes the single Mana Gem 36799 (Wowhead mop-classic, 2026-10-04).
    static const std::vector<uint32> gemIds = {
        36799   // Mana Gem
    };

    for (uint32 gemId : gemIds)
    {
        if (bot->GetItemCount(gemId, false) > 0)  // false = only in bags
            return false;
    }
    return true;
}

bool DeepFreezeCooldownTrigger::IsActive()
{
    constexpr uint32 SPELL_DEEP_FREEZE = 44572;
    return !bot->HasSpell(SPELL_DEEP_FREEZE) || SpellCooldownTrigger::IsActive();
}

const std::unordered_set<uint32> FlamestrikeNearbyTrigger::FLAMESTRIKE_SPELL_IDS = {
    2120, 2121, 8422, 8423, 10215, 10216, 27086, 42925, 42926
};

bool FlamestrikeNearbyTrigger::IsActive()
{
    for (uint32 spellId : FLAMESTRIKE_SPELL_IDS)
    {
        Aura* aura = bot->GetAura(spellId, bot->GetGUID());
        if (!aura)
            continue;

        DynamicObject* dynObj = aura->GetDynobjOwner();
        if (!dynObj)
            continue;

        float dist = bot->GetDistance2d(dynObj->GetPositionX(), dynObj->GetPositionY());
        if (dist <= radius)
            return true;
    }
    return false;
}

const std::unordered_set<uint32> BlizzardChannelCheckTrigger::BLIZZARD_SPELL_IDS = {
    10,     // Blizzard Rank 1
    6141,   // Blizzard Rank 2
    8427,   // Blizzard Rank 3
    10185,  // Blizzard Rank 4
    10186,  // Blizzard Rank 5
    10187,  // Blizzard Rank 6
    27085,  // Blizzard Rank 7
    42938,  // Blizzard Rank 8
    42939   // Blizzard Rank 9
};

bool BlizzardChannelCheckTrigger::IsActive()
{
    if (Spell* spell = bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL); spell && BLIZZARD_SPELL_IDS.count(spell->m_spellInfo->Id))
    {
        uint8 attackerCount = AI_VALUE(uint8, "attacker count");
        return attackerCount < minEnemies;
    }

    return false;
}

// Rotation task 10 (#865): PvP-gated triggers, see MageTriggers.h.
bool MagePvpEnemyPlayerInMeleeRangeTrigger::IsActive()
{
    if (!AI_VALUE(bool, "in pvp instance"))
        return false;

    Trigger* t = botAI->GetAiObjectContext()->GetTrigger("enemy player in melee range");
    return t && t->IsActive();
}

bool MagePvpEnemyCasterInRangeTrigger::IsActive()
{
    if (!AI_VALUE(bool, "in pvp instance"))
        return false;

    Trigger* t = botAI->GetAiObjectContext()->GetTrigger("enemy caster in range");
    return t && t->IsActive();
}

bool MageIceBlockHealthTrigger::IsActive()
{
    // Mage sheets: in a started duel block only after both escapes are spent.
    if (bot->duel && bot->duel->startTime)
        return bot->GetHealthPct() <= 30.0f && bot->HasSpellCooldown(122) && bot->HasSpellCooldown(1953);
    if (AI_VALUE(bool, "in pvp instance"))
        return bot->GetGroup() && bot->GetHealthPct() <= 10.0f;

    Trigger* t = botAI->GetAiObjectContext()->GetTrigger("critical health");  // AC GenericMageStrategy.cpp:100
    return t && t->IsActive();
}

// Rotation fix task H (#868): see MageTriggers.h.
bool MageMediumThreatTrigger::IsActive()
{
    if (AI_VALUE(bool, "in pvp instance"))
        return false;

    Trigger* t = botAI->GetAiObjectContext()->GetTrigger("medium threat");
    return t && t->IsActive();
}

bool MageFleeCloseTrigger::IsActive()
{
    if (AI_VALUE(bool, "in pvp instance"))
        return false;

    Trigger* t = botAI->GetAiObjectContext()->GetTrigger("enemy too close for spell");
    return t && t->IsActive();
}

// Mage sheets: use long cooldowns on bosses or elite packs, without a grid scan.
// BT-620: against an enemy player there is no boss or elite pack, so Arcane Power, Mirror Image and Icy Veins (the edges of this
// trigger) never fired in a duel, a battleground or an arena. A player target asks the burst window instead; the PvE gate for
// every other target is as it was.
bool MageImportantPullTrigger::IsActive()
{
    Unit* target = GetTarget();
    if (PvpBurstGlue::TargetIsEnemyPlayer(botAI, bot, target))
        return PvpBurstGlue::Open(botAI, bot, target);
    Creature* creature = target ? target->ToCreature() : nullptr;
    return creature && (creature->IsDungeonBoss() || creature->isWorldBoss() ||
        (creature->isElite() && AI_VALUE(uint8, "attacker count") >= 3));
}

bool MagePlayerNovaTrigger::IsActive()
{
    Unit* target = GetTarget();
    // In a 1v1 the 1v1 engine times Frost Nova by skill tier (OneVsOne.h).
    if (OneVsOneGlue::Owns(botAI, bot))
        return false;
    return target && target->GetTypeId() == TypeID::TYPEID_PLAYER && bot->IsValidAttackTarget(target) &&
        bot->GetDistance(target) <= 10.0f && !botAI->HasAura("frost nova", target);
}

bool MageMovingTrigger::IsActive()
{
    return AI_VALUE2(bool, "moving", "self target");
}

// Arcane sheet: save mana before Evocation is learned.
bool ArcaneBlastStackTrigger::IsActive()
{
    if (bot->getLevel() >= 40)
        return HasAuraStackTrigger::IsActive();
    return botAI->GetAura("arcane charge", bot, false, true, 3) != nullptr;
}

// Mage sheets: Cold Snap resets these spells in 5.4.8.
bool ColdSnapTrigger::IsActive()
{
    return bot->HasSpellCooldown(122) && (bot->HasSpellCooldown(45438) || bot->HasSpellCooldown(120));
}

// Mage sheets: use a chill when a player in range is not already slowed.
bool MagePlayerSlowTrigger::IsActive()
{
    Unit* target = GetTarget();
    return target && target->GetTypeId() == TypeID::TYPEID_PLAYER && bot->IsValidAttackTarget(target) &&
        bot->GetDistance(target) <= 10.0f && !target->HasAuraType(SPELL_AURA_MOD_DECREASE_SPEED);
}

// Mage sheets: count cached attackers near the caster and protect the marked or active CC target.
bool MageExplosionTrigger::IsActive()
{
    Unit* marked = AI_VALUE(Unit*, "rti cc target");
    if (marked && bot->GetDistance(marked) <= 10.0f)
        return false;
    GuidVector attackers = AI_VALUE(GuidVector, "attackers");
    unsigned count = 0;
    for (BotGuid const guid : attackers)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive() || bot->GetDistance(unit) > 10.0f)
            continue;
        if (botAI->HasAura("polymorph", unit))
            return false;
        ++count;
    }
    return count >= 3;
}

bool FirePyroblastOpenerTrigger::IsActive()
{
    constexpr uint32 SPELL_PYROBLAST = 11366;
    Unit* target = AI_VALUE(Unit*, "current target");
    return target && target->IsAlive() &&
           DpsRotationMath::Mage::PyroblastOpener(bot->GetGroup() != nullptr, target->IsInCombat(),
               target->GetTypeId() == TYPEID_PLAYER, bot->HasSpell(SPELL_PYROBLAST));
}

namespace
{
constexpr uint32 SPELL_PRESENCE_OF_MIND = 12043;

// The fights the Fire sheet bursts on: a pack of three with an elite, a boss, or a player.
bool FireBigFight(Unit* target, uint8 attackers)
{
    if (!target || !target->IsAlive())
        return false;
    if (target->GetTypeId() == TYPEID_PLAYER)
        return true;
    Creature* creature = target->ToCreature();
    return creature && (creature->IsDungeonBoss() || creature->isWorldBoss() || (creature->isElite() && attackers >= 3));
}
}  // namespace

namespace
{
constexpr uint32 SPELL_FROSTJAW = 102051;
constexpr uint32 SPELL_POLYMORPH = 118;

// BT-620: the facts of the Presence of Mind route against a player (PvpBurst::RoutePresenceOfMind).
PvpBurst::PomInputs PomFacts(PlayerbotAI* botAI, Player* bot, Unit* target)
{
    PvpBurst::PomInputs in;
    in.windowOpen = PvpBurstGlue::Open(botAI, bot, target);
    // BT-631: the team's control plan wants this mage's Polymorph on the enemy healer now (R4): the instant goes there.
    in.polymorphDue = PvpTeamGlue::ControlPickIs(botAI, bot, SPELL_POLYMORPH);
    in.frostjawReady = bot->HasSpell(SPELL_FROSTJAW) && botAI->CanCastSpell(SPELL_FROSTJAW, target, false);
    in.targetAlreadyHeld = target->HasUnitState(UNIT_STATE_STUNNED | UNIT_STATE_ROOT) || target->HasAuraWithMechanic(1u << MECHANIC_SILENCE);
    return in;
}
}  // namespace

// Fire guide: Presence of Mind goes on an instant control (Polymorph, Frostjaw), not on a Pyroblast, which does a quarter less than
// a Hot Streak one. Against a player the trigger therefore fires only when the route names a control; the Pyroblast spend of the
// aura (PresenceOfMindActiveTrigger) is for everything else.
bool FirePresenceOfMindTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    if (PvpBurstGlue::TargetIsEnemyPlayer(botAI, bot, target))
        return bot->HasSpell(SPELL_PRESENCE_OF_MIND) && !botAI->HasAura("presence of mind", bot) &&
               PvpBurst::RoutePresenceOfMind(PomFacts(botAI, bot, target)) != PvpBurst::PomRoute::HOLD;
    return DpsRotationMath::Mage::PresenceOfMindNow(bot->HasSpell(SPELL_PRESENCE_OF_MIND),
        FireBigFight(target, AI_VALUE(uint8, "attacker count")), botAI->HasAura("presence of mind", bot));
}

// The aura is up: against a player its instant goes to a control, never to the Pyroblast edge.
bool PresenceOfMindActiveTrigger::IsActive()
{
    if (!HasAuraTrigger::IsActive())
        return false;
    return !PvpBurstGlue::TargetIsEnemyPlayer(botAI, bot, AI_VALUE(Unit*, "current target"));
}

bool FirePomControlTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    if (!PvpBurstGlue::TargetIsEnemyPlayer(botAI, bot, target) || !botAI->HasAura("presence of mind", bot))
        return false;
    return PvpBurst::RoutePresenceOfMind(PomFacts(botAI, bot, target)) == PvpBurst::PomRoute::FROSTJAW;
}

bool FireAlterTimeTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    return DpsRotationMath::Mage::AlterTimeNow(bot->getLevel(), BotSkill::Uses(bot, BotSkillMath::Tier::GOOD),
        FireBigFight(target, AI_VALUE(uint8, "attacker count")), botAI->HasAura("pyroblast!", bot),
        botAI->HasAura("alter time", bot));
}

// BT-650: see MageTriggers.h. The decisions are in Utils/PvpFrostMagePolicy.h, the facts in PvP/PvpFrostMage.cpp.
bool FrostAlterTimeTrigger::IsActive()
{
    return PvpFrostMageGlue::AlterTimeDue(botAI, bot);
}

bool FrostPetFreezeTrigger::IsActive()
{
    return PvpFrostMageGlue::PetFreezeDue(botAI, bot);
}

bool FrostPetFrozenTargetTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    return PvpBurstGlue::TargetIsEnemyPlayer(botAI, bot, target) && PvpFrostMageGlue::TargetFrozenByPet(target);
}

bool FrostPresenceOfMindTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    return PvpBurstGlue::TargetIsEnemyPlayer(botAI, bot, target) && bot->HasSpell(SPELL_PRESENCE_OF_MIND) &&
        !botAI->HasAura("presence of mind", bot) &&
        PvpBurst::RoutePresenceOfMind(PomFacts(botAI, bot, target)) != PvpBurst::PomRoute::HOLD;
}

bool FrostIncantersWardTrigger::IsActive()
{
    return PvpFrostMageGlue::WardDue(botAI, bot);
}
