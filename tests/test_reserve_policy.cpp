/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */
#include "ReservePolicy.h"
#include "TestHarness.h"

#include <limits>

using namespace ReservePolicy;

namespace
{
constexpr uint64_t GOLD = PricePolicy::COPPER_PER_GOLD;
constexpr uint32_t MAGE = 8;

Facts BotAt(uint32_t level)
{
    Facts facts;
    facts.level = level;
    facts.classId = MAGE;
    return facts;
}

Params NoCap()
{
    Params params;
    params.goalSharePercent = 100;
    return params;
}

// A bot that bought everything its level allows: riding, the licence and a mount.
Facts Equipped(uint32_t level, uint32_t ridingSkill, bool licence = false)
{
    Facts facts = BotAt(level);
    facts.ridingSkill = ridingSkill;
    facts.knowsFlightLicence = licence;
    facts.knowsMount = true;
    return facts;
}
}  // namespace

// ---------------------------------------------------------------------------------------------------------------
// The generated table (tools/economy/export_reserve_table.py): shape and the facts the model rests on.
// ---------------------------------------------------------------------------------------------------------------
TEST(reserve_table_class_costs_never_fall_and_fit_the_levels)
{
    for (uint32_t classId = 0; classId < ReserveTable::CLASS_SLOTS; ++classId)
        for (uint32_t level = 1; level <= ReserveTable::MAX_LEVEL; ++level)
            CHECK(ReserveTable::CLASS_SPELL_COST[classId][level] >= ReserveTable::CLASS_SPELL_COST[classId][level - 1]);

    // Ten classes have trainer spells (1 to 9 and 11); slot 0 and 10 are empty.
    CHECK(ReserveTable::CLASS_SPELL_COST[0][ReserveTable::MAX_LEVEL] == 0);
    CHECK(ReserveTable::CLASS_SPELL_COST[10][ReserveTable::MAX_LEVEL] == 0);
    for (uint32_t classId : { 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 11u })
        CHECK(ReserveTable::CLASS_SPELL_COST[classId][ReserveTable::MAX_LEVEL] > 90 * GOLD);

    // The death knight trains nothing before level 55 (world.npc_trainer template 200006).
    CHECK(ReserveTable::CLASS_SPELL_COST[6][54] == 0 && ReserveTable::CLASS_SPELL_COST[6][55] > 0);
}

TEST(reserve_table_riding_and_ranks_are_in_buying_order)
{
    uint32_t previousLevel = 0;
    for (ReserveTable::RidingStep const& step : ReserveTable::RIDING_STEPS)
    {
        CHECK(step.level >= previousLevel);
        CHECK(step.cost > 0);
        previousLevel = step.level;
    }

    previousLevel = 0;
    uint32_t previousCost = 0;
    for (ReserveTable::RankStep const& rank : ReserveTable::PROFESSION_RANKS)
    {
        CHECK(rank.level > previousLevel && rank.cost > previousCost);
        previousLevel = rank.level;
        previousCost = rank.cost;
    }
}

TEST(reserve_table_holds_the_riding_prices_of_the_5_4_8_trainers)
{
    // world.npc_trainer: Apprentice 4 gold at level 20, Journeyman 50 at 40, Expert 250 at 60, the licence 250 at 60,
    // Artisan 5000 at 70, Master 5000 at 80; the cheapest vendor mount is 1 gold at level 20.
    CHECK(ReserveTable::RIDING_STEPS[0].skillGiven == 75 && ReserveTable::RIDING_STEPS[0].cost == 4 * GOLD &&
          ReserveTable::RIDING_STEPS[0].level == 20);
    CHECK(ReserveTable::RIDING_STEPS[1].skillGiven == 150 && ReserveTable::RIDING_STEPS[1].cost == 50 * GOLD);
    CHECK(ReserveTable::RIDING_STEPS[2].skillGiven == 225 && ReserveTable::RIDING_STEPS[2].cost == 250 * GOLD);
    CHECK(ReserveTable::RIDING_STEPS[3].skillGiven == 0 && ReserveTable::RIDING_STEPS[3].cost == 250 * GOLD);
    CHECK(ReserveTable::RIDING_STEPS[4].skillGiven == 300 && ReserveTable::RIDING_STEPS[4].cost == 5000 * GOLD);
    CHECK(ReserveTable::RIDING_STEPS[5].skillGiven == 375 && ReserveTable::RIDING_STEPS[5].level == 80);
    CHECK(ReserveTable::MOUNT_PRICE == 1 * GOLD && ReserveTable::MOUNT_LEVEL == 20);
}

