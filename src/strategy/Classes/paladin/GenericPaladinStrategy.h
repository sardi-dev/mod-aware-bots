/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#ifndef _PLAYERBOT_GENERICPALADINSTRATEGY_H
#define _PLAYERBOT_GENERICPALADINSTRATEGY_H

#include "CombatStrategy.h"

class PlayerbotAI;

class GenericPaladinStrategy : public CombatStrategy
{
public:
    GenericPaladinStrategy(PlayerbotAI* botAI);

    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::string const getName() override { return "paladin"; }

protected:
    // Class sheet, holy paladin (BT-132): the party heal lines every paladin strategy starts with (Word of Glory at one
    // Holy Power, Flash of Light below the low health threshold). The holy strategy overrides this with nothing and
    // carries its own chain, so these lines do not pre-empt its Eternal Flame and Holy Light.
    virtual void InitPartyHealTriggers(std::vector<TriggerNode*>& triggers);
};

class PaladinCureStrategy : public Strategy
{
public:
    PaladinCureStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::string const getName() override { return "cure"; }
};

class PaladinBoostStrategy : public Strategy
{
public:
    PaladinBoostStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::string const getName() override { return "boost"; }
};

class PaladinCcStrategy : public Strategy
{
public:
    PaladinCcStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::string const getName() override { return "cc"; }
};

class PaladinHealerDpsStrategy : public Strategy
{
public:
    PaladinHealerDpsStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::string const getName() override { return "healer dps"; }
};

#endif
