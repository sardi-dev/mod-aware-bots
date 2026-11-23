/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_TPPLAN_H
#define _PLAYERBOT_TPPLAN_H

// The Twin Peaks part of the two-flag plan (docs/bg-sheets/twin-peaks.md, owner approved 2026-11-18), the pieces of the plan
// that only map 726 has: the learned respawn rule, the enemy carrier's phase on its run home, the floater and the wiped
// attacks (states G and H), the lane posts of MID. The decisions are pure functions in Utils/TpPlanLogic.h (tested in
// tests/test_tp_plan.cpp); this file reads the plan's record into their input and writes their answer back. It runs inside
// the plan's refresh, on the match's map thread, under the plan's lock, and scans nothing of its own.

#include "BgFlagMap.h"
#include "Define.h"
#include "WsgPlanLogic.h"

namespace PvpMatch
{
class State;
}

namespace WsgPlanRec
{
struct Member;
struct TeamPlan;
struct Record;
}

namespace TpPlan
{
// A member died or came back since the last read: Twin Peaks also learns where the dead come back (BT-473): the first read
// after a resurrection is within a few yards of the graveyard it came out at. Called for every member of every map; it only
// keeps the death time on the other maps.
void NoteLifeChange(WsgPlanRec::Record const& r, BgFlagMap::MapData const& map, WsgPlanRec::TeamPlan& team, uint32 teamIndex,
                    WsgPlanRec::Member& m, bool wasAlive, uint32 nowMs);

// The graveyard the enemy team's dead come back at, the one nobody may camp (the graveyard rule). On Twin Peaks it is the one
// the enemy's respawns were seen at (BT-473), the middle graveyard until one was seen; on the other maps the enemy main
// graveyard.
BgFlagMap::Pos EnemyRespawnPoint(WsgPlanRec::Record const& r, BgFlagMap::MapData const& map, uint32 teamIndex);

// The Twin Peaks half of a refresh (BT-474): the enemy carrier's phase, the enemies near our base, the floater, the wiped
// attacks and what each bot does about them. inProgress is false in the preparation (then only the floater is read). `match`
// (may be null) is the shared match state: the lists of the wipe reply and the floater job are published there for the decision
// layer (BT-502), and the answers that stand are read back as the rule's first candidate's replacement.
void Refresh(WsgPlanRec::Record& r, BgFlagMap::MapData const& map, uint32 nowMs, bool inProgress, PvpMatch::State* match);

// Where a MID bot stands: the cut post or the cover post of its team (the centre; the caller adds the spread).
BgFlagMap::Pos MidPostOf(WsgPlanRec::TeamPlan const& team, WsgPlanRec::Member const& me, uint32 teamIndex);

// The Twin Peaks inputs of one bot's decision (state G, state H, the carrier's phase); everything else stays at its default.
void FillContext(WsgLogic::DecisionContext& c, WsgPlanRec::TeamPlan const& team, WsgPlanRec::Member const& me, bool efcKnown);
}

#endif
