/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */
#include "BotAuctionPolicy.h"
#include "TestHarness.h"

#include <map>
#include <set>
#include <vector>

using namespace BotAuctionPolicy;
using PricePolicy::HouseRates;
using PricePolicy::PriceClass;
using PricePolicy::PriceRange;

namespace
{
constexpr HouseRates FACTION_HOUSE{ 5, 5 };

// Loot the bot has no use for, listable at a faction house: a stack of gathered white goods (vendor 100 each, range
// 600 to 1200), which beats the vendor by far.
StackFacts Gathered(uint32_t entry, uint32_t count)
{
    StackFacts stack;
    stack.entry = entry;
    stack.count = count;
    stack.facts.gatheredMaterial = true;
    stack.facts.hasSellPrice = true;
    stack.tradable = true;
    stack.vendorSellPrice = 100;
    stack.hasRange = true;
    stack.range.minCopper = 600;
    stack.range.maxCopper = 1200;
    stack.range.priceClass = PriceClass::Gathered;
    return stack;
}

SellPlan Plan()
{
    SellPlan plan;
    plan.rates = FACTION_HOUSE;
    plan.maxListings = 10;
    plan.maxActiveListings = 20;
    plan.depositBudget = 1000000;
    return plan;
}

uint32_t Selected(std::vector<StackFacts> const& stacks, SellPlan const& plan = Plan())
{
    return uint32_t(SelectListings(stacks, plan).size());
}
}  // namespace

TEST(auction_policy_lists_a_whole_stack_of_surplus_goods)
{
    std::vector<StackFacts> const stacks = { Gathered(100, 17) };
    std::vector<Listing> const listings = SelectListings(stacks, Plan());
    CHECK(listings.size() == 1);
    CHECK(listings[0].stack == 0 && listings[0].entry == 100);
    CHECK(listings[0].count == 17);   // the whole stack, never a part
    CHECK(listings[0].minTotal == 600 * 17 && listings[0].vendorTotal == 100 * 17);
    CHECK(listings[0].deposit == 255);   // 0.15 x 100 x 17
}

TEST(auction_policy_every_vendor_protection_still_holds)
{
    bool BotSalePolicy::ItemFacts::* const protections[] = {
        &BotSalePolicy::ItemFacts::equipped,        &BotSalePolicy::ItemFacts::bag,
        &BotSalePolicy::ItemFacts::questItem,       &BotSalePolicy::ItemFacts::professionTool,
        &BotSalePolicy::ItemFacts::reagent,         &BotSalePolicy::ItemFacts::usefulGear,
        &BotSalePolicy::ItemFacts::precious
    };
    for (auto protection : protections)
    {
        StackFacts stack = Gathered(100, 5);
        stack.facts.*protection = true;
        CHECK(Selected({ stack }) == 0);
    }
}

TEST(auction_policy_needs_a_tradable_item_a_price_and_no_other_use)
{
    StackFacts stack = Gathered(100, 5);
    CHECK(Selected({ stack }) == 1);

    StackFacts bound = stack;
    bound.tradable = false;
    CHECK(Selected({ bound }) == 0);

    StackFacts wanted = stack;
    wanted.usageKeeps = true;
    CHECK(Selected({ wanted }) == 0);

    StackFacts unpriced = stack;
    unpriced.hasRange = false;
    CHECK(Selected({ unpriced }) == 0);

    StackFacts empty = stack;
    empty.count = 0;
    CHECK(Selected({ empty }) == 0);

    StackFacts huge = stack;
    huge.count = PricePolicy::MAX_LISTING_COUNT + 1;
    CHECK(Selected({ huge }) == 0);

    StackFacts aboveCap = stack;
    aboveCap.range.maxCopper = uint32_t(PricePolicy::TRADE_CAP_COPPER) + 1;
    CHECK(Selected({ aboveCap }) == 0);
}

