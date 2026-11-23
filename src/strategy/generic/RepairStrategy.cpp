/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "RepairStrategy.h"

#include "Playerbots.h"

void RepairStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // The same level as the vendor visit: above following and roaming, below looting, which finishes first.
    triggers.push_back(new TriggerNode("repair needed nearby", NextAction::array(0, new NextAction("visit repairer", 5.0f), nullptr)));
}