// ---------------------------------------------------------------------------------------------------------------
// The goals.
// ---------------------------------------------------------------------------------------------------------------
TEST(reserve_a_young_bot_keeps_nothing_for_riding_until_the_first_step_is_near)
{
    // Apprentice Riding wants level 20: five levels ahead is the first level the fund counts it.
    Goals goals = ComputeGoals(BotAt(14), Params());
    CHECK(goals.Total() == 0);

    goals = ComputeGoals(BotAt(15), Params());
    CHECK(goals.riding == 4 * GOLD && goals.mount == 1 * GOLD && goals.Total() == 5 * GOLD);
}

TEST(reserve_the_mount_is_kept_until_the_bot_knows_one)
{
    Facts facts = BotAt(25);
    facts.ridingSkill = 75;
    Goals goals = ComputeGoals(facts, Params());
    CHECK(goals.riding == 0 && goals.mount == 1 * GOLD);   // Journeyman wants level 40, ten levels away

    facts.knowsMount = true;
    CHECK(ComputeGoals(facts, Params()).Total() == 0);
}

TEST(reserve_the_next_riding_skill_follows_the_ladder_of_5_4_8)
{
    CHECK(ComputeGoals(Equipped(35, 75), Params()).riding == 50 * GOLD);
    CHECK(ComputeGoals(Equipped(30, 75), Params()).riding == 0);          // level 40 is ten levels away
    CHECK(ComputeGoals(Equipped(55, 150), Params()).riding == 250 * GOLD);
    CHECK(ComputeGoals(Equipped(60, 225), Params()).riding == 250 * GOLD);   // the licence, Expert is already bought
    CHECK(ComputeGoals(Equipped(65, 225, true), Params()).riding == 5000 * GOLD);
    CHECK(ComputeGoals(Equipped(80, 300, true), Params()).riding == 5000 * GOLD);   // Master Riding
    CHECK(ComputeGoals(Equipped(90, 375, true), Params()).Total() == 0);   // everything bought
}

TEST(reserve_only_the_next_riding_step_is_counted)
{
    // At level 60 a bot with Journeyman needs Expert (250) then the licence (250) then Artisan: only Expert is kept.
    CHECK(ComputeGoals(Equipped(60, 150), Params()).riding == 250 * GOLD);
    // A bot that has the skill of Artisan but not the licence keeps the licence price, not Artisan's.
    CHECK(ComputeGoals(Equipped(70, 300), Params()).riding == 250 * GOLD);
}

TEST(reserve_profession_ranks_in_the_window_count_once_per_profession)
{
    Facts facts = BotAt(31);
    facts.knowsMount = true;
    facts.ridingSkill = 75;
    facts.professionCount = 2;
    // Artisan wants level 35: inside 31 to 36; 50000 copper per profession.
    CHECK(ComputeGoals(facts, Params()).professionRanks == 2 * 50000);
    facts.level = 35;   // the window starts above the level: Master wants 50
    CHECK(ComputeGoals(facts, Params()).professionRanks == 0);
    facts.level = 46;   // Master (level 50) is four levels away, 100000 each
    facts.professionCount = 3;
    CHECK(ComputeGoals(facts, Params()).professionRanks == 3 * 100000);
    facts.professionCount = 0;
    CHECK(ComputeGoals(facts, Params()).professionRanks == 0);
    facts.professionCount = 99;   // capped at four professions
    CHECK(ComputeGoals(facts, Params()).professionRanks == MAX_COUNTED_PROFESSIONS * 100000);
}

