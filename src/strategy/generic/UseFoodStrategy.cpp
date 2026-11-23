#include "UseFoodStrategy.h"

#include "Playerbots.h"

// AC Ai/Base/Strategy/UseFoodStrategy.cpp:11-24, the branch without the food cheat (design decision 0016): refill when low.
void UseFoodStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    Strategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode("low health", NextAction::array(0, new NextAction("food", 3.0f), nullptr)));
    triggers.push_back(new TriggerNode("low mana", NextAction::array(0, new NextAction("drink", 3.0f), nullptr)));

    // BT-109: in a dungeon group healers and casters top up between pulls, well before "low mana".
    triggers.push_back(new TriggerNode("dungeon drink", NextAction::array(0, new NextAction("drink", 3.0f), nullptr)));
}
