/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */
#include "PricePolicy.h"
#include "TestHarness.h"

#include <cmath>
#include <limits>

using namespace PricePolicy;

namespace
{
PriceRange Range(uint32_t low, uint32_t high, PriceClass cls = PriceClass::WorldNormal)
{
    PriceRange range;
    range.minCopper = low;
    range.maxCopper = high;
    range.priceClass = cls;
    return range;
}

constexpr HouseRates FACTION_HOUSE{ 5, 5 };
constexpr HouseRates NEUTRAL_HOUSE{ 25, 15 };
constexpr double NaN = std::numeric_limits<double>::quiet_NaN();
}  // namespace

TEST(price_class_numbers_are_the_ones_the_table_stores)
{
    // tools/economy/export_price_table.py PRICE_CLASS_IDS writes these numbers; its tests read this enum too.
    CHECK(uint32_t(PriceClass::Vendor) == 1);
    CHECK(uint32_t(PriceClass::Gathered) == 2);
    CHECK(uint32_t(PriceClass::Crafted) == 3);
    CHECK(uint32_t(PriceClass::Recipe) == 4);
    CHECK(uint32_t(PriceClass::WorldNormal) == 5);
    CHECK(uint32_t(PriceClass::RaidBoss) == 12);
    CHECK(uint32_t(PriceClass::VendorToken) == 15);
    CHECK(uint32_t(PriceClass::Other) == PRICE_CLASS_LAST);
}

TEST(price_range_usable_only_with_a_floor_a_ceiling_and_a_class)
{
    CHECK(IsTradable(Range(1, 1)));
    CHECK(IsTradable(Range(22, 66, PriceClass::Vendor)));
    CHECK(!IsTradable(Range(0, 10)));
    CHECK(!IsTradable(Range(10, 9)));
    CHECK(!IsTradable(Range(10, 20, PriceClass(0))));
    CHECK(!IsTradable(Range(10, 20, PriceClass(17))));
}

TEST(price_range_ignores_items_above_ten_thousand_gold)
{
    uint32_t const cap = uint32_t(10000 * COPPER_PER_GOLD);
    CHECK(IsTradable(Range(cap - 1000, cap)));
    CHECK(!IsTradable(Range(cap - 1000, cap + 1)));
    CHECK(!IsTradable(Range(std::numeric_limits<uint32_t>::max(), std::numeric_limits<uint32_t>::max())));
}

TEST(price_table_normalize_sorts_drops_bad_rows_and_duplicates)
{
    std::vector<PriceRow> rows = {
        { 30, Range(300, 600) }, { 10, Range(100, 200) }, { 20, Range(0, 5) }, { 10, Range(111, 222) },
        { 0, Range(1, 2) },      { 40, Range(9, 3) }
    };
    CHECK(Normalize(rows) == 4);
    CHECK(rows.size() == 2);
    CHECK(rows[0].item == 10 && rows[0].range.minCopper == 100);
    CHECK(rows[1].item == 30);

    PriceRange found;
    CHECK(FindRange(rows, 30, found) && found.maxCopper == 600);
    CHECK(FindRange(rows, 10, found) && found.minCopper == 100);
    CHECK(!FindRange(rows, 20, found));
    CHECK(!FindRange(rows, 99, found));
    CHECK(!FindRange(std::vector<PriceRow>(), 10, found));
}

TEST(price_checked_arithmetic_reports_overflow)
{
    uint64_t out = 0;
    CHECK(MulChecked(1000, 1000, out) && out == 1000000);
    CHECK(MulChecked(0, UINT64_MAX, out) && out == 0);
    CHECK(!MulChecked(UINT64_MAX, 2, out));
    CHECK(!MulChecked(uint64_t(1) << 33, uint64_t(1) << 31, out));
    CHECK(AddChecked(UINT64_MAX - 1, 1, out) && out == UINT64_MAX);
    CHECK(!AddChecked(UINT64_MAX, 1, out));
    CHECK(TotalPrice(4294967295u, 4294967295u, out));
    CHECK(UnitPriceCeil(10, 3) == 4 && UnitPriceCeil(9, 3) == 3 && UnitPriceCeil(5, 0) == 0);
    CHECK(PercentOf(UINT64_MAX, 25) == UINT64_MAX / 4);
    CHECK(PercentOf(1000, 25) == 250 && PercentOf(7, 25) == 1);
}

