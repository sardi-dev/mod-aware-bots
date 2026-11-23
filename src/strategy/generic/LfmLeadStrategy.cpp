/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "LfmLeadStrategy.h"

#include "Playerbots.h"

// Above the roaming actions, which would walk the leader away from its party.
NextAction** LfmLeadStrategy::getDefaultActions()
{
    return NextAction::array(0, new NextAction("lfm lead", 6.0f), nullptr);
}
