/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#include "DKTriggers.h"

#include <string>

#include "BotSkill.h"
#include "DpsRotationMath.h"
#include "GenericTriggers.h"
#include "Group.h"
#include "MarkTriggers.h"
#include "PlayerbotSpec.h"
#include "PvpBurst.h"
#include "Playerbots.h"
#include "Pet.h"
#include "SharedDefines.h"

static constexpr uint32 DEATH_AND_DECAY_COOLDOWN_GATE_SECONDS = 2;

bool DeathAndDecayCooldownTrigger::IsActive()
{
    uint32 spellId = AI_VALUE2(uint32, "spell id", name);
    if (!spellId)
        return true;

    // Player::GetSpellCooldownDelay returns seconds; the old 2000 meant milliseconds and never matched, which
    // silenced every edge behind this gate (Scourge Strike, Festering Strike, Dark Transformation) from level 60.
    return bot->GetSpellCooldownDelay(spellId) >= DEATH_AND_DECAY_COOLDOWN_GATE_SECONDS;
}

bool NoRuneTrigger::IsActive()
{
    for (uint8 i = 0; i < MAX_RUNES; ++i)
    {
        if (!bot->GetRuneCooldown(i))
            return false;
    }
    return true;
}

namespace
{
    // Remaining "ms-equivalent" before rune index i is ready again; 0 if it already is.
    uint32 RuneRemaining(Player* bot, uint8 index)
    {
        return bot->GetRuneCooldown(index);
    }
}

bool HighBloodRuneTrigger::IsActive() { return RuneRemaining(bot, 0) <= 2000 && RuneRemaining(bot, 1) <= 2000; }

bool HighFrostRuneTrigger::IsActive()
{
    // Frost sheet: Death Runes can pay the Frost cost as well.
    uint32 ready = 0;
    for (uint8 i = 0; i < MAX_RUNES; ++i)
        if (RuneRemaining(bot, i) <= 2000 && (bot->GetCurrentRune(i) == RuneType::RUNE_FROST ||
                                              bot->GetCurrentRune(i) == RuneType::RUNE_DEATH))
            ++ready;
    return ready >= 2;
}

bool HighUnholyRuneTrigger::IsActive() { return RuneRemaining(bot, 2) <= 2000 && RuneRemaining(bot, 3) <= 2000; }


bool DKPresenceTrigger::IsActive()
{
    Unit* target = GetTarget();
    return !botAI->HasAura("blood presence", target) && !botAI->HasAura("unholy presence", target) &&
           !botAI->HasAura("frost presence", target);
}

// 5.4.8: inert - see the PestilenceGlyphTrigger comment in DKTriggers.h. AC returned true when the
// WotLK Glyph of Pestilence (63334) was up and a disease had <= 3s left; that glyph is gone in MoP.
bool PestilenceGlyphTrigger::IsActive()
{
    return false;
}

// ---------------------------------------------------------------------------------------------------------
// rot-task-2 (#857): MoP rotation triggers (declarations in DKTriggers.h)
// ---------------------------------------------------------------------------------------------------------
namespace
{
    constexpr uint32 SPELL_BLOOD_PLAGUE = 55078;
    constexpr uint32 SPELL_FROST_FEVER = 55095;
    constexpr uint32 SPELL_CHAINS_OF_ICE = 45524;
    constexpr uint32 SPELL_KILLING_MACHINE = 51124;
    constexpr uint32 SPELL_SUDDEN_DOOM = 81340;
    // rot-fix-F: 81136 is the Blood passive learned with the spec (SpecializationSpells.dbc row 3319, spec 250), so
    // HasAura(81136) was always true and "dk blood boil" fired every tick, starving Soul Reaper / Heart Strike.
    // The free-Blood-Boil proc buff is 81141 (also named "Crimson Scourge", spell_dk.cpp:3056 script is on 81136).
    constexpr uint32 SPELL_CRIMSON_SCOURGE = 81141;
    constexpr uint32 SPELL_OUTBREAK = 77575;
    constexpr uint32 SPELL_FESTERING_STRIKE = 85948;
}

int32 DKCurrentTargetTrigger::OwnAuraLeft(Unit* target, uint32 spellId) const
{
    Aura* aura = target->GetAura(spellId, bot->GetGUID());
    return aura ? aura->GetDuration() : 0;
}

