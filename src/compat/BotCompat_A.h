#ifndef MOD_AWARE_BOTS_COMPAT_BOTCOMPAT_A_H
#define MOD_AWARE_BOTS_COMPAT_BOTCOMPAT_A_H

#include "Unit.h"
#include "ObjectMgr.h"
#include "BattlegroundVOP.h"

inline float BotGetPowerPct(Unit const* unit, Powers power)
{
    int32 maximum = unit->GetMaxPower(power);
    return maximum ? 100.0f * unit->GetPower(power) / maximum : 0.0f;
}

// PORT: SkyFire cannot apply graveyard conditions for the requesting player.
inline WorldSafeLocsEntry const* BotGetClosestGraveYard(WorldLocation const& location, uint32 team)
{
    return sObjectMgr->GetClosestGraveYard(location.GetPositionX(), location.GetPositionY(), location.GetPositionZ(), location.GetMapId(), team);
}

inline bool BotIsFFAPvP(Unit const* unit)
{
    return unit->GetByteValue(UNIT_FIELD_SHAPESHIFT_FORM, 1) & UNIT_BYTE2_FLAG_FFA_PVP;
}

constexpr uint32 BG_TOK_SPELL_ORB_PICKED_UP_1 = BG_VOP_SPELL_ORB_PICKED_UP_1;
constexpr uint32 BG_TOK_SPELL_ORB_PICKED_UP_2 = BG_VOP_SPELL_ORB_PICKED_UP_2;
constexpr uint32 BG_TOK_SPELL_ORB_PICKED_UP_3 = BG_VOP_SPELL_ORB_PICKED_UP_3;
constexpr uint32 BG_TOK_SPELL_ORB_PICKED_UP_4 = BG_VOP_SPELL_ORB_PICKED_UP_4;

#endif
