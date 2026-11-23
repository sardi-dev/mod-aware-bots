/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * BT-157: the triggers every healing class shares - upkeep gate for the tank, top-up of a member who was just hit, and
 * keeping the tank in sight.
 */

#ifndef _PLAYERBOT_HEALERTRIGGERS_H
#define _PLAYERBOT_HEALERTRIGGERS_H

#include "BotSkillMath.h"
#include "Trigger.h"

class PlayerbotAI;

// Where the shared healer edges sit in the strategies, as offsets over the shared relevance bands (Strategy.h): the proactive
// heal over time is above the "almost full" top-ups (ACTION_LIGHT_HEAL + 0 to 3) and under every heal band for a member in
// trouble (ACTION_MEDIUM_HEAL and up); walking back to the tank beats shooting and buffs but no heal.
namespace HealerRelevance
{
constexpr float PROACTIVE_HOT_OFFSET_OVER_LIGHT_HEAL = 6.0f;
constexpr float REACH_TANK_OFFSET_OVER_NORMAL = 5.0f;
}  // namespace HealerRelevance

namespace HealerUpkeep
{
// May this healer spend mana on a shield or heal over time for the main tank right now? Needs more than 30 percent mana
// (the guardrail). In a fight - the healer's or the tank's - that is all. Before a fight only a pull that is about to
// start counts ("healer pre pull"), and only for a bot whose skill tier uses the line: `prePullTier` is core (CASUAL) for
// the first spell of the preparation, GOOD for the second and EXPERT for the third.
bool TankUpkeepAllowed(PlayerbotAI* botAI, BotSkillMath::Tier prePullTier);
}  // namespace HealerUpkeep

// A damage over time heal on the member "healer proactive target" names (the heal target who is hurt a little and may be
// topped up under the mana guardrails). One trigger per class spell: "renew proactive", "rejuvenation proactive", ...
class HealerProactiveHotTrigger : public Trigger
{
public:
    HealerProactiveHotTrigger(PlayerbotAI* botAI, std::string const spell)
        : Trigger(botAI, spell + " proactive"), spell(spell)
    {
    }

    bool IsActive() override;
    Value<Unit*>* GetTargetValue() override;

private:
    std::string const spell;
};

// The healer lost sight of the main tank or stands beyond its heal range from him: it walks back (BT-155 path walk).
class HealerTankOutOfSightTrigger : public Trigger
{
public:
    HealerTankOutOfSightTrigger(PlayerbotAI* botAI) : Trigger(botAI, "healer tank out of sight") {}

    bool IsActive() override;
};

#endif
