/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef PLAYERBOTS_SUNWELLACTIONS_H
#define PLAYERBOTS_SUNWELLACTIONS_H

#include "Action.h"
#include "AttackActions.h"
#include "InterruptGate.h"
#include "MovementActions.h"
#include "RaidGeometry.h"
#include <vector>

// Steps to the nearest point outside every circle (hazards, or the spread circles of the raid members).
class SunwellMoveOutOfCirclesAction : public MovementAction
{
public:
    SunwellMoveOutOfCirclesAction(PlayerbotAI* botAI, std::string const name) : MovementAction(botAI, name) {}

protected:
    bool MoveOutOf(std::vector<RaidGeometry::Circle> const& circles, float maxDistance, MovementPriority priority);
    bool MoveToPoint(RaidGeometry::Point point, MovementPriority priority);
};

// Brutallus: soakers to their spot in front of him, everyone else out of the Meteor Slash cone.
class SunwellMeteorSlashPositionAction : public SunwellMoveOutOfCirclesAction
{
public:
    SunwellMeteorSlashPositionAction(PlayerbotAI* botAI)
        : SunwellMoveOutOfCirclesAction(botAI, "sunwell meteor slash position") {}
    bool Execute(Event event) override;
};

// Brutallus: the second tank steps in front of him and taunts once the tank he hits carries four stacks.
class SunwellBrutallusTakeOverAction : public SunwellMoveOutOfCirclesAction
{
public:
    SunwellBrutallusTakeOverAction(PlayerbotAI* botAI)
        : SunwellMoveOutOfCirclesAction(botAI, "sunwell brutallus take over") {}
    bool Execute(Event event) override;
};

// Felmyst's Demonic Vapor or Kil'jaeden's Volatile Felfire Fiend chases the bot: lead it away from the raid.
class SunwellKiteChaserAction : public SunwellMoveOutOfCirclesAction
{
public:
    SunwellKiteChaserAction(PlayerbotAI* botAI) : SunwellMoveOutOfCirclesAction(botAI, "sunwell kite chaser") {}
    bool Execute(Event event) override;
};

// Felmyst in the air: gather where she took off.
class SunwellFelmystRegroupAction : public SunwellMoveOutOfCirclesAction
{
public:
    SunwellFelmystRegroupAction(PlayerbotAI* botAI) : SunwellMoveOutOfCirclesAction(botAI, "sunwell felmyst regroup") {}
    bool Execute(Event event) override;
};

class SunwellLeaveHazardAction : public SunwellMoveOutOfCirclesAction
{
public:
    SunwellLeaveHazardAction(PlayerbotAI* botAI) : SunwellMoveOutOfCirclesAction(botAI, "sunwell leave hazard") {}
    bool Execute(Event event) override;
};

class SunwellSpreadRangedAction : public SunwellMoveOutOfCirclesAction
{
public:
    SunwellSpreadRangedAction(PlayerbotAI* botAI) : SunwellMoveOutOfCirclesAction(botAI, "sunwell spread ranged") {}
    bool Execute(Event event) override;
};

class SunwellLeaveKalecgosConeAction : public MovementAction
{
public:
    SunwellLeaveKalecgosConeAction(PlayerbotAI* botAI) : MovementAction(botAI, "sunwell leave kalecgos cone") {}
    bool Execute(Event event) override;
};

class SunwellAttackSathrovarrAction : public AttackAction
{
public:
    SunwellAttackSathrovarrAction(PlayerbotAI* botAI) : AttackAction(botAI, "sunwell attack sathrovarr") {}
    bool Execute(Event event) override;
};

class SunwellAttackPriorityAddAction : public AttackAction
{
public:
    SunwellAttackPriorityAddAction(PlayerbotAI* botAI) : AttackAction(botAI, "sunwell attack priority add") {}
    bool Execute(Event event) override;
};

class SunwellTwinsTankSplitAction : public AttackAction
{
public:
    SunwellTwinsTankSplitAction(PlayerbotAI* botAI) : AttackAction(botAI, "sunwell twins tank split") {}
    bool Execute(Event event) override;
};

class SunwellInterruptAction : public Action
{
public:
    SunwellInterruptAction(PlayerbotAI* botAI) : Action(botAI, "sunwell interrupt") {}
    bool Execute(Event event) override;

private:
    MopDungeon::InterruptGate gate;
};

class SunwellRemovePyrogenicsAction : public Action
{
public:
    SunwellRemovePyrogenicsAction(PlayerbotAI* botAI) : Action(botAI, "sunwell remove pyrogenics") {}
    bool Execute(Event event) override;
};

#endif
