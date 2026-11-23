/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * BT-157: actions every healing class shares.
 */

#ifndef _PLAYERBOT_HEALERACTIONS_H
#define _PLAYERBOT_HEALERACTIONS_H

#include "GenericSpellActions.h"
#include "MovementActions.h"

class PlayerbotAI;

// Walks to the main tank when the healer lost sight of him or stands too far from him, by path (the walk BT-155 gave the
// reach of a hidden hurt member, which also turns corners).
class HealerReachTankAction : public MovementAction
{
public:
    HealerReachTankAction(PlayerbotAI* botAI) : MovementAction(botAI, "reach main tank to heal") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class MarkHealerMoveIntoSightAction : public MovementAction
{
public:
    MarkHealerMoveIntoSightAction(PlayerbotAI* botAI) : MovementAction(botAI, "mark healer move into sight") {}
    bool Execute(Event event) override;
};

// A damage over time heal on the "healer proactive target" (a plain buff cast, so the shared save-mana multiplier, which
// holds back top-ups on members above 65 percent health, does not veto it: the guardrails live in the value).
class CastHealerProactiveHotAction : public CastBuffSpellAction
{
public:
    CastHealerProactiveHotAction(PlayerbotAI* botAI, std::string const spell)
        : CastBuffSpellAction(botAI, spell, true), actionName(spell + " proactive")
    {
        range = botAI->GetRange("heal");
    }

    Value<Unit*>* GetTargetValue() override;
    std::string const getName() override { return actionName; }

private:
    std::string const actionName;
};

#endif
