#include "PvpKiteStrategy.h"

#include "Playerbots.h"

void PvpKiteStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // Below the class escapes (Frost Nova 50, Blink back 35) and above the damage and heal edges.
    triggers.push_back(new TriggerNode("pvp kite", NextAction::array(0, new NextAction("pvp kite", ACTION_MOVE + 2), nullptr)));
}