bool DKOutbreakTrigger::IsActive()
{
    if (!bot->HasSpell(SPELL_OUTBREAK))
        return false;
    Unit* target = GetTarget();
    if (!target || !target->IsAlive())
        return false;
    return !target->HasAura(SPELL_BLOOD_PLAGUE, bot->GetGUID()) || !target->HasAura(SPELL_FROST_FEVER, bot->GetGUID());
}

bool DKDiseaseExpiringTrigger::IsActive()
{
    if (!bot->HasSpell(SPELL_FESTERING_STRIKE))
        return false;
    Unit* target = GetTarget();
    if (!target || !target->IsAlive())
        return false;
    int32 bp = OwnAuraLeft(target, SPELL_BLOOD_PLAGUE);
    int32 ff = OwnAuraLeft(target, SPELL_FROST_FEVER);
    return (bp > 0 && bp <= withinMs) || (ff > 0 && ff <= withinMs);
}

bool DKBloodBoilTrigger::IsActive()
{
    if (bot->HasAura(SPELL_CRIMSON_SCOURGE))
        return true;
    Unit* target = GetTarget();
    if (!target || !target->IsAlive())
        return false;
    int32 bp = OwnAuraLeft(target, SPELL_BLOOD_PLAGUE);
    int32 ff = OwnAuraLeft(target, SPELL_FROST_FEVER);
    return (bp > 0 && bp <= 3000) || (ff > 0 && ff <= 3000);
}

bool DKFrostStrikeProcTrigger::IsActive()
{
    return BotGetPowerPct(bot, POWER_RUNIC_POWER) >= 89.0f || bot->HasAura(SPELL_KILLING_MACHINE);
}

bool DKDeathCoilProcTrigger::IsActive()
{
    return BotGetPowerPct(bot, POWER_RUNIC_POWER) >= 90.0f || bot->HasAura(SPELL_SUDDEN_DOOM);
}

bool DKChainsOfIcePlayerTrigger::IsActive()
{
    if (!AI_VALUE(bool, "in pvp instance"))
        return false;
    Unit* target = GetTarget();
    if (!target || !target->IsAlive() || target->GetTypeId() != TypeID::TYPEID_PLAYER)
        return false;
    if (target->HasAura(SPELL_CHAINS_OF_ICE, bot->GetGUID()))
        return false;
    return bot->GetDistance(target) >= 6.0f;
}

bool DKPvpEnemyCasterTrigger::IsActive()
{
    return AI_VALUE(bool, "in pvp instance") && AI_VALUE(Unit*, "enemy caster target");
}

// ---------------------------------------------------------------------------------------------------------
// Class sheet, blood death knight (BT-134): declarations in DKTriggers.h
// ---------------------------------------------------------------------------------------------------------
namespace
{
    constexpr uint32 SPELL_BLOOD_PRESENCE = 48263;  // level 57
    constexpr uint32 SPELL_FROST_PRESENCE = 48266;  // level 55
    constexpr uint32 SPELL_DEATH_STRIKE = 49998;    // level 56

    // A tank-spec death knight in a group that is not mounted (the cast would dismount it). Same rule as the warrior's
    // Defensive Stance out of combat (WarriorTriggers.cpp, TankDefensiveStanceTrigger).
    bool TankInGroupOnFoot(Player* bot) { return bot->GetGroup() && !bot->IsMounted() && PlayerBotSpec::IsTank(bot); }
}

bool TankBloodPresenceTrigger::IsActive()
{
    if (!bot->HasSpell(SPELL_BLOOD_PRESENCE) || !TankInGroupOnFoot(bot))
        return false;

    return BuffTrigger::IsActive();
}

bool TankFrostPresenceTrigger::IsActive()
{
    if (bot->HasSpell(SPELL_BLOOD_PRESENCE) || !bot->HasSpell(SPELL_FROST_PRESENCE) || !TankInGroupOnFoot(bot))
        return false;

    return BuffTrigger::IsActive();
}