TEST(reserve_class_spells_are_kept_only_when_the_bot_pays_for_them)
{
    Facts facts = BotAt(20);
    facts.knowsMount = true;
    facts.ridingSkill = 75;
    CHECK(ComputeGoals(facts, Params()).classSpells == 0);   // random bots learn them free at level-up

    facts.classSpellsPaid = true;
    uint64_t const window = ReserveTable::CLASS_SPELL_COST[MAGE][25] - ReserveTable::CLASS_SPELL_COST[MAGE][20];
    CHECK(window == 4400);
    CHECK(ComputeGoals(facts, Params()).classSpells == window);

    facts.pendingClassTrainCopper = 1234;   // learnable now, not yet bought
    CHECK(ComputeGoals(facts, Params()).classSpells == window + 1234);

    facts.classId = 0;
    CHECK(ComputeGoals(facts, Params()).classSpells == 0);
    facts.classId = 99;
    CHECK(ComputeGoals(facts, Params()).classSpells == 0);
}

TEST(reserve_the_lookahead_widens_and_is_capped)
{
    Params params;
    params.lookaheadLevels = 10;
    CHECK(ComputeGoals(Equipped(30, 75), params).riding == 50 * GOLD);   // level 40 is now ten levels away
    params.lookaheadLevels = 0;
    CHECK(ComputeGoals(Equipped(40, 75), params).riding == 50 * GOLD);   // a step due now still counts
    CHECK(ComputeGoals(Equipped(39, 75), params).riding == 0);
    params.lookaheadLevels = 1000;                                      // capped at MAX_LOOKAHEAD_LEVELS
    CHECK(ComputeGoals(Equipped(10, 75), params).riding == 0);          // level 40 is thirty levels away
    CHECK(ComputeGoals(Equipped(20, 75), params).riding == 50 * GOLD);
}

TEST(reserve_levels_past_the_table_do_not_read_outside_it)
{
    Facts facts = BotAt(90);
    facts.classSpellsPaid = true;
    facts.knowsMount = true;
    facts.ridingSkill = 375;
    facts.knowsFlightLicence = true;
    facts.professionCount = 2;
    CHECK(ComputeGoals(facts, Params()).Total() == 0);
    facts.level = std::numeric_limits<uint32_t>::max();
    CHECK(ComputeGoals(facts, Params()).Total() == 0);
}

// ---------------------------------------------------------------------------------------------------------------
// The reserve and the share cap.
// ---------------------------------------------------------------------------------------------------------------
TEST(reserve_adds_the_goals_to_the_repair_reserve_for_a_rich_bot)
{
    Facts facts = BotAt(20);
    Reserve const reserve = ComputeReserve(facts, Params(), 1000 * GOLD);
    CHECK(reserve.repair == BotSalePolicy::PurchaseReserve(20));   // 2000 copper
    CHECK(reserve.goals.Total() == 5 * GOLD);
    CHECK(reserve.heldForGoals == 5 * GOLD);
    CHECK(reserve.Total() == 2000 + 5 * GOLD);
}

