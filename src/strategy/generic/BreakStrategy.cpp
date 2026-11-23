/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "BreakStrategy.h"

#include "Playerbots.h"

// Above the roaming, following and leading actions: a bot on break sits.
NextAction** BreakStrategy::getDefaultActions()
{
    return NextAction::array(0, new NextAction("break", 7.5f), nullptr);
}
