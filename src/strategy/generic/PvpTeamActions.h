/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_PVPTEAMACTIONS_H
#define _PLAYERBOT_PVPTEAMACTIONS_H

// BT-629: the trigger and the action of the team control (PvP/PvpTeam.h): the bot is the executor of the best control on the
// enemy healer now, and casts it.

#include "Action.h"
#include "Multiplier.h"
#include "Trigger.h"

class PlayerbotAI;

/// This bot is the one that casts the next control link on the enemy healer.
class PvpControlDueTrigger : public Trigger
{
public:
    PvpControlDueTrigger(PlayerbotAI* botAI) : Trigger(botAI, "pvp control due", 1) {}

    bool IsActive() override;
};

class PvpControlAction : public Action
{
public:
    PvpControlAction(PlayerbotAI* botAI) : Action(botAI, "pvp control") {}

    bool Execute(Event event) override;
};

/// BT-630: no damage cast at a target that a teammate holds in a breakable control (sheep, fear, blind ...).
class PvpTeamMultiplier : public Multiplier
{
public:
    PvpTeamMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "pvp team") {}

    float GetValue(Action* action) override;
};

#endif
