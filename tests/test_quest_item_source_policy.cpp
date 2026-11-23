#include "TestHarness.h"
#include "QuestItemSourcePolicy.h"

using namespace QuestItemSourcePolicy;

namespace
{
Spot At(Kind kind, uint32_t spawnId, uint32_t map, float x, float y, float chance)
{
    Spot s;
    s.kind = kind;
    s.entry = 1125;
    s.spawnId = spawnId;
    s.map = map;
    s.x = x;
    s.y = y;
    s.chance = chance;
    return s;
}

BotFacts Bot(uint32_t level, uint64_t money)
{
    BotFacts bot;
    bot.level = level;
    bot.money = money;
    return bot;
}
}  // namespace

TEST(item_source_noise_rows_are_no_source)
{
    SourceFacts boar;
    boar.chance = 41.0f;
    boar.minLevel = 7;
    CHECK(Usable(boar, Bot(7, 0), 4));
    // Defias Squallshaper drops the boar rib at 0.0018 %.
    SourceFacts squallshaper = boar;
    squallshaper.chance = 0.0018f;
    CHECK(!Usable(squallshaper, Bot(20, 0), 4));
}

TEST(item_source_creature_needs_an_attackable_mob_in_level_reach)
{
    SourceFacts mob;
    mob.minLevel = 14;
    CHECK(Usable(mob, Bot(10, 0), 1));
    CHECK(!Usable(mob, Bot(9, 0), 1));
    mob.friendly = true;
    CHECK(!Usable(mob, Bot(60, 0), 1));
    // Deathclasp is an elite.
    SourceFacts deathclasp;
    deathclasp.minLevel = 54;
    deathclasp.elite = true;
    BotFacts bot = Bot(58, 0);
    CHECK(!Usable(deathclasp, bot, 1));
    bot.canFightElite = true;
    CHECK(Usable(deathclasp, bot, 1));
}

TEST(item_source_object_needs_the_lock_skill)
{
    SourceFacts chest;
    chest.kind = Kind::GameObject;
    CHECK(Usable(chest, Bot(5, 0), 6));
    chest.skillMissing = true;
    CHECK(!Usable(chest, Bot(5, 0), 6));
}

TEST(item_source_vendor_needs_money_above_the_reserve)
{
    // Rhapsody Malt: 50 copper, one per purchase.
    SourceFacts malt;
    malt.kind = Kind::Vendor;
    malt.lotPrice = 50;
    CHECK(Usable(malt, Bot(7, 50), 1));
    CHECK(!Usable(malt, Bot(7, 49), 1));
    BotFacts reserved = Bot(30, 30050);
    reserved.reserve = 30000;
    CHECK(Usable(malt, reserved, 1));
    reserved.money = 30049;
    CHECK(!Usable(malt, reserved, 1));
    SourceFacts hostile = malt;
    hostile.hostile = true;
    CHECK(!Usable(hostile, Bot(7, 1000), 1));
    SourceFacts token = malt;
    token.otherCost = true;
    CHECK(!Usable(token, Bot(7, 1000), 1));
}

TEST(item_source_lots_cover_the_missing_count_within_the_money)
{
    CHECK(LotsToBuy(0, 1, 1000, 0, 50) == 0);
    CHECK(LotsToBuy(1, 1, 1000, 0, 50) == 1);
    CHECK(LotsToBuy(6, 5, 1000, 0, 50) == 2);
    CHECK(LotsToBuy(6, 0, 1000, 0, 50) == 6);
    CHECK(LotsToBuy(6, 1, 120, 0, 50) == 2);
    CHECK(LotsToBuy(6, 1, 100, 200, 50) == 0);
    CHECK(LotsToBuy(3, 1, 0, 0, 0) == 3);
}

TEST(item_source_pick_weighs_distance_and_chance)
{
    std::vector<Spot> spots = {
        At(Kind::Creature, 1, 0, 200.0f, 0.0f, 25.0f),  // 200 + 300
        At(Kind::Vendor, 2, 0, 450.0f, 0.0f, 100.0f),   // 450
        At(Kind::Creature, 3, 1, 10.0f, 0.0f, 100.0f),  // other map
        At(Kind::GameObject, 4, 0, 3200.0f, 0.0f, 100.0f)};  // too far
    float score = 0.0f;
    CHECK(PickSpot(spots, 0, 0.0f, 0.0f, {}, &score) == 1);
    CHECK(score == 450.0f);
    CHECK(PickSpot(spots, 0, 0.0f, 0.0f, {2}) == 0);
    CHECK(PickSpot(spots, 0, 0.0f, 0.0f, {1, 2}) == -1);
    CHECK(PickSpot(spots, 1, 0.0f, 0.0f, {}) == 2);
    CHECK(PickSpot({}, 0, 0.0f, 0.0f, {}) == -1);
    // Equal scores go to the lower spawn id.
    std::vector<Spot> tie = {At(Kind::Creature, 9, 0, 100.0f, 0.0f, 50.0f), At(Kind::Creature, 5, 0, -100.0f, 0.0f, 50.0f)};
    CHECK(PickSpot(tie, 0, 0.0f, 0.0f, {}) == 1);
}

TEST(item_source_candidate_is_the_best_score_then_the_lower_item)
{
    std::vector<Candidate> candidates(3);
    candidates[0].itemId = 60496;
    candidates[0].score = 300.0f;
    candidates[1].itemId = 2894;
    candidates[1].score = 450.0f;
    candidates[2].itemId = 52558;
    candidates[2].score = 300.0f;
    CHECK(PickCandidate(candidates) == 2);
    CHECK(PickCandidate({}) == -1);
}

