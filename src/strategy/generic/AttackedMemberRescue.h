/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_ATTACKEDMEMBERRESCUE_H
#define _PLAYERBOT_ATTACKEDMEMBERRESCUE_H

#include "AttackActions.h"
#include "Trigger.h"

#include <ctime>

class Player;
class Unit;

// BT-162: select by victim urgency, caster status and distance.
namespace AttackedMemberRescue
{
    Unit* FindAttacker(Player* tank);
    // BT-190: for a damage dealer or healer in a dungeon: a creature attacking another member that `bot` cannot see,
    // while no attacker of a member is in its sight (nullptr otherwise); `victim` gets the member.
    Unit* FindHiddenAttacker(Player* bot, Player*& victim);
    // The living tank of `bot`'s group on its map, other than `bot`.
    Player* FindTank(Player* bot);
    // BT-377 (FloorAttackerPolicy.h): looks for a spot 12 to 16 yd from the bot, on its floor and away from `attacker`
    // (a mob that hits a member from another floor), and orders the walk there (PlayerbotAI::floorStep). False when the
    // limits forbid a step now or no spot qualifies. One search per call: call it at a give-up, never per tick.
    bool BeginStepAway(PlayerbotAI* botAI, Unit* attacker);
    // BT-377: for a member that is not the tank: an attacker of `bot` on another floor, out of its sight, that the tank
    // gave up on lately (nullptr otherwise). Cheap reads only, safe to call from a trigger.
    Unit* FindFloorAttackerOnMe(PlayerbotAI* botAI);
}

class PartyMemberAttackedOutOfSightTrigger : public Trigger
{
public:
    PartyMemberAttackedOutOfSightTrigger(PlayerbotAI* botAI)
        : Trigger(botAI, "party member attacked out of sight", 1000)
    {
    }

    bool IsActive() override;
};

// BT-190 (MemberRegroupPolicy.h): a damage dealer or healer with no target for 15 s while a member's attacker stays out
// of its sight.
class MemberAttackerOutOfSightTrigger : public Trigger
{
public:
    MemberAttackerOutOfSightTrigger(PlayerbotAI* botAI) : Trigger(botAI, "member attacker out of sight", 1000) {}

    bool IsActive() override;

private:
    time_t since = 0;
};

// BT-190: walks to the tank's side, never nearer to the attacker than the tank.
class MoveToTankForHiddenAttackerAction : public MovementAction
{
public:
    MoveToTankForHiddenAttackerAction(PlayerbotAI* botAI) : MovementAction(botAI, "move to tank for hidden attacker") {}

    bool Execute(Event event) override;
    bool isUseful() override;

private:
    uint64 loggedFor = 0;
};

// BT-377: a mob hits a member from another floor and the tank gave up walking to it. The tank (from its give-up) and the
// member under attack (from this trigger) step away from it; the walk is the action below. Active while a walk is ordered
// or, for a member, while such an attacker is on it.
class FloorAttackerStepAwayTrigger : public Trigger
{
public:
    FloorAttackerStepAwayTrigger(PlayerbotAI* botAI) : Trigger(botAI, "floor attacker step away", 1000) {}

    bool IsActive() override;
};

class StepAwayFromFloorAttackerAction : public MovementAction
{
public:
    StepAwayFromFloorAttackerAction(PlayerbotAI* botAI) : MovementAction(botAI, "step away from floor attacker") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class ReachAttackedMemberAction : public AttackAction
{
public:
    ReachAttackedMemberAction(PlayerbotAI* botAI) : AttackAction(botAI, "reach attacked member") {}

    bool Execute(Event event) override;
    bool isUseful() override;

private:
    // The walk toward the current pick; a give-up is remembered per attacker (TankRescuePolicy::GiveUpBook).
    uint64 rescueGuid = 0;
    time_t rescueSince = 0;
    // BT-388: the tank's height when the walk began, so a give-up can say that the walk climbed (a ledge over the mob).
    float rescueStartZ = 0.0f;
    time_t nextAreaAttempt = 0;

    // BT-174: one [RESCUE] line per attacker and how, at most every few seconds.
    void LogRescue(Unit* attacker, Unit* victim, char const* how, char const* detail = "");
    uint64 loggedGuid = 0;
    time_t loggedAt = 0;
    char const* loggedHow = "";
};

#endif
