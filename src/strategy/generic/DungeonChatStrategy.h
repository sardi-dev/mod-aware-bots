/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_DUNGEONCHATSTRATEGY_H
#define _PLAYERBOT_DUNGEONCHATSTRATEGY_H

#include "Strategy.h"

class PlayerbotAI;

// The small talk of a random bot in a dungeon group with a player (Mgr/Text/DungeonChat.h). Added to the combat and the
// non-combat engine: the moments of a run (boss pull, a long fight, a healer out of mana) happen in a fight.
class DungeonChatStrategy : public Strategy
{
public:
    DungeonChatStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::string const getName() override { return "dungeon chat"; }
};

#endif
