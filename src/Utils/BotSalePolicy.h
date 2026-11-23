/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */
#ifndef MOD_AWARE_BOTS_BOTSALEPOLICY_H
#define MOD_AWARE_BOTS_BOTSALEPOLICY_H

#include <cstdint>

// Pure rules of the vendor trip (BT-149, design decision 0016 and 0017: bots earn everything). No game core is used here so
// the unit tests run in seconds; BotSale.cpp turns an Item, a Player and a Creature into these plain values.
namespace BotSalePolicy
{
// BT-149: classify only after the caller has checked the bot's actual inventory,
// quest objectives, known recipes and item usability. Protected items always win.
struct ItemFacts
{
    bool equipped = false;
    bool bag = false;
    bool questItem = false;
    bool professionTool = false;
    bool professionMaterial = false;
    bool reagent = false;
    bool usableFoodOrDrink = false;
    bool usefulGear = false;
    // Heirlooms and artifacts: never sold on their own.
    bool precious = false;
    bool hasSellPrice = false;
    bool poorQuality = false;
    bool gatheredMaterial = false;
    bool unwantedLoot = false;
};

inline bool ShouldSell(ItemFacts const& item)
{
    if (item.equipped || item.bag || item.questItem || item.professionTool || item.professionMaterial ||
        item.reagent || item.usableFoodOrDrink || item.usefulGear || item.precious)
        return false;

    return item.hasSellPrice && (item.poorQuality || item.gatheredMaterial || item.unwantedLoot);
}

// The trade skills of a bot, one bit each; the caller fills it from Player::HasSkill.
enum ProfessionBit : uint32_t
{
    PROFESSION_NONE = 0,
    PROFESSION_ALCHEMY = 1u << 0,
    PROFESSION_BLACKSMITHING = 1u << 1,
    PROFESSION_COOKING = 1u << 2,
    PROFESSION_ENGINEERING = 1u << 3,
    PROFESSION_INSCRIPTION = 1u << 4,
    PROFESSION_JEWELCRAFTING = 1u << 5,
    PROFESSION_LEATHERWORKING = 1u << 6,
    PROFESSION_TAILORING = 1u << 7
};

// What a trade good or gem is, from the core's item class and subclass.
enum class MaterialKind : uint8_t
{
    Other,
    Herb,
    Metal,
    Leather,
    Cloth,
    Meat,
    Gem
};

// The materials a bot gathers or loots and may sell: herbs, ore and bars (metal and stone), leather, cloth, meat.
inline bool IsGatheredMaterial(MaterialKind kind)
{
    return kind == MaterialKind::Herb || kind == MaterialKind::Metal || kind == MaterialKind::Leather ||
           kind == MaterialKind::Cloth || kind == MaterialKind::Meat;
}

// Which of the bot's own professions work this material: herbs for Alchemy and Inscription, ore and bars for
// Blacksmithing, Engineering and Jewelcrafting, leather for Leatherworking, cloth for Tailoring, meat for Cooking,
// gems for Jewelcrafting. Mining and Herbalism alone use nothing, so what they gather is for sale.
inline bool ProfessionUses(uint32_t professions, MaterialKind kind)
{
    switch (kind)
    {
        case MaterialKind::Herb:
            return professions & (PROFESSION_ALCHEMY | PROFESSION_INSCRIPTION);
        case MaterialKind::Metal:
            return professions & (PROFESSION_BLACKSMITHING | PROFESSION_ENGINEERING | PROFESSION_JEWELCRAFTING);
        case MaterialKind::Leather:
            return professions & PROFESSION_LEATHERWORKING;
        case MaterialKind::Cloth:
            return professions & PROFESSION_TAILORING;
        case MaterialKind::Meat:
            return professions & PROFESSION_COOKING;
        case MaterialKind::Gem:
            return professions & PROFESSION_JEWELCRAFTING;
        case MaterialKind::Other:
            break;
    }
    return false;
}

// Bots never craft yet, so what a profession "uses" is kept only up to a cap per item; the rest is sold. The same for
// the food and drink a bot keeps. keptSoFar counts the items of that entry already kept in this pass.
constexpr uint32_t PROFESSION_MATERIAL_KEEP_MAX = 60;
constexpr uint32_t FOOD_DRINK_KEEP_MAX = 40;

inline bool KeepStack(uint32_t keptSoFar, uint32_t cap) { return keptSoFar < cap; }

// The "bags almost full" level: the same 80 percent at which gathering stops.
constexpr uint32_t BAGS_ALMOST_FULL_PERCENT = 80;

inline bool BagsAlmostFull(uint32_t percentUsed) { return percentUsed >= BAGS_ALMOST_FULL_PERCENT; }

// One info log line per trip, and at most one per bot in this time.
constexpr uint32_t LOG_MIN_INTERVAL_MS = 60 * 1000;

inline bool ShouldLogTrip(uint32_t msSinceLastLog, bool loggedBefore)
{
    return !loggedBefore || msSinceLastLog >= LOG_MIN_INTERVAL_MS;
}

// Supplies (food and drink) bought at the vendor: the bot buys when it holds fewer than SUPPLY_LOW_WATER of the kind
// and fills up to SUPPLY_STACK_TARGET, with its own money, never below the reserve kept for repairs.
constexpr uint32_t SUPPLY_LOW_WATER = 5;
constexpr uint32_t SUPPLY_STACK_TARGET = 20;
constexpr uint64_t COPPER_PER_GOLD = 10000;
constexpr uint32_t RESERVE_LEVELS_PER_GOLD = 10;
// A bot with less than this above its reserve does not walk to a vendor for supplies.
constexpr uint64_t SUPPLY_MIN_SPARE_COPPER = 100;

// BT-213: repair costs grow with the item level, so the reserve grows with the square of the bot's level (5 copper
// per level squared: 5 silver at level 10, 18 at 19, 4 gold at 90), never above the old one gold per 10 levels. The
// flat gold per 10 levels kept 1 gold at levels 10 to 19, more than most bots of that band own (live 2026-11-16: 57
// of 95 mana users), and none of them ever bought food or drink.
constexpr uint64_t RESERVE_COPPER_PER_LEVEL_SQUARED = 5;

inline uint64_t PurchaseReserve(uint32_t level)
{
    uint64_t const perTenLevels = uint64_t(level / RESERVE_LEVELS_PER_GOLD) * COPPER_PER_GOLD;
    uint64_t const byRepairCost = uint64_t(level) * level * RESERVE_COPPER_PER_LEVEL_SQUARED;
    return byRepairCost < perTenLevels ? byRepairCost : perTenLevels;
}

// Fewer than the low water left is what starts a trip before the bags are full (BT-178: before, only "none left", so
// a caster could leave for a dungeon with one drink).
inline bool SupplyLow(uint32_t held) { return held < SUPPLY_LOW_WATER; }

inline bool CanAffordSupplies(uint64_t money, uint64_t reserve)
{
    return money > reserve && money - reserve >= SUPPLY_MIN_SPARE_COPPER;
}

// BT-213: what a bot low on food or drink does about it. A player short of money sells the junk in the bags first;
// one with nothing worth selling waits until loot or quests pay. sellValue is what the bot's sellable items fetch
// (the caller adds it up only when the money alone does not do).
enum class SupplyPlan : uint8_t
{
    None,
    Buy,
    SellThenBuy,
    CannotPay
};

inline SupplyPlan PlanSupplies(bool supplyLow, uint64_t money, uint64_t reserve, uint64_t sellValue)
{
    if (!supplyLow)
        return SupplyPlan::None;

    if (CanAffordSupplies(money, reserve))
        return SupplyPlan::Buy;

    return CanAffordSupplies(money + sellValue, reserve) ? SupplyPlan::SellThenBuy : SupplyPlan::CannotPay;
}

inline bool SupplyTripWanted(SupplyPlan plan) { return plan == SupplyPlan::Buy || plan == SupplyPlan::SellThenBuy; }

// BT-213: when food and drink are both low, the kind bought first may spend only half of the money above the reserve,
// so a poor bot leaves the vendor with some of each instead of a full stack of one and none of the other.
inline uint64_t FirstKindMoney(uint64_t money, uint64_t reserve, bool otherKindLow)
{
    if (!otherKindLow || money <= reserve)
        return money;

    return reserve + (money - reserve) / 2;
}

// How many single items to buy: up to the target when fewer than the low water are held, as many as the money above
// the reserve pays for. unitPrice is what one purchase costs after the reputation discount (0 means free).
inline uint32_t SupplyUnitsToBuy(uint32_t held, uint64_t money, uint64_t reserve, uint64_t unitPrice)
{
    if (held >= SUPPLY_LOW_WATER)
        return 0;

    uint32_t const wanted = SUPPLY_STACK_TARGET - held;
    if (!unitPrice)
        return wanted;

    if (money <= reserve)
        return 0;

    uint64_t const affordable = (money - reserve) / unitPrice;
    return affordable < wanted ? uint32_t(affordable) : wanted;
}

// BT-178 (owner decision 2026-11-13): a mana user shops for drink before the Dungeon Finder takes it. One on its way
// to a vendor is not offered to the finder until the trip ends (bounded by the trip's own timeout); one low on drink
// that cannot shop now (no money, no vendor in sight) is offered after every bot that holds drink, so the finder never
// waits for it.
enum class FinderSupply : uint8_t
{
    Ready,
    Shopping,
    NoMoney,
    NoVendor
};

inline FinderSupply FinderSupplyOf(bool manaUser, bool conjuresOwn, uint32_t drinkHeld, bool canAfford, bool onTrip)
{
    if (!manaUser || conjuresOwn || !SupplyLow(drinkHeld))
        return FinderSupply::Ready;

    if (onTrip)
        return FinderSupply::Shopping;

    return canAfford ? FinderSupply::NoVendor : FinderSupply::NoMoney;
}

inline bool FinderMayTake(FinderSupply supply) { return supply != FinderSupply::Shopping; }

// Bots low on drink come after every ready bot of the same order.
inline bool FinderTakesLast(FinderSupply supply)
{
    return supply == FinderSupply::NoMoney || supply == FinderSupply::NoVendor;
}

inline char const* FinderSupplyReason(FinderSupply supply)
{
    switch (supply)
    {
        case FinderSupply::Ready:
            return "ready";
        case FinderSupply::Shopping:
            return "shopping";
        case FinderSupply::NoMoney:
            return "no money";
        case FinderSupply::NoVendor:
            return "no vendor in sight";
    }
    return "";
}
}
#endif
