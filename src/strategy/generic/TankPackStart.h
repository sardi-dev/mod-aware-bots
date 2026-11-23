/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_TANKPACKSTART_H
#define _PLAYERBOT_TANKPACKSTART_H

#include "Action.h"
#include "Strategy.h"
#include "Trigger.h"

#include <string>
#include <vector>

class PlayerbotAI;

// Rules: TankPackStartPolicy.h. Fires in the first seconds of a fight once three mobs are on the tank.
class TankPackStartTrigger : public Trigger
{
public:
    TankPackStartTrigger(PlayerbotAI* botAI) : Trigger(botAI, "tank pack start", 1) {}

    bool IsActive() override;

private:
    uint32 combatSinceMs = 0;
};

// Casts the first ready pack-start cooldown of the tank's class through its own class action.
class TankPackStartAction : public Action
{
public:
    TankPackStartAction(PlayerbotAI* botAI) : Action(botAI, "tank pack start") {}

    bool Execute(Event event) override;
    bool isUseful() override;

    static std::vector<std::string> const& Cooldowns(uint8 playerClass);
};

// Added beside "tank danger" for good and expert tanks.
class TankPackStartStrategy : public Strategy
{
public:
    TankPackStartStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    std::string const getName() override { return "tank pack start"; }
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
};

#endif
