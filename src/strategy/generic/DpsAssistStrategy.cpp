#include "DpsAssistStrategy.h"

#include "Playerbots.h"

void DpsAssistStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(
        new TriggerNode("not dps target active", NextAction::array(0, new NextAction("dps assist", 50.0f), nullptr)));

    // BT-190: a member's attacker out of sight for a while: walk to the tank's side (below a target of its own).
    triggers.push_back(new TriggerNode("member attacker out of sight",
        NextAction::array(0, new NextAction("move to tank for hidden attacker", 45.0f), nullptr)));

    // BT-377: a mob hits this member from another floor and the tank gave up on it: step away from it. Below the critical
    // heal (ACTION_CRITICAL_HEAL, 30), so a healer in danger heals first, and above the follow and the medium heals.
    triggers.push_back(new TriggerNode("floor attacker step away",
        NextAction::array(0, new NextAction("step away from floor attacker", ACTION_MEDIUM_HEAL + 5.0f), nullptr)));
}

void DpsAoeStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(
        new TriggerNode("not dps aoe target active", NextAction::array(0, new NextAction("dps aoe", 50.0f), nullptr)));
}
