/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "MarkPullStrategy.h"

#include "Playerbots.h"

void MarkPullStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // Above following: the tank leaves the master's side to pull.
    triggers.push_back(new TriggerNode("marked target ready", NextAction::array(0, new NextAction("pull rti target", 50.0f), nullptr)));
    // BT-200: below the pull and above following, like the pull: a mark out of reach and out of sight is no reason to stay
    // where the leader is not.
    triggers.push_back(new TriggerNode("marked target out of reach",
        NextAction::array(0, new NextAction("walk to leader for mark", 50.0f), nullptr)));
    // BT-290: a healer still out of sight after her own walk: the tank steps to where she sees it, the spot it pulls to.
    triggers.push_back(new TriggerNode("mark healer out of tank sight",
        NextAction::array(0, new NextAction("step into healer sight", 50.0f), nullptr)));
    // BT-191: a marked boss that is immune to players until it aggroes is walked up to instead.
    triggers.push_back(new TriggerNode("marked boss ready for body pull",
        NextAction::array(0, new NextAction("body pull mark", 50.0f), nullptr)));
}