TEST(auction_policy_uses_the_price_table_not_the_vendor_price_to_decide_an_item_has_a_price)
{
    StackFacts noVendorPrice = Gathered(100, 3);
    noVendorPrice.facts.hasSellPrice = false;   // the vendor would not buy it
    noVendorPrice.vendorSellPrice = 0;
    CHECK(Selected({ noVendorPrice }) == 1);    // the auction house still can

    StackFacts onlyVendorPrice = Gathered(100, 3);
    onlyVendorPrice.hasRange = false;
    CHECK(Selected({ onlyVendorPrice }) == 0);
}

namespace
{
StackFacts Material(uint32_t entry, uint32_t count, MaterialUse use)
{
    StackFacts stack = Gathered(entry, count);
    stack.material = use;
    return stack;
}

// A recipe a trainer teaches from `rank` (the spell id is the skill and the rank, so it is unique and readable).
RecipeUse Taught(uint32_t skill, uint32_t rank) { return { skill, skill * 1000 + rank, rank, 0, 0, RECIPE_UNTAUGHT }; }

// A recipe no trainer teaches: only a bot that knows the spell counts it.
RecipeUse Untaught(uint32_t skill, uint32_t spell) { return { skill, spell, RECIPE_UNTAUGHT, 0, 0, RECIPE_UNTAUGHT }; }

// Ore: the bar's recipe, reached through a smelting the bot also needs.
RecipeUse Smelted(uint32_t skill, uint32_t rank, uint32_t smeltRank)
{
    return { skill, skill * 1000 + rank, rank, Skill::MINING, Skill::MINING * 1000 + smeltRank, smeltRank };
}

std::vector<RecipeUse> Uses(std::vector<RecipeUse> uses) { return uses; }

// A bot's skills as the caller reads them: 0 for a skill it does not have.
auto Skills(std::map<uint32_t, uint32_t> skills)
{
    return [skills](uint32_t skill)
    {
        auto const itr = skills.find(skill);
        return itr == skills.end() ? 0u : itr->second;
    };
}

// The recipes the bot has learned.
auto Knows(std::set<uint32_t> spells)
{
    return [spells](uint32_t spell) { return spells.count(spell) > 0; };
}
}  // namespace

TEST(auction_policy_level_none_lists_every_stack_of_a_material_no_profession_uses)
{
    std::vector<StackFacts> const stacks = { Material(200, 20, MaterialUse::None), Material(200, 20, MaterialUse::None),
                                              Material(200, 5, MaterialUse::None) };
    std::vector<Listing> const listings = SelectListings(stacks, Plan());
    CHECK(listings.size() == 3);
    CHECK(MaterialKeepCap(MaterialUse::None) == 0);
}

TEST(auction_policy_level_secondary_keeps_twenty_across_stacks)
{
    static_assert(MaterialKeepCap(MaterialUse::Secondary) == 20 && MaterialKeepCap(MaterialUse::Gathered) == 20);
    for (MaterialUse use : { MaterialUse::Secondary, MaterialUse::Gathered })
    {
        // The first stack of 20 is kept (the cap is reached at 20); the next three are surplus, all equal here.
        std::vector<StackFacts> const stacks = { Material(200, 20, use), Material(200, 20, use), Material(200, 20, use),
                                                  Material(200, 20, use) };
        std::vector<Listing> const listings = SelectListings(stacks, Plan());
        CHECK(listings.size() == 3 && listings[0].stack == 1 && listings[1].stack == 2 && listings[2].stack == 3);

        // The cap is checked before a stack: a first stack of 200 is kept whole, the next is surplus.
        std::vector<Listing> const second = SelectListings({ Material(200, 200, use), Material(200, 5, use) }, Plan());
        CHECK(second.size() == 1 && second[0].stack == 1);

        // 15 + 10: the cap has not been reached before the second stack, so both are kept; the third is surplus.
        std::vector<Listing> const split = SelectListings({ Material(200, 15, use), Material(200, 10, use), Material(200, 1, use) }, Plan());
        CHECK(split.size() == 1 && split[0].stack == 2);
    }
}