TEST(price_listing_lands_in_the_band_of_the_market_depth)
{
    PriceRange const range = Range(1000, 3000);
    Market empty, thin, crowded;
    thin.listings = 1;
    crowded.listings = 3;
    CHECK(ListingPrice(range, empty, 0.5, 0.0, 0.0) == 2150);     // 0.35 + 0.45 * 0.5 of the range
    CHECK(ListingPrice(range, thin, 0.5, 0.0, 0.0) == 1800);      // 0.15 + 0.50 * 0.5
    CHECK(ListingPrice(range, crowded, 0.5, 0.0, 0.0) == 1250);   // 0.00 + 0.25 * 0.5
    CHECK(ListingPrice(range, empty, 0.5, 0.0, 0.0) > ListingPrice(range, thin, 0.5, 0.0, 0.0));
    CHECK(ListingPrice(range, thin, 0.5, 0.0, 0.0) > ListingPrice(range, crowded, 0.5, 0.0, 0.0));
    crowded.listings = 40;
    CHECK(ListingPrice(range, crowded, 0.5, 0.0, 0.0) == 1250);
}

TEST(price_listing_never_leaves_the_range)
{
    PriceRange const range = Range(1000, 3000);
    for (uint32_t listings : { 0u, 1u, 2u, 3u, 9u })
        for (uint32_t cheapest : { 0u, 500u, 1000u, 1005u, 2000u, 9000u })
            for (double roll : { -3.0, 0.0, 0.5, 0.999, 1.0, 7.0, NaN })
                for (double mood : { -9.0, -1.0, 0.0, 1.0, 9.0, NaN })
                {
                    Market market;
                    market.listings = listings;
                    market.cheapestUnit = cheapest;
                    uint32_t const price = ListingPrice(range, market, roll, roll, mood);
                    CHECK(price >= 1000 && price <= 3000);
                }
}

TEST(price_listing_mood_moves_it_by_a_tenth_of_the_range)
{
    PriceRange const range = Range(1000, 3000);
    Market const market;
    uint32_t const base = ListingPrice(range, market, 0.5, 0.0, 0.0);
    CHECK(ListingPrice(range, market, 0.5, 0.0, 1.0) == base + 200);
    CHECK(ListingPrice(range, market, 0.5, 0.0, -1.0) == base - 200);
    CHECK(ListingPrice(range, market, 0.5, 0.0, 50.0) == base + 200);   // clamped
}

TEST(price_listing_undercuts_the_cheapest_but_never_below_the_minimum)
{
    PriceRange const range = Range(1000, 3000);
    Market market;
    market.listings = 1;
    market.cheapestUnit = 2000;
    uint32_t const light = ListingPrice(range, market, 0.99, 0.0, 0.0);
    uint32_t const deep = ListingPrice(range, market, 0.99, 1.0, 0.0);
    CHECK(light < 2000 && light >= 1960);       // 1 percent under, give or take rounding
    CHECK(deep < light && deep >= 1895);        // 5 percent under
    market.cheapestUnit = 1005;                 // under the minimum after the undercut: the minimum wins
    CHECK(ListingPrice(range, market, 0.0, 1.0, -1.0) == 1000);
    market.cheapestUnit = 1000;                 // a listing at the minimum is not undercut below it
    CHECK(ListingPrice(range, market, 0.99, 1.0, 0.0) >= 1000);
    market.cheapestUnit = 700;                  // cheaper than the minimum: no undercut, the band decides
    CHECK(ListingPrice(range, market, 0.5, 0.5, 0.0) == 1800);
}