TEST(reserve_a_poor_bot_keeps_only_its_share_of_the_gold_above_the_repair_reserve)
{
    Facts facts = BotAt(20);
    CHECK(DEFAULT_GOAL_SHARE_PERCENT == 75);   // owner answer 2026-11-22
    // 10 gold: repair 2000, spare 98000, three quarters could be kept (73500), the goals want only 50000.
    Reserve reserve = ComputeReserve(facts, Params(), 10 * GOLD);
    CHECK(reserve.heldForGoals == 50000 && reserve.Total() == 52000);
    // 5 gold: repair 2000, spare 48000, three quarters = 36000 of the 50000 wanted.
    reserve = ComputeReserve(facts, Params(), 5 * GOLD);
    CHECK(reserve.heldForGoals == 36000 && reserve.Total() == 38000);
    // The earlier recommendation, one half, is still a setting.
    Params half;
    half.goalSharePercent = 50;
    reserve = ComputeReserve(facts, half, 10 * GOLD);
    CHECK(reserve.heldForGoals == 49000 && reserve.Total() == 51000);
    // The owner's literal rule: the whole fund, up to every copper above the repair reserve.
    reserve = ComputeReserve(facts, NoCap(), 10 * GOLD);
    CHECK(reserve.heldForGoals == 50000 && reserve.Total() == 52000);
    reserve = ComputeReserve(facts, NoCap(), 3 * GOLD);
    CHECK(reserve.heldForGoals == 28000 && reserve.Total() == 30000);   // all of the 28000 above the repair reserve
    CHECK(reserve.Total() == 3 * GOLD);
}

TEST(reserve_a_bot_below_its_repair_reserve_keeps_the_repair_reserve_and_no_goal)
{
    Reserve const reserve = ComputeReserve(BotAt(20), Params(), 1500);
    CHECK(reserve.repair == 2000 && reserve.heldForGoals == 0 && reserve.Total() == 2000);
}

TEST(reserve_share_percent_is_clamped_to_a_hundred)
{
    Params params;
    params.goalSharePercent = 5000;
    Reserve const reserve = ComputeReserve(BotAt(20), params, 3 * GOLD);
    CHECK(reserve.Total() == 3 * GOLD);   // never more than the bot holds
    params.goalSharePercent = 0;
    CHECK(ComputeReserve(BotAt(20), params, 3 * GOLD).Total() == 2000);   // no cap share: the repair reserve alone
}

TEST(reserve_a_level_70_bot_saving_for_5000_gold_is_not_locked_out_with_the_cap)
{
    // The case the cap exists for: 300 gold held against an Artisan Riding fund of 5000 gold.
    Facts facts = Equipped(66, 225, true);
    Reserve capped = ComputeReserve(facts, Params(), 300 * GOLD);
    CHECK(capped.goals.riding == 5000 * GOLD);
    CHECK(capped.Total() < 300 * GOLD);
    CHECK(MakeVisitMoney(300 * GOLD, capped).wallet.budget > 0);

    Reserve literal = ComputeReserve(facts, NoCap(), 300 * GOLD);
    CHECK(literal.Total() == 300 * GOLD);
    CHECK(MakeVisitMoney(300 * GOLD, literal).wallet.budget == 0);   // the owner's rule taken literally: nothing to buy with
}

// ---------------------------------------------------------------------------------------------------------------
// The visit's money.
// ---------------------------------------------------------------------------------------------------------------
TEST(reserve_visit_budget_is_a_quarter_of_the_gold_above_the_full_reserve)
{
    Reserve const reserve = ComputeReserve(BotAt(20), Params(), 100 * GOLD);   // 2000 + 5 gold
    VisitMoney visit = MakeVisitMoney(100 * GOLD, reserve);
    CHECK(visit.reserve == 52000);
    CHECK(visit.wallet.budget == (100 * GOLD - 52000) / 4);
    CHECK(visit.depositBudget == (100 * GOLD - 2000) / 4);   // deposits count against the repair reserve only
}

TEST(reserve_a_purchase_never_dips_into_the_fund)
{
    Reserve const reserve = ComputeReserve(BotAt(20), Params(), 100 * GOLD);
    VisitMoney visit = MakeVisitMoney(100 * GOLD, reserve);
    uint64_t money = 100 * GOLD;
    // The budget allows 237,000 copper of purchases; a purchase that would leave less than the reserve is refused.
    CHECK(CanBuy(visit, money, 200000));
    CHECK(!CanBuy(visit, 60000, 10000));   // 60000 - 10000 = 50000 < the reserve of 52000
    CHECK(CanBuy(visit, 62000, 10000));
    CHECK(SpendPurchase(visit, money, 200000));
    money -= 200000;
    CHECK(!CanBuy(visit, money, 100000));   // the 25 percent budget is nearly used: 37,000 left
    CHECK(CanBuy(visit, money, 37000));
}

