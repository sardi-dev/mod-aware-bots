/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * Alt bots buy their gear upgrades like a player (design decision 0021): when a vendor they can see sells an
 * upgrade for their spec that they can pay for, with money, currency or reputation, they walk to it and buy it.
 * Nothing is handed out; the money and currency are their own.
 */

#ifndef MOD_AWARE_BOTS_BOTSHOPPING_H
#define MOD_AWARE_BOTS_BOTSHOPPING_H

#include "Define.h"

class Creature;
class Player;
class PlayerbotAI;

namespace BotShopping
{
    // AiPlayerbot.AltBotsShopForGear (default 1).
    bool Enabled();

    // AiPlayerbot.RandomBotsShopForGear (default 1): random bots buy gear upgrades at the vendor they visit for supplies.
    bool RandomBotsEnabled();

    // A random bot standing at a vendor (sell or repair trip) buys the gear upgrades for its spec that the vendor sells
    // and its gear budget pays for, and equips them, as a player does while there. True when it spent money.
    bool BuyRecipesAt(PlayerbotAI* botAI, Player* bot, Creature* vendor);
    bool BuyUpgradesAt(PlayerbotAI* botAI, Player* bot, Creature* vendor);

    // BT-218: a random bot that has trained riding and knows no mount yet (RandomBotsShopForGear on).
    bool WantsMount(Player* bot);

    // Whether the bot knows any spell that mounts it (a learned mount, as the mount purchase checks).
    bool KnowsMount(Player* bot);

    // The nearest vendor within Range() that sells a mount the bot may use and can pay for above its repair reserve, or
    // nullptr. Kept for a few seconds per bot.
    Creature* FindMountVendor(PlayerbotAI* botAI, Player* bot);

    // Buys the cheapest such mount at this vendor and learns it, as using the item does. True when it did.
    bool BuyMountAt(Player* bot, Creature* vendor);

    // AiPlayerbot.AltBotVendorRange: how far an alt walks to a vendor it can see, in yards (default 60).
    float Range();

    // Gear upgrades for the bot's spec that this vendor sells and the bot can pay for now.
    uint32 CountUpgradesAt(PlayerbotAI* botAI, Player* bot, Creature* vendor);

    // The nearest living, friendly vendor within Range() that sells the bot an upgrade it can pay for, or nullptr.
    // The answer is kept for a few seconds per bot, since checking a vendor's list is expensive.
    Creature* FindNearbyVendor(PlayerbotAI* botAI, Player* bot);

    // Forget the kept answer, after a purchase.
    void Forget(Player* bot);
}

#endif
