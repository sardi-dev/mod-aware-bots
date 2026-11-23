/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "MaraudonStrategy.h"

#include "GridObject.h"
#include "DynamicObject.h"
#include "CellImpl.h"
#include "Map.h"
#include "CataGbShared.h"
#include "FollowActions.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "MaraudonCloudLogic.h"
#include "Playerbots.h"
#include "PlayerbotSpec.h"
#include "ReachTargetActions.h"
#include "Timer.h"
#include <cmath>
#include <list>

namespace
{
constexpr float CLOUD_MARGIN = 1.5f;

std::vector<CataGb::Hazard> Clouds(Player* bot)
{
    std::vector<CataGb::Hazard> hazards;
    if (bot->GetMapId() != 349 || !bot->IsAlive())
        return hazards;

    std::list<WorldObject*> objects;
    Skyfire::AllWorldObjectsInRange check(bot, 30.0f);
    Skyfire::WorldObjectListSearcher<Skyfire::AllWorldObjectsInRange> searcher(
        bot, objects, check, GRID_MAP_TYPE_MASK_DYNAMICOBJECT);
    bot->VisitNearbyObject(30.0f, searcher);
    for (WorldObject* object : objects)
    {
        DynamicObject* cloud = static_cast<DynamicObject*>(object);
        if (cloud->IsInWorld() && MaraudonCloud::Relevant(cloud->GetSpellId(), cloud->GetDuration(),
                cloud->GetRadius(), cloud->GetPositionZ() - bot->GetPositionZ()))
            hazards.push_back({cloud->GetPositionX(), cloud->GetPositionY(), cloud->GetRadius()});
    }
    return hazards;
}

bool Contains(std::vector<CataGb::Hazard> const& hazards, Unit* unit)
{
    if (!unit)
        return false;
    for (CataGb::Hazard const& hazard : hazards)
        if (std::hypot(unit->GetPositionX() - hazard.x, unit->GetPositionY() - hazard.y) < hazard.radius + CLOUD_MARGIN)
            return true;
    return false;
}

class CloudMovementMultiplier : public Multiplier
{
public:
    CloudMovementMultiplier(PlayerbotAI* ai) : Multiplier(ai, "maraudon cloud") {}
    float GetValue(Action* action) override
    {
        if (!action || dynamic_cast<MaraudonCloudAction*>(action) ||
            (!dynamic_cast<MovementAction*>(action) && !dynamic_cast<CastReachTargetSpellAction*>(action)))
            return 1.0f;
        uint32 const now = getMSTime();
        if (now - lastCheck >= 150)
        {
            lastCheck = now;
            hazards = Clouds(bot);
        }
        if (CataGb::InsideHazard(bot, hazards, CLOUD_MARGIN))
            return 0.0f;
        // Once clear, do not immediately follow the leader or a melee target back into the poison.
        if (dynamic_cast<FollowAction*>(action) && Contains(hazards, botAI->GetMaster()))
            return 0.0f;
        if (!PlayerBotSpec::IsRanged(bot) && Contains(hazards, AI_VALUE(Unit*, "current target")))
            return 0.0f;
        return 1.0f;
    }
private:
    uint32 lastCheck = 0;
    std::vector<CataGb::Hazard> hazards;
};
}

bool MaraudonCloudTrigger::IsActive()
{
    return CataGb::InsideHazard(bot, Clouds(bot), CLOUD_MARGIN);
}

bool MaraudonCloudAction::Execute(Event /*event*/)
{
    std::vector<CataGb::Hazard> const hazards = Clouds(bot);
    if (!CataGb::InsideHazard(bot, hazards, CLOUD_MARGIN))
        return false;
    float x = 0.0f, y = 0.0f;
    CataGb::Leash const unbounded{0.0f, 0.0f, 0.0f};
    if (!CataGb::FindSafeSpot(bot, hazards, CLOUD_MARGIN, unbounded, 18.0f, x, y))
        return false;
    return MoveTo(bot->GetMapId(), x, y, CataGb::GroundZ(bot, x, y), false, false, false, true,
        MovementPriority::MOVEMENT_FORCED);
}

void MaraudonStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("maraudon cloud",
        NextAction::array(0, new NextAction("maraudon cloud", ACTION_EMERGENCY + 10), nullptr)));
}

void MaraudonStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new CloudMovementMultiplier(botAI));
}
