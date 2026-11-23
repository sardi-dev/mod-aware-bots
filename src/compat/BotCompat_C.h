#ifndef MOD_AWARE_BOTS_COMPAT_BOTCOMPAT_C_H
#define MOD_AWARE_BOTS_COMPAT_BOTCOMPAT_C_H

#include "Player.h"

inline Position BotPosition(WorldObject const* object)
{
    Position position;
    object->GetPosition(&position);
    return position;
}

inline void BotResetEmoteStateIfNeed(Player* player)
{
    if (player->GetUInt32Value(UNIT_FIELD_NPC_EMOTESTATE))
        player->SetUInt32Value(UNIT_FIELD_NPC_EMOTESTATE, 0);
}

#endif
