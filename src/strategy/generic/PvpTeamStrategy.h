/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_PVPTEAMSTRATEGY_H
#define _PLAYERBOT_PVPTEAMSTRATEGY_H

#include "Strategy.h"

class PlayerbotAI;

// Rules R4 and R6 in a combat engine (PvP/PvpTeam.h): the control chain on the enemy healer and the hold on breakable control, for a
// bot in a battleground or an arena. The trigger is silent everywhere else. AiPlayerbot.Pvp.TeamPlay.Enable in the configuration.
class PvpTeamStrategy : public Strategy
{
public:
    PvpTeamStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    void InitMultipliers(std::vector<Multiplier*>& multipliers) override;
    std::string const getName() override { return "pvp team"; }
};

#endif
