/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * Bots repair their gear like a player (design decision 0016 and 0017): when their equipped gear is worn they
 * walk to a repair NPC they can see, sell their junk if it is also a vendor, and pay for the repair with their own
 * money. Nothing is repaired for free.
 */

#ifndef MOD_AWARE_BOTS_BOTREPAIR_H
#define MOD_AWARE_BOTS_BOTREPAIR_H

#include "Define.h"

class Creature;
class Player;
class PlayerbotAI;

namespace BotRepair
{
    // Equipped gear is worn: an item is broken, or the gear's total durability is below 30%. Bags do not count.
    bool NeedsRepair(Player* bot);

    // The nearest living, friendly repair NPC the bot can see, within 60 yards, or nullptr. The answer is kept for a
    // few seconds per bot, since the trigger asks every tick while the bot walks.
    Creature* FindNearbyRepairer(PlayerbotAI* botAI, Player* bot);

    // Forget the kept answer, after a visit.
    void Forget(Player* bot);
}

#endif
