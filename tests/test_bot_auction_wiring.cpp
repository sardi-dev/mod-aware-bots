/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */
#include "BotAuctionPolicy.h"
#include "TestHarness.h"

using namespace BotAuctionPolicy;
using PricePolicy::BuyNeed;
using PricePolicy::HouseRates;
using PricePolicy::PriceClass;
using PricePolicy::PriceRange;

namespace
{
constexpr HouseRates FACTION_HOUSE{ 5, 5 };

PriceRange Range(uint32_t low, uint32_t high, PriceClass priceClass = PriceClass::WorldNormal)
{
    PriceRange range;
    range.minCopper = low;
    range.maxCopper = high;
    range.priceClass = priceClass;
    return range;
}

StackFacts Stack(uint32_t entry, uint32_t count, uint32_t vendor, PriceRange range)
{
    StackFacts stack;
    stack.entry = entry;
    stack.count = count;
    stack.vendorSellPrice = vendor;
    stack.hasRange = true;
    stack.range = range;
    stack.tradable = true;
    return stack;
}

SellPlan Plan()
{
    SellPlan plan;
    plan.rates = FACTION_HOUSE;
    plan.longListingAboveCopper = 5 * PricePolicy::COPPER_PER_GOLD;
    plan.maxListings = 4;
    plan.maxActiveListings = 20;
    return plan;
}

// A level 40 bot with 100 gold: repair reserve 8,000 copper, a fund of 50,000, a purchase budget of a quarter of what is left.
ReservePolicy::VisitMoney VisitOf(uint64_t money, uint64_t repair, uint64_t fund)
{
    ReservePolicy::VisitMoney visit;
    visit.reserve = repair + fund;
    visit.repairReserve = repair;
    visit.wallet = PricePolicy::MakeWallet(money, visit.reserve);
    visit.depositBudget = money > repair ? PricePolicy::PercentOf(money - repair, PricePolicy::VISIT_SPEND_PERCENT) : 0;
    return visit;
}

PurchaseCandidate Offer(uint32_t id, uint32_t entry, uint32_t count, uint64_t buyout, BuyNeed need, PriceRange range)
{
    PurchaseCandidate candidate;
    candidate.id = id;
    candidate.entry = entry;
    candidate.count = count;
    candidate.buyout = buyout;
    candidate.need = need;
    candidate.hasRange = true;
    candidate.range = range;
    return candidate;
}
}  // namespace

// ---------------------------------------------------------------------------------------------------------------
// The start bid.
// ---------------------------------------------------------------------------------------------------------------
TEST(auction_start_bid_is_95_percent_rounded_up_never_above_the_buyout)
{
    CHECK(StartBidFor(0) == 0);
    CHECK(StartBidFor(1) == 1);
    CHECK(StartBidFor(7) == 7);       // 6.65 rounds up
    CHECK(StartBidFor(20) == 19);
    CHECK(StartBidFor(100) == 95);
    CHECK(StartBidFor(101) == 96);    // 95.95 rounds up
    CHECK(StartBidFor(10000) == 9500);
    for (uint64_t buyout = 1; buyout < 3000; ++buyout)
    {
        CHECK(StartBidFor(buyout) >= 1);
        CHECK(StartBidFor(buyout) <= buyout);
        CHECK(StartBidFor(buyout) * 100 >= buyout * 95);   // at least 95 percent
    }
    CHECK(StartBidFor(MAX_BUYOUT_COPPER) <= MAX_BUYOUT_COPPER);
}

// ---------------------------------------------------------------------------------------------------------------
// The market of an item from the snapshot.
// ---------------------------------------------------------------------------------------------------------------
TEST(auction_market_takes_the_cheapest_unit_of_other_sellers)
{
    std::vector<SnapshotListing> listings(4);
    listings[0] = { 1, 500, 10, 1000, 0, false };   // 100 each
    listings[1] = { 2, 500, 5, 400, 0, false };     // 80 each
    listings[2] = { 3, 500, 2, 50, 0, true };       // own, 25 each: not the market's price
    listings[3] = { 4, 600, 1, 1, 0, false };       // another item
    PricePolicy::Market const market = MarketOf(listings, 500);
    CHECK(market.cheapestUnit == 80);
    CHECK(market.listings == 3);   // the bot's own stack crowds the market too
}

