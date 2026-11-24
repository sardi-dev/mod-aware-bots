#include "TestHarness.h"
#include "TradeChatterPolicy.h"
using namespace TradeChatterPolicy;

TEST(trade_chatter_selects_only_active_owned_priced_listings)
{
    std::vector<Listing> listings = { {"old", 10, false, true}, {"other", 10, true, false},
        {"no price", 0, true, true}, {"Ore", 12345, true, true} };
    auto ad = Choose(Style::Trader, listings, {}, 2);
    CHECK(ad.kind == Kind::Sale);
    CHECK(ad.goods == "Ore");
    CHECK(ad.price == 12345);
    CHECK(Choose(Style::Trader, {}, {"Potion"}, 2).kind == Kind::None);
}
TEST(trade_chatter_recipes_and_personality_text)
{
    auto ad = Choose(Style::GathererCrafter, {}, {"", "Potion", "Bread", "Cloth"}, 2);
    CHECK(ad.kind == Kind::Recipes);
    CHECK(ad.goods == "Potion, Bread");
    CHECK(Text(ad, 0) == "I can craft Potion, Bread.");
    CHECK(Text(ad, 1) == "Happy to craft Potion, Bread.");
    CHECK(Choose(Style::GathererCrafter, {}, {"Potion"}, 0).kind == Kind::None);
    CHECK(Choose(Style::Other, {}, {"Potion"}, 2).kind == Kind::None);
    CHECK(Text({Kind::Sale, "Ore", 12345}, 0) == "Listed Ore for 1g 23s 45c.");
}
TEST(trade_chatter_rolling_caps_and_boundaries)
{
    std::deque<uint64_t> bot, channel, faction;
    CHECK(Check(bot, channel, faction, 100, 1, 1, 2) == Limit::Allowed);
    Commit(bot, channel, faction, 100);
    CHECK(Check(bot, channel, faction, 159, 1, 1, 2) == Limit::Bot);
    std::deque<uint64_t> another;
    CHECK(Check(another, channel, faction, 159, 1, 1, 2) == Limit::Channel);
    CHECK(Check(another, channel, faction, 160, 1, 1, 2) == Limit::Allowed);
    Commit(another, channel, faction, 160);
    std::deque<uint64_t> third;
    CHECK(Check(third, channel, faction, 220, 1, 1, 2) == Limit::Faction);
    CHECK(Check(bot, channel, faction, 3700, 1, 1, 2) == Limit::Allowed);
    CHECK(Check(third, channel, faction, 3700, 0, 1, 2) == Limit::Bot);
}
