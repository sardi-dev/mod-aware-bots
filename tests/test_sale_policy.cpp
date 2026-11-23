/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */
#include "BotSalePolicy.h"
#include "TestHarness.h"

#include <string>

TEST(sale_policy_sells_only_priced_unwanted_items)
{
    BotSalePolicy::ItemFacts item;
    CHECK(!BotSalePolicy::ShouldSell(item));
    item.poorQuality = true;
    CHECK(!BotSalePolicy::ShouldSell(item));
    item.hasSellPrice = true;
    CHECK(BotSalePolicy::ShouldSell(item));
    item.poorQuality = false;
    item.gatheredMaterial = true;
    CHECK(BotSalePolicy::ShouldSell(item));
    item.gatheredMaterial = false;
    item.unwantedLoot = true;
    CHECK(BotSalePolicy::ShouldSell(item));
    item.unwantedLoot = false;
    CHECK(!BotSalePolicy::ShouldSell(item));
}

TEST(sale_policy_protection_overrides_even_poor_quality)
{
    BotSalePolicy::ItemFacts item;
    item.hasSellPrice = true;
    item.poorQuality = true;
    bool BotSalePolicy::ItemFacts::* const protections[] = {
        &BotSalePolicy::ItemFacts::equipped, &BotSalePolicy::ItemFacts::bag,
        &BotSalePolicy::ItemFacts::questItem, &BotSalePolicy::ItemFacts::professionTool,
        &BotSalePolicy::ItemFacts::professionMaterial, &BotSalePolicy::ItemFacts::reagent,
        &BotSalePolicy::ItemFacts::usableFoodOrDrink, &BotSalePolicy::ItemFacts::usefulGear
    };
    for (auto protection : protections)
    {
        item.*protection = true;
        CHECK(!BotSalePolicy::ShouldSell(item));
        item.*protection = false;
    }
}

TEST(sale_policy_gathered_materials_are_the_five_kinds)
{
    using BotSalePolicy::MaterialKind;
    CHECK(BotSalePolicy::IsGatheredMaterial(MaterialKind::Herb));
    CHECK(BotSalePolicy::IsGatheredMaterial(MaterialKind::Metal));
    CHECK(BotSalePolicy::IsGatheredMaterial(MaterialKind::Leather));
    CHECK(BotSalePolicy::IsGatheredMaterial(MaterialKind::Cloth));
    CHECK(BotSalePolicy::IsGatheredMaterial(MaterialKind::Meat));
    CHECK(!BotSalePolicy::IsGatheredMaterial(MaterialKind::Gem));
    CHECK(!BotSalePolicy::IsGatheredMaterial(MaterialKind::Other));
}

TEST(sale_policy_professions_use_their_own_materials)
{
    using namespace BotSalePolicy;
    CHECK(ProfessionUses(PROFESSION_ALCHEMY, MaterialKind::Herb));
    CHECK(ProfessionUses(PROFESSION_INSCRIPTION, MaterialKind::Herb));
    CHECK(!ProfessionUses(PROFESSION_TAILORING, MaterialKind::Herb));
    CHECK(ProfessionUses(PROFESSION_BLACKSMITHING, MaterialKind::Metal));
    CHECK(ProfessionUses(PROFESSION_ENGINEERING, MaterialKind::Metal));
    CHECK(ProfessionUses(PROFESSION_JEWELCRAFTING, MaterialKind::Metal));
    CHECK(!ProfessionUses(PROFESSION_COOKING, MaterialKind::Metal));
    CHECK(ProfessionUses(PROFESSION_LEATHERWORKING, MaterialKind::Leather));
    CHECK(!ProfessionUses(PROFESSION_BLACKSMITHING, MaterialKind::Leather));
    CHECK(ProfessionUses(PROFESSION_TAILORING, MaterialKind::Cloth));
    CHECK(ProfessionUses(PROFESSION_COOKING, MaterialKind::Meat));
    CHECK(ProfessionUses(PROFESSION_JEWELCRAFTING, MaterialKind::Gem));
    CHECK(!ProfessionUses(PROFESSION_NONE, MaterialKind::Herb));
    CHECK(!ProfessionUses(PROFESSION_ALCHEMY | PROFESSION_COOKING, MaterialKind::Other));
    // Several professions: any of them is enough.
    CHECK(ProfessionUses(PROFESSION_TAILORING | PROFESSION_COOKING, MaterialKind::Meat));
}

