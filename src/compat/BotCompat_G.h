/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * Movement and reachability helpers for actions and values (pitch, collision-checked reachability, movement updates).
 */

#ifndef MOD_AWARE_BOTS_COMPAT_BOTCOMPAT_G_H
#define MOD_AWARE_BOTS_COMPAT_BOTCOMPAT_G_H

#include "BotTerrain.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include <cmath>

inline float BotNormalizePitch(float pitch)
{
    return pitch > -M_PI && pitch < M_PI ? pitch : Position::NormalizeOrientation(pitch + M_PI) - M_PI;
}

// LoA Unit::SendMovementFlagUpdate (Unit.cpp): the unit's movement state to everyone around it.
inline void BotSendMovementFlagUpdate(Unit* unit, bool self = false)
{
    WorldPacket data(SMSG_PLAYER_MOVE, 40);
    unit->WriteMovementInfo(data);
    unit->SendMessageToSet(&data, self);
}

#endif
