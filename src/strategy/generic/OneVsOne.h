/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_ONEVSONE_H
#define _PLAYERBOT_ONEVSONE_H

// Game side of the 1v1 engine (Utils/OneVsOnePolicy.h, docs/duels/one-vs-one.md): finds the one enemy player a bot
// fights, reads the facts a player would see, asks the policy, and picks the class spell for the chosen option. The
// "1v1" strategy (OneVsOneStrategy) runs it in every combat engine; the same entry serves duels, open-world fights and
// isolated battleground skirmishes. Arenas stay with the arena tactics.

#include "OneVsOnePolicy.h"

class Item;
class Player;
class PlayerbotAI;

namespace OneVsOneGlue
{
// The single enemy player this bot fights, or nullptr: its started duel's opponent; else a hostile player it targets
// with no other enemy player within ENEMY_SCAN_YARDS. Never in an arena, mounted or in flight, while carrying a
// battleground objective, or for a healer in a group (it stays with its group).
Player* LoneEnemy(PlayerbotAI* botAI, Player* bot);

// True when the "1v1" strategy is on and the bot is in a 1v1 now: the class edges that would do the same thing outside
// the tier rules (Disengage on contact, Frost Nova on a near player, circling behind a player) stand down.
bool Owns(PlayerbotAI* botAI, Player* bot);

struct Decision
{
    OneVsOne::Option option{ OneVsOne::Choice::HOLD, 0, "" };
    OneVsOne::KitSpell spell{ 0, OneVsOne::Kind::SLOW, 0, OneVsOne::Aim::ENEMY, 0.0f, 0.0f };
    bool hasSpell = false;
    bool trinket = false;       // the spell is a PvP trinket's item spell: use the item, not the spell
    Item* trinketItem = nullptr;
    float stepBack = 0.0f;
    // The back-off point (the choice STEP_BACK) and the cover point (the choice COVER), when the policy chose them.
    bool hasPoint = false;
    float pointX = 0.0f;
    float pointY = 0.0f;
    float pointZ = 0.0f;
    Player* enemy = nullptr;
};

// Remembers that the bot just cast a speed spell, so the engine does not cast it again while it runs.
void NoteSpeedCast(Player* bot);

// False while the 1v1 engine owns the fight and the bot has not yet noticed it lost control (the tier's reaction time,
// and a slipping window for good and casual bots): the racial triggers (Every Man for Himself, Will of the Forsaken,
// Escape Artist) wait for it so that the tiers differ. True outside a 1v1.
bool CcBreakAllowed(PlayerbotAI* botAI, Player* bot);

// Reads the facts, asks the policy and fills `out`. True when the engine acts (anything but HOLD and CLOSE).
bool Decide(PlayerbotAI* botAI, Player* bot, Decision& out);
}  // namespace OneVsOneGlue

#endif
