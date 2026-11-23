/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */
#ifndef MOD_AWARE_BOTS_PRICEPOLICY_H
#define MOD_AWARE_BOTS_PRICEPOLICY_H

#include <algorithm>
#include <cstdint>
#include <vector>

// Pure rules of the bots' auction house prices (design decision 0033 and 0034, docs/economy/pricing-audit.md). Every item that
// can be listed has one price range (minimum and maximum copper per unit) from the table the module loads at start
// (Manager/PriceTable.h). This header decides where inside the range a bot lists, how high it buys, and what the
// auction house costs it. No game core is used, so the unit tests run in seconds; money is in copper and every sum
// is checked, so an overflow means "do not trade", never a wrong price.
namespace PricePolicy
{
// Where an item mostly comes from; the table stores it as a number (tools/economy/export_price_table.py PRICE_CLASS_IDS).
enum class PriceClass : uint8_t
{
    Vendor = 1,
    Gathered,
    Crafted,
    Recipe,
    WorldNormal,
    WorldElite,
    WorldRare,
    WorldChest,
    DungeonTrash,
    DungeonBoss,
    RaidTrash,
    RaidBoss,
    Container,
    QuestReward,
    VendorToken,
    Other
};

constexpr uint8_t PRICE_CLASS_FIRST = 1;
constexpr uint8_t PRICE_CLASS_LAST = 16;

constexpr bool IsKnownPriceClass(uint32_t value) { return value >= PRICE_CLASS_FIRST && value <= PRICE_CLASS_LAST; }

// Owner rule (design decision 0034 point 7): bots ignore items whose ceiling is above 10,000 gold.
constexpr uint64_t COPPER_PER_GOLD = 10000;
constexpr uint64_t TRADE_CAP_COPPER = 10000 * COPPER_PER_GOLD;

struct PriceRange
{
    uint32_t minCopper = 0;
    uint32_t maxCopper = 0;
    PriceClass priceClass = PriceClass::Other;
};

// A usable range: a positive floor, a ceiling at or above it, at or under the trading cap, a known class.
inline bool IsTradable(PriceRange const& range)
{
    return range.minCopper > 0 && range.maxCopper >= range.minCopper && range.maxCopper <= TRADE_CAP_COPPER &&
           IsKnownPriceClass(uint32_t(range.priceClass));
}

// ---------------------------------------------------------------------------------------------------------------
// Checked arithmetic. All return false on overflow and leave the result unspecified.
// ---------------------------------------------------------------------------------------------------------------
inline bool MulChecked(uint64_t a, uint64_t b, uint64_t& out)
{
    if (a && b > UINT64_MAX / a)
        return false;

    out = a * b;
    return true;
}

inline bool AddChecked(uint64_t a, uint64_t b, uint64_t& out)
{
    if (b > UINT64_MAX - a)
        return false;

    out = a + b;
    return true;
}

// value * percent / 100 without overflow, rounded down.
constexpr uint64_t PercentOf(uint64_t value, uint32_t percent) { return value / 100 * percent + value % 100 * percent / 100; }

// Total of a price per unit and a count.
inline bool TotalPrice(uint64_t unitCopper, uint32_t count, uint64_t& out) { return MulChecked(unitCopper, count, out); }

// Price per unit of a whole auction (a stack bought for `total`), rounded up so a comparison with a unit cap is never
// too generous. 0 for a count of 0.
inline uint64_t UnitPriceCeil(uint64_t total, uint32_t count)
{
    return count ? total / count + (total % count ? 1 : 0) : 0;
}

// ---------------------------------------------------------------------------------------------------------------
// The table: sorted rows and a lookup. The loader fills the rows; invalid rows are dropped, never trusted.
// ---------------------------------------------------------------------------------------------------------------
struct PriceRow
{
    uint32_t item = 0;
    PriceRange range;
};

// Drops rows with an unusable range and duplicate items (the first one wins), sorts by item. Returns the rows dropped.
inline uint32_t Normalize(std::vector<PriceRow>& rows)
{
    size_t const before = rows.size();
    rows.erase(std::remove_if(rows.begin(), rows.end(), [](PriceRow const& row) { return !row.item || !IsTradable(row.range); }),
               rows.end());
    std::stable_sort(rows.begin(), rows.end(), [](PriceRow const& a, PriceRow const& b) { return a.item < b.item; });
    rows.erase(std::unique(rows.begin(), rows.end(), [](PriceRow const& a, PriceRow const& b) { return a.item == b.item; }),
               rows.end());
    return uint32_t(before - rows.size());
}

// rows must come from Normalize. false means "do not trade this item".
inline bool FindRange(std::vector<PriceRow> const& rows, uint32_t item, PriceRange& out)
{
    auto const itr = std::lower_bound(rows.begin(), rows.end(), item, [](PriceRow const& row, uint32_t id) { return row.item < id; });
    if (itr == rows.end() || itr->item != item)
        return false;

    out = itr->range;
    return true;
}

// ---------------------------------------------------------------------------------------------------------------
// Listing: where in the range a new listing lands.
// ---------------------------------------------------------------------------------------------------------------
// Start and width of the part of the range a roll picks from, by how many listings of the same item the house shows:
// an empty market lists in the upper middle, a thin one lower, a crowded one in the bottom quarter.
struct Band
{
    double start;
    double width;
};

constexpr Band BAND_EMPTY_MARKET{ 0.35, 0.45 };
constexpr Band BAND_THIN_MARKET{ 0.15, 0.50 };
constexpr Band BAND_CROWDED_MARKET{ 0.00, 0.25 };
constexpr uint32_t THIN_MARKET_LISTINGS = 1;
constexpr uint32_t CROWDED_MARKET_LISTINGS = 3;

// A listing goes 1 to 5 percent under the cheapest one already there (when that one is above the minimum).
constexpr double UNDERCUT_MIN = 0.01;
constexpr double UNDERCUT_SPAN = 0.04;

// design decision 0034 point 6: the whole range shifts by up to 10 percent of its width per day and price class.
constexpr double DAILY_MOOD_SPAN = 0.10;

struct Market
{
    uint32_t cheapestUnit = 0;   // copper per unit of the cheapest listing of this item on the house, 0 when none
    uint32_t listings = 0;       // how many listings of this item the house shows
};

namespace detail
{
inline double Clamp(double value, double low, double high)
{
    // NaN fails both comparisons and falls to `low`.
    return value >= low ? (value <= high ? value : high) : low;
}

inline Band BandFor(uint32_t listings)
{
    if (listings >= CROWDED_MARKET_LISTINGS)
        return BAND_CROWDED_MARKET;
    if (listings >= THIN_MARKET_LISTINGS)
        return BAND_THIN_MARKET;
    return BAND_EMPTY_MARKET;
}
}  // namespace detail

// A number in [-1, 1] that is the same all day for one price class and moves from day to day; dayNumber is any count of
// days (game time divided by 86400). Deterministic, so every bot of the day sees the same market.
inline double DailyMood(uint32_t dayNumber, PriceClass priceClass)
{
    uint64_t x = (uint64_t(dayNumber) << 8) ^ uint64_t(priceClass);
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    x ^= x >> 31;
    return double(x >> 40) / double((uint64_t(1) << 24) - 1) * 2.0 - 1.0;
}

// The unit price of a new listing, in copper: never below the range minimum, never above its maximum. roll and
// undercutRoll are uniform numbers in [0, 1), mood is DailyMood (any number is clamped to [-1, 1]). 0 means "do not
// list" (an unusable range).
inline uint32_t ListingPrice(PriceRange const& range, Market const& market, double roll, double undercutRoll, double mood)
{
    if (!IsTradable(range))
        return 0;

    Band const band = detail::BandFor(market.listings);
    double const position = detail::Clamp(
        band.start + band.width * detail::Clamp(roll, 0.0, 1.0) + detail::Clamp(mood, -1.0, 1.0) * DAILY_MOOD_SPAN, 0.0, 1.0);
    double const low = double(range.minCopper);
    double price = low + position * double(range.maxCopper - range.minCopper);
    if (market.cheapestUnit > range.minCopper)
        price = std::min(price, double(market.cheapestUnit) *
                                    (1.0 - UNDERCUT_MIN - UNDERCUT_SPAN * detail::Clamp(undercutRoll, 0.0, 1.0)));

    // Rounded to the nearest copper after the clamp, so the answer is still inside the range.
    price = detail::Clamp(price, low, double(range.maxCopper));
    return uint32_t(price + 0.5);
}

// ---------------------------------------------------------------------------------------------------------------
// Buying: what a bot needs the item for sets the highest price it pays, as a share of the range above its minimum.
// ---------------------------------------------------------------------------------------------------------------
enum class BuyNeed : uint8_t
{
    None,                // not wanted: never bought
    GearUpgrade,
    Bag,
    ProfessionMaterial,  // for the bot's own profession only
    Consumable
};

constexpr uint32_t BUY_CAP_GEAR_PERCENT = 50;
constexpr uint32_t BUY_CAP_BAG_PERCENT = 50;
constexpr uint32_t BUY_CAP_MATERIAL_PERCENT = 40;
constexpr uint32_t BUY_CAP_CONSUMABLE_PERCENT = 35;

constexpr uint32_t BuyCapPercent(BuyNeed need)
{
    switch (need)
    {
        case BuyNeed::GearUpgrade:
            return BUY_CAP_GEAR_PERCENT;
        case BuyNeed::Bag:
            return BUY_CAP_BAG_PERCENT;
        case BuyNeed::ProfessionMaterial:
            return BUY_CAP_MATERIAL_PERCENT;
        case BuyNeed::Consumable:
            return BUY_CAP_CONSUMABLE_PERCENT;
        case BuyNeed::None:
            break;
    }
    return 0;
}

// Highest unit price a bot pays for an item it needs; 0 for a need that is not wanted or an unusable range.
inline uint32_t BuyCap(PriceRange const& range, BuyNeed need)
{
    uint32_t const percent = BuyCapPercent(need);
    if (!percent || !IsTradable(range))
        return 0;

    return uint32_t(range.minCopper + uint64_t(range.maxCopper - range.minCopper) * percent / 100);
}

// ---------------------------------------------------------------------------------------------------------------
// The visit budget (owner rule 2026-11-22): at most 25 percent of the gold above the reserve per visit to the auction
// house. Deposits count as spending. A bid locks its gold at once, and a refund that arrives later does not give the
// visit more to spend.
// ---------------------------------------------------------------------------------------------------------------
constexpr uint32_t VISIT_SPEND_PERCENT = 25;

struct Wallet
{
    uint64_t budget = 0;      // what this visit may spend in all, fixed on arrival
    uint64_t committed = 0;   // deposits and purchases so far
};

// moneyOnArrival and reserve are copper; the reserve is BotSalePolicy::PurchaseReserve of the bot's level.
inline Wallet MakeWallet(uint64_t moneyOnArrival, uint64_t reserve)
{
    Wallet wallet;
    wallet.budget = moneyOnArrival > reserve ? PercentOf(moneyOnArrival - reserve, VISIT_SPEND_PERCENT) : 0;
    return wallet;
}

inline uint64_t BudgetLeft(Wallet const& wallet)
{
    return wallet.committed < wallet.budget ? wallet.budget - wallet.committed : 0;
}

// Whether spending `amount` more fits the visit budget and leaves the bot's gold at or above its reserve right now.
inline bool CanSpend(Wallet const& wallet, uint64_t moneyNow, uint64_t reserve, uint64_t amount)
{
    uint64_t needed = 0;
    return amount <= BudgetLeft(wallet) && AddChecked(reserve, amount, needed) && moneyNow >= needed;
}

// Records the spending when CanSpend allows it; false and no change otherwise.
inline bool Spend(Wallet& wallet, uint64_t moneyNow, uint64_t reserve, uint64_t amount)
{
    if (!CanSpend(wallet, moneyNow, reserve, amount))
        return false;

    wallet.committed += amount;
    return true;
}

// The buy decision, the same for a listing of a bot and of a human (there is no seller in the call on purpose).
// totalCopper is what the bot would pay now for the whole listing (the buyout, or the bid when it is a bid) and
// quantity the items in it; the unit price is compared with BuyCap.
inline bool WillBuy(uint64_t totalCopper, uint32_t quantity, PriceRange const& range, BuyNeed need, Wallet const& wallet,
                    uint64_t moneyNow, uint64_t reserve)
{
    if (!totalCopper || !quantity)
        return false;

    uint32_t const cap = BuyCap(range, need);
    if (!cap || UnitPriceCeil(totalCopper, quantity) > cap)
        return false;

    return CanSpend(wallet, moneyNow, reserve, totalCopper);
}

// ---------------------------------------------------------------------------------------------------------------
// What the house costs the seller (core: AuctionHouseMgr::GetAuctionDeposit and AuctionEntry::GetAuctionCut). The
// percentages come from the house's AuctionHouse.dbc row: faction houses 5 and 5, the neutral house 25 and 15.
// ---------------------------------------------------------------------------------------------------------------
struct HouseRates
{
    uint32_t depositPercent = 0;
    uint32_t cutPercent = 0;
};

enum class House : uint8_t
{
    Alliance,
    Horde,
    Neutral
};

// design decision 0034 point 10: a bot uses only the house of its own faction, never the neutral ones.
constexpr bool HouseAllowed(House house, bool botIsAlliance)
{
    return house == (botIsAlliance ? House::Alliance : House::Horde);
}

constexpr uint64_t MIN_DEPOSIT_COPPER = 100;            // core AH_MINIMUM_DEPOSIT
constexpr uint32_t LISTING_SECONDS = 12 * 60 * 60;      // core MIN_AUCTION_TIME, the shortest and cheapest listing
constexpr uint32_t LONG_LISTING_SECONDS = 24 * 60 * 60; // the next duration the core accepts (it also takes 48 hours)
constexpr uint32_t DEPOSIT_UNIT_SECONDS = 12 * 60 * 60; // the core charges per started 12 hours of the duration
constexpr uint32_t MAX_LISTING_COUNT = 1000;            // core handler refuses larger counts

// The deposit of listing `count` items whose vendor price (SellPrice) is vendorSellPrice for durationSeconds.
// The core computes 3 * depositPercent / 100 * SellPrice * count * (duration / 12 hours) in floating point; this is
// the same value in integers, rounded up (never less than the core charges, at most one copper more). Under the
// minimum of 100 copper the minimum is charged. false on overflow.
inline bool AuctionDeposit(uint32_t vendorSellPrice, uint32_t count, uint32_t durationSeconds, HouseRates const& rates,
                           uint64_t& out)
{
    uint64_t const units = durationSeconds / DEPOSIT_UNIT_SECONDS;
    if (!vendorSellPrice || !units)
    {
        out = MIN_DEPOSIT_COPPER;
        return true;
    }

    uint64_t value = 3ull * rates.depositPercent;
    if (!MulChecked(value, vendorSellPrice, value) || !MulChecked(value, count, value) || !MulChecked(value, units, value) ||
        !AddChecked(value, 99, value))
        return false;

    out = std::max<uint64_t>(value / 100, MIN_DEPOSIT_COPPER);
    return true;
}

// The house's cut of a sale for `total` copper, rounded up. false on overflow.
inline bool AuctionCut(uint64_t total, HouseRates const& rates, uint64_t& out)
{
    uint64_t value = 0;
    if (!MulChecked(total, rates.cutPercent, value) || !AddChecked(value, 99, value))
        return false;

    out = value / 100;
    return true;
}

// Owner answer 2026-11-22: a listing runs 12 hours, and 24 hours when the stack is worth more than about 5 gold (the
// config key AiPlayerbot.Auction.LongListingMinGold). A dearer stack is worth a longer wait for a buyer; a cheap one is not
// worth a doubled deposit. 0 switches the long listing off.
constexpr uint64_t DEFAULT_LONG_LISTING_ABOVE_COPPER = 5 * COPPER_PER_GOLD;

inline uint32_t ListingDuration(uint64_t stackTotalCopper, uint64_t longListingAboveCopper)
{
    return longListingAboveCopper && stackTotalCopper > longListingAboveCopper ? LONG_LISTING_SECONDS : LISTING_SECONDS;
}

namespace detail
{
// Whether selling gross copper for a stack beats the vendor after the house's cut and the deposit of this duration. The
// deposit is counted as lost, which is the cautious reading (a sold auction gives it back, an expired one does not).
inline bool BeatsVendorAt(uint64_t gross, uint32_t count, uint32_t vendorSellPrice, HouseRates const& rates,
                          uint32_t durationSeconds, uint64_t& depositOut)
{
    uint64_t vendor = 0, cut = 0, costs = 0;
    if (!TotalPrice(vendorSellPrice, count, vendor) || !AuctionCut(gross, rates, cut) ||
        !AuctionDeposit(vendorSellPrice, count, durationSeconds, rates, depositOut) || !AddChecked(cut, depositOut, costs) ||
        !AddChecked(costs, vendor, costs))
        return false;

    return gross > costs;
}
}  // namespace detail

// Owner rule 2026-11-22: a bot goes to the auction house only when the minimum of the range beats what a vendor pays,
// after the deposit and the cut.
inline bool ListingBeatsVendor(PriceRange const& range, uint32_t count, uint32_t vendorSellPrice, HouseRates const& rates,
                               uint32_t durationSeconds)
{
    uint64_t gross = 0, deposit = 0;
    if (!IsTradable(range) || !count || count > MAX_LISTING_COUNT || !TotalPrice(range.minCopper, count, gross))
        return false;

    return detail::BeatsVendorAt(gross, count, vendorSellPrice, rates, durationSeconds, deposit);
}

// The terms a listing really gets once its price is rolled: the duration the price asks for, and its deposit. A long
// listing that no longer beats the vendor (a doubled deposit) or does not fit the deposit budget falls back to 12 hours;
// false when neither duration does. depositLeft is what the visit's money still allows for deposits.
struct ListingTerms
{
    uint32_t durationSeconds = LISTING_SECONDS;
    uint64_t deposit = 0;
};

inline bool ResolveTerms(uint64_t listingTotalCopper, uint32_t count, uint32_t vendorSellPrice, HouseRates const& rates,
                         uint64_t longListingAboveCopper, uint64_t depositLeft, ListingTerms& out)
{
    if (!listingTotalCopper || !count || count > MAX_LISTING_COUNT)
        return false;

    uint32_t const wanted = ListingDuration(listingTotalCopper, longListingAboveCopper);
    for (uint32_t duration : { wanted, LISTING_SECONDS })
    {
        uint64_t deposit = 0;
        if (detail::BeatsVendorAt(listingTotalCopper, count, vendorSellPrice, rates, duration, deposit) && deposit <= depositLeft)
        {
            out.durationSeconds = duration;
            out.deposit = deposit;
            return true;
        }
    }
    return false;
}
}  // namespace PricePolicy

#endif
