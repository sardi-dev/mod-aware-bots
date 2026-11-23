/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_VOAACTIONS_H
#define PLAYERBOTS_VOAACTIONS_H

#include "Action.h"
#include "MovementActions.h"

class Unit;

// AC Ai/Raid/VoA/VoAActions.h class EmalonMarkBossAction
class EmalonMarkBossAction : public MovementAction
{
public:
    EmalonMarkBossAction(PlayerbotAI* botAI) : MovementAction(botAI, "emalon mark boss action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

// AC Ai/Raid/VoA/VoAActions.h class EmalonLightingNovaAction
class EmalonLightingNovaAction : public MovementAction
{
public:
    EmalonLightingNovaAction(PlayerbotAI* botAI) : MovementAction(botAI, "emalon lighting nova action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

// AC Ai/Raid/VoA/VoAActions.h class EmalonOverchargeAction
class EmalonOverchargeAction : public Action
{
public:
    EmalonOverchargeAction(PlayerbotAI* botAI) : Action(botAI, "emalon overcharge action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

namespace VoAHelpers
{
    // AC's marking rule shared by the two mark actions: with a real player as main tank the first of up to three bot
    // assist tanks marks, else the main tank bot does. True when the skull moved.
    bool MarkWithSkullAsTank(Player* bot, Unit* mainTank, Unit* target);
}

#endif
