/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE). See BgFlagView.h for what this is.
 */

#include "BgFlagView.h"

// DatabaseEnv.h must precede the battleground headers (see BattleGroundTactics.cpp).
#include "DatabaseEnv.h"

#include "Battleground.h"
#include "BattlegroundEY.h"
#include "BattlegroundTP.h"
#include "BattlegroundWS.h"
#include "TpPlanLogic.h"
#include "Player.h"

namespace
{
// The core's Team value (ALLIANCE or HORDE) of a team index, for the calls that take a Team and not a TeamId.
uint32 TeamValue(uint32 teamIndex)
{
    return teamIndex == uint32(TEAM_ALLIANCE) ? uint32(ALLIANCE) : uint32(HORDE);
}

bool IsWarsong(Battleground* bg)
{
    return bg && bg->GetMapId() == BgFlagMap::WSG_MAP_ID;
}

bool IsTwinPeaks(Battleground* bg)
{
    return bg && bg->GetMapId() == TpLogic::TP_MAP_ID;
}

// Eye of the Storm (BT-583): one flag in the middle for both teams, with its own aura (34976).
bool IsEye(Battleground* bg)
{
    return bg && bg->GetMapId() == BgFlagMap::EYE_MAP_ID;
}

// The two classes carry the same flag auras and flag states in separate enums (BattlegroundWS.h, BattlegroundTP.h): the
// view relies on it.
static_assert(uint32(BG_WS_SPELL_WARSONG_FLAG) == uint32(BG_TP_SPELL_HORDE_FLAG), "flag aura 23333");
static_assert(uint32(BG_WS_SPELL_SILVERWING_FLAG) == uint32(BG_TP_SPELL_ALLIANCE_FLAG), "flag aura 23335");
static_assert(uint32(BG_TP_FLAG_STATE_ON_BASE) == 0 && uint32(BG_TP_FLAG_STATE_WAIT_RESPAWN) == 1 &&
                  uint32(BG_TP_FLAG_STATE_ON_PLAYER) == 2 && uint32(BG_TP_FLAG_STATE_ON_GROUND) == 3,
              "flag states 0 to 3, as FlagView documents");
}  // namespace

namespace BgFlagView
{
bool Supported(Battleground* bg)
{
    return IsWarsong(bg) || IsTwinPeaks(bg);
}

bool Fill(Battleground* bg, BgFlagMap::FlagView& out)
{
    if (IsWarsong(bg))
    {
        BattlegroundWS* const ws = static_cast<BattlegroundWS*>(bg);
        for (uint32 t = 0; t < 2; ++t)
        {
            out.state[t] = ws->GetFlagState(TeamValue(t));
            out.picker[t] = ws->GetFlagPickerGUID(int32(t));
            out.dropped[t] = ws->GetDroppedFlagGUID(TeamValue(t));
        }
        return true;
    }
    if (IsTwinPeaks(bg))
    {
        BattlegroundTP* const tp = static_cast<BattlegroundTP*>(bg);
        for (uint32 t = 0; t < 2; ++t)
        {
            out.state[t] = tp->GetFlagState(TeamValue(t));
            out.picker[t] = tp->GetFlagPickerGUID(int32(t));
            out.dropped[t] = tp->GetDroppedFlagGUID(TeamValue(t));
        }
        return true;
    }
    // Eye of the Storm has one flag: both entries of the view read it (the state numbers are the same four, BG_EY_FlagState).
    if (IsEye(bg))
    {
        BattlegroundEY* const ey = static_cast<BattlegroundEY*>(bg);
        for (uint32 t = 0; t < 2; ++t)
        {
            out.state[t] = ey->GetFlagState();
            out.picker[t] = ey->GetFlagPickerGUID();
            out.dropped[t] = ey->GetDroppedFlagGUID();
        }
        return true;
    }
    return false;
}

bool CarriesFlag(Player* bot)
{
    return bot->HasAura(BG_WS_SPELL_WARSONG_FLAG) || bot->HasAura(BG_WS_SPELL_SILVERWING_FLAG) ||
           bot->HasAura(BG_EY_NETHERSTORM_FLAG_SPELL);
}

GameObject* StandObject(Battleground* bg, uint32 flagTeamIndex)
{
    if (IsWarsong(bg))
        return bg->GetBGObject(flagTeamIndex == uint32(TEAM_ALLIANCE) ? uint32(BG_WS_OBJECT_A_FLAG) : uint32(BG_WS_OBJECT_H_FLAG));
    if (IsTwinPeaks(bg))
        return bg->GetBGObject(flagTeamIndex == uint32(TEAM_ALLIANCE) ? uint32(BG_TP_OBJECT_A_FLAG) : uint32(BG_TP_OBJECT_H_FLAG));
    return nullptr;
}

Powerups PowerupsOf(Battleground* bg, uint32 teamIndex)
{
    Powerups result;
    // Warsong Gulch: the Alliance's pair is the first object of each kind, the Horde's the second (BattlegroundWS.cpp:663-668).
    if (IsWarsong(bg))
    {
        result.regen = uint32(BG_WS_OBJECT_REGENBUFF_1) + teamIndex;
        result.berserk = uint32(BG_WS_OBJECT_BERSERKBUFF_1) + teamIndex;
    }
    // Twin Peaks (BattlegroundTP.cpp:713-716): the first restoration object is the Horde lane's (1753.96, 242.09) and the
    // second the Alliance lane's (1952.12, 383.86), so a team's own one is the pair index reversed; the first berserk
    // object (1934.37, 226.06) is on the Alliance's cut post, the second (1725.24, 446.43) on the Horde's.
    else if (IsTwinPeaks(bg))
    {
        result.regen = uint32(BG_TP_OBJECT_REGENBUFF_1) + (1 - teamIndex);
        result.berserk = uint32(BG_TP_OBJECT_BERSERKBUFF_1) + teamIndex;
    }
    return result;
}
}  // namespace BgFlagView
