/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "OneVsOneStrategy.h"

#include "Playerbots.h"

void OneVsOneStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // Above the damage edges and the old kite (ACTION_MOVE + 2) and below interrupts (ACTION_INTERRUPT) and the big
    // defensives, so a kick or an Ice Block still comes first.
    triggers.push_back(new TriggerNode("1v1", NextAction::array(0, new NextAction("1v1", ACTION_MOVE + 7), nullptr)));
}
