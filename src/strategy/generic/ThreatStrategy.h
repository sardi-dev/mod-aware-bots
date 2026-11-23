#ifndef _PLAYERBOT_THREATSTRATEGY_H
#define _PLAYERBOT_THREATSTRATEGY_H

#include "Strategy.h"
#include "Trigger.h"

class PlayerbotAI;

class ThreatMultiplier : public Multiplier
{
public:
    ThreatMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "threat") {}

    float GetValue(Action* action) override;
};

class ThreatStrategy : public Strategy
{
public:
    ThreatStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    void InitMultipliers(std::vector<Multiplier*>& multipliers) override;
    std::string const getName() override { return "threat"; }
};

class FocusMultiplier : public Multiplier
{
public:
    FocusMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "focus") {}

    float GetValue(Action* action) override;
};

class FocusStrategy : public Strategy
{
public:
    FocusStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    void InitMultipliers(std::vector<Multiplier*>& multipliers) override;
    std::string const getName() override { return "focus"; }
};

class DpsCooldownMultiplier : public Multiplier
{
public:
    explicit DpsCooldownMultiplier(PlayerbotAI* ai) : Multiplier(ai, "dps cooldown") {}
    float GetValue(Action* action) override;
};
class DpsCooldownStrategy : public Strategy
{
public:
    explicit DpsCooldownStrategy(PlayerbotAI* ai) : Strategy(ai) {}
    void InitMultipliers(std::vector<Multiplier*>& multipliers) override;
    std::string const getName() override { return "dps cooldown"; }
};

class DpsOffhealTrigger : public Trigger
{
public:
    explicit DpsOffhealTrigger(PlayerbotAI* ai) : Trigger(ai, "dps emergency offheal"), enabled(false) {}
    bool IsActive() override;
private:
    bool enabled;
};

class DpsOffhealMultiplier : public Multiplier
{
public:
    explicit DpsOffhealMultiplier(PlayerbotAI* ai) : Multiplier(ai, "dps offheal") {}
    float GetValue(Action* action) override;
};

class DpsOffhealStrategy : public Strategy
{
public:
    explicit DpsOffhealStrategy(PlayerbotAI* ai) : Strategy(ai) {}
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    void InitMultipliers(std::vector<Multiplier*>& multipliers) override;
    std::string const getName() override { return "dps emergency offheal"; }
};

#endif
