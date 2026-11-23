/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "HealerTriggers.h"

#include "BotSkill.h"
#include "HealerGuardrails.h"
#include "PlayerbotSpec.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"

bool HealerUpkeep::TankUpkeepAllowed(PlayerbotAI* botAI, BotSkillMath::Tier prePullTier)
{
    Player* bot = botAI->GetBot();
    AiObjectContext* context = botAI->GetAiObjectContext();

    if (!HealerGuardrails::TankUpkeepAllowed(uint32(BotGetPowerPct(bot, POWER_MANA))))
        return false;

    if (bot->IsInCombat())
        return true;

    Unit* tank = AI_VALUE(Unit*, "main tank");
    if (tank && tank->IsInCombat())
        return true;

    return BotSkill::Uses(bot, prePullTier) && AI_VALUE(bool, "healer pre pull");
}

bool HealerProactiveHotTrigger::IsActive()
{
    uint32 const spellId = AI_VALUE2(uint32, "spell id", spell);
    if (!spellId || !bot->HasSpell(spellId) || bot->HasSpellCooldown(spellId))
        return false;

    Unit* target = GetTarget();
    return target && !botAI->GetAura(spell, target, true, false);
}

Value<Unit*>* HealerProactiveHotTrigger::GetTargetValue() { return context->GetValue<Unit*>("healer proactive target"); }

bool HealerTankOutOfSightTrigger::IsActive()
{
    // A sitting bot is drinking: the group waits for it, so it stays where it is.
    if (!bot->IsAlive() || bot->getStandState() != UNIT_STAND_STATE_STAND || !PlayerBotSpec::IsHeal(bot))
        return false;

    return AI_VALUE(Unit*, "healer tank to reach") != nullptr;
}
