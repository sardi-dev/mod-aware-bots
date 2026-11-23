/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// AC Bot/PlayerbotAI.cpp role and index helpers the raid strategies use (IsAssistTankOfIndex, IsAssistHealOfIndex,
// IsAssistRangedDpsOfIndex, GetRangedIndex, GetClassIndex, GetRangedDpsIndex, IsBotMainTank, EqualLowercaseName).
// This fork's PlayerbotAI predates them and keeps its role checks in PlayerBotSpec (AI/PlayerbotSpec.h), whose own
// IsAssistTankOfIndex has the older (bot, player, index) form; the raid ports call these instead.

#ifndef PLAYERBOTS_RAIDROLES_H
#define PLAYERBOTS_RAIDROLES_H

#include "Define.h"

#include <string>

class Player;

namespace RaidRoles
{
    // indexLivingOnly leaves dead members out of the count, so the living ones move up to fill the gap; by default
    // roles stay put across deaths.
    bool IsAssistTankOfIndex(Player* player, uint8 index, bool indexLivingOnly = false);
    bool IsAssistHealOfIndex(Player* player, uint8 index, bool indexLivingOnly = false);
    bool IsAssistRangedDpsOfIndex(Player* player, uint8 index, bool indexLivingOnly = false);

    // Position of the player among the group members of that kind, in group order; -1 when the player is not of
    // that kind or has no group.
    int32 GetRangedIndex(Player* player);
    int32 GetClassIndex(Player* player, uint8 cls);
    int32 GetRangedDpsIndex(Player* player);

    // The tank that runs a bot tank's duties: the main tank when it is a bot, else the first bot assist tank.
    bool IsBotMainTank(Player* player);

    bool EqualLowercaseName(std::string const& s1, std::string const& s2);
}

#endif
