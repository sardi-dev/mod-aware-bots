/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "DpsSheetStrategy.h"

#include "BotSkill.h"
#include "DpsSheetPolicy.h"
#include "GenericSpellActions.h"
#include "Playerbots.h"

float DpsSheetMultiplier::GetValue(Action* action)
{
    Player* bot = botAI->GetBot();
    uint32 const spec = BotGetSpecialization(bot);
    if (!DpsSheetPolicy::IsDamageSpec(spec))
        return 1.0f;
    CastSpellAction* cast = dynamic_cast<CastSpellAction*>(action);
    // Heals, dispels, resurrections and protections stay with every tier: the sheets gate damage play, not the
    // group's safety.
    if (!cast || dynamic_cast<CastHealingSpellAction*>(action) || dynamic_cast<CastCureSpellAction*>(action) ||
        dynamic_cast<ResurrectPartyMemberAction*>(action) || dynamic_cast<CastProtectSpellAction*>(action))
        return 1.0f;
    return DpsSheetPolicy::Allows(BotSkill::TierOf(bot), spec, cast->getSpell()) ? 1.0f : 0.0f;
}

void DpsSheetStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new DpsSheetMultiplier(botAI));
}
