/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "DungeonChatStrategy.h"

#include "Playerbots.h"

// A line costs the bot no game time, so it is rare and short. The relevance sits above the follow and the buffs and
// below every fight action that matters.
void DungeonChatStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("dungeon chat", NextAction::array(0, new NextAction("dungeon chat", ACTION_NORMAL + 6), nullptr)));
}