// Rune slots hold a base type; a Death Rune is the current type of a slot that was converted (Blood Rites). A slot with no
// cooldown left is ready. The missing half of the cost is paid by Death Runes, as Spell::CheckRuneCost does.
bool DKDeathStrikeRunesTrigger::IsActive()
{
    if (!bot->HasSpell(SPELL_DEATH_STRIKE))
        return false;

    bool unholyReady = false;
    bool frostReady = false;
    uint32 deathReady = 0;
    for (uint8 i = 0; i < MAX_RUNES; ++i)
    {
        if (bot->GetRuneCooldown(i))
            continue;

        RuneType const rune = bot->GetCurrentRune(i);
        if (rune == RuneType::RUNE_UNHOLY)
            unholyReady = true;
        else if (rune == RuneType::RUNE_FROST)
            frostReady = true;
        else if (rune == RuneType::RUNE_DEATH)
            ++deathReady;
    }

    uint32 const missing = (unholyReady ? 0 : 1) + (frostReady ? 0 : 1);
    return deathReady >= missing;
}

// Class sheets, Frost and Unholy: execute only below 35 percent.
bool DKSoulReaperTrigger::IsActive()
{
    Unit* target = GetTarget();
    return bot->getLevel() >= 87 && target && target->IsAlive() && target->GetHealthPct() < 35.0f;
}

bool DKCoilNormalTrigger::IsActive()
{
    Pet* pet = bot->GetPet();
    return !pet || !pet->HasAura(63560);
}

bool DKUnholyFrostDiseaseTrigger::IsActive()
{
    return bot->getLevel() < 68 && IcyTouch3sDebuffTrigger::IsActive();
}

bool UnholyFrenzyTrigger::IsActive()
{
    return bot->getLevel() >= 66 && bot->GetHealthPct() >= 70.0f && BoostTrigger::IsActive();
}

// Damage dealer sheets: establish the appropriate presence before the pull.
bool DKDpsPresenceTrigger::IsActive()
{
    if (!bot->GetGroup() || bot->IsMounted() || PlayerBotSpec::IsTank(bot, true))
        return false;
    bool const wantsUnholy = PlayerBotSpec::GetSpectab(bot) == 2 && bot->getLevel() >= 64;
    return unholy == wantsUnholy && BuffTrigger::IsActive();
}

// BT-620: against an enemy player the elite, boss and pack gate never opened, so Pillar of Frost (and the Gargoyle of an Unholy
// bot) never fired in a duel, a battleground or an arena. A player target asks the burst window; PvE is as it was.
bool DKHardFightTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    if (PvpBurstGlue::TargetIsEnemyPlayer(botAI, bot, target))
        return PvpBurstGlue::Open(botAI, bot, target);
    Creature* creature = target ? target->ToCreature() : nullptr;
    return (creature && (creature->isElite() || creature->IsDungeonBoss() || creature->isWorldBoss())) ||
           AI_VALUE(uint8, "attacker count") >= 3;
}

bool DkPrePullArmyTrigger::IsActive()
{
    constexpr uint32 SPELL_ARMY_OF_THE_DEAD = 42650;
    if (bot->IsInCombat() || !bot->IsAlive() || bot->IsMounted() || !bot->HasSpell(SPELL_ARMY_OF_THE_DEAD))
        return false;
    Group* group = bot->GetGroup();
    if (!group || !bot->GetMap() || !bot->GetMap()->IsDungeon())
        return false;
    Specializations const spec = BotGetSpecialization(bot);
    bool const damageSpec = spec == SPEC_DEATH_KNIGHT_FROST || spec == SPEC_DEATH_KNIGHT_UNHOLY;
    bool const expert = BotSkill::Uses(bot, BotSkillMath::Tier::EXPERT);
    if (!expert || !damageSpec)
        return false;
    Unit* mark = MarkedTargetReadyTrigger::CurrentMark(botAI);
    if (!mark || !mark->IsAlive() || mark->IsInCombat())
        return false;
    Player* tank = AI_VALUE(Unit*, "main tank") ? AI_VALUE(Unit*, "main tank")->ToPlayer() : nullptr;
    bool const ready = tank && MarkedTargetReadyTrigger::GroupReady(group, tank, mark);
    return DpsRotationMath::DeathKnight::PrePullArmy(bot->getLevel(), expert, damageSpec,
        MarkedTargetReadyTrigger::IsBossMark(mark), ready, bot->GetDistance(mark));
}
