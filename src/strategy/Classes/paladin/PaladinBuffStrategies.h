/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#ifndef _PLAYERBOT_PALADINBUFFSTRATEGIES_H
#define _PLAYERBOT_PALADINBUFFSTRATEGIES_H

#include "Strategy.h"

class PlayerbotAI;

class PaladinBuffHealthStrategy : public Strategy
{
public:
    PaladinBuffHealthStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::string const getName() override { return "bhealth"; }
};

class PaladinBuffDpsStrategy : public Strategy
{
public:
    PaladinBuffDpsStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::string const getName() override { return "bdps"; }
};

// #383 (AC PaladinAiObjectContext.cpp:76-77,83-84): AC key aliases "bmight" -> Dps blessing strategy,
// "bkings" -> Stats blessing strategy. Own getName so Engine::addStrategy/removeStrategy (keyed by
// getName) can remove them by the same key. Triggers are inherited from the aliased MoP strategy.
class PaladinBuffMightStrategy : public PaladinBuffDpsStrategy
{
public:
    PaladinBuffMightStrategy(PlayerbotAI* botAI) : PaladinBuffDpsStrategy(botAI) {}

    std::string const getName() override { return "bmight"; }
};

class PaladinBuffArmorStrategy : public Strategy
{
public:
    PaladinBuffArmorStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::string const getName() override { return "barmor"; }
};

class PaladinBuffThreatStrategy : public Strategy
{
public:
    PaladinBuffThreatStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::string const getName() override { return "bthreat"; }
};

class PaladinBuffStatsStrategy : public Strategy
{
public:
    PaladinBuffStatsStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::string const getName() override { return "bstats"; }
};

// #383: AC "bkings" alias (see PaladinBuffMightStrategy above)
class PaladinBuffKingsStrategy : public PaladinBuffStatsStrategy
{
public:
    PaladinBuffKingsStrategy(PlayerbotAI* botAI) : PaladinBuffStatsStrategy(botAI) {}

    std::string const getName() override { return "bkings"; }
};

// Class sheet, holy paladin (BT-132): Blessing of Might as the second choice, cast only on a member whose Kings comes from
// another paladin (BlessingOfMightSecondTrigger). A holy paladin runs this next to "bkings".
class PaladinBuffMightSecondStrategy : public Strategy
{
public:
    PaladinBuffMightSecondStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::string const getName() override { return "bmight2"; }
};

#endif
