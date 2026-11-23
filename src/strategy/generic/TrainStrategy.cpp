/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "TrainStrategy.h"

#include "Playerbots.h"

void TrainStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // Above following (it leaves the master's side for the walk) and below looting, which finishes first.
    triggers.push_back(new TriggerNode("trainer nearby", NextAction::array(0, new NextAction("visit trainer", 5.0f), nullptr)));
    triggers.push_back(new TriggerNode("spells to train", NextAction::array(0, new NextAction("announce training", 1.0f), nullptr)));
    // BT-218: after riding is trained, a random bot buys its first mount at a vendor it sees.
    triggers.push_back(new TriggerNode("mount vendor nearby", NextAction::array(0, new NextAction("buy mount", 5.0f), nullptr)));
}
