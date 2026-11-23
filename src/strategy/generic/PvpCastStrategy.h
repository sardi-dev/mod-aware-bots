/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_PVPCASTSTRATEGY_H
#define _PLAYERBOT_PVPCASTSTRATEGY_H

#include "Strategy.h"

class PlayerbotAI;

// Casting under pressure (rule R11) in a combat engine: the fake cast of an expert bot (FakeCast.h). Silent for every other
// tier and while no enemy interrupt threatens the cast. AiPlayerbot.Pvp.FakeCast.Enable in the configuration.
class PvpCastStrategy : public Strategy
{
public:
    PvpCastStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::string const getName() override { return "pvp cast"; }
};

#endif
