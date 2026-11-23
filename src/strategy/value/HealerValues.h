/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * BT-157: what a proactive healer needs to know and the existing values do not say. Each value is cached for a fraction
 * of a second, so the group is read a few times per second at most and not on every tick.
 */

#ifndef _PLAYERBOT_HEALERVALUES_H
#define _PLAYERBOT_HEALERVALUES_H

#include <ctime>
#include <unordered_map>
#include "HealerGuardrails.h"
#include "Value.h"

class PlayerbotAI;

// "healer pre pull": the tank is about to pull (the leader's mark or the tank's own target is a hostile that is not
// fighting yet, near the tank) and the preparation of this target has not run for longer than the pre-pull window.
class HealerPrePullValue : public BoolCalculatedValue
{
public:
    HealerPrePullValue(PlayerbotAI* botAI) : BoolCalculatedValue(botAI, "healer pre pull", PRE_PULL_CHECK_MS) {}

protected:
    bool Calculate() override;

private:
    static constexpr int32 PRE_PULL_CHECK_MS = 500;

    HealerGuardrails::PrePullWindow window;
};

// "healer proactive target": the member a healer in a fight may top up before he drops under the heal bands - the heal
// target when it is not the main tank (the tank has his own upkeep), is hurt a little, and the mana guardrails and the
// skill tier allow it. Null otherwise.
class HealerProactiveTargetValue : public UnitCalculatedValue
{
public:
    HealerProactiveTargetValue(PlayerbotAI* botAI) : UnitCalculatedValue(botAI, "healer proactive target", PROACTIVE_CHECK_MS) {}

protected:
    Unit* Calculate() override;

private:
    static constexpr int32 PROACTIVE_CHECK_MS = 500;

    HealerGuardrails::DamageWindow damage;
};

// "healer tank to reach": the main tank when the healer has lost sight of him or stands farther from him than its heal
// range; null while the tank is in sight and in range, or there is no tank to follow. BT-175: with the tank in order, a
// member (the leader included) who stayed under 60 percent for 5 s out of the healer's sight or heal range is the target
// instead, and a healer with a hurt member it cannot heal says why in the log.
class HealerTankToReachValue : public UnitCalculatedValue
{
public:
    HealerTankToReachValue(PlayerbotAI* botAI) : UnitCalculatedValue(botAI, "healer tank to reach", TANK_REACH_CHECK_MS) {}

protected:
    Unit* Calculate() override;

private:
    static constexpr int32 TANK_REACH_CHECK_MS = 2000;

    // BT-175: since when each member of the group is under UNHEALED_HEALTH_PCT, and the last [HEAL] line.
    std::unordered_map<uint64, time_t> hurtSince;
    time_t lastHealLog = 0;
};

#endif
