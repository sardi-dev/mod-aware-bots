/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_ARENAPLANACTIONS_H
#define _PLAYERBOT_ARENAPLANACTIONS_H

// BT-154: the triggers and actions that put the arena plan (ArenaPlan.h) into the strategy engine. They are added to the
// "arena" strategy (BattlegroundStrategy.cpp), so only a bot in an arena runs them, and each does nothing unless the bot
// is a member of a ladder team in a running match.

#include "MovementActions.h"
#include "Trigger.h"

class PlayerbotAI;

/// The shared focus differs from what this bot is attacking: attack it (the target comes from "enemy player target").
class ArenaFocusOffTrigger : public Trigger
{
public:
    ArenaFocusOffTrigger(PlayerbotAI* botAI) : Trigger(botAI, "arena focus off", 1) {}

    bool IsActive() override;
};

/// The tier's rule says a big self defensive is due.
class ArenaDefensiveDueTrigger : public Trigger
{
public:
    ArenaDefensiveDueTrigger(PlayerbotAI* botAI) : Trigger(botAI, "arena defensive due", 1) {}

    bool IsActive() override;
};

/// The bot should break the line of sight of the enemy casters that target it.
class ArenaCoverDueTrigger : public Trigger
{
public:
    ArenaCoverDueTrigger(PlayerbotAI* botAI) : Trigger(botAI, "arena cover due", 1) {}

    bool IsActive() override;
};

class ArenaDefensiveAction : public Action
{
public:
    ArenaDefensiveAction(PlayerbotAI* botAI) : Action(botAI, "arena defensive") {}

    bool Execute(Event event) override;
};

class ArenaCoverAction : public MovementAction
{
public:
    ArenaCoverAction(PlayerbotAI* botAI) : MovementAction(botAI, "arena cover") {}

    bool Execute(Event event) override;
};

#endif
