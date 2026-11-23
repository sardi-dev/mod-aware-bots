/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "BotTerrain.h"

#include "CellImpl.h"
#include "GameObject.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Map.h"
#include "Player.h"

namespace
{
// A door model can be wide (the Deadmines Iron Clad Door box is 9 yd across): the grid is searched this far for a door.
constexpr float BOT_DOOR_SEARCH_RADIUS = 12.0f;

// BT-173: the hit counts as the door's only when it lies within this many yards of the door's own display box. Live
// 2026-11-13 (Deadmines run 3) the ray hit the Defias Cannon, 2.5 to 4.6 yd from the cannon and 8 to 9.5 yd from the
// door centre, and the old 10 yd test named the door for it; with the box test the cannon no longer reads as a door,
// the path goes on to the door itself (a real closed door still stops the bot 2 yd short).
constexpr float BOT_DOOR_HIT_MARGIN = 3.0f;

class ClosedDoorNearCheck
{
public:
    explicit ClosedDoorNearCheck(G3D::Vector3 const& point) : _point(point) { }

    bool operator()(GameObject* go) const
    {
        return go->GetGoType() == GAMEOBJECT_TYPE_DOOR && go->GetGoState() == GO_STATE_READY &&
               go->IsInRange(_point.x, _point.y, _point.z, BOT_DOOR_HIT_MARGIN);
    }

private:
    G3D::Vector3 _point;
};
}

uint32 BotClosedDoorEntryNear(Player* bot, G3D::Vector3 const& point)
{
    if (!bot || !bot->IsInWorld())
        return 0;
    GameObject* door = nullptr;
    ClosedDoorNearCheck check(point);
    Skyfire::GameObjectLastSearcher<ClosedDoorNearCheck> searcher(bot, door, check);
    bot->GetMap()->VisitGrid(point.x, point.y, BOT_DOOR_SEARCH_RADIUS, searcher);
    return door ? door->GetEntry() : 0;
}