TEST(price_listing_refuses_an_unusable_range)
{
    CHECK(ListingPrice(Range(0, 10), Market(), 0.5, 0.5, 0.0) == 0);
    CHECK(ListingPrice(Range(10, 5), Market(), 0.5, 0.5, 0.0) == 0);
    uint32_t const above = uint32_t(10000 * COPPER_PER_GOLD) + 1;
    CHECK(ListingPrice(Range(1000, above), Market(), 0.5, 0.5, 0.0) == 0);
}

TEST(price_listing_a_range_of_one_copper)
{
    CHECK(ListingPrice(Range(5, 5), Market(), 0.9, 0.9, 1.0) == 5);
}

TEST(price_daily_mood_is_stable_bounded_and_moves)
{
    double sum = 0.0;
    bool moved = false;
    for (uint32_t day = 0; day < 2000; ++day)
    {
        double const mood = DailyMood(day, PriceClass::Gathered);
        CHECK(mood >= -1.0 && mood <= 1.0);
        CHECK(mood == DailyMood(day, PriceClass::Gathered));
        sum += mood;
        moved = moved || mood != DailyMood(day + 1, PriceClass::Gathered);
    }
    CHECK(moved);
    CHECK_NEAR(sum / 2000.0, 0.0, 0.1);
    bool classesDiffer = false;
    for (uint32_t day = 0; day < 50; ++day)
        classesDiffer = classesDiffer || DailyMood(day, PriceClass::Gathered) != DailyMood(day, PriceClass::RaidBoss);
    CHECK(classesDiffer);
}

TEST(price_buy_cap_by_need)
{
    PriceRange const range = Range(1000, 3000);
    CHECK(BuyCap(range, BuyNeed::GearUpgrade) == 2000);
    CHECK(BuyCap(range, BuyNeed::Bag) == 2000);
    CHECK(BuyCap(range, BuyNeed::ProfessionMaterial) == 1800);
    CHECK(BuyCap(range, BuyNeed::Consumable) == 1700);
    CHECK(BuyCap(range, BuyNeed::None) == 0);
    CHECK(BuyCap(Range(0, 100), BuyNeed::Bag) == 0);
    CHECK(BuyCap(Range(5, 5), BuyNeed::GearUpgrade) == 5);
    uint32_t const cap = uint32_t(10000 * COPPER_PER_GOLD);
    CHECK(BuyCap(Range(cap - 10, cap), BuyNeed::GearUpgrade) == cap - 5);
    CHECK(BuyCap(Range(cap - 10, cap + 1), BuyNeed::GearUpgrade) == 0);
}

TEST(price_wallet_is_a_quarter_of_the_gold_above_the_reserve)
{
    CHECK(MakeWallet(10100, 100).budget == 2500);
    CHECK(MakeWallet(100, 100).budget == 0);
    CHECK(MakeWallet(50, 100).budget == 0);
    CHECK(MakeWallet(0, 0).budget == 0);
    CHECK(MakeWallet(UINT64_MAX, 0).budget == UINT64_MAX / 4);
}

TEST(price_wallet_repeated_spends_stay_inside_the_budget_and_the_reserve)
{
    Wallet wallet = MakeWallet(10100, 100);   // 2500 to spend
    uint64_t money = 10100;
    CHECK(Spend(wallet, money, 100, 1000));   // a deposit
    money -= 1000;
    CHECK(wallet.committed == 1000 && BudgetLeft(wallet) == 1500);
    CHECK(!Spend(wallet, money, 100, 1501));
    CHECK(wallet.committed == 1000);          // a refused spend changes nothing
    CHECK(Spend(wallet, money, 100, 1500));
    money -= 1500;
    CHECK(BudgetLeft(wallet) == 0);
    CHECK(!Spend(wallet, money, 100, 1));
    // The bot's gold fell below what the budget assumed (it paid for repairs): the reserve still holds.
    Wallet other = MakeWallet(10100, 100);
    CHECK(!Spend(other, 500, 100, 450));
    CHECK(Spend(other, 500, 100, 400));
    CHECK(!CanSpend(other, UINT64_MAX, UINT64_MAX, 10));   // reserve plus amount overflows: refused
}

