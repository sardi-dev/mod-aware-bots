/*
 * AC parity (#408): ported from AC mod-playerbots Ai/Base/Actions/PullActions.h.
 *
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option) any later version.
 */

/*
 * Ported from the CMaNGOS playerbots project (https://github.com/cmangos/playerbots), GPL v2,
 * with modifications for AzerothCore.
 * Original authors:
 *   David Parra Ausina (davidonete/Flekz) <davidparraausina@gmail.com> - original author
 *   Sebastiaan Keek (mostlikely4r) <sebastiaan.keek@gmail.com>
 *   Cyberium <cyberium@users.noreply.github.com>
 */

#ifndef _PLAYERBOT_PULLACTIONS_H
#define _PLAYERBOT_PULLACTIONS_H

#include "GenericSpellActions.h"
#include "ReachTargetActions.h"

#include <ctime>

class PullRequestAction : public Action
{
public:
    PullRequestAction(PlayerbotAI* botAI, std::string const name) : Action(botAI, name) {}

    bool Execute(Event event) override;

protected:
    virtual Unit* GetPullTarget(Event event) = 0;
};

class PullMyTargetAction : public PullRequestAction
{
public:
    PullMyTargetAction(PlayerbotAI* botAI) : PullRequestAction(botAI, "pull my target") {}

private:
    Unit* GetPullTarget(Event event) override;
};

class PullRtiTargetAction : public PullRequestAction
{
public:
    PullRtiTargetAction(PlayerbotAI* botAI) : PullRequestAction(botAI, "pull rti target") {}

private:
    Unit* GetPullTarget(Event event) override;
};

class PullStartAction : public Action
{
public:
    PullStartAction(PlayerbotAI* botAI, std::string const name = "pull start") : Action(botAI, name) {}

    bool Execute(Event event) override;
};

class PullAction : public CastSpellAction
{
public:
    PullAction(PlayerbotAI* botAI, std::string const name = "pull action");

    bool Execute(Event event) override;
    bool isPossible() override;
    // #408: MoP's Action::getPrerequisites() returns NextAction**, not std::vector<NextAction>.
    NextAction** getPrerequisites() override;
    Unit* GetTarget() override;

private:
    void InitPullAction();

    // The pull logs one attempt per target every PULL_LOG_SECONDS, so retries of the same mark show too (BT-143).
    uint64 loggedTarget = 0;
    time_t loggedAt = 0;
};

class PullEndAction : public Action
{
public:
    PullEndAction(PlayerbotAI* botAI, std::string const name = "pull end") : Action(botAI, name) {}

    bool Execute(Event event) override;
};

class ReachPullAction : public ReachTargetAction
{
public:
    ReachPullAction(PlayerbotAI* botAI) : ReachTargetAction(botAI, "reach pull", botAI->GetRange("spell")) {}

    bool Execute(Event event) override;
    bool isUseful() override;
    Unit* GetTarget() override;

private:
    // BT-181: the last reach-failed line, per target.
    uint64 loggedTarget = 0;
    time_t loggedAt = 0;

    // BT-333: walks to a spot round the mark from where it is in sight; false when none has sight and a short path.
    bool WalkToSightSpot(Unit* target);
    time_t lastSightSearch = 0;
};

// BT-191: the tank walks up to a marked dungeon boss that is immune to players until it aggroes and wakes when someone
// comes close (MarkedTargetReadyTrigger::IsBodyPullMark); the pull spell cannot reach it.
class BodyPullMarkAction : public MovementAction
{
public:
    BodyPullMarkAction(PlayerbotAI* botAI) : MovementAction(botAI, "body pull mark") {}

    bool Execute(Event event) override;
    bool isUseful() override;

private:
    uint64 announcedMark = 0;
    uint64 loggedTarget = 0;
    time_t loggedAt = 0;
};

// BT-200 (MarkPullPolicy::WalksToLeader): the mark is out of reach and out of sight and the tank is far from the leader:
// walk to the leader's side. Never toward the mark, never a pull.
class WalkToLeaderForMarkAction : public MovementAction
{
public:
    WalkToLeaderForMarkAction(PlayerbotAI* botAI) : MovementAction(botAI, "walk to leader for mark") {}

    bool Execute(Event event) override;
    bool isUseful() override;

private:
    uint64 announcedMark = 0;
    time_t failedLoggedAt = 0;
};

// BT-290 (MarkHealerOutOfTankSightTrigger): the healer is still out of the tank's sight after her own walk had its time:
// the tank steps toward her until they see each other. It never pulls by this walk.
class StepIntoHealerSightAction : public MovementAction
{
public:
    StepIntoHealerSightAction(PlayerbotAI* botAI) : MovementAction(botAI, "step into healer sight") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class ReturnToPullPositionAction : public MovementAction
{
public:
    ReturnToPullPositionAction(PlayerbotAI* botAI) : MovementAction(botAI, "return to pull position") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

#endif
