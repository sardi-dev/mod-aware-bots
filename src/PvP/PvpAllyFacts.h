/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_PVPALLYFACTS_H
#define _PLAYERBOT_PVPALLYFACTS_H

// BT-657 and BT-668: what a healer sees of its own side in a fight against players, shared by the Mistweaver and Restoration Druid
// packages: the allies a heal can reach, the players that are hitting one of them and whether one of those shows a big offensive
// cooldown (an aura on the enemy, public like a unit frame). Map thread only (bot AI).

#include "Define.h"

#include <vector>

class Player;
class PlayerbotAI;
class Unit;

namespace PvpAllyFacts
{
// The bot and the group members that a heal can reach (alive, on the map, within `range` and in sight).
std::vector<Player*> Reachable(Player* bot, float range);

// BT-699: the same facts read once per AI tick (PlayerbotAI::aiTickSerial) instead of once per trigger. The healer rules asked
// for the group with a line-of-sight ray per member five or six times a tick before they knew whether a fight against
// players was on at all.
// A battleground, a duel, or a group member within `range` (in sight or not) with a player on him. False means
// InPvpFight over Reachable(bot, range) is false too, because Reachable is a subset of these members.
bool PvpFightNear(PlayerbotAI* botAI, Player* bot, float range);
// Reachable(bot, range), built once per tick. The reference stays valid until the bot's next AI tick.
std::vector<Player*> const& ReachableCached(PlayerbotAI* botAI, Player* bot, float range);

// The attackable players that are hitting `ally`; `burstSeen` is set when one of them shows a big offensive cooldown.
int32 PlayerAttackers(Player* bot, Unit* ally, bool& burstSeen);

// A battleground, a duel, or an ally with a player on him.
bool InPvpFight(Player* bot, std::vector<Player*> const& allies);

// BT-682: the control casts of hostile players (a sheep, a fear, a hex, a sleep: not a heal, not a damage spell) that are on their way
// to the bot or to one of `allies`, with what is left of each cast (the bot's first look at the cast starts the clock, so a cast the
// bot has just noticed counts as nearly whole). Public state only: a cast bar is on every unit frame.
struct ControlCast
{
    Unit* caster = nullptr;
    Player* aimed = nullptr;
    uint32 leftMs = 0;
};
std::vector<ControlCast> IncomingControl(PlayerbotAI* botAI, Player* bot, std::vector<Player*> const& allies, float watchYards);

// Drops the bot's cast tracks (logout).
void ForgetTracks(uint32 guidLow);

float LowestPercent(std::vector<Player*> const& allies);
Player* Weakest(std::vector<Player*> const& allies);
bool Silenced(Unit* unit);
}  // namespace PvpAllyFacts

#endif