TEST(auction_policy_level_crafting_keeps_sixty_across_stacks)
{
    static_assert(MaterialKeepCap(MaterialUse::Crafting) == 60);
    // 20 + 20 + 20 are all kept (the keep cap is reached at 60); the fourth stack is surplus.
    std::vector<StackFacts> const stacks = { Material(200, 20, MaterialUse::Crafting), Material(200, 20, MaterialUse::Crafting),
                                              Material(200, 20, MaterialUse::Crafting), Material(200, 20, MaterialUse::Crafting) };
    std::vector<Listing> const listings = SelectListings(stacks, Plan());
    CHECK(listings.size() == 1 && listings[0].stack == 3);

    // A first stack of 200 is kept whole (the cap is checked before a stack), the next is sold.
    std::vector<Listing> const second = SelectListings({ Material(200, 200, MaterialUse::Crafting), Material(200, 5, MaterialUse::Crafting) }, Plan());
    CHECK(second.size() == 1 && second[0].stack == 1);
}

TEST(auction_policy_keeps_each_material_entry_on_its_own_level)
{
    // Another entry has a reserve of its own, and a level of its own.
    CHECK(Selected({ Material(200, 20, MaterialUse::Secondary), Material(201, 20, MaterialUse::Secondary) }) == 0);
    CHECK(Selected({ Material(200, 60, MaterialUse::Crafting), Material(201, 20, MaterialUse::None) }) == 1);
    CHECK(Selected({ Material(200, 20, MaterialUse::Secondary), Material(200, 1, MaterialUse::Secondary), Material(201, 60, MaterialUse::Crafting) }) == 1);
}

TEST(auction_policy_ignores_the_vendor_trips_profession_flag)
{
    // The vendor trip's flag keeps 60 of anything a profession of the bot touches; the auction has its own levels.
    StackFacts stack = Material(200, 20, MaterialUse::None);
    stack.facts.professionMaterial = true;
    CHECK(Selected({ stack, stack, stack }) == 3);
}

TEST(auction_policy_lists_meat_and_ore_a_cooking_bot_held_back_under_the_vendor_cap)
{
    // Regression of the soak of 2026-11-23: three stacks of 20 (60 held) used to be kept whole, so nothing was ever listed.
    CHECK(Selected({ Material(300, 20, MaterialUse::Secondary), Material(300, 20, MaterialUse::Secondary),
                     Material(300, 20, MaterialUse::Secondary) }) == 2);
}

TEST(auction_policy_lists_trade_goods_the_vendor_trip_does_not_sell)
{
    // Elemental, parts and sacs are not gathered materials to the vendor trip, but a profession material nobody of the bot
    // uses is listed on the auction.
    StackFacts motes = Material(600, 5, MaterialUse::None);
    motes.facts.gatheredMaterial = false;
    CHECK(Selected({ motes }) == 0);
    motes.tradeGood = true;
    CHECK(Selected({ motes }) == 1);

    // The keep level still holds it back.
    motes.material = MaterialUse::Crafting;
    CHECK(Selected({ motes }) == 0);

    // A trade good is still not listed when a protection applies.
    motes.material = MaterialUse::None;
    motes.facts.questItem = true;
    CHECK(Selected({ motes }) == 0);
}

TEST(auction_material_use_is_crafting_when_a_crafting_profession_of_the_bot_works_it)
{
    using BotSalePolicy::MaterialKind;
    std::vector<RecipeUse> const uses = Uses({ Taught(Skill::BLACKSMITHING, 1), Taught(Skill::COOKING, 1) });
    // A blacksmith that also cooks: the crafting use wins over the secondary one, in either order of the recipes.
    CHECK(MaterialUseOf(uses, MaterialKind::Metal, Skills({ { Skill::BLACKSMITHING, 20 }, { Skill::COOKING, 5 } }), Knows({})) == MaterialUse::Crafting);
    std::vector<RecipeUse> const reversed = Uses({ Taught(Skill::COOKING, 1), Taught(Skill::BLACKSMITHING, 1) });
    CHECK(MaterialUseOf(reversed, MaterialKind::Metal, Skills({ { Skill::BLACKSMITHING, 20 }, { Skill::COOKING, 5 } }), Knows({})) == MaterialUse::Crafting);

    // A trainer teaches the recipe only from rank 150: below it the bot does not use the material, from it the bot does.
    std::vector<RecipeUse> const high = Uses({ Taught(Skill::BLACKSMITHING, 150) });
    CHECK(MaterialUseOf(high, MaterialKind::Metal, Skills({ { Skill::BLACKSMITHING, 149 } }), Knows({})) == MaterialUse::None);
    CHECK(MaterialUseOf(high, MaterialKind::Metal, Skills({ { Skill::BLACKSMITHING, 150 } }), Knows({})) == MaterialUse::Crafting);
}

