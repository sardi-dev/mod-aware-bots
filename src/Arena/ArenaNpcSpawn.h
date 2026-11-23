/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_ARENANPCSPAWN_H
#define _PLAYERBOT_ARENANPCSPAWN_H

#include "Define.h"

// BT-154: puts the ladder NPC "Pit Chronicler" into the world at run time (spots and rules: ArenaNpcSpots.h). The core has
// no creature rows for it, so without this module no copy exists anywhere.
//
// Threads. Both functions run on the world thread only: OnStartup after the data stores and the creature templates are
// loaded, OnUpdate from the world update script, which World::Update runs after sMapMgr->Update has waited for the map
// threads. At that point no map thread touches a map, so looking at a grid and adding a creature to it is as safe as it is
// for the bots the ladder moves in the same tick (ArenaLadder.h).
namespace ArenaNpcSpawn
{
// Reads AiPlayerbot.ArenaTeams.LadderNpc and checks that the creature template exists. Call after ArenaLadder::OnStartup.
void OnStartup();

// The spawn pass, every ArenaNpcSpots::SCAN_INTERVAL_MS of world time.
void OnUpdate(uint32 diff);
}  // namespace ArenaNpcSpawn

#endif
