/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "PvpTalentsStrategy.h"

#include "Playerbots.h"

void PvpTalentsStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // BT-320: above the battleground objectives (up to ACTION_EMERGENCY) and roaming, so a bot sets its talents in the
    // preparation phase before it runs out of the gate; below joining and accepting a match (100). The change is one
    // instant step and the trigger is off right after it, so it holds nothing up.
    triggers.push_back(new TriggerNode("pvp talent set due",
                                       NextAction::array(0, new NextAction("pvp talent set", ACTION_EMERGENCY + 5.0f), nullptr)));
}
