/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_STUCKMEMBER_H
#define _PLAYERBOT_STUCKMEMBER_H

#include <cstdint>
#include <string>

class Player;
class PlayerbotAI;

// BT-201 (StuckMemberPolicy.h): the [DEFEND] and [FOLLOW] lines for a bot flagged in combat that is far from every
// member and has not moved.
namespace StuckMember
{
// The living member of the bot's group on its map that stands nearest (distance set), or nullptr.
Player* NearestMember(Player* bot, float& distance);
// Who a bot left behind walks to (BT-202): the main tank, else the leader, else the nearest member; nullptr if none.
Player* WalkTarget(PlayerbotAI* botAI);
// Reads the bot's distance to its group (at most every CHECK_SECONDS). True when the two lines are due now.
bool Observe(PlayerbotAI* botAI);
// Writes both lines; `trigger` and `decision` are what the engine did on this tick.
void Report(PlayerbotAI* botAI, std::string const& trigger, std::string const& decision, uint32_t triggersChecked,
           uint32_t triggersFired, bool minimal);
}

#endif
