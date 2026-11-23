/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * "pvp talents": a random bot changes to its PvP talent and glyph set for battlegrounds and arenas and back to its PvE
 * set afterwards (Factory/BotPvpTalents.h, BT-320).
 */

#ifndef MOD_AWARE_BOTS_PVPTALENTSSTRATEGY_H
#define MOD_AWARE_BOTS_PVPTALENTSSTRATEGY_H

#include "Strategy.h"

class PvpTalentsStrategy : public Strategy
{
public:
    PvpTalentsStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::string const getName() override { return "pvp talents"; }
};

#endif
