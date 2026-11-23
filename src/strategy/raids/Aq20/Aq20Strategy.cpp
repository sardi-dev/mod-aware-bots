/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "Aq20Strategy.h"
#include "Strategy.h"

// AC Ai/Raid/Aq20/Aq20Strategy.cpp:1-16. The strategy covers Ossirian only (his crystals), as in AC.

void RaidAq20Strategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("aq20 move to crystal",
        NextAction::array(0, new NextAction("aq20 use crystal", ACTION_RAID), nullptr)));
}
