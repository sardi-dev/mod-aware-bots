/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef AWARE_BOTS_MARAUDON_STRATEGY_H
#define AWARE_BOTS_MARAUDON_STRATEGY_H

#include "MovementActions.h"
#include "Multiplier.h"
#include "NamedObjectContext.h"
#include "Strategy.h"
#include "Trigger.h"

class MaraudonCloudAction : public MovementAction
{
public:
    MaraudonCloudAction(PlayerbotAI* ai) : MovementAction(ai, "maraudon cloud") {}
    bool Execute(Event event) override;
};

class MaraudonCloudTrigger : public Trigger
{
public:
    MaraudonCloudTrigger(PlayerbotAI* ai) : Trigger(ai, "maraudon cloud") {}
    bool IsActive() override;
};

class MaraudonStrategy : public Strategy
{
public:
    MaraudonStrategy(PlayerbotAI* ai) : Strategy(ai) {}
    std::string const getName() override { return "maraudon"; }
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    void InitMultipliers(std::vector<Multiplier*>& multipliers) override;
};

class MaraudonActionContext : public NamedObjectContext<Action>
{
public:
    MaraudonActionContext() { creators["maraudon cloud"] = &cloud; }
private:
    static Action* cloud(PlayerbotAI* ai) { return new MaraudonCloudAction(ai); }
};

class MaraudonTriggerContext : public NamedObjectContext<Trigger>
{
public:
    MaraudonTriggerContext() { creators["maraudon cloud"] = &cloud; }
private:
    static Trigger* cloud(PlayerbotAI* ai) { return new MaraudonCloudTrigger(ai); }
};

#endif
