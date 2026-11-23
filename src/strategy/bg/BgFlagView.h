/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_BGFLAGVIEW_H
#define _PLAYERBOT_BGFLAGVIEW_H

// BT-470: the game-bound half of the map adapter (Utils/BgFlagMap.h). BattlegroundWS and BattlegroundTP are two unrelated
// core classes with the same flag game; the two-flag plan, the carrier rule and the tactics read one view of them
// through this file and never cast a battleground to either class themselves.

#include "BgFlagMap.h"
#include "Define.h"

class Battleground;
class GameObject;
class Player;

namespace BgFlagView
{
// The core object types of the two own-side power-ups the plan sends bots to (restoration and berserk).
struct Powerups
{
    uint32 regen = 0;
    uint32 berserk = 0;
};

// True for the two-flag maps this adapter reads: Warsong Gulch (489) and Twin Peaks (726). Eye of the Storm (566, one flag) is read by
// Fill and CarriesFlag too (BT-583) but is not a two-flag map: the two-flag plan does not play it.
bool Supported(Battleground* bg);

// The flag state of the match, read from whichever core class the map has. False for any other battleground.
bool Fill(Battleground* bg, BgFlagMap::FlagView& out);

// The bot carries a flag (the flag auras 23333 and 23335 are the same numbers in both core classes).
bool CarriesFlag(Player* bot);

// The flag stand object of the team's flag (the object a carrier of the OTHER team captures at when it is its own flag).
GameObject* StandObject(Battleground* bg, uint32 flagTeamIndex);

// Own-side power-up object types of a team (index = TeamId).
Powerups PowerupsOf(Battleground* bg, uint32 teamIndex);
}

#endif
