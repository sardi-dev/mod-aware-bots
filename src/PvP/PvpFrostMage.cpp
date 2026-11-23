/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE). See PvpFrostMage.h for what this is.
 */

#include "PvpFrostMage.h"

#include "BotSkill.h"
#include "Log.h"
#include "Pet.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "PvpBurst.h"
#include "PvpConfig.h"
#include "PvpFrostMagePolicy.h"
#include "PvpTeam.h"
#include "PvpUnitFacts.h"
#include "SpellAuras.h"

namespace PvpFrostMageGlue
{
namespace
{
using namespace PvpFrostMage;

constexpr char const* LOG_CATEGORY = "playerbots";

std::uint32_t FingersOf(PlayerbotAI* botAI, Player* bot)
{
    Aura* aura = botAI->GetAura("fingers of frost", bot, false, true, -1);
    return aura ? std::uint32_t(aura->GetStackAmount()) : 0;
}

bool Active(Player* bot)
{
    return PvpConfig::BurstRulesEnabled() && bot && bot->IsAlive() && bot->getClass() == CLASS_MAGE;
}

Unit* CurrentTarget(PlayerbotAI* botAI)
{
    return botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
}

bool Ready(Player* bot, uint32 spellId)
{
    return bot->HasSpell(spellId) && !bot->HasSpellCooldown(spellId);
}

FreezeInputs FreezeFacts(PlayerbotAI* botAI, Player* bot, Unit* target, Pet* pet)
{
    FreezeInputs in;
    in.petPresent = pet != nullptr && pet->IsAlive();
    in.petReady = in.petPresent && pet->HasSpell(SPELL_PET_FREEZE) && !pet->HasSpellCooldown(SPELL_PET_FREEZE) &&
        !pet->HasUnitState(UNIT_STATE_CASTING);
    in.targetIsPlayer = PvpBurstGlue::TargetIsEnemyPlayer(botAI, bot, target);
    if (in.targetIsPlayer && in.petPresent)
    {
        in.targetDistance = pet->GetExactDist(target);
        // The pet needs sight of the target like any cast; no sight counts as out of range.
        if (!pet->IsWithinLOSInMap(target))
            in.targetDistance = PET_FREEZE_RANGE_YD + 1.0f;
    }
    if (in.targetIsPlayer)
        in.targetHeld = PvpUnitFacts::IsHeldStill(target) || PvpUnitFacts::HasBreakableCc(target) ||
            target->HasAura(SPELL_PET_FREEZE);
    in.deepFreezeReady = Ready(bot, SPELL_DEEP_FREEZE);
    in.fingers = FingersOf(botAI, bot);
    in.windowOpen = in.targetIsPlayer && PvpBurstGlue::Open(botAI, bot, target);
    return in;
}
}  // namespace

bool AlterTimeDue(PlayerbotAI* botAI, Player* bot)
{
    if (!Active(bot))
        return false;
    Unit* target = CurrentTarget(botAI);
    if (!PvpBurstGlue::TargetIsEnemyPlayer(botAI, bot, target))
        return false;

    AlterInputs in;
    in.knows = bot->HasSpell(SPELL_ALTER_TIME);
    in.windowOpen = PvpBurstGlue::Open(botAI, bot, target);
    in.alterTimeUp = botAI->HasAura("alter time", bot);
    in.hypothermia = bot->HasAura(SPELL_HYPOTHERMIA);
    in.icyVeinsUp = botAI->HasAura("icy veins", bot);
    in.fingers = FingersOf(botAI, bot);
    in.brainFreeze = botAI->HasAura("brain freeze", bot);
    AlterVerdict const verdict = AlterTime(in, bot->getLevel(), BotSkill::TierOf(bot));
    if (verdict.cast)
        SF_LOG_DEBUG(LOG_CATEGORY, "[PVPFROST] bot=%u tier=%u kind=ALTER_TIME why=%s score=%d", bot->GetGUIDLow(),
            uint32(BotSkill::TierOf(bot)), AlterReasonName(verdict.reason), verdict.score);
    return verdict.cast;
}

bool OrbHeld(PlayerbotAI* botAI, Player* bot, Unit* target)
{
    if (!Active(bot) || !PvpBurstGlue::TargetIsEnemyPlayer(botAI, bot, target))
        return false;
    OrbInputs in;
    in.targetIsPlayer = true;
    in.windowOpen = PvpBurstGlue::Open(botAI, bot, target);
    in.targetBreakableCc = PvpUnitFacts::HasBreakableCc(target);
    return !OrbAllowed(FrozenOrb(in, BotSkill::TierOf(bot)));
}

bool DeepFreezeKept(PlayerbotAI* botAI, Player* bot, Unit* target)
{
    if (!Active(bot) || !PvpConfig::TeamPlayEnabled() || !bot->InBattleground() ||
        !PvpBurstGlue::TargetIsEnemyPlayer(botAI, bot, target))
        return false;
    PvpTeamGlue::HealerFacts const healer = PvpTeamGlue::HealerNear(botAI, bot, target);
    DeepFreezeInputs in;
    in.teamContext = true;
    in.healerPresent = healer.present;
    in.targetIsHealer = healer.isTarget;
    in.healerLockedMs = healer.lockedMs;
    return !DeepFreezeAllowed(DeepFreezeUse(in, BotSkill::TierOf(bot)));
}

bool PetFreezeDue(PlayerbotAI* botAI, Player* bot)
{
    if (!Active(bot))
        return false;
    Unit* target = CurrentTarget(botAI);
    FreezeVerdict const verdict = PetFreeze(FreezeFacts(botAI, bot, target, bot->GetPet()), BotSkill::TierOf(bot));
    return verdict.cast;
}

bool PetFreezeCast(PlayerbotAI* botAI, Player* bot)
{
    if (!PetFreezeDue(botAI, bot))
        return false;
    Unit* target = CurrentTarget(botAI);
    Pet* pet = bot->GetPet();
    if (!target || !pet)
        return false;
    // Freeze strikes the ground under the target (a destination spell), so the pet casts at its position.
    pet->CastSpell(target->GetPositionX(), target->GetPositionY(), target->GetPositionZ(), SPELL_PET_FREEZE, false);
    SF_LOG_DEBUG(LOG_CATEGORY, "[PVPFROST] bot=%u tier=%u kind=PET_FREEZE target=%u", bot->GetGUIDLow(),
        uint32(BotSkill::TierOf(bot)), target->GetGUIDLow());
    return true;
}

bool WardDue(PlayerbotAI* botAI, Player* bot)
{
    if (!Active(bot))
        return false;
    Unit* target = CurrentTarget(botAI);
    WardInputs in;
    in.knows = bot->HasSpell(SPELL_INCANTERS_WARD);
    in.ready = Ready(bot, SPELL_INCANTERS_WARD);
    in.up = botAI->HasAura("incanter's ward", bot);
    in.targetIsPlayer = PvpBurstGlue::TargetIsEnemyPlayer(botAI, bot, target);
    in.windowOpen = in.targetIsPlayer && PvpBurstGlue::Open(botAI, bot, target);
    for (Unit* attacker : bot->getAttackers())
        if (attacker && attacker->GetTypeId() == TYPEID_PLAYER)
            in.attackedByPlayer = true;
    in.healthPercent = bot->GetHealthPct();
    return PvpFrostMage::WardDue(IncantersWard(in, BotSkill::TierOf(bot)));
}

bool TargetFrozenByPet(Unit* target)
{
    return target && target->HasAura(SPELL_PET_FREEZE);
}
}  // namespace PvpFrostMageGlue
