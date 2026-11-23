/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_REJOINGROUP_H
#define _PLAYERBOT_REJOINGROUP_H

#include "MovementActions.h"
#include "Strategy.h"
#include "Trigger.h"

// BT-202 (policy: GroupRejoinPolicy.h): a bot left behind with no member in range, a healer included, walks to the tank
// by the full path and, when that fails twice, drops a stale combat state and follows. Owner decision 2026-11-15; never
// a teleport.
class FarFromGroupInCombatTrigger : public Trigger
{
public:
    FarFromGroupInCombatTrigger(PlayerbotAI* botAI) : Trigger(botAI, "far from group in combat", 2000) {}

    bool IsActive() override;
};

class RejoinGroupAction : public MovementAction
{
public:
    RejoinGroupAction(PlayerbotAI* botAI) : MovementAction(botAI, "rejoin group") {}

    bool Execute(Event event) override;
    bool isUseful() override;

private:
    // Drops the target and the in-combat flag when the flag is stale (GroupRejoinPolicy::FlagIsStale); false when not.
    bool DropStaleCombat(time_t now);
};

// Carries the trigger in both engines: the bot may be flagged in combat while its combat or its non-combat engine runs.
class RejoinGroupStrategy : public Strategy
{
public:
    RejoinGroupStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    std::string const getName() override { return "rejoin group"; }
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
};

#endif
