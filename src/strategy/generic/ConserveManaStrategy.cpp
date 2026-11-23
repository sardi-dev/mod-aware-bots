/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#include "ConserveManaStrategy.h"

#include "GenericSpellActions.h"
#include "HealerGuardrails.h"
#include "LastSpellCastValue.h"
#include "PlayerbotAIConfig.h"
#include "PlayerbotSpec.h"
#include "Playerbots.h"

float HealerAutoSaveManaMultiplier::GetValue(Action* action)
{
    uint8 mana = BotGetPowerPct(bot, Powers::POWER_MANA);
    if (mana > sPlayerbotAIConfig->saveManaThreshold)
        return 1.0f;
    CastHealingSpellAction* healingAction = dynamic_cast<CastHealingSpellAction*>(action);

    if (!healingAction)
        return 1.0f;

    Unit* target = healingAction->GetTarget();
    if (!target)
        return 1.0f;
    bool isTank = target->ToPlayer() ? PlayerBotSpec::IsTank(target->ToPlayer()) : false;
    uint8 health = target->GetHealthPct();
    // From the author's earlier private bot module: a target about to die is healed with whatever is castable; saving
    // mana is for topping up.
    if (health < sPlayerbotAIConfig->criticalHealth)
        return 1.0f;

    HealingManaEfficiency manaEfficiency = healingAction->manaEfficiency;
    uint8 estAmount = healingAction->estAmount;
    uint8 lossAmount = 100 - health;
    if (isTank)
    {
        estAmount /= 1.5;  // tanks have more health
        if (health >= sPlayerbotAIConfig->mediumHealth &&
            (lossAmount < estAmount || manaEfficiency <= HealingManaEfficiency::MEDIUM))
            return 0.0f;
        if (health >= sPlayerbotAIConfig->lowHealth &&
            (lossAmount < estAmount || manaEfficiency <= HealingManaEfficiency::LOW))
            return 0.0f;
    }
    else
    {
        if (health >= sPlayerbotAIConfig->mediumHealth &&
            (lossAmount < estAmount || manaEfficiency <= HealingManaEfficiency::MEDIUM))
            return 0.0f;
        if (lossAmount < estAmount || manaEfficiency <= HealingManaEfficiency::LOW)
            return 0.0f;
    }
    return 1.0f;
}

void HealerAutoSaveManaStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new HealerAutoSaveManaMultiplier(botAI));
}

float HealerTriageMultiplier::GetValue(Action* action)
{
    // Only the two dispel shapes; everything else is untouched.
    if (!dynamic_cast<CurePartyMemberAction*>(action) && !dynamic_cast<CastCureSpellAction*>(action))
        return 1.0f;

    Unit* lowest = AI_VALUE(Unit*, "party member to heal");
    if (!lowest || !lowest->IsAlive())
        return 1.0f;

    Unit* const cureTarget = action->GetTarget();
    Unit* const tank = AI_VALUE(Unit*, "main tank");
    bool const healableNow = lowest->GetMapId() == bot->GetMapId() &&
        bot->IsWithinDistInMap(lowest, sPlayerbotAIConfig->healDistance) && bot->IsWithinLOSInMap(lowest);
    return HealerGuardrails::DispelWaits(lowest->GetHealthPct(), healableNow, cureTarget && cureTarget == tank) ? 0.0f
                                                                                                                 : 1.0f;
}

void HealerTriageStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new HealerTriageMultiplier(botAI));
}