TEST(auction_material_use_counts_a_recipe_the_bot_knows_that_no_trainer_teaches)
{
    using BotSalePolicy::MaterialKind;
    std::vector<RecipeUse> const uses = Uses({ Untaught(Skill::JEWELCRAFTING, 4242) });
    CHECK(MaterialUseOf(uses, MaterialKind::Gem, Skills({ { Skill::JEWELCRAFTING, 300 } }), Knows({})) == MaterialUse::None);
    CHECK(MaterialUseOf(uses, MaterialKind::Gem, Skills({ { Skill::JEWELCRAFTING, 300 } }), Knows({ 4242 })) == MaterialUse::Crafting);
    // A known spell without the skill is not a profession of the bot.
    CHECK(MaterialUseOf(uses, MaterialKind::Gem, Skills({}), Knows({ 4242 })) == MaterialUse::None);

    // A known recipe above the skill value of a trainer's rank still counts.
    std::vector<RecipeUse> const high = Uses({ Taught(Skill::BLACKSMITHING, 150) });
    CHECK(MaterialUseOf(high, MaterialKind::Metal, Skills({ { Skill::BLACKSMITHING, 20 } }), Knows({ Skill::BLACKSMITHING * 1000 + 150 })) == MaterialUse::Crafting);
}

TEST(auction_material_use_is_secondary_for_a_secondary_skill_only)
{
    using BotSalePolicy::MaterialKind;
    std::vector<RecipeUse> const meat = Uses({ Taught(Skill::COOKING, 1) });
    CHECK(MaterialUseOf(meat, MaterialKind::Meat, Skills({ { Skill::COOKING, 1 }, { Skill::BLACKSMITHING, 300 } }), Knows({})) == MaterialUse::Secondary);
    // Linen cloth: First Aid and Tailoring. A bot with First Aid only keeps 20; with Tailoring it keeps 60.
    std::vector<RecipeUse> const cloth = Uses({ Taught(Skill::FIRST_AID, 1), Taught(Skill::TAILORING, 1) });
    CHECK(MaterialUseOf(cloth, MaterialKind::Cloth, Skills({ { Skill::FIRST_AID, 150 } }), Knows({})) == MaterialUse::Secondary);
    CHECK(MaterialUseOf(cloth, MaterialKind::Cloth, Skills({ { Skill::FIRST_AID, 150 }, { Skill::TAILORING, 1 } }), Knows({})) == MaterialUse::Crafting);
}

TEST(auction_material_use_is_none_for_a_profession_the_bot_lacks)
{
    using BotSalePolicy::MaterialKind;
    std::vector<RecipeUse> const gem = Uses({ Taught(Skill::JEWELCRAFTING, 1) });
    CHECK(MaterialUseOf(gem, MaterialKind::Gem, Skills({ { Skill::BLACKSMITHING, 300 }, { Skill::COOKING, 300 } }), Knows({})) == MaterialUse::None);
    // A skill the bot has at 0 is a skill it does not have.
    CHECK(MaterialUseOf(gem, MaterialKind::Gem, Skills({ { Skill::JEWELCRAFTING, 0 } }), Knows({})) == MaterialUse::None);
    // No recipe takes the item at all.
    CHECK(MaterialUseOf({}, MaterialKind::Other, Skills({ { Skill::JEWELCRAFTING, 300 } }), Knows({})) == MaterialUse::None);
}

