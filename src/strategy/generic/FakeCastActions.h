/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_FAKECASTACTIONS_H
#define _PLAYERBOT_FAKECASTACTIONS_H

// BT-523: the trigger and the action of the fake cast (FakeCast.h).

#include "Action.h"
#include "Trigger.h"

class PlayerbotAI;

/// An expert bot's cast is a fake whose cancel point has come.
class FakeCastDueTrigger : public Trigger
{
public:
    FakeCastDueTrigger(PlayerbotAI* botAI) : Trigger(botAI, "fake cast due", 1) {}

    bool IsActive() override;
};

class FakeCastAction : public Action
{
public:
    FakeCastAction(PlayerbotAI* botAI) : Action(botAI, "fake cast") {}

    bool Execute(Event event) override;
};

#endif