TEST(item_source_craft_casts_fetches_or_skips)
{
    std::size_t idx = 99;
    CHECK(PlanCraft({}, idx) == CraftStep::Skip);
    std::vector<Reagent> reagents = {{2672, 1, 1, false}, {2678, 1, 0, true}, {159, 2, 0, true}};
    CHECK(PlanCraft(reagents, idx) == CraftStep::FetchReagent);
    CHECK(idx == 1);
    reagents[1].have = 1;
    reagents[2].have = 2;
    CHECK(PlanCraft(reagents, idx) == CraftStep::Cast);
    // A missing reagent without a source drops the recipe, even when another one could be fetched.
    reagents[1].have = 0;
    reagents[2].have = 0;
    reagents[2].obtainable = false;
    CHECK(PlanCraft(reagents, idx) == CraftStep::Skip);
    CHECK(CastsFor(4, 1) == 4);
    CHECK(CastsFor(5, 2) == 3);
    CHECK(CastsFor(5, 0) == 5);
}

TEST(item_source_judge_travels_works_and_moves_on)
{
    Spot spot = At(Kind::Creature, 7, 0, 0.0f, 0.0f, 41.0f);
    Fetch fetch = Start(384, 60496, 0, 4, 0, spot, 300.0f, 1000);
    CHECK(fetch.active && fetch.budgetMs == BudgetMs(300.0f));
    CHECK(Judge(fetch, 2000, 250.0f, 0) == Step::Travel);
    CHECK(fetch.spotMs == 0);
    CHECK(Judge(fetch, 3000, 8.0f, 0) == Step::Work);
    CHECK(fetch.spotMs == 3000);
    // Chasing a mob away from the spot keeps the spot clock.
    CHECK(Judge(fetch, 3000 + SPOT_WAIT_MS - 1, 60.0f, 0) == Step::Work);
    // A new item restarts it.
    CHECK(Judge(fetch, 3000 + SPOT_WAIT_MS - 1, 60.0f, 1) == Step::Work);
    CHECK(fetch.spotMs == 3000 + SPOT_WAIT_MS - 1);
    CHECK(Judge(fetch, 3000 + 2 * SPOT_WAIT_MS - 1, 5.0f, 1) == Step::NextSpot);
    CHECK(Judge(fetch, 3000 + 2 * SPOT_WAIT_MS - 1, 5.0f, 4) == Step::Done);
}

TEST(item_source_leave_spot_gives_up_after_the_limit_and_resets_on_an_item)
{
    Fetch fetch = Start(25176, 52558, 0, 6, 0, At(Kind::GameObject, 1, 1, 0.0f, 0.0f, 100.0f), 0.0f, 0);
    for (uint32_t i = 1; i < MAX_SPOTS_WITHOUT_ITEM; ++i)
    {
        CHECK(!LeaveSpot(fetch));
        GoToSpot(fetch, At(Kind::GameObject, 1 + i, 1, 0.0f, 0.0f, 100.0f));
    }
    CHECK(fetch.visited.size() == MAX_SPOTS_WITHOUT_ITEM - 1);
    CHECK(Judge(fetch, 10, 50.0f, 1) == Step::Travel);
    CHECK(fetch.spotsWithoutItem == 0);
    CHECK(!LeaveSpot(fetch));
    Fetch stuck = Start(25176, 52558, 0, 6, 0, At(Kind::GameObject, 1, 1, 0.0f, 0.0f, 100.0f), 0.0f, 0);
    bool gaveUp = false;
    for (uint32_t i = 0; i < MAX_SPOTS_WITHOUT_ITEM; ++i)
        gaveUp = LeaveSpot(stuck);
    CHECK(gaveUp);
}

TEST(item_source_judge_expires_and_survives_the_clock_wrap)
{
    Spot spot = At(Kind::Vendor, 3, 0, 0.0f, 0.0f, 100.0f);
    Fetch fetch = Start(384, 2894, 0, 1, 0, spot, 0.0f, 0xfffff000u);
    CHECK(!Expired(fetch, 0xfffff000u + 1000u));
    CHECK(Judge(fetch, 100, 500.0f, 0) == Step::Travel);
    CHECK(Judge(fetch, 0xfffff000u + MIN_BUDGET_MS, 500.0f, 0) == Step::Expired);
    // The item in the bags ends the fetch even when the time is up.
    CHECK(Judge(fetch, 0xfffff000u + MIN_BUDGET_MS, 500.0f, 1) == Step::Done);
    Fetch at0 = Start(384, 2894, 0, 1, 0, spot, 0.0f, 0xffffff00u);
    CHECK(Judge(at0, 0, 1.0f, 0) == Step::Work);
    CHECK(at0.spotMs == 1);
    CHECK(BudgetMs(-1.0f) == MIN_BUDGET_MS);
    CHECK(BudgetMs(1000000.0f) == MAX_BUDGET_MS);
}

TEST(item_source_entries_and_dest_belong_to_the_active_fetch)
{
    Fetch fetch = Start(25232, 52564, 0, 6, 0, At(Kind::Creature, 4, 1, 100.0f, 50.0f, 86.0f), 900.0f, 0);
    fetch.creatureEntries = {3195, 3196, 3199};
    CHECK(IsSourceEntry(fetch, 3196));
    CHECK(!IsSourceEntry(fetch, 3197));
    CHECK(SameDest(fetch, 1, 100.4f, 50.4f));
    CHECK(!SameDest(fetch, 0, 100.0f, 50.0f));
    CHECK(!SameDest(fetch, 1, 103.0f, 50.0f));
    Fetch none;
    none.creatureEntries = {3195};
    CHECK(!IsSourceEntry(none, 3195));
    CHECK(!SameDest(none, 0, 0.0f, 0.0f));
}
