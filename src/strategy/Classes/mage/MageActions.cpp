/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#include "MageActions.h"

#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "PlayerbotSpec.h"
#include "PvpFrostMage.h"
#include "ServerFacade.h"
#include "SharedDefines.h"

// #385 (AC MageActions.cpp:32-36): only in combat, only when the gem is in the bags. Item 36799 = Mana Gem
// (the single 5.4.8 conjured gem, Wowhead mop-classic tooltip item 36799, 2026-10-04).
bool UseManaGemAction::isUseful()
{
    Player* bot = botAI->GetBot();
    return AI_VALUE2(bool, "combat", "self target") && bot->GetItemCount(36799, false) > 0;  // Mana Gem
}

Value<Unit*>* CastPolymorphAction::GetTargetValue() { return context->GetValue<Unit*>("cc target", getName()); }

bool CastFrostNovaAction::isUseful()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    return sServerFacade->IsDistanceLessOrEqualThan(AI_VALUE2(float, "distance", GetTargetName()), 10.f);
}

bool CastBlinkAction::isUseful()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    return sServerFacade->IsDistanceLessOrEqualThan(AI_VALUE2(float, "distance", GetTargetName()), 5.0f);
}

// #408 (AC parity, AC MageActions.cpp:151-159): face away from the current target, then Blink past
// it. Kept identical to AC - botAI->CastSpell resolves the "blink" spell name the same way here.
bool CastBlinkBackAction::Execute(Event event)
{
    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target)
        return false;

    bot->SetOrientation(bot->GetAngle(target) + M_PI);
    return CastSpellAction::Execute(event);
}

// Task 10 (#865): Blizzard only when NOT in an arena.
bool CastBlizzardAction::isUseful() { return !bot->InArena() && CastSpellAction::isUseful(); }

bool CastSlowAction::isUseful()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    return sServerFacade->IsDistanceLessOrEqualThan(AI_VALUE2(float, "distance", GetTargetName()), 25.0f);
}
bool CastDeepFreezeAction::isUseful()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target) return false;

    // Frost sheet: Deep Freeze is a PvP stun, not a damage spell. BT-654: in a team fight it is kept for the enemy healer until the
    // healer is held (the control chain casts it there), so a Fingers of Frost proc does not spend it on a damage dealer.
    return target->GetTypeId() == TypeID::TYPEID_PLAYER && !botAI->HasAura("deep freeze", target) &&
        !PvpFrostMageGlue::DeepFreezeKept(botAI, bot, target);
}
bool CastConeOfColdAction::isUseful()
{
    bool facingTarget = AI_VALUE2(bool, "facing", "current target");
    bool targetClose = sServerFacade->IsDistanceLessOrEqualThan(AI_VALUE2(float, "distance", GetTargetName()), 10.f);
    return facingTarget && targetClose;
}

bool CastFrozenOrbAction::isUseful()
{
    bool facingTarget = AI_VALUE2(bool, "facing", "current target");
    // Rotation fix task B (#865): 10 -> 30 yd; the MoP rotation casts Frozen Orb "always" at the current target.
    bool targetClose = sServerFacade->IsDistanceLessOrEqualThan(AI_VALUE2(float, "distance", GetTargetName()), 30.f);
    // BT-653: against a player it is a damage cooldown of the burst window, not a filler on cooldown, and never into a sheep.
    return facingTarget && targetClose && !PvpFrostMageGlue::OrbHeld(botAI, bot, AI_VALUE(Unit*, "current target"));
}

bool CastPetFreezeAction::isUseful()
{
    return PvpFrostMageGlue::PetFreezeDue(botAI, bot);
}

bool CastPetFreezeAction::Execute(Event /*event*/)
{
    return PvpFrostMageGlue::PetFreezeCast(botAI, bot);
}

bool CastDragonsBreathAction::isUseful()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target)
        return false;
    // Fire sheet: do not disorient the tank's target just because it is nearby.
    if (target->GetTypeId() != TypeID::TYPEID_PLAYER && target->GetVictim() != bot)
        return false;
    bool facingTarget = AI_VALUE2(bool, "facing", "current target");
    bool targetClose = bot->IsWithinCombatRange(target, 10.0f);
    return facingTarget && targetClose;
}

bool CastRuneOfPowerAction::Execute(Event event)
{
    return botAI->CastSpell(spell, bot);
}

// Frost sheet: preserve a nearby player root long enough to step away.
bool CastFrostboltAction::isUseful()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    return !(target && target->GetTypeId() == TypeID::TYPEID_PLAYER && bot->IsWithinMeleeRange(target) &&
        botAI->HasAura("frost nova", target)) && CastSpellAction::isUseful();
}
bool CastFrostfireBoltAction::isUseful()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    return !(target && target->GetTypeId() == TypeID::TYPEID_PLAYER && bot->IsWithinMeleeRange(target) &&
        botAI->HasAura("frost nova", target)) && CastSpellAction::isUseful();
}
