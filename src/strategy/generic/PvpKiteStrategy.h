#ifndef _PLAYERBOT_PVPKITESTRATEGY_H
#define _PLAYERBOT_PVPKITESTRATEGY_H

#include "Strategy.h"

class PlayerbotAI;

// Ranged bots kite an enemy player in melee like a player would (duels and open-world PvP; battlegrounds and arenas keep their
// own tactics). The class escapes (Blink, Frost Nova, Fear, Disengage, ...) have higher priorities and run first; this edge only
// moves the bot away when none of them did. PvE targets never fire it. AiPlayerbot.PvpKite.* in the configuration.
class PvpKiteStrategy : public Strategy
{
public:
    PvpKiteStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::string const getName() override { return "pvp kite"; }
};

#endif
