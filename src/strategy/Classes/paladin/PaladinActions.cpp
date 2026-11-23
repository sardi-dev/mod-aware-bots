/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#include "PaladinActions.h"

#include "AiFactory.h"
#include "Event.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "PlayerbotSpec.h"
#include "Playerbots.h"
#include "SharedDefines.h"
#include "TankDangerPolicy.h"

namespace
{
constexpr uint32 SPELL_ARDENT_DEFENDER = 31850;

// Class sheet, protection paladin (BT-127): a tank spends Lay on Hands on itself only at the last threshold and only when
// Ardent Defender cannot be used (not learned before level 70, or on its 3 minute cooldown).
bool TankMayLayOnHandsOnSelf(Player* bot)
{
    if (bot->GetHealthPct() > float(TANK_LAY_ON_HANDS_HEALTH_PCT))
        return false;

    return !bot->HasSpell(SPELL_ARDENT_DEFENDER) || bot->HasSpellCooldown(SPELL_ARDENT_DEFENDER);
}
}  // namespace

// BT-129: see CastBlessingOfProtectionProtectAction in PaladinActions.h.
bool CastBlessingOfProtectionProtectAction::isUseful()
{
    Unit* target = GetTarget();
    Player* targetPlayer = target ? target->ToPlayer() : nullptr;
    if (targetPlayer && PlayerBotSpec::IsTank(targetPlayer))
        return false;

    return CastProtectSpellAction::isUseful();
}

bool CastSealSpellAction::isUseful() { return AI_VALUE2(bool, "combat", "self target"); }

Value<Unit*>* CastTurnUndeadAction::GetTargetValue() { return context->GetValue<Unit*>("cc target", getName()); }

// #163 S8: same movement-impaired check as PlayerbotAI::IsMovementImpaired on AC
// (Bot/PlayerbotAI.cpp:2109-2112); not added to PlayerbotAI.h/.cpp on this fork, so duplicated here
// and in PartyMemberSnaredTargetValue.cpp (the two are small enough that sharing isn't worth a new
// header).
static bool IsSnared(Unit* unit)
{
    // 5.4.8: Unit::IsRooted() doesn't exist on this fork; HasUnitState(UNIT_STATE_ROOT) is the
    // equivalent (root effects apply this state regardless of source).
    return unit &&
        (unit->HasAuraType(SPELL_AURA_MOD_ROOT) || unit->HasUnitState(UNIT_STATE_ROOT) || unit->GetSpeedRate(MOVE_RUN) < 1.0f);
}

Unit* CastHandOfFreedomOnPartyAction::GetTarget()
{
    bool const selfImpaired = IsSnared(bot) && !botAI->HasAura("hand of freedom", bot);

    if (!bot->GetGroup())
        return selfImpaired ? bot : nullptr;

    if (selfImpaired)
        return bot;

    return CastBuffSpellAction::GetTarget();
}

Value<Unit*>* CastHandOfFreedomOnPartyAction::GetTargetValue()
{
    return context->GetValue<Unit*>("party member snared target");
}

bool CastHandOfFreedomOnPartyAction::isUseful()
{
    Unit* target = GetTarget();
    if (!target)
        return false;

    return CastBuffSpellAction::isUseful() && !botAI->HasAura("hand of freedom", target);
}


bool CastCrusaderStrikeAction::isUseful()
{
    Unit* target = GetTarget();
    if (!target) return false;

    // Class sheet, protection paladin (BT-127): the skip below assumes a Holy Power spender. A tank has none before level
    // 40, and Crusader Strike is its main threat builder, so it is never skipped for a tank. Retribution keeps the rule
    // (BT-051).
    if (PlayerBotSpec::IsTank(bot))
        return true;

    return !botAI->HasAura("weakened blows", GetTarget(), false, true, -1, false) || bot->GetPower(Powers::POWER_HOLY_POWER) < 3;
}

// Class sheet, protection paladin (BT-127): Lay on Hands on the bot itself follows the tank rule above, through the self
// action and through the party action when the party target is the bot; other party members keep the old rule.
bool CastLayOnHandsAction::isUseful()
{
    if (PlayerBotSpec::IsTank(bot) && !TankMayLayOnHandsOnSelf(bot))
        return false;

    return CastHealingSpellAction::isUseful();
}

bool CastLayOnHandsOnPartyAction::isUseful()
{
    if (GetTarget() == bot && PlayerBotSpec::IsTank(bot) && !TankMayLayOnHandsOnSelf(bot))
        return false;

    return HealPartyMemberAction::isUseful();
}

// ---- rotation task 6 (#861) ----
#include "PlayerbotSpec.h"

// Divine Shield only when NOT carrying a PvP objective (flag/orb); shared value from task 0.
bool CastDivineShieldAction::isUseful()
{
    return CastBuffSpellAction::isUseful() && !AI_VALUE(bool, "carrying objective");
}

static bool HasHolyPowerForSpender(Player* bot)
{
    // 3 Holy Power, or the Divine Purpose proc (90174) which makes the next spender free
    return bot->GetPower(POWER_HOLY_POWER) >= 3 || bot->HasAura(90174);
}

// Prot self and holy party Eternal Flame: holy power value >= 3
bool CastEternalFlameAction::isUseful() { return HasHolyPowerForSpender(bot) && CastHealingSpellAction::isUseful(); }

bool CastEternalFlameOnPartyAction::isUseful()
{
    return HasHolyPowerForSpender(bot) && HealPartyMemberAction::isUseful();
}

// Execution Sentence on the tank when it is <= 60% health
bool CastExecutionSentenceOnPartyAction::isUseful()
{
    Unit* target = GetTarget();
    if (!target || !target->ToPlayer() || target->GetHealthPct() > 60.0f || !PlayerBotSpec::IsTank(target->ToPlayer()))
        return false;

    return HealPartyMemberAction::isUseful();
}

bool CastHandOfSacrificeOnMainTankAction::isUseful()
{
    Unit* tank = GetTarget();
    if (!tank || !TankDangerPolicy::HandOfSacrificeSafe(bot->GetHealthPct(), tank == bot))
        return false;
    // Hand of Protection on the tank already stops the physical damage the Hand would share.
    if (botAI->HasAura("hand of protection", tank))
        return false;
    return BuffOnMainTankAction::isUseful();
}

// BT-683 to BT-687: see PaladinActions.h. The decisions are in Utils/PvpHolyPaladinPolicy.h, the facts in PvP/PvpHolyPaladin.cpp.
bool PvpHolyPaladinAction::isUseful()
{
    return PvpHolyPaladinGlue::Due(botAI, bot, rule);
}

bool PvpHolyPaladinAction::Execute(Event /*event*/)
{
    return PvpHolyPaladinGlue::Cast(botAI, bot, rule);
}
