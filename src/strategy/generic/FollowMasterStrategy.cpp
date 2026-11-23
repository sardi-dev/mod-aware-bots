#include "FollowMasterStrategy.h"

#include "Playerbots.h"

NextAction** FollowMasterStrategy::getDefaultActions()
{
    return NextAction::array(0, new NextAction("follow", 1.0f), nullptr);
}

void FollowMasterStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    NonCombatStrategy::InitTriggers(triggers);

    // A random bot that is far from its player's group travels to it (TravelToGroupAction); above the plain follow.
    triggers.push_back(
        new TriggerNode("far from group leader", NextAction::array(0, new NextAction("travel to group", 3.0f), nullptr)));
    // At a meeting stone with a bot group mate far away, the bot summons the mate.
    triggers.push_back(new TriggerNode("group mate far", NextAction::array(0, new NextAction("stone summon", 2.5f), nullptr)));
    triggers.push_back(new TriggerNode("voice wipe", NextAction::array(0, new NextAction("voice wipe", 1.5f), nullptr)));
    // BT-119: a bot behind its player in an instance walks, steps aside, then teleports unseen (above travel to
    // group, which skips instances).
    triggers.push_back(new TriggerNode("far from master in instance",
        NextAction::array(0, new NextAction("dungeon unstuck", 3.5f), nullptr)));
}
