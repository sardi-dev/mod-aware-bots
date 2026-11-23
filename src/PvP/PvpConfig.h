/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_PVPCONFIG_H
#define _PLAYERBOT_PVPCONFIG_H

// BT-481: the settings of the shared PvP layers (conf/playerbots.conf.dist, section "Battleground leader and call-outs"),
// read once from the server configuration like the other local settings of the battleground plans.

#include "PvpObservationMath.h"

#include <cstdint>

namespace PvpConfig
{
// AiPlayerbot.Pvp.Leader.Enable: one leader bot per side confirms and calls the stance. With 0 no leader is elected and the
// plans' own call-outs run as before.
bool LeaderEnabled();

// BT-482, the human call-out reader. AiPlayerbot.Pvp.CalloutReader.Enable: a human's short line in the battleground chat ("inc bs
// 3", "help fr", "efc mid", "fc") is read as information (never as a command) and weighed by the plan. With 0 the lines are
// ignored (the replies of AiPlayerbot.BattlegroundChat.Replies need this too).
bool CalloutReaderEnabled();

// .TtlSeconds (15, how long a line counts), .MaxPerSide (32, lines kept per side), .SenderGapMs (2000, one line per sender
// per gap): the limits of the observation store.
PvpObs::Limits ReaderLimits();

// BT-520, AiPlayerbot.Pvp.Awareness.Enable: every bot keeps a tier-filtered book of what it saw the enemy do (trinket, big
// cooldowns, interrupts, diminishing returns). With 0 no event is recorded and the rules that read the book assume ready.
bool AwarenessEnabled();

// BT-521, AiPlayerbot.Pvp.Interrupts.Enable: the PvP interrupt rules (what to kick, when, who goes first). With 0 every shared
// interrupt trigger fires on any casting enemy as before.
bool InterruptRulesEnabled();

// BT-522, AiPlayerbot.Pvp.Defensives.Enable: the PvP defensive and control break rules (smaller defensives early, cheaper
// breaks before the medallion, the medallion by role) in duels, battlegrounds and arenas. With 0 the older health lines and the
// racial triggers stand.
bool DefensiveRulesEnabled();

// BT-523, AiPlayerbot.Pvp.FakeCast.Enable and .ChancePercent (65: the owner's 60 to 70 percent): expert bots start an important
// cast and cancel it when an enemy interrupt that was not seen used is in range. With 0 no cast is faked.
bool FakeCastEnabled();
std::uint32_t FakeCastChancePercent();

// BT-620, AiPlayerbot.Pvp.Bursts.Enable: the big offensive cooldowns are spent in a kill window against an enemy player. With 0 the
// player target keeps the old behaviour (the PvE gates only, so no burst).
bool BurstRulesEnabled();

// BT-628, AiPlayerbot.Pvp.TeamPlay.Enable: the shared team state of an arena or battleground skirmish: the enemy healer's lock and
// the kill window, control chains with one executor and no wasted overlap, partners going in together, and no damage on a target
// held in a breakable control. With 0 the bursts judge the fight by themselves (the local reasons) and no control is cast.
bool TeamPlayEnabled();
}

#endif
