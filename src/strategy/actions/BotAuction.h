/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * The auction house visit of random bots (design decision 0033 and 0034, docs/economy/auction-wiring.md). A random bot that
 * reaches an auctioneer on its town errand (ErrandPolicy::Errand::Auction) lists what it does not need, buys what it
 * does need under the price caps, and takes its auction mail at a mailbox, all with its own gold and items. Everything
 * is off until AiPlayerbot.Auction.Enable is 1 and, while AiPlayerbot.Auction.OnlyBots names bots, limited to them.
 *
 * Threads. The core's auction manager, a player's inventory and the mail helpers are world thread only, and the bot AI runs
 * on the map threads. So the errand (a map thread) only posts a request and polls a flag; OnUpdate runs from the world
 * update script, where the map threads have finished (the pattern of Arena/ArenaLadder.h), and does at most one core
 * operation per bot per slice, 2 to 5 seconds apart. The decisions are in Utils/BotAuctionPolicy.h,
 * Utils/AuctionVisitPolicy.h, Utils/AuctionMailPolicy.h, Utils/PricePolicy.h and Utils/ReservePolicy.h.
 */

#ifndef MOD_AWARE_BOTS_BOTAUCTION_H
#define MOD_AWARE_BOTS_BOTAUCTION_H

#include "Define.h"

class Player;
class PlayerbotAI;
class WorldObject;

namespace BotAuction
{
    // Map thread. The bot has reached its errand target: the auctioneer (auctionHouse true) or a mailbox. Starts a visit
    // when AuctionVisitPolicy::Check allows it. A bot that fails the check just dwells there as it did before.
    void OnErrandArrival(PlayerbotAI* botAI, Player* bot, WorldObject* target, bool auctionHouse);

    // Map thread. True while this bot's visit is queued or working; the errand keeps the bot at the counter until it ends.
    bool VisitWorking(Player* bot);

    // Map thread. The errand is over; a visit that is still working is cancelled at the next slice.
    void OnErrandEnd(Player* bot);

    // World thread, once from the module's OnStartup: the config line and the warnings about the core's auction rates.
    void OnStartup();

    // World thread, every tick from the world update script (after the map threads).
    void OnUpdate(uint32 diff);

    // World thread. Writes the "[AUCTION] summary" line.
    void LogSummary(char const* why);
}

// Registers the AuctionHouseScript that logs sales and expiries ([AUCTION] sale / expire lines).
void AddSC_bot_auction();

#endif