TEST(sale_policy_a_miner_without_a_smithing_skill_sells_its_ore)
{
    using namespace BotSalePolicy;
    ItemFacts ore;
    ore.hasSellPrice = true;
    ore.gatheredMaterial = IsGatheredMaterial(MaterialKind::Metal);
    ore.professionMaterial = ProfessionUses(PROFESSION_COOKING, MaterialKind::Metal);
    CHECK(ShouldSell(ore));
    ore.professionMaterial = ProfessionUses(PROFESSION_BLACKSMITHING, MaterialKind::Metal);
    CHECK(!ShouldSell(ore));
}

TEST(sale_policy_precious_items_are_never_sold)
{
    BotSalePolicy::ItemFacts item;
    item.hasSellPrice = true;
    item.unwantedLoot = true;
    CHECK(BotSalePolicy::ShouldSell(item));
    item.precious = true;
    CHECK(!BotSalePolicy::ShouldSell(item));
}

TEST(sale_policy_keep_cap_sells_the_surplus_stacks)
{
    using namespace BotSalePolicy;
    CHECK(KeepStack(0, FOOD_DRINK_KEEP_MAX));
    CHECK(KeepStack(FOOD_DRINK_KEEP_MAX - 1, FOOD_DRINK_KEEP_MAX));
    CHECK(!KeepStack(FOOD_DRINK_KEEP_MAX, FOOD_DRINK_KEEP_MAX));
    CHECK(KeepStack(PROFESSION_MATERIAL_KEEP_MAX - 1, PROFESSION_MATERIAL_KEEP_MAX));
    CHECK(!KeepStack(PROFESSION_MATERIAL_KEEP_MAX, PROFESSION_MATERIAL_KEEP_MAX));
}

TEST(sale_policy_bags_almost_full_at_eighty_percent)
{
    CHECK(!BotSalePolicy::BagsAlmostFull(0));
    CHECK(!BotSalePolicy::BagsAlmostFull(79));
    CHECK(BotSalePolicy::BagsAlmostFull(80));
    CHECK(BotSalePolicy::BagsAlmostFull(100));
}

TEST(sale_policy_log_is_rate_limited_per_bot)
{
    using namespace BotSalePolicy;
    CHECK(ShouldLogTrip(0, false));
    CHECK(!ShouldLogTrip(0, true));
    CHECK(!ShouldLogTrip(LOG_MIN_INTERVAL_MS - 1, true));
    CHECK(ShouldLogTrip(LOG_MIN_INTERVAL_MS, true));
}

TEST(supply_reserve_follows_the_repair_cost_of_the_level)
{
    // BT-213: 5 copper per level squared, never above one gold per 10 levels.
    using namespace BotSalePolicy;
    CHECK(PurchaseReserve(1) == 0);
    CHECK(PurchaseReserve(9) == 0);
    CHECK(PurchaseReserve(10) == 500);
    CHECK(PurchaseReserve(15) == 1125);
    CHECK(PurchaseReserve(19) == 1805);
    CHECK(PurchaseReserve(35) == 6125);
    CHECK(PurchaseReserve(90) == 40500);
    for (uint32_t level = 1; level <= 90; ++level)
    {
        CHECK(PurchaseReserve(level) <= uint64_t(level / RESERVE_LEVELS_PER_GOLD) * COPPER_PER_GOLD);
        CHECK(PurchaseReserve(level) >= PurchaseReserve(level - 1));
    }
}

TEST(supply_reserve_lets_a_poor_low_level_bot_buy)
{
    // BT-213, live 2026-11-16: a level 14 priest with 22 silver and a level 15 monk with 20 silver carried no food or
    // drink, because the old reserve kept 1 gold. Both can buy now.
    using namespace BotSalePolicy;
    CHECK(CanAffordSupplies(2235, PurchaseReserve(14)));
    CHECK(CanAffordSupplies(2061, PurchaseReserve(15)));
    CHECK(SupplyUnitsToBuy(0, 2061, PurchaseReserve(15), 25) == SUPPLY_STACK_TARGET);
}

