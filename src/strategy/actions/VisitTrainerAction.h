/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * Actions for alt bots that learn at a class trainer like a player (BotTraining.h).
 */

#ifndef MOD_AWARE_BOTS_VISITTRAINERACTION_H
#define MOD_AWARE_BOTS_VISITTRAINERACTION_H

#include "MovementActions.h"

// Walks to the nearby class trainer, then learns what the bot can afford and tells its group (or master).
class VisitTrainerAction : public MovementAction
{
public:
    VisitTrainerAction(PlayerbotAI* botAI) : MovementAction(botAI, "visit trainer") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

// Tells its group (or master) once per level that the bot has spells to learn at its class trainer.
class AnnounceTrainingAction : public Action
{
public:
    AnnounceTrainingAction(PlayerbotAI* botAI) : Action(botAI, "announce training") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

#endif
