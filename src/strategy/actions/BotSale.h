/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * BT-149, design decision 0016 and 0017: random bots earn everything, and selling is how they pay for repairs and supplies.
 * A random bot whose bags are almost full, or who is low on food (or, for a mana user, drink), walks to a vendor
 * it can see, sells what it does not need and buys food and drink with its own money. The rules that decide what is
 * kept are in Utils/BotSalePolicy.h; this file reads the bot and the items and applies them.
 */

#ifndef MOD_AWARE_BOTS_BOTSALE_H
#define MOD_AWARE_BOTS_BOTSALE_H

#include "BotAuctionPolicy.h"
#include "BotSalePolicy.h"
#include "Define.h"

#include <vector>

class Creature;
class Item;
struct ItemTemplate;
class Player;
class PlayerbotAI;

namespace BotSale
{
    // A random bot that may leave what it does now for a vendor: alive, out of combat, in the open world, not in a
    // battleground or arena, and not following a real player.
    bool IsEligible(PlayerbotAI* botAI, Player* bot);

    // A vendor trip is due: the bags are almost full with things to sell, or the food or drink is low and the bot can
    // pay for more, with its money or by selling junk first (BT-213). Evaluated at most every few seconds per bot (the
    // item checks are not free), once a minute for a bot that cannot pay; true all along a trip that has begun.
    bool TripWanted(PlayerbotAI* botAI, Player* bot);

    // The nearest living, friendly vendor within 60 yards that the bot can see, or nullptr. When the bot needs food
    // or drink, one that sells some it can use comes first. Kept for a few seconds per bot.
    Creature* FindNearbyVendor(PlayerbotAI* botAI, Player* bot);

    // The bag items that BotSalePolicy::ShouldSell lets go, with every protection applied (equipped, bags, quest
    // items, tools, the bot's own professions' materials, food and drink, useful gear).
    std::vector<Item*> SelectItemsToSell(PlayerbotAI* botAI, Player* bot);

    bool IsJunk(PlayerbotAI* botAI, Player* bot, Item* item);
    Item* LowestJunk(PlayerbotAI* botAI, Player* bot, ItemTemplate const* incoming, uint64 excludedGuid);
    uint32 FreeBagSlots(Player* bot);

    // The trade skills of the bot as BotSalePolicy::ProfessionBit flags.
    uint32 ProfessionsOf(Player* bot);

    // The protections of one bag item as the vendor trip reads them, before the keep caps on materials and food (a caller
    // that looks at several stacks applies the caps across them, as BotAuctionPolicy::IsListable does).
    BotSalePolicy::ItemFacts FactsOf(Player* bot, Item* item, uint32 professions);

    // The item usage value's veto on selling what ShouldSell lets go: a quest item, gear to equip or replace, a recipe the
    // bot can learn or use. Asked only for items that would be sold, since the value is not free.
    bool UsageKeeps(PlayerbotAI* botAI, ItemTemplate const* proto, Item const* item = nullptr);

    // The auction's level of use of an item for this bot: Crafting when one of its crafting professions has a recipe it
    // can make with the item, Secondary for a secondary skill's recipe, Gathered for a material it gathers itself, None
    // otherwise (BotAuctionPolicy::MaterialUse). The recipes are read from the client data once (skill line abilities and
    // their reagents), not guessed from the item class.
    BotAuctionPolicy::MaterialUse MaterialUseOf(Player* bot, ItemTemplate const* proto);

    // Food or drink the bot can use at its level.
    bool IsUsableSupply(Player* bot, ItemTemplate const* proto);

    // How many items of this item's kind (food or drink) the bot carries; 0 for an item that is neither.
    uint32 SupplyHeld(Player* bot, ItemTemplate const* proto);

    // Buys food and drink fit for the bot's level (drink only for a mana user, and first, with half of the spare money
    // when food is low too) from the vendor it stands at, up to BotSalePolicy::SUPPLY_STACK_TARGET each when it holds
    // fewer than SUPPLY_LOW_WATER, with its own money and never below the repair reserve. Returns the items bought.
    uint32 BuySupplies(PlayerbotAI* botAI, Player* bot, Creature* vendor, bool reserveSlot = false);

    // BT-219: buys the profession tools the bot lacks (mining pick, skinning knife, blacksmith hammer, arclight spanner)
    // when this vendor sells them, with its own money above the repair reserve. Returns the tools bought.
    uint32 BuyTools(Player* bot, Creature* vendor, bool reserveSlot = false);

    // The trip's bookkeeping: Begin on the first step, End when the bot is done at the vendor (or gave up).
    void BeginTrip(Player* bot);
    void EndTrip(Player* bot, bool productive);

    // The "[SELL]" info line, at most once a minute per bot.
    void LogSale(Player* bot, uint32 items, uint64 money);

    // BT-178: whether the Dungeon Finder may take this bot now as far as drink goes (BotSalePolicy::FinderSupplyOf);
    // drinkHeld is the drink it carries (0 for a bot that needs none). World thread, after the map updates.
    BotSalePolicy::FinderSupply FinderSupplyOf(PlayerbotAI* botAI, Player* bot, uint32& drinkHeld);

    // The "[SUPPLY] ... finder=" info line (what the finder did with a bot low on drink and why); the line of a bot
    // held back while it shops at most once a minute per bot.
    void LogFinderSupply(Player* bot, BotSalePolicy::FinderSupply supply, uint32 drinkHeld, char const* what);
}

#endif
