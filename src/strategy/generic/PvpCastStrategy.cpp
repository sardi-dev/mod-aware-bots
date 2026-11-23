/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "PvpCastStrategy.h"

#include "Playerbots.h"

void PvpCastStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // Above every spell edge: the cancel must come at its moment, in the middle of the cast the rotation started.
    triggers.push_back(new TriggerNode("fake cast due", NextAction::array(0, new NextAction("fake cast", ACTION_EMERGENCY + 2), nullptr)));
}
