/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_PVPDEFENSEACTIONS_H
#define _PLAYERBOT_PVPDEFENSEACTIONS_H

// BT-522: the triggers and actions of rule R2 (PvpDefense.h) for a bot that fights in a battleground or an arena in a group:
// a defensive when the rules say so, and a way out of control. The 1v1 engine does the same inside a duel or a lone fight,
// and the arena plan picks its own moment (the ladder teams); both ask the same rules.

#include "Action.h"
#include "Trigger.h"

class PlayerbotAI;

/// A defensive is due now (after the tier's reaction time).
class PvpDefensiveDueTrigger : public Trigger
{
public:
    PvpDefensiveDueTrigger(PlayerbotAI* botAI) : Trigger(botAI, "pvp defensive due", 1) {}

    bool IsActive() override;

private:
    std::uint32_t _wantSince = 0;
};

/// The bot is held by control and the rules say to break it now (after the tier's reaction time).
class PvpBreakDueTrigger : public Trigger
{
public:
    PvpBreakDueTrigger(PlayerbotAI* botAI) : Trigger(botAI, "pvp break due", 1) {}

    bool IsActive() override;

private:
    std::uint32_t _heldSince = 0;
};

class PvpDefensiveAction : public Action
{
public:
    PvpDefensiveAction(PlayerbotAI* botAI) : Action(botAI, "pvp defensive") {}

    bool Execute(Event event) override;
};

class PvpBreakAction : public Action
{
public:
    PvpBreakAction(PlayerbotAI* botAI) : Action(botAI, "pvp break") {}

    bool Execute(Event event) override;
};

#endif