TEST(reserve_a_poor_bot_can_still_pay_a_deposit_but_buys_nothing_with_the_full_cap)
{
    // 5000 copper at level 20: repair reserve 2000; goals 50000.
    Facts facts = BotAt(20);
    uint64_t money = 5000;
    VisitMoney visit = MakeVisitMoney(money, ComputeReserve(facts, NoCap(), money));
    CHECK(visit.wallet.budget == 0 && !CanBuy(visit, money, 1));
    CHECK(visit.depositBudget == 750);                // a quarter of the 3000 above the repair reserve
    CHECK(DepositLeft(visit, money) == 750);
    CHECK(SpendDeposit(visit, money, 255));
    money -= 255;
    CHECK(DepositLeft(visit, money) == 495);
}

TEST(reserve_deposits_stop_at_the_repair_reserve_and_at_the_visit_share)
{
    Facts facts = BotAt(20);
    uint64_t money = 2400;   // 400 above the repair reserve
    VisitMoney visit = MakeVisitMoney(money, ComputeReserve(facts, Params(), money));
    CHECK(visit.depositBudget == 100);
    CHECK(!SpendDeposit(visit, money, 101));
    CHECK(SpendDeposit(visit, money, 100));
    CHECK(!SpendDeposit(visit, money - 100, 1));   // the share is used

    // The share is generous but the gold is not: after a purchase elsewhere only 50 copper sit above the repair reserve.
    visit = MakeVisitMoney(10 * GOLD, ComputeReserve(facts, Params(), 10 * GOLD));
    CHECK(DepositLeft(visit, 2050) == 50);
    CHECK(!SpendDeposit(visit, 2050, 51));
    CHECK(DepositLeft(visit, 1000) == 0);
}

TEST(reserve_deposits_count_against_the_purchase_budget)
{
    Reserve const reserve = ComputeReserve(BotAt(20), Params(), 100 * GOLD);
    VisitMoney visit = MakeVisitMoney(100 * GOLD, reserve);
    uint64_t const budget = visit.wallet.budget;
    CHECK(SpendDeposit(visit, 100 * GOLD, 10000));
    CHECK(PricePolicy::BudgetLeft(visit.wallet) == budget - 10000);
    // A deposit larger than the whole purchase budget empties it without wrapping around.
    VisitMoney tight = MakeVisitMoney(100 * GOLD, reserve);
    tight.wallet.budget = 100;
    CHECK(SpendDeposit(tight, 100 * GOLD, 500));
    CHECK(PricePolicy::BudgetLeft(tight.wallet) == 0 && !CanBuy(tight, 100 * GOLD, 1));
}

TEST(reserve_a_bot_with_no_money_spends_nothing)
{
    Reserve const reserve = ComputeReserve(BotAt(20), Params(), 0);
    VisitMoney visit = MakeVisitMoney(0, reserve);
    CHECK(visit.wallet.budget == 0 && visit.depositBudget == 0);
    CHECK(DepositLeft(visit, 0) == 0 && !SpendDeposit(visit, 0, 1) && !CanBuy(visit, 0, 1));
}

TEST(reserve_fund_replaces_nothing_of_the_repair_reserve)
{
    // The old reserve is still the floor: levels 10 and 90 keep the figures of BotSalePolicy::PurchaseReserve.
    CHECK(ComputeReserve(Equipped(10, 0), Params(), 1000 * GOLD).repair == BotSalePolicy::PurchaseReserve(10));
    CHECK(ComputeReserve(Equipped(90, 375, true), Params(), 1000 * GOLD).Total() == BotSalePolicy::PurchaseReserve(90));
}