TEST(auction_material_use_is_gathered_for_a_gatherer_with_no_crafting_use)
{
    using BotSalePolicy::MaterialKind;
    // Copper ore: Mining smelts it; Blacksmithing works the bar the smelting makes (the smelting is also needed).
    std::vector<RecipeUse> const ore = Uses({ Taught(Skill::MINING, 1), Smelted(Skill::BLACKSMITHING, 1, 1) });
    CHECK(MaterialUseOf(ore, MaterialKind::Metal, Skills({ { Skill::MINING, 50 } }), Knows({})) == MaterialUse::Gathered);
    CHECK(MaterialUseOf(ore, MaterialKind::Metal, Skills({ { Skill::MINING, 50 }, { Skill::BLACKSMITHING, 5 } }), Knows({})) == MaterialUse::Crafting);
    // A blacksmith that cannot smelt does not use the ore (the bars carry the use), and does not gather it either.
    CHECK(MaterialUseOf(ore, MaterialKind::Metal, Skills({ { Skill::BLACKSMITHING, 5 } }), Knows({})) == MaterialUse::None);
    // The smelting rank is checked too, and a smelting the miner knows counts below it.
    std::vector<RecipeUse> const tin = Uses({ Smelted(Skill::BLACKSMITHING, 1, 65) });
    CHECK(MaterialUseOf(tin, MaterialKind::Metal, Skills({ { Skill::MINING, 64 }, { Skill::BLACKSMITHING, 5 } }), Knows({})) == MaterialUse::Gathered);
    CHECK(MaterialUseOf(tin, MaterialKind::Metal, Skills({ { Skill::MINING, 65 }, { Skill::BLACKSMITHING, 5 } }), Knows({})) == MaterialUse::Crafting);
    CHECK(MaterialUseOf(tin, MaterialKind::Metal, Skills({ { Skill::MINING, 64 }, { Skill::BLACKSMITHING, 5 } }), Knows({ Skill::MINING * 1000 + 65 })) == MaterialUse::Crafting);

    // A herbalist with no recipe use keeps its herbs at 20; a skinner its leather; nothing for cloth or meat.
    CHECK(MaterialUseOf({}, MaterialKind::Herb, Skills({ { Skill::HERBALISM, 1 } }), Knows({})) == MaterialUse::Gathered);
    CHECK(MaterialUseOf({}, MaterialKind::Leather, Skills({ { Skill::SKINNING, 1 } }), Knows({})) == MaterialUse::Gathered);
    CHECK(MaterialUseOf({}, MaterialKind::Herb, Skills({ { Skill::SKINNING, 1 } }), Knows({})) == MaterialUse::None);
    CHECK(MaterialUseOf({}, MaterialKind::Cloth, Skills({ { Skill::HERBALISM, 1 }, { Skill::MINING, 1 }, { Skill::SKINNING, 1 } }), Knows({})) == MaterialUse::None);
    CHECK(MaterialUseOf({}, MaterialKind::Meat, Skills({ { Skill::SKINNING, 1 } }), Knows({})) == MaterialUse::None);

    // Alchemy beats the gatherer level for the herbs it works.
    CHECK(MaterialUseOf(Uses({ Taught(Skill::ALCHEMY, 1) }), MaterialKind::Herb, Skills({ { Skill::HERBALISM, 1 }, { Skill::ALCHEMY, 1 } }), Knows({})) == MaterialUse::Crafting);
}

TEST(auction_policy_keeps_forty_food_or_drink)
{
    auto food = [](uint32_t count)
    {
        StackFacts stack = Gathered(300, count);
        stack.facts.gatheredMaterial = false;
        stack.facts.unwantedLoot = true;
        stack.facts.usableFoodOrDrink = true;
        return stack;
    };
    std::vector<Listing> const listings = SelectListings({ food(20), food(20), food(20) }, Plan());
    CHECK(listings.size() == 1 && listings[0].stack == 2);
}