TEST(auction_market_rounds_the_unit_up_and_skips_bid_only_auctions)
{
    std::vector<SnapshotListing> listings(2);
    listings[0] = { 1, 500, 3, 10, 0, false };      // 3.33 each, rounds up to 4
    listings[1] = { 2, 500, 1, 0, 5, false };       // no buyout
    PricePolicy::Market market = MarketOf(listings, 500);
    CHECK(market.cheapestUnit == 4);
    CHECK(market.listings == 2);

    market = MarketOf(listings, 999);
    CHECK(market.cheapestUnit == 0 && market.listings == 0);
    CHECK(MarketOf({}, 500).listings == 0);
}

// ---------------------------------------------------------------------------------------------------------------
// A quote: price inside the range, start bid, terms.
// ---------------------------------------------------------------------------------------------------------------
TEST(auction_quote_price_stays_inside_the_range_for_every_roll)
{
    StackFacts const stack = Stack(100, 4, 100, Range(600, 1200, PriceClass::Gathered));
    for (double roll = 0.0; roll <= 1.0; roll += 0.05)
        for (double mood : { -1.0, 0.0, 1.0 })
        {
            Quote quote;
            CHECK(QuoteListing(stack, {}, roll, 0.5, mood, Plan(), 1000000, quote));
            CHECK(quote.unit >= 600 && quote.unit <= 1200);
            CHECK(quote.buyout == uint64_t(quote.unit) * 4);
            CHECK(quote.startBid == StartBidFor(quote.buyout));
            CHECK(quote.startBid <= quote.buyout);
        }
}

TEST(auction_quote_runs_12_hours_and_24_above_five_gold)
{
    Quote cheap;
    CHECK(QuoteListing(Stack(100, 2, 100, Range(600, 1200)), {}, 0.5, 0.5, 0.0, Plan(), 1000000, cheap));
    CHECK(cheap.buyout < 50000 && cheap.terms.durationSeconds == PricePolicy::LISTING_SECONDS);

    Quote dear;
    CHECK(QuoteListing(Stack(101, 1, 3000, Range(80000, 120000)), {}, 0.5, 0.5, 0.0, Plan(), 1000000, dear));
    CHECK(dear.buyout > 50000 && dear.terms.durationSeconds == PricePolicy::LONG_LISTING_SECONDS);
    // 24 hours doubles the deposit: 3 x 5 percent x 3,000 x 2 half-days.
    CHECK(dear.terms.deposit == 900);
}

TEST(auction_quote_falls_back_to_12_hours_when_the_long_deposit_does_not_fit)
{
    StackFacts const stack = Stack(101, 1, 3000, Range(80000, 120000));
    Quote quote;
    CHECK(QuoteListing(stack, {}, 0.5, 0.5, 0.0, Plan(), 450, quote));
    CHECK(quote.terms.durationSeconds == PricePolicy::LISTING_SECONDS && quote.terms.deposit == 450);
    CHECK(!QuoteListing(stack, {}, 0.5, 0.5, 0.0, Plan(), 449, quote));
}

TEST(auction_quote_refuses_what_must_not_be_listed)
{
    Quote quote;
    StackFacts stack = Stack(100, 4, 100, Range(600, 1200));
    stack.hasRange = false;
    CHECK(!QuoteListing(stack, {}, 0.5, 0.5, 0.0, Plan(), 1000000, quote));

    stack = Stack(100, 0, 100, Range(600, 1200));
    CHECK(!QuoteListing(stack, {}, 0.5, 0.5, 0.0, Plan(), 1000000, quote));

    // The range does not beat the vendor: 100 each, vendor pays 100.
    stack = Stack(100, 4, 100, Range(100, 110));
    CHECK(!QuoteListing(stack, {}, 0.5, 0.5, 0.0, Plan(), 1000000, quote));

    // A stack of a thousand at the top of the cap is above what the core accepts as a buyout.
    stack = Stack(100, 1000, 100, Range(uint32_t(PricePolicy::TRADE_CAP_COPPER) - 1, uint32_t(PricePolicy::TRADE_CAP_COPPER)));
    CHECK(!QuoteListing(stack, {}, 0.5, 0.5, 0.0, Plan(), UINT64_MAX, quote));
}

