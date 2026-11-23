#include "TankAssistStrategy.h"

#include "Playerbots.h"

void TankAssistStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(
        new TriggerNode("tank assist", NextAction::array(0, new NextAction("tank assist", 50.0f), nullptr)));

    // BT-155: defend a member immediately, including from the non-combat engine.
    triggers.push_back(new TriggerNode("party member attacked out of sight",
        NextAction::array(0, new NextAction("reach attacked member", ACTION_RAID), nullptr)));

    // BT-377: after a rescue give-up on a mob on another floor, step away from it (below the rescue itself, above the
    // interrupts, so a mob that comes into sight is taken at once).
    triggers.push_back(new TriggerNode("floor attacker step away",
        NextAction::array(0, new NextAction("step away from floor attacker", ACTION_INTERRUPT + 5.0f), nullptr)));
}
