/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_FAKECAST_H
#define _PLAYERBOT_FAKECAST_H

// BT-523: the game side of the fake cast (Utils/FakeCastPolicy.h, docs/pvp/awareness-r1-r2.md). The class rotation casts as it
// always did; this adds one decision per cast of an EXPERT bot: when the cast starts (first look) the policy rolls whether it is
// a fake, and a fake is cancelled at its point (45 to 85 percent of the cast) if an enemy with a cast stopper is still in range,
// in sight and facing the bot. The cancel is the core's own `Unit::InterruptSpell` of the generic cast (the global cooldown, the
// mana and the spell's cooldown are only taken when a cast completes, so a cancel costs the time of the cast and nothing else),
// and the core tells the other players the cast failed. The rotation then casts again, and the cast after a fake is real.

class Player;
class PlayerbotAI;

namespace FakeCastGlue
{
// The bot's current cast is a fake whose point has come and the threat is still there.
bool CancelDue(PlayerbotAI* botAI, Player* bot);

// Cancels that cast and notes the fake (the next cast within 8 s is real). False when there is nothing to cancel.
bool Cancel(PlayerbotAI* botAI, Player* bot);
}  // namespace FakeCastGlue

#endif