// ---------------------------------------------------------------------------------------------------------------
// The recipe share of a crafting bot (owner answer 2026-11-22).
// ---------------------------------------------------------------------------------------------------------------
namespace
{
Facts Crafter(uint32_t level, uint32_t craftingProfessions = 1)
{
    Facts facts = Equipped(level, 75);
    facts.professionCount = craftingProfessions;
    facts.craftingProfessionCount = craftingProfessions;
    return facts;
}
}  // namespace

TEST(reserve_recipe_defaults_and_the_crafting_skill_lines)
{
    CHECK(DEFAULT_RECIPE_SHARE_PERCENT == 10 && DEFAULT_GOAL_SHARE_PERCENT == 75);
    for (uint32_t skill : { 164u, 165u, 171u, 197u, 202u, 333u, 755u, 773u })   // the eight crafts
        CHECK(IsCraftingSkill(skill));
    for (uint32_t skill : { 182u, 186u, 393u, 129u, 185u, 356u, 794u, 0u })   // herbalism, mining, skinning, first aid, cooking, fishing, archaeology
        CHECK(!IsCraftingSkill(skill));
}

TEST(reserve_a_crafter_keeps_a_recipe_share_a_non_crafter_does_not)
{
    Params const params;
    // Level 40, median gold 44 g: 10 percent of the gold above the repair reserve is held for recipes.
    uint64_t const money = 44 * GOLD;
    Reserve const crafter = ComputeReserve(Crafter(40), params, money);
    uint64_t const spare = money - crafter.repair;
    CHECK(crafter.heldForRecipes == spare / 10);
    // Journeyman Riding (50 gold) is due at 40: the goals want more than the goal share of 44 gold allows.
    CHECK(crafter.goals.Total() == 50 * GOLD && crafter.heldForGoals == PricePolicy::PercentOf(spare, 75));
    CHECK(crafter.Total() == crafter.repair + crafter.heldForGoals + crafter.heldForRecipes);

    Facts gatherer = Crafter(40);
    gatherer.craftingProfessionCount = 0;   // herbalism and mining only
    CHECK(ComputeReserve(gatherer, params, money).heldForRecipes == 0);
    CHECK(ComputeReserve(BotAt(40), params, money).heldForRecipes == 0);
}

TEST(reserve_the_recipe_share_does_not_grow_with_the_number_of_crafts)
{
    Params const params;
    CHECK(ComputeReserve(Crafter(40, 1), params, 100 * GOLD).heldForRecipes ==
          ComputeReserve(Crafter(40, 2), params, 100 * GOLD).heldForRecipes);
}

TEST(reserve_goal_and_recipe_shares_never_pass_the_spare_gold)
{
    Facts const facts = Crafter(66);   // riding goal is large: the goal share binds
    Facts bigGoal = facts;
    bigGoal.ridingSkill = 225;
    bigGoal.knowsFlightLicence = true;
    for (uint32_t goalShare : { 0u, 10u, 50u, 75u, 90u, 95u, 100u, 5000u })
        for (uint32_t recipeShare : { 0u, 10u, 25u, 50u, 100u, 5000u })
            for (uint64_t money : { uint64_t(0), uint64_t(1500), uint64_t(5000), 3 * GOLD, 300 * GOLD, 100000 * GOLD })
            {
                Params params;
                params.goalSharePercent = goalShare;
                params.recipeSharePercent = recipeShare;
                Reserve const reserve = ComputeReserve(bigGoal, params, money);
                // The whole reserve fits in the gold held (or is the repair reserve of a bot below it) and the purchase
                // budget stays at or above zero.
                CHECK(reserve.Total() <= std::max<uint64_t>(money, reserve.repair));
                CHECK(reserve.heldForGoals + reserve.heldForRecipes <= (money > reserve.repair ? money - reserve.repair : 0));
                VisitMoney const visit = MakeVisitMoney(money, reserve);
                CHECK(visit.wallet.budget <= money);
            }
}

