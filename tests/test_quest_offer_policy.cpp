#include "TestHarness.h"
#include "QuestOfferPolicy.h"

using QuestOfferPolicy::Candidate;
using QuestOfferPolicy::Decide;
using QuestOfferPolicy::Facts;
using QuestOfferPolicy::Tuning;
using QuestOfferPolicy::Verdict;

namespace
{
Facts Good()
{
    Facts facts;
    facts.yards = 30.0f;
    facts.botLevel = 20;
    facts.humanLevel = 22;
    facts.sharedQuest = true;
    return facts;
}
}  // namespace

TEST(quest_offer_a_free_bot_with_a_shared_quest_in_range_offers)
{
    Tuning const tuning;
    CHECK(Decide(tuning, Good()) == Verdict::Offer);
}
TEST(quest_offer_needs_a_shared_quest)
{
    Tuning const tuning;
    Facts facts = Good();
    facts.sharedQuest = false;
    CHECK(Decide(tuning, facts) == Verdict::NoSharedQuest);
}
TEST(quest_offer_distance_zone_and_map)
{
    Tuning const tuning;
    Facts facts = Good();
    facts.yards = 60.0f;
    CHECK(Decide(tuning, facts) == Verdict::Offer);
    facts.yards = 60.5f;
    CHECK(Decide(tuning, facts) == Verdict::TooFar);
    facts = Good();
    facts.sameZone = false;
    CHECK(Decide(tuning, facts) == Verdict::OtherZone);
    facts = Good();
    facts.sameMap = false;
    CHECK(Decide(tuning, facts) == Verdict::OtherZone);
}
TEST(quest_offer_level_gap_has_a_tolerance_both_ways)
{
    Tuning const tuning;
    Facts facts = Good();
    facts.botLevel = 20;
    facts.humanLevel = 25;
    CHECK(Decide(tuning, facts) == Verdict::Offer);
    facts.humanLevel = 26;
    CHECK(Decide(tuning, facts) == Verdict::LevelGap);
    facts.botLevel = 31;
    facts.humanLevel = 26;
    CHECK(Decide(tuning, facts) == Verdict::Offer);
    facts.botLevel = 32;
    CHECK(Decide(tuning, facts) == Verdict::LevelGap);
}
TEST(quest_offer_a_human_in_a_group_or_busy_is_left_alone)
{
    Tuning const tuning;
    Facts facts = Good();
    facts.humanGrouped = true;
    CHECK(Decide(tuning, facts) == Verdict::HumanBusy);
    facts = Good();
    facts.humanBusy = true;
    CHECK(Decide(tuning, facts) == Verdict::HumanBusy);
}
TEST(quest_offer_a_bot_busy_or_grouped_with_others_does_not_offer)
{
    Tuning const tuning;
    Facts facts = Good();
    facts.botBusy = true;
    CHECK(Decide(tuning, facts) == Verdict::BotBusy);
    facts = Good();
    facts.botGrouped = true;
    CHECK(Decide(tuning, facts) == Verdict::BotBusy);
}
TEST(quest_offer_tuning_changes_the_radius_and_the_level_gap)
{
    Tuning wide;
    wide.radiusYards = 100.0f;
    wide.levelTolerance = 10;
    Facts facts = Good();
    facts.yards = 90.0f;
    facts.botLevel = 10;
    facts.humanLevel = 20;
    CHECK(Decide(wide, facts) == Verdict::Offer);
    CHECK(Decide(Tuning(), facts) == Verdict::TooFar);
}
TEST(quest_offer_first_shared_quest_is_the_lowest_id_in_both_logs)
{
    CHECK(QuestOfferPolicy::FirstSharedQuest({ 5, 9, 12 }, { 12, 9, 40 }) == 9);
    CHECK(QuestOfferPolicy::FirstSharedQuest({ 5, 9 }, { 6, 10 }) == 0);
    CHECK(QuestOfferPolicy::FirstSharedQuest({}, { 6 }) == 0);
    CHECK(QuestOfferPolicy::FirstSharedQuest({ 7 }, {}) == 0);
    // Empty log slots (id 0) never count as a shared quest.
    CHECK(QuestOfferPolicy::FirstSharedQuest({ 0, 0, 3 }, { 0, 3 }) == 3);
    CHECK(QuestOfferPolicy::FirstSharedQuest({ 0, 0 }, { 0 }) == 0);
}
TEST(quest_offer_candidates_are_tried_nearest_first)
{
    std::vector<Candidate> candidates = { { 30, 12.0f }, { 10, 40.0f }, { 20, 12.0f }, { 5, 3.0f } };
    QuestOfferPolicy::OrderNearestFirst(candidates);
    CHECK(candidates[0].bot == 5);
    // The tie at 12 yards goes to the lower guid, always.
    CHECK(candidates[1].bot == 20);
    CHECK(candidates[2].bot == 30);
    CHECK(candidates[3].bot == 10);
}
TEST(quest_offer_a_bot_outside_the_only_bots_list_does_not_offer)
{
    Facts facts = Good();
    facts.inScope = false;
    CHECK(Decide(Tuning(), facts) == Verdict::OutOfScope);
    // The scope is the last test: a bot that would not have offered anyway is not counted as held back by it.
    facts.sharedQuest = false;
    CHECK(Decide(Tuning(), facts) == Verdict::NoSharedQuest);
    facts = Good();
    facts.botBusy = true;
    facts.inScope = false;
    CHECK(Decide(Tuning(), facts) == Verdict::BotBusy);
    CHECK(Decide(Tuning(), Good()) == Verdict::Offer);
}
