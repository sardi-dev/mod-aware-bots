/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_TANKDANGER_H
#define _PLAYERBOT_TANKDANGER_H

#include "Action.h"
#include "Strategy.h"
#include "TankDangerPolicy.h"
#include "Trigger.h"
#include "Value.h"

#include <string>
#include <vector>

class Player;
class PlayerbotAI;

// BT-292 (policy: TankDangerPolicy.h): "tank danger" is the tank's danger level, 0 to 3, read in a fight from the mobs on
// it, the health it lost in the last five seconds, whether a healer can reach it and whether a boss hits it. Each reading
// is also written to PlayerbotAI::tankDanger, so the group's healers can read it without computing it again.
class TankDangerValue : public Uint8CalculatedValue
{
public:
    TankDangerValue(PlayerbotAI* botAI) : Uint8CalculatedValue(botAI, "tank danger", DANGER_CHECK_MS) {}

protected:
    uint8 Calculate() override;

private:
    static constexpr uint32 DANGER_CHECK_MS = 500;

    TankDangerPolicy::HealthDropWindow drop;
};

namespace TankDanger
{
// The level a healer may use for `tank`: the bot tank's last reading while it is fresh, 0 for a real player tank or a
// stale reading.
unsigned LevelFor(Player* tank);
}

class TankDangerDefensiveTrigger : public Trigger
{
public:
    TankDangerDefensiveTrigger(PlayerbotAI* botAI, bool big)
        : Trigger(botAI, big ? "tank danger big defensive" : "tank danger small defensive", 2), big(big)
    {
    }

    bool IsActive() override;

private:
    bool const big;
};

// Casts the first of the class's small or big tank defensives that is ready (each through its own action, so the spell,
// the cooldown and the existing aura checks stay where they are).
class TankDangerDefensiveAction : public Action
{
public:
    TankDangerDefensiveAction(PlayerbotAI* botAI, bool big)
        : Action(botAI, big ? "tank danger big defensive" : "tank danger small defensive"), big(big)
    {
    }

    bool Execute(Event event) override;
    bool isUseful() override;

    // The class's defensives of one size, strongest first; empty for a class with no tank specialisation.
    static std::vector<std::string> const& Defensives(uint8 playerClass, bool big);

private:
    bool const big;
    uint32 lastBigMs = 0;
    time_t lastLog = 0;
};

// Added to every tank of the good and expert tiers (role sheet tank.md: the danger rule is a "good" line).
class TankDangerStrategy : public Strategy
{
public:
    TankDangerStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    std::string const getName() override { return "tank danger"; }
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
};

// BT-293: a healer's external on the main tank (Pain Suppression, Guardian Spirit, Ironbark, Life Cocoon, Hand of
// Sacrifice) fires earlier when the tank's danger reading says so (TankDangerPolicy::ExternalOnTank).
class MainTankInDangerTrigger : public Trigger
{
public:
    MainTankInDangerTrigger(PlayerbotAI* botAI) : Trigger(botAI, "main tank in danger", 2) {}

    bool IsActive() override;
};

#endif
