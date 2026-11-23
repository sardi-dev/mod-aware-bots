/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_OSACTIONS_H
#define PLAYERBOTS_OSACTIONS_H

#include <utility>

#include "AttackActions.h"  // 5.4.8: AC "AttackAction.h" -> strategy/actions/AttackActions.h
#include "MovementActions.h"

// AC Ai/Raid/OS/OSActions.h. AC's tsunami safe lines (TSUNAMI_LEFT_SAFE_MELEE and the others) are left out: they sit
// between AC's waves, and the 5.4.8 script runs its waves in other lanes (see OsTsunamiLogic.h).
std::pair<float, float> const SARTHARION_MAINTANK_POSITION = {3258.5f, 532.5f};
std::pair<float, float> const SARTHARION_OFFTANK_POSITION = {3230.0f, 526.0f};

// AC Ai/Raid/OS/OSActions.h class SartharionTankPositionAction
class SartharionTankPositionAction : public AttackAction
{
public:
    SartharionTankPositionAction(PlayerbotAI* botAI, std::string const name = "sartharion tank position")
        : AttackAction(botAI, name) {}
    bool Execute(Event event) override;
};

// AC Ai/Raid/OS/OSActions.h class AvoidTwilightFissureAction
class AvoidTwilightFissureAction : public MovementAction
{
public:
    AvoidTwilightFissureAction(PlayerbotAI* botAI, std::string const name = "avoid twilight fissure")
        : MovementAction(botAI, name) {}
    bool Execute(Event event) override;
};

// AC Ai/Raid/OS/OSActions.h class AvoidFlameTsunamiAction
class AvoidFlameTsunamiAction : public MovementAction
{
public:
    AvoidFlameTsunamiAction(PlayerbotAI* botAI, std::string const name = "avoid flame tsunami")
        : MovementAction(botAI, name) {}
    bool Execute(Event event) override;
};

// AC Ai/Raid/OS/OSActions.h class SartharionAttackPriorityAction
class SartharionAttackPriorityAction : public AttackAction
{
public:
    SartharionAttackPriorityAction(PlayerbotAI* botAI, std::string const name = "sartharion attack priority")
        : AttackAction(botAI, name) {}
    bool Execute(Event event) override;
};

// AC Ai/Raid/OS/OSActions.h class EnterTwilightPortalAction
class EnterTwilightPortalAction : public MovementAction
{
public:
    EnterTwilightPortalAction(PlayerbotAI* botAI, std::string const name = "enter twilight portal")
        : MovementAction(botAI, name) {}
    bool Execute(Event event) override;
};

#endif
