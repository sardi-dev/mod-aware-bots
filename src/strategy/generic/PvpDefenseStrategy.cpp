/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "PvpDefenseStrategy.h"

#include "Playerbots.h"

void PvpDefenseStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // Like the arena plan's defensive (ACTION_EMERGENCY), and the way out of control one step above it, as the racial
    // triggers it replaces in a fight (ACTION_EMERGENCY + 1).
    triggers.push_back(new TriggerNode("pvp defensive due", NextAction::array(0, new NextAction("pvp defensive", ACTION_EMERGENCY), nullptr)));
    triggers.push_back(new TriggerNode("pvp break due", NextAction::array(0, new NextAction("pvp break", ACTION_EMERGENCY + 1), nullptr)));
}
