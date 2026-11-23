#include "TestHarness.h"
#include "PlaystylePolicy.h"

using PlaystylePolicy::Activity;
using PlaystylePolicy::Shares;
using PlaystylePolicy::Style;

namespace
{
constexpr uint32_t BOTS = 20000;
// The first guid of a run of random bots, as the realm's characters table numbers them.
constexpr uint32_t FIRST_BOT_GUID = 1000;
}  // namespace

TEST(playstyle_a_bot_keeps_its_style_for_the_same_shares)
{
    Shares const shares;
    for (uint32_t guid = FIRST_BOT_GUID; guid < FIRST_BOT_GUID + 200; ++guid)
        CHECK(PlaystylePolicy::StyleFor(guid, shares) == PlaystylePolicy::StyleFor(guid, shares));
}
TEST(playstyle_styles_follow_the_shares_over_many_bots)
{
    Shares const shares;
    uint32_t seen[PlaystylePolicy::STYLE_COUNT] = {};
    for (uint32_t guid = FIRST_BOT_GUID; guid < FIRST_BOT_GUID + BOTS; ++guid)
        ++seen[uint32_t(PlaystylePolicy::StyleFor(guid, shares))];
    for (uint32_t i = 0; i < PlaystylePolicy::STYLE_COUNT; ++i)
    {
        double const share = double(shares.percent[i]) / 100.0;
        CHECK_NEAR(double(seen[i]) / double(BOTS), share, 0.02);
    }
}
TEST(playstyle_neighbouring_guids_do_not_share_one_style)
{
    Shares const shares;
    uint32_t changes = 0;
    for (uint32_t guid = FIRST_BOT_GUID; guid < FIRST_BOT_GUID + 1000; ++guid)
        if (PlaystylePolicy::StyleFor(guid, shares) != PlaystylePolicy::StyleFor(guid + 1, shares))
            ++changes;
    // With these shares two neighbours differ about 70 percent of the time; a modulo of a run would give far fewer.
    CHECK(changes > 500);
}
TEST(playstyle_shares_are_proportions_not_percent_of_one_hundred)
{
    Shares sixty;
    sixty.percent[0] = 120;
    sixty.percent[1] = 30;
    sixty.percent[2] = sixty.percent[3] = sixty.percent[4] = 0;
    uint32_t seen[PlaystylePolicy::STYLE_COUNT] = {};
    for (uint32_t guid = FIRST_BOT_GUID; guid < FIRST_BOT_GUID + BOTS; ++guid)
        ++seen[uint32_t(PlaystylePolicy::StyleFor(guid, sixty))];
    CHECK_NEAR(double(seen[0]) / double(BOTS), 0.8, 0.02);
    CHECK_NEAR(double(seen[1]) / double(BOTS), 0.2, 0.02);
    CHECK(seen[2] == 0 && seen[3] == 0 && seen[4] == 0);
}
TEST(playstyle_all_zero_shares_make_every_bot_a_quester)
{
    Shares none;
    for (uint32_t& share : none.percent)
        share = 0;
    for (uint32_t guid = FIRST_BOT_GUID; guid < FIRST_BOT_GUID + 50; ++guid)
        CHECK(PlaystylePolicy::StyleFor(guid, none) == Style::Quester);
}
TEST(playstyle_a_single_share_gives_that_style_to_all)
{
    Shares only;
    for (uint32_t& share : only.percent)
        share = 0;
    only.percent[uint32_t(Style::PvPer)] = 1;
    for (uint32_t guid = FIRST_BOT_GUID; guid < FIRST_BOT_GUID + 50; ++guid)
        CHECK(PlaystylePolicy::StyleFor(guid, only) == Style::PvPer);
}
TEST(playstyle_each_style_leans_toward_its_own_activities)
{
    auto const pct = [](Style style, Activity activity) { return PlaystylePolicy::Percent(style, activity, 100); };
    CHECK(pct(Style::Quester, Activity::DoQuest) > 100);
    CHECK(pct(Style::GathererCrafter, Activity::DoGather) > pct(Style::Quester, Activity::DoGather));
    CHECK(pct(Style::GathererCrafter, Activity::DoGather) == 300);
    CHECK(pct(Style::Trader, Activity::WanderNpc) == 200);
    CHECK(pct(Style::Socializer, Activity::DuelSpot) > 100);
    CHECK(pct(Style::Socializer, Activity::Rest) == 200);
    CHECK(pct(Style::PvPer, Activity::OutdoorPvp) == 300);
    CHECK(pct(Style::PvPer, Activity::DoQuest) < 100);
}
TEST(playstyle_every_style_still_quests)
{
    // No style may starve questing: the leveling path of the random bots (design decision 0017) runs on it.
    for (uint32_t i = 0; i < PlaystylePolicy::STYLE_COUNT; ++i)
        CHECK(PlaystylePolicy::Percent(Style(i), Activity::DoQuest, 100) >= 60);
}
TEST(playstyle_strength_scales_the_lean_around_one_hundred)
{
    CHECK(PlaystylePolicy::Percent(Style::PvPer, Activity::OutdoorPvp, 0) == 100);
    CHECK(PlaystylePolicy::Percent(Style::PvPer, Activity::OutdoorPvp, 50) == 200);
    CHECK(PlaystylePolicy::Percent(Style::PvPer, Activity::OutdoorPvp, 100) == 300);
    CHECK(PlaystylePolicy::Percent(Style::PvPer, Activity::OutdoorPvp, 200) == 500);
    CHECK(PlaystylePolicy::Percent(Style::PvPer, Activity::DoQuest, 50) == 80);
}
TEST(playstyle_a_lean_never_goes_below_the_floor)
{
    CHECK(PlaystylePolicy::Percent(Style::PvPer, Activity::DoGather, 1000) == PlaystylePolicy::MIN_PERCENT);
}
TEST(playstyle_weights_scale_and_a_switched_off_weight_stays_off)
{
    CHECK(PlaystylePolicy::ScaledWeight(60, Style::Quester, Activity::DoQuest, 100) == 90);
    CHECK(PlaystylePolicy::ScaledWeight(15, Style::GathererCrafter, Activity::DoGather, 100) == 45);
    CHECK(PlaystylePolicy::ScaledWeight(10, Style::PvPer, Activity::OutdoorPvp, 100) == 30);
    CHECK(PlaystylePolicy::ScaledWeight(0, Style::PvPer, Activity::DuelSpot, 100) == 0);
    CHECK(PlaystylePolicy::ScaledWeight(1, Style::PvPer, Activity::DoGather, 100) == 1);
    CHECK(PlaystylePolicy::ScaledWeight(60, Style::PvPer, Activity::DoQuest, 0) == 60);
}
TEST(playstyle_names_are_stable)
{
    CHECK(std::string(PlaystylePolicy::StyleName(Style::GathererCrafter)) == "gatherer-crafter");
    CHECK(std::string(PlaystylePolicy::StyleName(Style::PvPer)) == "pvper");
}

