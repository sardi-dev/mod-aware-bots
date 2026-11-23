#ifndef _PLAYERBOT_CONSERVEMANASTRATEGY_H
#define _PLAYERBOT_CONSERVEMANASTRATEGY_H

#include "Strategy.h"

class PlayerbotAI;

class HealerAutoSaveManaMultiplier : public Multiplier
{
public:
    HealerAutoSaveManaMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "save mana") {}

    float GetValue(Action* action) override;
};

class HealerAutoSaveManaStrategy : public Strategy
{
public:
    HealerAutoSaveManaStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    void InitMultipliers(std::vector<Multiplier*>& multipliers) override;
    std::string const getName() override { return "save mana"; }
};

// BT-295 (HealerGuardrails::DispelWaits): a healer's dispels wait while a member it can heal now is close to death.
class HealerTriageMultiplier : public Multiplier
{
public:
    HealerTriageMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "healer triage") {}

    float GetValue(Action* action) override;
};

class HealerTriageStrategy : public Strategy
{
public:
    HealerTriageStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    void InitMultipliers(std::vector<Multiplier*>& multipliers) override;
    std::string const getName() override { return "healer triage"; }
};

#endif