TEST(reserve_the_recipe_share_is_cut_to_what_the_goal_share_leaves)
{
    Params params;
    CHECK(RecipeSharePercentFor(params) == 10);    // 75 + 10
    params.goalSharePercent = 95;
    CHECK(RecipeSharePercentFor(params) == 5);     // 95 + 5
    params.goalSharePercent = 100;
    CHECK(RecipeSharePercentFor(params) == 0);     // the literal rule leaves nothing for recipes
    params.goalSharePercent = 5000;                // clamped to 100 first
    CHECK(RecipeSharePercentFor(params) == 0);
    params.goalSharePercent = 0;
    params.recipeSharePercent = 5000;
    CHECK(RecipeSharePercentFor(params) == 100);

    params.goalSharePercent = 100;
    CHECK(ComputeReserve(Crafter(20), params, 1000 * GOLD).heldForRecipes == 0);
}

TEST(reserve_the_goal_share_stays_at_seventy_five_percent_with_a_recipe_share_beside_it)
{
    // Level 66 with 300 gold against a 5,000 gold riding goal: the goals take their full 75 percent, the crafter keeps 10 more.
    Facts facts = Crafter(66);
    facts.ridingSkill = 225;
    facts.knowsFlightLicence = true;
    Reserve const reserve = ComputeReserve(facts, Params(), 300 * GOLD);
    uint64_t const spare = 300 * GOLD - reserve.repair;
    CHECK(reserve.goals.riding == 5000 * GOLD);
    CHECK(reserve.heldForGoals == PricePolicy::PercentOf(spare, 75));
    CHECK(reserve.heldForRecipes == PricePolicy::PercentOf(spare, 10));
    VisitMoney const visit = MakeVisitMoney(300 * GOLD, reserve);
    CHECK(visit.wallet.budget == PricePolicy::PercentOf(300 * GOLD - reserve.Total(), PricePolicy::VISIT_SPEND_PERCENT));
    CHECK(visit.wallet.budget > 0);
}

TEST(reserve_the_recipe_part_is_capped_for_a_rich_crafter)
{
    Reserve const reserve = ComputeReserve(Crafter(90), Params(), 1000000 * GOLD);
    CHECK(reserve.heldForRecipes == MAX_RECIPE_RESERVE_COPPER && MAX_RECIPE_RESERVE_COPPER == 700 * GOLD);
}

TEST(reserve_a_bot_below_its_repair_reserve_keeps_no_recipe_gold)
{
    Reserve const reserve = ComputeReserve(Crafter(20), Params(), 1500);
    CHECK(reserve.heldForRecipes == 0 && reserve.Total() == 2000);
}

TEST(reserve_purchases_keep_the_recipe_part_but_a_recipe_purchase_may_spend_it)
{
    Reserve const reserve = ComputeReserve(Crafter(40), Params(), 100 * GOLD);
    CHECK(reserve.heldForRecipes > 0);
    VisitMoney const visit = MakeVisitMoney(100 * GOLD, reserve);
    CHECK(visit.reserve == reserve.Total());   // the auction wallet leaves the recipe gold alone
    CHECK(visit.repairReserve == reserve.repair);

    CHECK(RecipeBudget(reserve, 100 * GOLD) == reserve.heldForRecipes);
    CHECK(RecipeBudget(reserve, reserve.repair + 100) == 100);   // never below the repair reserve
    CHECK(RecipeBudget(reserve, reserve.repair) == 0);
    CHECK(RecipeBudget(reserve, 0) == 0);
    CHECK(RecipeBudget(ComputeReserve(BotAt(40), Params(), 100 * GOLD), 100 * GOLD) == 0);
}

TEST(reserve_deposits_ignore_the_recipe_part)
{
    uint64_t const money = 100 * GOLD;
    Reserve const reserve = ComputeReserve(Crafter(40), Params(), money);
    VisitMoney const visit = MakeVisitMoney(money, reserve);
    CHECK(visit.depositBudget == (money - reserve.repair) / 4);   // the repair reserve alone, as for any bot
    CHECK(DepositLeft(visit, money) == visit.depositBudget);
}
