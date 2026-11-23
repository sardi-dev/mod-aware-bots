#include "TestHarness.h"
#include "QuestLevelingPolicy.h"

using namespace QuestLevelingPolicy;

TEST(leveling_quest_tier_hand_in_then_at_level_then_low_level)
{
    CHECK(QuestTier(true, false) == Tier::HandIn);
    CHECK(QuestTier(true, true) == Tier::HandIn);
    CHECK(QuestTier(false, false) == Tier::AtLevel);
    CHECK(QuestTier(false, true) == Tier::LowLevel);
}

TEST(leveling_best_tier_keeps_every_entry_of_the_best_tier)
{
    std::vector<std::size_t> best = BestTier({Tier::LowLevel, Tier::AtLevel, Tier::LowLevel, Tier::AtLevel});
    CHECK(best.size() == 2 && best[0] == 1 && best[1] == 3);
    best = BestTier({Tier::AtLevel, Tier::HandIn});
    CHECK(best.size() == 1 && best[0] == 1);
    best = BestTier({Tier::LowLevel, Tier::LowLevel});
    CHECK(best.size() == 2);
    CHECK(BestTier({}).empty());
}

TEST(leveling_no_grind_or_flight_while_the_zone_has_quest_work)
{
    CHECK(!KeepInDraw(Draw::Grind, true));
    CHECK(!KeepInDraw(Draw::Flight, true));
    CHECK(KeepInDraw(Draw::Quest, true));
    CHECK(KeepInDraw(Draw::Other, true));
    CHECK(KeepInDraw(Draw::Grind, false));
    CHECK(KeepInDraw(Draw::Flight, false));
}

TEST(leveling_next_zone_pass_looks_farther_than_the_zone_pass)
{
    // The zone pass checks the 24 nearest givers within the configured radius (NewRpgBaseAction.cpp, quest hub search).
    CHECK(NEXT_ZONE_RADIUS_FACTOR > 1.0f);
    CHECK(NEXT_ZONE_GIVERS_CHECKED > 24u);
}

TEST(leveling_nearest_of_tier_keeps_the_quests_at_hand)
{
    std::vector<float> const dists = {900.0f, 120.0f, 180.0f, 60.0f, 400.0f};
    std::vector<std::size_t> const near = NearestOfTier({0, 1, 2, 4}, dists);
    CHECK(near.size() == 2 && near[0] == 1 && near[1] == 2);
    std::vector<std::size_t> const one = NearestOfTier({0, 4}, dists);
    CHECK(one.size() == 1 && one[0] == 4);
    CHECK(NearestOfTier({}, dists).empty());
}

TEST(leveling_looks_for_a_better_quest_before_a_grey_one)
{
    CHECK(LookForBetterFirst(Tier::LowLevel));
    CHECK(!LookForBetterFirst(Tier::AtLevel));
    CHECK(!LookForBetterFirst(Tier::HandIn));
}
