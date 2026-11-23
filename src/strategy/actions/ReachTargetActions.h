/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#ifndef _PLAYERBOT_REACHTARGETACTIONS_H
#define _PLAYERBOT_REACHTARGETACTIONS_H

#include "GenericSpellActions.h"
#include "MovementActions.h"

class PlayerbotAI;

class ReachTargetAction : public MovementAction
{
public:
    ReachTargetAction(PlayerbotAI* botAI, std::string const name, float distance)
        : MovementAction(botAI, name), distance(distance)
    {
    }

    bool Execute(Event event) override;
    bool isUseful() override;
    std::string const GetTargetName() override;

protected:
    float distance;
};

class CastReachTargetSpellAction : public CastSpellAction
{
public:
    CastReachTargetSpellAction(PlayerbotAI* botAI, std::string const spell, float distance);

    bool isUseful() override;

protected:
    float distance;
};

class ReachMeleeAction : public ReachTargetAction
{
public:
    ReachMeleeAction(PlayerbotAI* botAI) : ReachTargetAction(botAI, "reach melee", sPlayerbotAIConfig->meleeDistance) {}
};

class ReachSpellAction : public ReachTargetAction
{
public:
    ReachSpellAction(PlayerbotAI* botAI);
};

// BT-165: before the pull the controller walks to cast range of its marked target (ReachCombatTo, the shared reach),
// so the control spell can be cast on a pack the group has not engaged yet.
class ReachCcDutyTargetAction : public ReachTargetAction
{
public:
    ReachCcDutyTargetAction(PlayerbotAI* botAI) : ReachTargetAction(botAI, "reach cc duty target", 0.0f) {}

    std::string const GetTargetName() override { return "cc duty target"; }
    bool Execute(Event event) override;
    bool isUseful() override;
};

class ReachPartyMemberToHealAction : public ReachTargetAction
{
public:
    ReachPartyMemberToHealAction(PlayerbotAI* botAI);

    std::string const GetTargetName() override;
    bool Execute(Event event) override;
    bool isUseful() override;

    // BT-117: the hurt group member with the lowest health that "party member to heal" cannot return because it is
    // out of sight or too far (that value wants line of sight and twice the heal distance). The healer walks to it.
    static Unit* FindHurtMemberOutOfSight(PlayerbotAI* botAI);
    static Unit* SelectHealReachTarget(PlayerbotAI* botAI, Unit* visible);
};

class ReachPartyMemberToResurrectAction : public ReachTargetAction
{
public:
    ReachPartyMemberToResurrectAction(PlayerbotAI* botAI);

    std::string const GetTargetName() override;
    // BT-157: out of combat the bot walks by path to the dead member's body (a released player's corpse, the player himself
    // otherwise); in combat it keeps the shared reach.
    bool Execute(Event event) override;
    bool isUseful() override;

private:
    // The place to walk to for a dead member: the corpse of a released player on this map, else the member.
    WorldObject* ResurrectionGoal(Unit* dead) const;
};

#endif
