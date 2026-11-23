/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#ifndef _PLAYERBOT_TOTEMSSHAMANSTRATEGY_H
#define _PLAYERBOT_TOTEMSSHAMANSTRATEGY_H

#include "GenericShamanStrategy.h"

class PlayerbotAI;

class TotemsShamanStrategy : public GenericShamanStrategy
{
public:
    TotemsShamanStrategy(PlayerbotAI* botAI);

    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::string const getName() override { return "totems"; }
};

// #431 (AC parity, AC Strategy/TotemsShamanStrategy.h:176-364): one small combat strategy per totem. Each one
// is registered under the AC name (ShamanAiObjectContext.cpp) in its element's sibling context, so a bot holds at
// most one strategy per element (earth / fire / water / air). The per-totem NoXTotem triggers (ShamanTriggers.cpp)
// read these strategies through botAI->HasStrategy() to know which totem the slot should hold.

class EarthTotemStrategy : public GenericShamanStrategy
{
public:
    EarthTotemStrategy(PlayerbotAI* botAI);
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::string const getName() override { return "tremor"; }
};

class EarthbindTotemStrategy : public GenericShamanStrategy
{
public:
    EarthbindTotemStrategy(PlayerbotAI* botAI);
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::string const getName() override { return "earthbind"; }
};

// Fire Totem Strategies
class SearingTotemStrategy : public GenericShamanStrategy
{
public:
    SearingTotemStrategy(PlayerbotAI* botAI);
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::string const getName() override { return "searing"; }
};

class MagmaTotemStrategy : public GenericShamanStrategy
{
public:
    MagmaTotemStrategy(PlayerbotAI* botAI);
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::string const getName() override { return "magma"; }
};

// Water Totem Strategies
class HealingStreamTotemStrategy : public GenericShamanStrategy
{
public:
    HealingStreamTotemStrategy(PlayerbotAI* botAI);
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::string const getName() override { return "healing stream"; }
};

class GroundingTotemStrategy : public GenericShamanStrategy
{
public:
    GroundingTotemStrategy(PlayerbotAI* botAI);
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::string const getName() override { return "grounding"; }
};

#endif
