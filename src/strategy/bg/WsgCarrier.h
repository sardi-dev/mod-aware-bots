/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_WSGCARRIER_H
#define _PLAYERBOT_WSGCARRIER_H

// BT-583: Eye of the Storm plays the same rule (the carrier of the one flag runs to a base its team holds).
// The Warsong Gulch flag carrier rule (owner, 2026-11-17; docs/bg-sheets/warsong-gulch.md section 6): a carrier runs home
// at once by the shortest route, uses its defensives and its speed, and fights only when it cannot run (rooted, slowed)
// or has been caught alone. The decision itself is pure (WsgLogic::ChooseCarrier, Utils/WsgPlanLogic.h); this file reads
// the facts from the core and puts the choice into the two places that decide whether a carrier fights.

class Battleground;
class Player;
class PlayerbotAI;

namespace WsgCarrier
{
    // True when the carrier stands and fights now (the class rotation runs), false when it runs. The maps of WsgPlan::Enabled; a bot that
    // is not carrying a flag always gets false. Called by the tactics every tick of a carrier that is being attacked.
    bool Fights(Player* bot, Battleground* bg);

    // The carrier's own help while it runs: a big class defensive when hurt, a speed spell while chased, a freedom spell
    // when rooted or slowed. One spell per call, at most one call a second; never an immunity (Divine Shield, Ice Block and
    // Hand of Protection drop the flag).
    void UseHelpers(PlayerbotAI* botAI, Player* bot, Battleground* bg);
}

#endif