TEST(auction_policy_bot_made_goods_are_listed_like_unwanted_loot)
{
    StackFacts crafted = Gathered(400, 4);
    crafted.facts.gatheredMaterial = false;
    crafted.crafted = true;
    CHECK(Selected({ crafted }) == 1);

    StackFacts notCrafted = crafted;
    notCrafted.crafted = false;
    CHECK(Selected({ notCrafted }) == 0);   // an item with no sell reason is not sold

    StackFacts worn = crafted;
    worn.facts.equipped = true;
    CHECK(Selected({ worn }) == 0);         // a protection beats "crafted"
}

TEST(auction_policy_leaves_junk_to_the_vendor)
{
    StackFacts junk = Gathered(500, 10);
    junk.facts.gatheredMaterial = false;
    junk.facts.poorQuality = true;
    junk.range.minCopper = 110;   // 1.1 x the vendor price: gone to the cut and the deposit
    junk.range.maxCopper = 160;
    CHECK(Selected({ junk }) == 0);

    StackFacts single = Gathered(501, 1);
    single.vendorSellPrice = 25;
    single.range.minCopper = 60;
    single.range.maxCopper = 90;
    CHECK(Selected({ single }) == 0);   // the 100 copper minimum deposit
}

TEST(auction_policy_picks_the_biggest_gains_first_within_the_caps)
{
    StackFacts small = Gathered(1, 5);       // gain 5 x 500
    StackFacts big = Gathered(2, 50);        // gain 50 x 500
    StackFacts middle = Gathered(3, 20);     // gain 20 x 500
    SellPlan plan = Plan();
    plan.maxListings = 2;
    std::vector<Listing> const listings = SelectListings({ small, big, middle }, plan);
    CHECK(listings.size() == 2);
    CHECK(listings[0].stack == 1 && listings[1].stack == 2);

    plan.maxListings = 0;
    CHECK(SelectListings({ small, big, middle }, plan).empty());

    plan.maxListings = 5;
    plan.activeListings = 19;   // one place left of 20
    std::vector<Listing> const room = SelectListings({ small, big, middle }, plan);
    CHECK(room.size() == 1 && room[0].stack == 1);

    plan.activeListings = 20;
    CHECK(SelectListings({ small, big, middle }, plan).empty());
}

TEST(auction_policy_deposits_count_against_the_visit_budget)
{
    StackFacts big = Gathered(2, 50);      // deposit 3 x 5 x 100 x 50 / 100 = 750
    StackFacts small = Gathered(1, 5);     // deposit 75 -> the 100 copper minimum
    SellPlan plan = Plan();
    plan.depositBudget = 800;
    std::vector<Listing> listings = SelectListings({ small, big }, plan);
    CHECK(listings.size() == 1 && listings[0].stack == 1);   // the big one first, then 50 left is not enough

    plan.depositBudget = 849;
    listings = SelectListings({ small, big }, plan);
    CHECK(listings.size() == 1);

    plan.depositBudget = 850;
    CHECK(SelectListings({ small, big }, plan).size() == 2);

    plan.depositBudget = 700;   // the big one does not fit but the small one behind it does
    listings = SelectListings({ small, big }, plan);
    CHECK(listings.size() == 1 && listings[0].stack == 0);

    plan.depositBudget = 0;
    CHECK(SelectListings({ small, big }, plan).empty());
}

TEST(auction_policy_one_stack_is_never_split_across_two_listings)
{
    std::vector<StackFacts> const stacks = { Gathered(7, 33), Gathered(7, 12) };
    std::vector<Listing> const listings = SelectListings(stacks, Plan());
    CHECK(listings.size() == 2);
    uint32_t total = 0;
    for (Listing const& listing : listings)
        total += listing.count;
    CHECK(total == 45);
    CHECK(listings[0].count == 33 || listings[0].count == 12);
}

TEST(auction_policy_a_longer_listing_can_stop_beating_the_vendor)
{
    StackFacts stack = Gathered(9, 10);
    stack.vendorSellPrice = 700;
    stack.range.minCopper = 1000;
    stack.range.maxCopper = 2000;
    SellPlan plan = Plan();
    CHECK(Selected({ stack }, plan) == 1);                  // 12 hours: deposit 1050
    plan.durationSeconds = 4 * PricePolicy::LISTING_SECONDS;
    CHECK(Selected({ stack }, plan) == 0);                  // 48 hours: deposit 4200
}

