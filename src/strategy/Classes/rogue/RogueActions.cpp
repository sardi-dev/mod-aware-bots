/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#include "RogueActions.h"
#include "BotSkill.h"
#include "DpsRotationMath.h"
#include "RogueTriggers.h"
#include "RogueFinishingActions.h"

#include "Event.h"
#include "BotGuid.h"
#include "Player.h"
#include "Playerbots.h"

// #742 (AC Ai/Class/Rogue/Action/RogueActions.cpp:21-27): no Stealth when the current target is spell distance or more away.
bool CastStealthAction::isUseful()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    if (target && bot->GetDistance(target) >= sPlayerbotAIConfig->spellDistance)
        return false;
    return true;
}

bool CastStealthAction::isPossible()
{
    // (Stealth, prio group 5): "NOT carrying a PvP objective".
    // do not use with WSG flag or EYE flag
    return !botAI->HasAura(23333, bot) && !botAI->HasAura(23335, bot) && !botAI->HasAura(34976, bot) &&
           !AI_VALUE(bool, "carrying objective");
}

bool UnstealthAction::Execute(Event event)
{
    botAI->RemoveAura("stealth");
    // botAI->ChangeStrategy("+dps,-stealthed", BOT_STATE_COMBAT);

    return true;
}

bool CheckStealthAction::Execute(Event event)
{
    // Restore the chosen spec after the opener, including the separate Subtlety strategy
    // (BT-059 and rogue-subtlety sheet).
    std::string const spec =
        BotGetSpecialization(bot) == Specializations::SPEC_ROGUE_COMBAT ? "combat" :
        (BotGetSpecialization(bot) == Specializations::SPEC_ROGUE_SUBTLETY ? "subtlety" : "assassin");
    if (botAI->HasAura("stealth", bot))
        botAI->ChangeStrategy("-" + spec + ",+stealthed", BOT_STATE_COMBAT);
    else
        botAI->ChangeStrategy("+" + spec + ",-stealthed", BOT_STATE_COMBAT);

    return true;
}

bool CastVanishAction::isUseful()
{
    // do not use with WSG flag or EYE flag; MoP rule: no stealth-class spell while carrying a PvP objective.
    return !botAI->HasAura(23333, bot) && !botAI->HasAura(23335, bot) && !botAI->HasAura(34976, bot) &&
           !AI_VALUE(bool, "carrying objective");
}

// PvP stun guard (kidney shot): only stun a target that is not already CC'd, BG/arena only.
bool CastKidneyShotAction::isUseful()
{
    return CastMeleeSpellAction::isUseful() &&
           (!AI_VALUE(bool, "in pvp instance") || !AI_VALUE2(bool, "is cc", "current target"));
}

// Smoke Bomb at <= 20% health. BG/arena only so the open world stays AC.
bool CastSmokeBombAction::isUseful()
{
    return CastBuffSpellAction::isUseful() && AI_VALUE(bool, "in pvp instance");
}

// Kick any casting enemy (target or random valid enemy); BG/arena only.
bool CastKickOnEnemyCasterAction::isUseful()
{
    return (AI_VALUE(bool, "in pvp instance") || MopDungeon::GroupPveInterrupt(bot, GetTarget())) && CastSpellOnEnemyCasterAction::isUseful();
}

bool CastTricksOfTheTradeOnMainTankAction::isUseful()
{
    return CastSpellAction::isUseful() && AI_VALUE2(float, "distance", GetTargetName()) < 20.0f;
}

bool CancelBladeFlurryAction::Execute(Event event)
{
    botAI->RemoveAura("blade flurry");
    return true;
}

// Never turn the party's current target into a Gouge target.
bool RoguePeelAction::isUseful()
{
    Unit* target = GetTarget();
    return BotSkill::Uses(bot, BotSkillMath::Tier::EXPERT) && target && target != AI_VALUE(Unit*, "current target") &&
           target->GetVictim() == bot && CastDebuffSpellOnMeleeAttackerAction::isUseful();
}

// Avoid spending Killing Spree during competing burst buffs (rogue-combat sheet).
bool CastKillingSpreeAction::isUseful()
{
    // Combat sheet level 80 line: a good or expert bot also waits for low energy.
    return DpsRotationMath::Rogue::KillingSpreeNow(BotSkill::Uses(bot, BotSkillMath::Tier::GOOD),
               bot->GetPower(POWER_ENERGY), botAI->HasAura("adrenaline rush", bot),
               botAI->HasAura("shadow blades", bot)) &&
           CastMeleeSpellAction::isUseful();
}

bool CastShadowBladesAction::isUseful()
{
    if (BotGetSpecialization(bot) == SPEC_ROGUE_SUBTLETY &&
        !DpsRotationMath::Rogue::SubtletyShadowBladesNow(BotSkill::Uses(bot, BotSkillMath::Tier::GOOD),
            RogueFindWeaknessUp(botAI)))
        return false;
    return CastBuffSpellAction::isUseful();
}

// BT-691 to BT-693: see RogueActions.h. The decisions are in Utils/PvpSubtletyPolicy.h, the facts in PvP/PvpSubtlety.cpp.
bool PvpSubtletyAction::isUseful()
{
    return PvpSubtletyGlue::Due(botAI, bot, rule);
}

bool PvpSubtletyAction::Execute(Event /*event*/)
{
    return PvpSubtletyGlue::Cast(botAI, bot, rule);
}