TEST(price_will_buy_inside_the_cap_and_the_budget)
{
    PriceRange const range = Range(1000, 3000);
    Wallet const wallet = MakeWallet(10100, 100);   // 2500
    CHECK(WillBuy(1700, 1, range, BuyNeed::Consumable, wallet, 10100, 100));
    CHECK(!WillBuy(1701, 1, range, BuyNeed::Consumable, wallet, 10100, 100));   // above the 0.35 cap
    CHECK(WillBuy(1701, 1, range, BuyNeed::GearUpgrade, wallet, 10100, 100));   // 0.50 allows it
    CHECK(!WillBuy(0, 1, range, BuyNeed::GearUpgrade, wallet, 10100, 100));
    CHECK(!WillBuy(1000, 0, range, BuyNeed::GearUpgrade, wallet, 10100, 100));
    CHECK(!WillBuy(1000, 1, range, BuyNeed::None, wallet, 10100, 100));
}

TEST(price_will_buy_compares_the_price_per_unit_of_the_whole_listing)
{
    PriceRange const range = Range(100, 300);       // consumable cap 170 per unit
    Wallet const wallet = MakeWallet(100100, 100);  // 25000
    CHECK(WillBuy(1700, 10, range, BuyNeed::Consumable, wallet, 100100, 100));
    CHECK(!WillBuy(1701, 10, range, BuyNeed::Consumable, wallet, 100100, 100));   // 171 a unit, rounded up
    CHECK(!WillBuy(30000, 200, range, BuyNeed::Consumable, wallet, 100100, 100)); // cheap enough, over the budget
}

TEST(price_will_buy_refuses_what_the_wallet_or_the_reserve_cannot_pay)
{
    PriceRange const range = Range(1000, 3000);
    Wallet wallet = MakeWallet(10100, 100);
    CHECK(!WillBuy(1500, 1, range, BuyNeed::GearUpgrade, wallet, 1550, 100));   // would dip under the reserve
    CHECK(WillBuy(1500, 1, range, BuyNeed::GearUpgrade, wallet, 1600, 100));
    wallet.committed = 1500;                                                    // already spent this visit
    CHECK(!WillBuy(1500, 1, range, BuyNeed::GearUpgrade, wallet, 10100, 100));
    CHECK(!WillBuy(1500, 1, Range(0, 0), BuyNeed::GearUpgrade, MakeWallet(10100, 100), 10100, 100));
}

TEST(price_will_buy_has_no_seller_so_a_human_listing_gets_the_same_answer)
{
    // The call takes the price, the range and the bot's money and nothing else: there is no seller to weigh.
    PriceRange const range = Range(1000, 3000);
    Wallet const wallet = MakeWallet(20100, 100);
    bool const answers[] = { WillBuy(1800, 1, range, BuyNeed::ProfessionMaterial, wallet, 20100, 100),
                             WillBuy(1800, 1, range, BuyNeed::ProfessionMaterial, wallet, 20100, 100) };
    CHECK(answers[0] && answers[1]);
}

TEST(price_will_buy_never_overflows)
{
    PriceRange const range = Range(1000, 3000);
    Wallet wallet;
    wallet.budget = UINT64_MAX;
    CHECK(!WillBuy(UINT64_MAX, 1, range, BuyNeed::GearUpgrade, wallet, UINT64_MAX, 1));
    CHECK(!WillBuy(UINT64_MAX, UINT32_MAX, range, BuyNeed::GearUpgrade, wallet, UINT64_MAX, 0));
}

TEST(price_houses_a_bot_may_use_are_its_own_factions)
{
    CHECK(HouseAllowed(House::Alliance, true));
    CHECK(!HouseAllowed(House::Horde, true));
    CHECK(!HouseAllowed(House::Neutral, true));
    CHECK(HouseAllowed(House::Horde, false));
    CHECK(!HouseAllowed(House::Alliance, false));
    CHECK(!HouseAllowed(House::Neutral, false));
}