TEST(auction_policy_plans_a_long_listing_for_a_stack_worth_more_than_the_threshold)
{
    // 17 items at a range minimum of 600 is 10200 in all: below 5 gold, so 12 hours; 100 items is 60000: 24 hours.
    SellPlan plan = Plan();
    plan.longListingAboveCopper = PricePolicy::DEFAULT_LONG_LISTING_ABOVE_COPPER;
    std::vector<StackFacts> const stacks = { Gathered(100, 17), Gathered(200, 100) };
    std::vector<Listing> const listings = SelectListings(stacks, plan);
    CHECK(listings.size() == 2);
    for (Listing const& listing : listings)
    {
        if (listing.entry == 100)
        {
            CHECK(listing.durationSeconds == PricePolicy::LISTING_SECONDS);
            CHECK(listing.deposit == 255);
        }
        else
        {
            CHECK(listing.durationSeconds == PricePolicy::LONG_LISTING_SECONDS);
            CHECK(listing.deposit == 3000);   // 0.15 x 100 x 100 x 2
        }
    }
}

TEST(auction_policy_keeps_every_listing_short_when_the_long_listing_is_off)
{
    SellPlan plan = Plan();   // longListingAboveCopper stays 0
    std::vector<Listing> const listings = SelectListings({ Gathered(200, 100) }, plan);
    CHECK(listings.size() == 1 && listings[0].durationSeconds == PricePolicy::LISTING_SECONDS && listings[0].deposit == 1500);
}

TEST(auction_policy_counts_the_doubled_deposit_of_a_long_listing_against_the_budget)
{
    SellPlan plan = Plan();
    plan.longListingAboveCopper = PricePolicy::DEFAULT_LONG_LISTING_ABOVE_COPPER;
    plan.depositBudget = 3000;   // the long deposit is 3000: it fits exactly
    std::vector<Listing> listings = SelectListings({ Gathered(200, 100) }, plan);
    CHECK(listings.size() == 1 && listings[0].durationSeconds == PricePolicy::LONG_LISTING_SECONDS);
    plan.depositBudget = 2000;   // too little for 24 hours, enough for the short one (1500)
    listings = SelectListings({ Gathered(200, 100) }, plan);
    CHECK(listings.size() == 1 && listings[0].durationSeconds == PricePolicy::LISTING_SECONDS && listings[0].deposit == 1500);
    plan.depositBudget = 1000;   // neither
    CHECK(SelectListings({ Gathered(200, 100) }, plan).empty());
}

TEST(auction_policy_falls_back_to_the_short_listing_when_the_long_one_would_lose_to_the_vendor)
{
    // One item with vendor price 40000 and a range minimum of 51000 (above 5 gold): the doubled deposit (12000) makes the
    // long listing lose to the vendor (54550 against 51000), where the short one wins (48550).
    StackFacts stack = Gathered(300, 1);
    stack.vendorSellPrice = 40000;
    stack.range.minCopper = 51000;
    stack.range.maxCopper = 60000;
    SellPlan plan = Plan();
    plan.longListingAboveCopper = PricePolicy::DEFAULT_LONG_LISTING_ABOVE_COPPER;
    std::vector<Listing> const listings = SelectListings({ stack }, plan);
    CHECK(listings.size() == 1);
    CHECK(listings[0].durationSeconds == PricePolicy::LISTING_SECONDS && listings[0].deposit == 6000);
}

TEST(auction_policy_drops_a_stack_that_loses_to_the_vendor_at_either_duration)
{
    StackFacts stack = Gathered(300, 1);
    stack.vendorSellPrice = 40000;
    stack.range.minCopper = 44000;   // short costs 2200 + 6000 + 40000 = 48200: loses
    stack.range.maxCopper = 60000;
    SellPlan plan = Plan();
    plan.longListingAboveCopper = 1;
    CHECK(SelectListings({ stack }, plan).empty());
}
