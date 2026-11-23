/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef PLAYERBOTS_TOCACTIONS_H
#define PLAYERBOTS_TOCACTIONS_H

#include "Action.h"
#include "AttackActions.h"
#include "MovementActions.h"

// Trial of the Crusader. The bots only walk, pick targets and cast like a player: no auras added or removed, no
// teleports, no triggered casts.

// The tank that marks puts the skull on the unit the damage dealers should kill
class ToCMarkPriorityAction : public Action
{
public:
    ToCMarkPriorityAction(PlayerbotAI* botAI) : Action(botAI, "toc mark priority") {}
    bool Execute(Event event) override;
};

class ToCAttackPriorityAction : public AttackAction
{
public:
    ToCAttackPriorityAction(PlayerbotAI* botAI) : AttackAction(botAI, "toc attack priority") {}
    bool Execute(Event event) override;
};

// The main tank and the assist tank each take the unit the encounter gives them (ToC::MainTankUnit, AssistTankUnit)
class ToCTankTargetAction : public AttackAction
{
public:
    ToCTankTargetAction(PlayerbotAI* botAI, std::string const name, bool main)
        : AttackAction(botAI, name), _main(main) {}
    bool Execute(Event event) override;

private:
    bool _main;
};

class ToCAvoidHazardAction : public MovementAction
{
public:
    ToCAvoidHazardAction(PlayerbotAI* botAI) : MovementAction(botAI, "toc avoid hazard") {}
    bool Execute(Event event) override;
};

class ToCDodgeChargeAction : public MovementAction
{
public:
    ToCDodgeChargeAction(PlayerbotAI* botAI) : MovementAction(botAI, "toc dodge charge") {}
    bool Execute(Event event) override;
};

class ToCRunFromRaidAction : public MovementAction
{
public:
    ToCRunFromRaidAction(PlayerbotAI* botAI) : MovementAction(botAI, "toc run from raid") {}
    bool Execute(Event event) override;
};

class ToCHealIncineratedAction : public Action
{
public:
    ToCHealIncineratedAction(PlayerbotAI* botAI) : Action(botAI, "toc heal incinerated") {}
    bool Execute(Event event) override;
};

class ToCTakeEssenceAction : public MovementAction
{
public:
    ToCTakeEssenceAction(PlayerbotAI* botAI) : MovementAction(botAI, "toc take essence") {}
    bool Execute(Event event) override;
};

class ToCHandleOrbsAction : public MovementAction
{
public:
    ToCHandleOrbsAction(PlayerbotAI* botAI) : MovementAction(botAI, "toc handle orbs") {}
    bool Execute(Event event) override;
};

class ToCKiteSpikeAction : public MovementAction
{
public:
    ToCKiteSpikeAction(PlayerbotAI* botAI) : MovementAction(botAI, "toc kite spike") {}
    bool Execute(Event event) override;
};

class ToCPullBurrowerAwayAction : public MovementAction
{
public:
    ToCPullBurrowerAwayAction(PlayerbotAI* botAI) : MovementAction(botAI, "toc pull burrower away") {}
    bool Execute(Event event) override;
};

#endif