TEST(price_deposit_follows_the_core_formula)
{
    uint64_t deposit = 0;
    // 0.15 x SellPrice x count x (duration in 12 hour units) at a faction house.
    CHECK(AuctionDeposit(1000, 5, LISTING_SECONDS, FACTION_HOUSE, deposit) && deposit == 750);
    CHECK(AuctionDeposit(1000, 5, 2 * LISTING_SECONDS, FACTION_HOUSE, deposit) && deposit == 1500);
    CHECK(AuctionDeposit(1000, 5, 4 * LISTING_SECONDS, FACTION_HOUSE, deposit) && deposit == 3000);
    CHECK(AuctionDeposit(1000, 5, LISTING_SECONDS, NEUTRAL_HOUSE, deposit) && deposit == 3750);
    CHECK(AuctionDeposit(100, 1, LISTING_SECONDS, FACTION_HOUSE, deposit) && deposit == MIN_DEPOSIT_COPPER);   // 15 -> 100
    CHECK(AuctionDeposit(0, 7, LISTING_SECONDS, FACTION_HOUSE, deposit) && deposit == MIN_DEPOSIT_COPPER);
    CHECK(AuctionDeposit(1000, 5, LISTING_SECONDS - 1, FACTION_HOUSE, deposit) && deposit == MIN_DEPOSIT_COPPER);
    CHECK(AuctionDeposit(7, 1, LISTING_SECONDS, NEUTRAL_HOUSE, deposit) && deposit == MIN_DEPOSIT_COPPER);
    // 3 x 5 x 1001 x 1 / 100 = 150.15, rounded up to 151 (the core's float result is 150).
    CHECK(AuctionDeposit(1001, 1, LISTING_SECONDS, FACTION_HOUSE, deposit) && deposit == 151);
}

TEST(price_deposit_and_cut_report_overflow)
{
    uint64_t out = 0;
    HouseRates const huge{ UINT32_MAX, UINT32_MAX };
    CHECK(!AuctionDeposit(UINT32_MAX, UINT32_MAX, 4 * LISTING_SECONDS, huge, out));
    CHECK(!AuctionCut(UINT64_MAX, FACTION_HOUSE, out));
    CHECK(AuctionCut(10000, FACTION_HOUSE, out) && out == 500);
    CHECK(AuctionCut(1, FACTION_HOUSE, out) && out == 1);
    CHECK(AuctionCut(0, FACTION_HOUSE, out) && out == 0);
    CHECK(AuctionCut(10000, NEUTRAL_HOUSE, out) && out == 1500);
}

TEST(price_listing_must_beat_the_vendor_after_deposit_and_cut)
{
    // 200 Copper Ore: vendor pays 5 each, the range starts at 22: 4400 gross, 220 cut, 150 deposit, 1000 at a vendor.
    CHECK(ListingBeatsVendor(Range(22, 66), 200, 5, FACTION_HOUSE, LISTING_SECONDS));
    // Junk at 1.1 x the vendor price loses to the vendor once the cut and the deposit are paid.
    CHECK(!ListingBeatsVendor(Range(110, 160), 10, 100, FACTION_HOUSE, LISTING_SECONDS));
    // A single cheap item: the 100 copper minimum deposit eats the gain.
    CHECK(!ListingBeatsVendor(Range(60, 90), 1, 25, FACTION_HOUSE, LISTING_SECONDS));
    // The line between the two: vendor 1000 each, 10 items, deposit 1500, so 1210 each (12,100 less a cut of 605) loses
    // and 1211 wins.
    CHECK(!ListingBeatsVendor(Range(1210, 2000), 10, 1000, FACTION_HOUSE, LISTING_SECONDS));
    CHECK(ListingBeatsVendor(Range(1211, 2000), 10, 1000, FACTION_HOUSE, LISTING_SECONDS));
    // 10 x 1000 gross, cut 500, deposit 0.15 x 700 x 10 = 1050, vendor 7000 -> costs 8550 against 10000: beats.
    CHECK(ListingBeatsVendor(Range(1000, 2000), 10, 700, FACTION_HOUSE, LISTING_SECONDS));
    // Longer listings cost more deposit: 48 hours at the same numbers is 4200, costs 11700: loses.
    CHECK(!ListingBeatsVendor(Range(1000, 2000), 10, 700, FACTION_HOUSE, 4 * LISTING_SECONDS));
    // The neutral house takes more.
    CHECK(!ListingBeatsVendor(Range(1000, 2000), 10, 700, NEUTRAL_HOUSE, LISTING_SECONDS));
}

