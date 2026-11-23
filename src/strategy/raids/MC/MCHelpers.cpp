/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "MCHelpers.h"
#include "Map.h"
#include "Player.h"

bool MoltenCoreHelpers::IsOverMagma(Player* player)
{
    return player->GetMap()->getLiquidStatus(player->GetPositionX(), player->GetPositionY(), player->GetPositionZ(),
        MAP_LIQUID_TYPE_MAGMA) != LIQUID_MAP_NO_WATER;
}

bool MoltenCoreHelpers::IsInMagma(Player* player)
{
    ZLiquidStatus const status = player->GetMap()->getLiquidStatus(player->GetPositionX(), player->GetPositionY(),
        player->GetPositionZ(), MAP_LIQUID_TYPE_MAGMA);
    return status & (LIQUID_MAP_WATER_WALK | LIQUID_MAP_IN_WATER | LIQUID_MAP_UNDER_WATER);
}
