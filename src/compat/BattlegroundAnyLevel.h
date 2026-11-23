/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * Legends of Azeroth's Battleground.AnyLevel: bots below a battleground's level range may join it, and their healing
 * and damage are scaled. SkyFire has none of it, so this is the switched-off behaviour: the stock bracket lookup and
 * no scaling.
 */

#ifndef MOD_AWARE_BOTS_COMPAT_BATTLEGROUNDANYLEVEL_H
#define MOD_AWARE_BOTS_COMPAT_BATTLEGROUNDANYLEVEL_H

#include "DBCStores.h"
#include "Define.h"

class Battleground;
class Unit;

// PORT: SkyFire has no Battleground.AnyLevel; Enabled() is false, nothing is scaled, and the bracket is the stock one.
namespace BattlegroundAnyLevel
{
    inline bool Enabled() { return false; }
    inline void ScaleHealing(Unit* /*healer*/, Unit* /*target*/, uint32& /*heal*/) { }

    // With the switch off LoA's lookup is the stock one, as WorldSession::HandleBattleFieldPortOpcode does it.
    inline PvPDifficultyEntry const* GetBracketByLevel(Battleground* /*bg*/, uint32 mapId, uint32 level)
    {
        return GetBattlegroundBracketByLevel(mapId, level);
    }
}

#endif