TEST(auction_quote_undercuts_the_cheapest_other_listing_but_not_below_the_minimum)
{
    StackFacts const stack = Stack(100, 1, 100, Range(600, 1200));
    PricePolicy::Market market;
    market.cheapestUnit = 700;
    market.listings = 2;
    Quote quote;
    CHECK(QuoteListing(stack, market, 1.0, 1.0, 0.0, Plan(), 1000000, quote));
    CHECK(quote.unit < 700 && quote.unit >= 600);

    market.cheapestUnit = 610;
    CHECK(QuoteListing(stack, market, 1.0, 1.0, 0.0, Plan(), 1000000, quote));
    CHECK(quote.unit >= 600);
}

// ---------------------------------------------------------------------------------------------------------------
// Needs.
// ---------------------------------------------------------------------------------------------------------------
TEST(auction_need_is_the_strongest_that_applies)
{
    CHECK(NeedOf(true, true, true, true) == BuyNeed::GearUpgrade);
    CHECK(NeedOf(false, true, true, true) == BuyNeed::Bag);
    CHECK(NeedOf(false, false, true, true) == BuyNeed::ProfessionMaterial);
    CHECK(NeedOf(false, false, false, true) == BuyNeed::Consumable);
    CHECK(NeedOf(false, false, false, false) == BuyNeed::None);
}

TEST(auction_materials_are_bought_only_up_to_the_keep_cap_of_their_level)
{
    // A crafter buys its own materials up to 60.
    CHECK(MaterialWanted(MaterialUse::Crafting, 0, 20));
    CHECK(MaterialWanted(MaterialUse::Crafting, 40, 20));    // exactly 60
    CHECK(!MaterialWanted(MaterialUse::Crafting, 41, 20));   // 61 would be listed again at once

    // A secondary skill up to 20.
    CHECK(MaterialWanted(MaterialUse::Secondary, 0, 20));
    CHECK(MaterialWanted(MaterialUse::Secondary, 10, 10));   // exactly 20
    CHECK(!MaterialWanted(MaterialUse::Secondary, 11, 10));  // 21 would be listed again at once
    CHECK(!MaterialWanted(MaterialUse::Secondary, 20, 1));

    // Nobody buys what no profession of theirs uses, or what they only gather themselves.
    CHECK(!MaterialWanted(MaterialUse::None, 0, 1));
    CHECK(!MaterialWanted(MaterialUse::Gathered, 0, 1));

    CHECK(!MaterialWanted(MaterialUse::Crafting, 0, 0));
    CHECK(!MaterialWanted(MaterialUse::Crafting, UINT32_MAX, UINT32_MAX));
}

TEST(auction_consumables_are_bought_only_when_low_and_up_to_the_keep_cap)
{
    CHECK(ConsumableWanted(true, 0, 20));
    CHECK(ConsumableWanted(true, 4, 20));
    CHECK(!ConsumableWanted(true, 5, 20));      // not low
    CHECK(!ConsumableWanted(true, 0, 41));      // above the keep cap
    CHECK(!ConsumableWanted(false, 0, 20));
}

// ---------------------------------------------------------------------------------------------------------------
// Purchases.
// ---------------------------------------------------------------------------------------------------------------
TEST(auction_purchases_buy_under_the_cap_of_the_need_and_nothing_else)
{
    // Gear caps at 50 percent of the range: 1,500 for a 1,000 to 2,000 item.
    std::vector<PurchaseCandidate> offers = {
        Offer(1, 10, 1, 1400, BuyNeed::GearUpgrade, Range(1000, 2000)),
        Offer(2, 11, 1, 1501, BuyNeed::GearUpgrade, Range(1000, 2000)),   // above the cap
        Offer(3, 12, 1, 1000, BuyNeed::None, Range(1000, 2000)),          // not needed
        Offer(4, 13, 1, 1000, BuyNeed::GearUpgrade, Range(1000, 2000)),
    };
    offers[3].own = true;                                                  // its own listing
    std::vector<Purchase> const bought = SelectPurchases(offers, VisitOf(1000000, 8000, 50000), 1000000, 3);
    CHECK(bought.size() == 1);
    CHECK(bought[0].id == 1 && bought[0].capUnit == 1500 && bought[0].need == BuyNeed::GearUpgrade);
}

TEST(auction_purchases_skip_bid_only_auctions_and_items_without_a_range)
{
    std::vector<PurchaseCandidate> offers = {
        Offer(1, 10, 1, 0, BuyNeed::GearUpgrade, Range(1000, 2000)),   // no buyout
        Offer(2, 11, 1, 1000, BuyNeed::GearUpgrade, Range(1000, 2000)),
        Offer(3, 12, 0, 1000, BuyNeed::GearUpgrade, Range(1000, 2000)),
    };
    offers[1].hasRange = false;
    CHECK(SelectPurchases(offers, VisitOf(1000000, 8000, 50000), 1000000, 3).empty());
}

