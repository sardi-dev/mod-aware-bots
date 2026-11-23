/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 *
 * Ported from AC mod-playerbots Ai/Base/Trigger/WithinAreaTrigger.cpp (#179). AC's `AreaTrigger const*`
 * (a DBC-only struct) does not exist on this core; the geometry comes from `sAreaTriggerStore`
 * (AreaTriggerEntry, DBCStructure.h - ContinentID/Pos.X-Y-Z/Radius/BoxLength/BoxWidth/BoxHeight/BoxYaw,
 * verified against this core's own CMSG_AREATRIGGER handler, Handlers/MiscHandler.cpp), and the
 * teleport-target existence check is `ObjectMgr::GetAreaTrigger()` (see AreaTriggerAction.cpp's file
 * comment for the same mapping). The box/radius math itself is unchanged from AC.
 */

#include "WithinAreaTrigger.h"

#include "DBCStores.h"
#include "DBCStructure.h"
#include "LastMovementValue.h"
#include "Playerbots.h"

bool WithinAreaTrigger::IsActive()
{
    LastMovement& movement = AI_VALUE(LastMovement&, "last area trigger");
    if (!movement.lastAreaTrigger)
        return false;

    AreaTriggerEntry const* at = sAreaTriggerStore.LookupEntry(movement.lastAreaTrigger);
    if (!at)
        return false;

    if (!sObjectMgr->GetAreaTrigger(movement.lastAreaTrigger))
        return false;

    return IsPointInAreaTriggerZone(at, bot->GetMapId(), bot->GetPositionX(), bot->GetPositionY(),
                                    bot->GetPositionZ(), 0.5f);
}

bool WithinAreaTrigger::IsPointInAreaTriggerZone(AreaTriggerEntry const* atEntry, uint32 mapid, float x, float y,
                                                  float z, float delta)
{
    if (mapid != atEntry->mapid)
        return false;

    if (atEntry->radius > 0)
    {
        // if we have radius check it
        float dist2 = (x - atEntry->x) * (x - atEntry->x) + (y - atEntry->y) * (y - atEntry->y) +
                      (z - atEntry->z) * (z - atEntry->z);
        if (dist2 > (atEntry->radius + delta) * (atEntry->radius + delta))
            return false;
    }
    else
    {
        // we have only extent

        // rotate the players position instead of rotating the whole cube, that way we can make a simplified
        // is-in-cube check and we have to calculate only one point instead of 4

        // 2PI = 360, keep in mind that ingame orientation is counter-clockwise
        double rotation = 2 * M_PI - atEntry->box_orientation;
        double sinVal = sin(rotation);
        double cosVal = cos(rotation);

        float playerBoxDistX = x - atEntry->x;
        float playerBoxDistY = y - atEntry->y;

        float rotPlayerX = float(atEntry->x + playerBoxDistX * cosVal - playerBoxDistY * sinVal);
        float rotPlayerY = float(atEntry->y + playerBoxDistY * cosVal + playerBoxDistX * sinVal);

        // box edges are parallel to coordiante axis, so we can treat every dimension independently :D
        float dz = z - atEntry->z;
        float dx = rotPlayerX - atEntry->x;
        float dy = rotPlayerY - atEntry->y;
        if ((fabs(dx) > atEntry->box_x / 2 + delta) || (fabs(dy) > atEntry->box_y / 2 + delta) ||
            (fabs(dz) > atEntry->box_z / 2 + delta))
        {
            return false;
        }
    }

    return true;
}