TEST(supply_plan_buys_when_the_money_pays)
{
    using namespace BotSalePolicy;
    CHECK(PlanSupplies(false, 0, 0, 0) == SupplyPlan::None);
    CHECK(PlanSupplies(false, 100000, 0, 5000) == SupplyPlan::None);
    CHECK(PlanSupplies(true, 1000 + SUPPLY_MIN_SPARE_COPPER, 1000, 0) == SupplyPlan::Buy);
    CHECK(SupplyTripWanted(SupplyPlan::Buy));
    CHECK(!SupplyTripWanted(SupplyPlan::None));
}

TEST(supply_plan_sells_junk_first_when_short_of_money)
{
    using namespace BotSalePolicy;
    // 50 copper above the reserve is too little; the junk's 80 copper makes it enough.
    CHECK(PlanSupplies(true, 1050, 1000, 80) == SupplyPlan::SellThenBuy);
    CHECK(SupplyTripWanted(SupplyPlan::SellThenBuy));
    // Junk worth too little, or nothing to sell: the bot waits for loot and quests.
    CHECK(PlanSupplies(true, 1050, 1000, 49) == SupplyPlan::CannotPay);
    CHECK(PlanSupplies(true, 500, 1000, 0) == SupplyPlan::CannotPay);
    CHECK(!SupplyTripWanted(SupplyPlan::CannotPay));
    // Below the reserve, the sale must cover the gap too.
    CHECK(PlanSupplies(true, 500, 1000, 600) == SupplyPlan::SellThenBuy);
    CHECK(PlanSupplies(true, 500, 1000, 599) == SupplyPlan::CannotPay);
}

TEST(supply_first_kind_gets_half_the_spare_money_when_both_are_low)
{
    using namespace BotSalePolicy;
    CHECK(FirstKindMoney(3000, 1000, false) == 3000);
    CHECK(FirstKindMoney(3000, 1000, true) == 2000);
    CHECK(FirstKindMoney(1000, 1000, true) == 1000);
    CHECK(FirstKindMoney(500, 1000, true) == 500);
    // A poor caster with 2000 copper spare and 25 copper items: 20 drinks, and the rest still pays for 20 food.
    uint64_t const reserve = 1000;
    uint64_t const money = 3000;
    uint32_t const drink = SupplyUnitsToBuy(0, FirstKindMoney(money, reserve, true), reserve, 25);
    CHECK(drink == SUPPLY_STACK_TARGET);
    CHECK(SupplyUnitsToBuy(0, money - drink * 25, reserve, 25) == SUPPLY_STACK_TARGET);
    // With 600 spare and 50 copper items: 6 drinks, then 6 food, instead of 12 drinks and no food.
    uint32_t const fewDrinks = SupplyUnitsToBuy(0, FirstKindMoney(1600, reserve, true), reserve, 50);
    CHECK(fewDrinks == 6);
    CHECK(SupplyUnitsToBuy(0, 1600 - fewDrinks * 50, reserve, 50) == 6);
}

TEST(supply_trip_starts_below_the_low_water_when_the_bot_can_pay)
{
    using namespace BotSalePolicy;
    CHECK(SupplyLow(0));
    CHECK(SupplyLow(SUPPLY_LOW_WATER - 1));
    CHECK(!SupplyLow(SUPPLY_LOW_WATER));
    CHECK(!SupplyLow(SUPPLY_STACK_TARGET));
    CHECK(!CanAffordSupplies(0, 0));
    CHECK(CanAffordSupplies(SUPPLY_MIN_SPARE_COPPER, 0));
    CHECK(!CanAffordSupplies(SUPPLY_MIN_SPARE_COPPER - 1, 0));
    CHECK(!CanAffordSupplies(30000, 30000));
    CHECK(!CanAffordSupplies(30050, 30000));
    CHECK(CanAffordSupplies(30000 + SUPPLY_MIN_SPARE_COPPER, 30000));
}

TEST(supply_units_to_buy_fill_the_stack_when_low)
{
    using namespace BotSalePolicy;
    uint64_t const plenty = 1000 * COPPER_PER_GOLD;
    CHECK(SupplyUnitsToBuy(0, plenty, 0, 50) == SUPPLY_STACK_TARGET);
    CHECK(SupplyUnitsToBuy(SUPPLY_LOW_WATER - 1, plenty, 0, 50) == SUPPLY_STACK_TARGET - (SUPPLY_LOW_WATER - 1));
    // Holding the low water or more: nothing is bought.
    CHECK(SupplyUnitsToBuy(SUPPLY_LOW_WATER, plenty, 0, 50) == 0);
    CHECK(SupplyUnitsToBuy(SUPPLY_STACK_TARGET, plenty, 0, 50) == 0);
}