TEST(price_listing_beats_vendor_refuses_bad_input_and_overflow)
{
    CHECK(!ListingBeatsVendor(Range(0, 0), 1, 0, FACTION_HOUSE, LISTING_SECONDS));
    CHECK(!ListingBeatsVendor(Range(100, 200), 0, 1, FACTION_HOUSE, LISTING_SECONDS));
    CHECK(!ListingBeatsVendor(Range(100, 200), MAX_LISTING_COUNT + 1, 1, FACTION_HOUSE, LISTING_SECONDS));
    uint32_t const cap = uint32_t(10000 * COPPER_PER_GOLD);
    CHECK(!ListingBeatsVendor(Range(cap - 1, cap), 1, UINT32_MAX, FACTION_HOUSE, 4 * LISTING_SECONDS));
    CHECK(ListingBeatsVendor(Range(cap - 1, cap), 1000, 1, FACTION_HOUSE, LISTING_SECONDS));
    CHECK(!ListingBeatsVendor(Range(cap - 1, cap), 1000, UINT32_MAX, HouseRates{ UINT32_MAX, UINT32_MAX }, 4 * LISTING_SECONDS));
}

TEST(price_sorting_of_a_realistic_table_row)
{
    // Copper Ore 2770 (audit): vendor class, 22 to 66 copper; Greater Healing Potion 1710: 550 to 1600.
    std::vector<PriceRow> rows = { { 2770, Range(22, 66, PriceClass::Vendor) }, { 1710, Range(550, 1600, PriceClass::Vendor) } };
    CHECK(Normalize(rows) == 0);
    PriceRange found;
    CHECK(FindRange(rows, 1710, found) && found.priceClass == PriceClass::Vendor);
    CHECK(BuyCap(found, BuyNeed::Consumable) == 550 + 1050 * 35 / 100);
}

// ---------------------------------------------------------------------------------------------------------------
// Listing duration (owner answer 2026-11-22): 12 hours, 24 hours above about 5 gold.
// ---------------------------------------------------------------------------------------------------------------
TEST(price_duration_is_twelve_hours_and_twenty_four_above_the_threshold)
{
    uint64_t const five = DEFAULT_LONG_LISTING_ABOVE_COPPER;
    CHECK(five == 50000);
    CHECK(ListingDuration(0, five) == LISTING_SECONDS);
    CHECK(ListingDuration(five, five) == LISTING_SECONDS);   // "more than": the threshold itself stays short
    CHECK(ListingDuration(five + 1, five) == LONG_LISTING_SECONDS);
    CHECK(ListingDuration(UINT64_MAX, five) == LONG_LISTING_SECONDS);
    CHECK(LONG_LISTING_SECONDS == 2 * LISTING_SECONDS);
}

TEST(price_duration_threshold_zero_switches_the_long_listing_off)
{
    CHECK(ListingDuration(UINT64_MAX, 0) == LISTING_SECONDS);
}

TEST(price_deposit_of_a_long_listing_is_double_the_short_one)
{
    uint64_t shortDeposit = 0, longDeposit = 0;
    CHECK(AuctionDeposit(1000, 5, LISTING_SECONDS, FACTION_HOUSE, shortDeposit) && shortDeposit == 750);
    CHECK(AuctionDeposit(1000, 5, LONG_LISTING_SECONDS, FACTION_HOUSE, longDeposit) && longDeposit == 1500);
    // The minimum deposit holds for both.
    CHECK(AuctionDeposit(10, 1, LONG_LISTING_SECONDS, FACTION_HOUSE, longDeposit) && longDeposit == MIN_DEPOSIT_COPPER);
}