TEST(auction_purchases_prefer_the_cheapest_against_their_cap)
{
    std::vector<PurchaseCandidate> offers = {
        Offer(1, 10, 1, 1450, BuyNeed::GearUpgrade, Range(1000, 2000)),   // 3 percent under the cap
        Offer(2, 11, 1, 1050, BuyNeed::GearUpgrade, Range(1000, 2000)),   // 30 percent under
        Offer(3, 12, 1, 1200, BuyNeed::GearUpgrade, Range(1000, 2000)),   // 20 percent under
    };
    std::vector<Purchase> const bought = SelectPurchases(offers, VisitOf(1000000, 8000, 50000), 1000000, 2);
    CHECK(bought.size() == 2);   // the cap of two purchases
    CHECK(bought[0].id == 2 && bought[1].id == 3);
}

TEST(auction_purchases_stop_at_the_visit_budget_and_keep_the_reserve)
{
    // 20,000 copper held, repair reserve 8,000, no fund: the visit may spend a quarter of the 12,000 above it, 3,000.
    ReservePolicy::VisitMoney const visit = VisitOf(20000, 8000, 0);
    CHECK(visit.wallet.budget == 3000);
    std::vector<PurchaseCandidate> offers = {
        Offer(1, 10, 1, 1400, BuyNeed::GearUpgrade, Range(1000, 2000)),
        Offer(2, 11, 1, 1400, BuyNeed::GearUpgrade, Range(1000, 2000)),
        Offer(3, 12, 1, 1400, BuyNeed::GearUpgrade, Range(1000, 2000)),
    };
    std::vector<Purchase> const bought = SelectPurchases(offers, visit, 20000, 3);
    CHECK(bought.size() == 2);   // 2,800 fits 3,000, a third does not
}

TEST(auction_purchases_leave_a_poor_bot_with_nothing_to_buy)
{
    // The fund of a bot saving for riding holds back everything it owns.
    ReservePolicy::VisitMoney const visit = VisitOf(30000, 8000, 50000);
    CHECK(visit.wallet.budget == 0);
    std::vector<PurchaseCandidate> offers = { Offer(1, 10, 1, 1000, BuyNeed::GearUpgrade, Range(1000, 2000)) };
    CHECK(SelectPurchases(offers, visit, 30000, 3).empty());
}

TEST(auction_purchases_count_the_deposits_already_spent_against_the_budget)
{
    ReservePolicy::VisitMoney visit = VisitOf(20000, 8000, 0);
    CHECK(ReservePolicy::SpendDeposit(visit, 20000, 2000));
    std::vector<PurchaseCandidate> offers = {
        Offer(1, 10, 1, 1400, BuyNeed::GearUpgrade, Range(1000, 2000)),
        Offer(2, 11, 1, 1400, BuyNeed::GearUpgrade, Range(1000, 2000)),
    };
    CHECK(SelectPurchases(offers, visit, 18000, 3).size() == 0);   // 1,000 left of the 3,000, a 1,400 offer does not fit
}

TEST(auction_purchases_judge_a_stack_by_its_unit_price)
{
    // 20 potions for 3,000 is 150 each; consumables cap at 35 percent: 100 + 0.35 x 100 = 135 for a 100 to 200 range.
    std::vector<PurchaseCandidate> offers = {
        Offer(1, 10, 20, 3000, BuyNeed::Consumable, Range(100, 200)),
        Offer(2, 10, 20, 2700, BuyNeed::Consumable, Range(100, 200)),
    };
    std::vector<Purchase> const bought = SelectPurchases(offers, VisitOf(1000000, 8000, 0), 1000000, 3);
    CHECK(bought.size() == 1 && bought[0].id == 2 && bought[0].capUnit == 135);
}

TEST(auction_purchases_with_a_zero_maximum_buy_nothing)
{
    std::vector<PurchaseCandidate> offers = { Offer(1, 10, 1, 1000, BuyNeed::GearUpgrade, Range(1000, 2000)) };
    CHECK(SelectPurchases(offers, VisitOf(1000000, 8000, 0), 1000000, 0).empty());
}
