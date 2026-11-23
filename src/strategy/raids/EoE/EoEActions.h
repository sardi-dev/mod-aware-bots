/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_EOEACTIONS_H
#define PLAYERBOTS_EOEACTIONS_H

#include <utility>

#include "AttackActions.h"  // 5.4.8: AC "AttackAction.h" -> strategy/actions/AttackActions.h
#include "MovementActions.h"

// AC Ai/Raid/EoE/EoEActions.h. The stack spot is the platform centre (the core's Vortex triggers stand there).
std::pair<float, float> const MALYGOS_MAINTANK_POSITION = {757.0f, 1337.0f};
std::pair<float, float> const MALYGOS_STACK_POSITION = {755.0f, 1301.0f};

// AC Ai/Raid/EoE/EoEActions.h class MalygosPositionAction
class MalygosPositionAction : public MovementAction
{
public:
    MalygosPositionAction(PlayerbotAI* botAI, std::string const name = "malygos position") : MovementAction(botAI, name)
    {
    }

    bool Execute(Event event) override;
};

// AC Ai/Raid/EoE/EoEActions.h class MalygosTargetAction
class MalygosTargetAction : public AttackAction
{
public:
    MalygosTargetAction(PlayerbotAI* botAI, std::string const name = "malygos target") : AttackAction(botAI, name) {}

    bool Execute(Event event) override;
};

// AC Ai/Raid/EoE/EoEActions.h class EoEFlyDrakeAction
class EoEFlyDrakeAction : public MovementAction
{
public:
    EoEFlyDrakeAction(PlayerbotAI* botAI) : MovementAction(botAI, "eoe fly drake") {}

    bool Execute(Event event) override;
    bool isPossible() override;
};

// AC Ai/Raid/EoE/EoEActions.h class EoEDrakeAttackAction
class EoEDrakeAttackAction : public Action
{
public:
    EoEDrakeAttackAction(PlayerbotAI* botAI) : Action(botAI, "eoe drake attack") {}

    bool Execute(Event event) override;
    bool isPossible() override;

protected:
    Unit* vehicleBase = nullptr;
    bool CastDrakeSpellAction(Unit* target, uint32 spellId);
    bool DrakeDpsAction(Unit* target);
    bool DrakeHealAction();
    // 5.4.8: the drake's combo points belong to the rider (Spell::EffectAddComboPoints gives them to m_movedPlayer)
    uint8 RiderComboPointsOn(Unit* target) const;
};

#endif