TEST(price_terms_pick_the_duration_of_the_price_and_charge_its_deposit)
{
    ListingTerms terms;
    uint64_t const five = DEFAULT_LONG_LISTING_ABOVE_COPPER;
    // 10 items, vendor 1000 each, listed for 20000 in all: short, deposit 0.15 x 1000 x 10 = 1500.
    CHECK(ResolveTerms(20000, 10, 1000, FACTION_HOUSE, five, 1000000, terms));
    CHECK(terms.durationSeconds == LISTING_SECONDS && terms.deposit == 1500);
    // 10 items for 60000 in all: long, deposit 3000.
    CHECK(ResolveTerms(60000, 10, 1000, FACTION_HOUSE, five, 1000000, terms));
    CHECK(terms.durationSeconds == LONG_LISTING_SECONDS && terms.deposit == 3000);
    // Exactly the threshold stays short.
    CHECK(ResolveTerms(five, 10, 1000, FACTION_HOUSE, five, 1000000, terms));
    CHECK(terms.durationSeconds == LISTING_SECONDS);
}

TEST(price_terms_fall_back_to_twelve_hours_when_the_long_deposit_does_not_fit)
{
    ListingTerms terms;
    uint64_t const five = DEFAULT_LONG_LISTING_ABOVE_COPPER;
    // The long deposit is 3000, the short one 1500: with 2000 left the listing is still made, for 12 hours.
    CHECK(ResolveTerms(60000, 10, 1000, FACTION_HOUSE, five, 2000, terms));
    CHECK(terms.durationSeconds == LISTING_SECONDS && terms.deposit == 1500);
    // With 1000 left neither fits.
    CHECK(!ResolveTerms(60000, 10, 1000, FACTION_HOUSE, five, 1000, terms));
    // Exactly the deposit is enough.
    CHECK(ResolveTerms(60000, 10, 1000, FACTION_HOUSE, five, 3000, terms) && terms.durationSeconds == LONG_LISTING_SECONDS);
}

TEST(price_terms_fall_back_when_the_long_listing_no_longer_beats_the_vendor)
{
    ListingTerms terms;
    // 1 item, vendor 40000: the deposit is 6000 for 12 hours and 12000 for 24. A price of 51000 is above 5 gold, so it asks
    // for 24 hours; costs are cut 2550 + deposit 12000 + vendor 40000 = 54550 against 51000: the long listing loses, the
    // short one (costs 48550) wins.
    CHECK(ResolveTerms(51000, 1, 40000, FACTION_HOUSE, DEFAULT_LONG_LISTING_ABOVE_COPPER, 1000000, terms));
    CHECK(terms.durationSeconds == LISTING_SECONDS && terms.deposit == 6000);
}

TEST(price_terms_refuse_a_listing_that_cannot_beat_the_vendor_or_is_malformed)
{
    ListingTerms terms;
    CHECK(!ResolveTerms(1000, 10, 1000, FACTION_HOUSE, DEFAULT_LONG_LISTING_ABOVE_COPPER, 1000000, terms));   // below the vendor
    CHECK(!ResolveTerms(0, 10, 1000, FACTION_HOUSE, DEFAULT_LONG_LISTING_ABOVE_COPPER, 1000000, terms));
    CHECK(!ResolveTerms(20000, 0, 1000, FACTION_HOUSE, DEFAULT_LONG_LISTING_ABOVE_COPPER, 1000000, terms));
    CHECK(!ResolveTerms(20000, MAX_LISTING_COUNT + 1, 1, FACTION_HOUSE, DEFAULT_LONG_LISTING_ABOVE_COPPER, 1000000, terms));
    CHECK(!ResolveTerms(UINT64_MAX, 1000, UINT32_MAX, HouseRates{ UINT32_MAX, UINT32_MAX }, 1, UINT64_MAX, terms));
}
