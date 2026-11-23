/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "ShopStrategy.h"

#include "Playerbots.h"

void ShopStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // Like the trainer visit: above following and below looting, which finishes first.
    triggers.push_back(new TriggerNode("vendor upgrade nearby", NextAction::array(0, new NextAction("visit vendor", 5.0f), nullptr)));
}