TEST(supply_units_to_buy_never_spend_the_reserve)
{
    using namespace BotSalePolicy;
    CHECK(SupplyUnitsToBuy(0, 1000, 1000, 50) == 0);
    CHECK(SupplyUnitsToBuy(0, 900, 1000, 50) == 0);
    // 1000 above the reserve pays for 4 items of 250 copper.
    CHECK(SupplyUnitsToBuy(0, 11000, 10000, 250) == 4);
    // Exactly enough for the whole stack.
    CHECK(SupplyUnitsToBuy(0, 10000 + 20 * 50, 10000, 50) == 20);
    CHECK(SupplyUnitsToBuy(0, 10000 + 20 * 50 - 1, 10000, 50) == 19);
}

TEST(supply_units_to_buy_free_items_fill_the_stack)
{
    CHECK(BotSalePolicy::SupplyUnitsToBuy(2, 0, 0, 0) == BotSalePolicy::SUPPLY_STACK_TARGET - 2);
}

TEST(supply_a_visit_from_empty_fills_past_the_low_water)
{
    // BT-177: one plan per visit. A bot that arrives with none buys the whole stack, not just the low water.
    using namespace BotSalePolicy;
    uint32_t const planned = SupplyUnitsToBuy(0, 1000 * COPPER_PER_GOLD, 0, 475);
    CHECK(planned == SUPPLY_STACK_TARGET);
    CHECK(planned > SUPPLY_LOW_WATER);
    // Planned again after the low water, the visit would stop there: that was the bug.
    CHECK(SupplyUnitsToBuy(SUPPLY_LOW_WATER, 1000 * COPPER_PER_GOLD, 0, 475) == 0);
}

TEST(finder_supply_bots_without_a_mana_need_are_ready)
{
    using namespace BotSalePolicy;
    // Rage, energy and focus users, and a mage that conjures its own, never wait for drink.
    CHECK(FinderSupplyOf(false, false, 0, true, true) == FinderSupply::Ready);
    CHECK(FinderSupplyOf(true, true, 0, true, true) == FinderSupply::Ready);
    // A caster that holds the low water or more is ready.
    CHECK(FinderSupplyOf(true, false, SUPPLY_LOW_WATER, false, false) == FinderSupply::Ready);
    CHECK(FinderSupplyOf(true, false, SUPPLY_STACK_TARGET, true, true) == FinderSupply::Ready);
}

TEST(finder_supply_a_caster_on_its_way_to_buy_drink_is_held_back)
{
    using namespace BotSalePolicy;
    FinderSupply const supply = FinderSupplyOf(true, false, 0, true, true);
    CHECK(supply == FinderSupply::Shopping);
    CHECK(!FinderMayTake(supply));
    CHECK(!FinderTakesLast(supply));
    CHECK(FinderSupplyOf(true, false, SUPPLY_LOW_WATER - 1, true, true) == FinderSupply::Shopping);
}

TEST(finder_supply_a_caster_that_cannot_shop_joins_last_and_says_why)
{
    using namespace BotSalePolicy;
    // The live case: a level 21 warlock with no drink and no vendor in sight.
    FinderSupply const noVendor = FinderSupplyOf(true, false, 0, true, false);
    CHECK(noVendor == FinderSupply::NoVendor);
    CHECK(FinderMayTake(noVendor));
    CHECK(FinderTakesLast(noVendor));
    CHECK(std::string(FinderSupplyReason(noVendor)) == "no vendor in sight");

    FinderSupply const noMoney = FinderSupplyOf(true, false, 2, false, false);
    CHECK(noMoney == FinderSupply::NoMoney);
    CHECK(FinderMayTake(noMoney));
    CHECK(FinderTakesLast(noMoney));
    CHECK(std::string(FinderSupplyReason(noMoney)) == "no money");

    CHECK(FinderMayTake(FinderSupply::Ready));
    CHECK(!FinderTakesLast(FinderSupply::Ready));
}
