/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "SellTripStrategy.h"

#include "Playerbots.h"

void SellTripStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // BT-149: the same level as the repair trip (RepairStrategy.cpp), so it interrupts a quest no more than a repair
    // does: above following and roaming, below looting, which finishes first.
    triggers.push_back(new TriggerNode("vendor trip wanted", NextAction::array(0, new NextAction("sell trip", 5.0f), nullptr)));
}
