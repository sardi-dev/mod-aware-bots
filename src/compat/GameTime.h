/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * TrinityCore's GameTime namespace; SkyFire keeps the game time in World.
 */

#ifndef MOD_AWARE_BOTS_COMPAT_GAMETIME_H
#define MOD_AWARE_BOTS_COMPAT_GAMETIME_H

#include "Timer.h"
#include "World.h"

namespace GameTime
{
    inline time_t GetGameTime() { return sWorld->GetGameTime(); }
    inline uint32 GetGameTimeMS() { return getMSTime(); }
}

#endif
