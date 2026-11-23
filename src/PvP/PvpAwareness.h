/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_PVPAWARENESS_H
#define _PLAYERBOT_PVPAWARENESS_H

// BT-520: the game side of the per-bot awareness book (Utils/PvpAwarenessPolicy.h, docs/pvp/awareness-r1-r2.md).
//
// Hook. The core reports every spell a player casts to PlayerScript::OnSpellCast, at the moment the cast completes (an
// instant spell: when it is cast; a cast-time spell: when the bar fills), BEFORE the cast is checked, so the hook alone is
// only an attempt. The hook keeps the spells of the catalog (Utils/PvpSpellCatalog.h) cast in a PvP context and queues one
// small immutable event, with the caster's position, under a short lock. Nothing else is read at that moment.
//
// Reading. A bot asks for its book through ViewOf() when a rule needs it. That call, on the bot's own map thread, takes the
// events it has not read for its map instance, waits a quarter second for each to settle, and then confirms what a player
// would have seen: a cooldown that started (the cast went off), an aura on the target (the crowd control landed, with its
// duration). Only then are the sight facts (range, line of sight, detectability from where the BOT stands) put to the pure
// book, which applies the tier (what it notices, one stable roll per event). A cast that failed, or that the bot could not
// have seen, never reaches the book.
//
// What is NOT covered: casts of pets and totems (the core has no hook for them), and the results of spells the hook does not
// see (an item the bot cannot name). The trinket is booked as an attempt: the medallion's own cooldown is not readable.
//
// Threads. The queue has its own lock and is the only shared state; a book is read and written by its bot's map thread only.
// The registry of books has its own lock, never held while a book is read.

#include "Define.h"
#include "PvpAwarenessPolicy.h"

#include <memory>

class Player;

namespace PvpAwareness
{
// The bot's book, current to now. nullptr when the awareness is switched off, or the bot is not in the world.
std::shared_ptr<PvpAware::Book> ViewOf(Player* bot);

// Drops the bot's book (logout, a change of tier).
void Forget(uint32 guidLow);

// Events queued and books held (the status line and the tests of the glue).
std::size_t QueuedEvents();
std::size_t BookCount();
}  // namespace PvpAwareness

void AddSC_pvp_awareness();

#endif
