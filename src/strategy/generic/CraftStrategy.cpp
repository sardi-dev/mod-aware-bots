/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "CraftStrategy.h"

#include "Playerbots.h"

void CraftStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // Below shopping: a vendor upgrade within reach is bought first, crafting fills what is left.
    triggers.push_back(new TriggerNode("craftable upgrade", NextAction::array(0, new NextAction("craft upgrade", 4.0f), nullptr)));
}