TEST(playstyle_every_weighted_status_maps_to_its_own_activity)
{
    // NewRpgBaseAction.cpp static_asserts the enum values these numbers stand for.
    Activity const expected[] = { Activity::GoGrind, Activity::GoCamp, Activity::WanderRandom, Activity::WanderNpc,
        Activity::DoQuest, Activity::TravelFlight, Activity::Rest, Activity::OutdoorPvp, Activity::DuelSpot,
        Activity::DoGather };
    uint32_t seen = 0;
    for (int status = 1; status <= 10; ++status)
    {
        Activity activity = Activity::Count;
        CHECK(PlaystylePolicy::ActivityOfStatus(status, activity));
        CHECK(activity == expected[status - 1]);
        seen |= 1u << uint32_t(activity);
    }
    // Ten statuses, ten different activities.
    CHECK(seen == (1u << PlaystylePolicy::ACTIVITY_COUNT) - 1);
}
TEST(playstyle_idle_and_unknown_statuses_have_no_activity)
{
    Activity activity = Activity::DoQuest;
    CHECK(!PlaystylePolicy::ActivityOfStatus(0, activity));
    CHECK(!PlaystylePolicy::ActivityOfStatus(11, activity));
    CHECK(!PlaystylePolicy::ActivityOfStatus(-1, activity));
    CHECK(activity == Activity::DoQuest);
}
