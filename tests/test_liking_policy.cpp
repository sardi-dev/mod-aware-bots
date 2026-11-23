#include "TestHarness.h"
#include "LikingPolicy.h"

using LikingPolicy::Apply;
using LikingPolicy::Band;
using LikingPolicy::Event;
using LikingPolicy::Pair;
using LikingPolicy::Tuning;

namespace
{
constexpr uint32_t DAY = LikingPolicy::SECONDS_PER_DAY;
// A fixed morning, so a "same day" test cannot cross midnight: day number 20000 plus nine hours.
constexpr uint32_t NOW = 20000 * DAY + 9 * 3600;
}  // namespace

TEST(liking_a_new_pair_starts_at_neutral)
{
    Tuning const tuning;
    Pair const pair = LikingPolicy::Fresh(tuning, NOW);
    CHECK_NEAR(pair.value, 0.30, 1e-9);
    CHECK(pair.updatedAt == NOW);
    CHECK(LikingPolicy::BandOf(tuning, pair.value) == Band::Stranger);
}
TEST(liking_events_move_the_score_by_their_delta)
{
    Tuning const tuning;
    Pair const start = LikingPolicy::Fresh(tuning, NOW);
    CHECK_NEAR(Apply(tuning, start, Event::DungeonDone, 1, NOW).value, 0.36, 1e-9);
    CHECK_NEAR(Apply(tuning, start, Event::Traded, 1, NOW).value, 0.32, 1e-9);
    CHECK_NEAR(Apply(tuning, start, Event::Dueled, 1, NOW).value, 0.32, 1e-9);
    CHECK_NEAR(Apply(tuning, start, Event::Thanked, 1, NOW).value, 0.32, 1e-9);
    CHECK_NEAR(Apply(tuning, start, Event::GroupTime, 3, NOW).value, 0.33, 1e-9);
    CHECK_NEAR(Apply(tuning, start, Event::GuildTime, 2, NOW).value, 0.32, 1e-9);
    CHECK_NEAR(Apply(tuning, start, Event::Insulted, 1, NOW).value, 0.20, 1e-9);
    CHECK_NEAR(Apply(tuning, start, Event::Kicked, 1, NOW).value, 0.22, 1e-9);
    CHECK_NEAR(Apply(tuning, start, Event::LootNinja, 1, NOW).value, 0.15, 1e-9);
}
TEST(liking_grouped_minutes_count_in_whole_steps)
{
    Tuning const tuning;
    CHECK(LikingPolicy::GroupSteps(tuning, 9) == 0);
    CHECK(LikingPolicy::GroupSteps(tuning, 10) == 1);
    CHECK(LikingPolicy::GroupSteps(tuning, 59) == 5);
    Tuning off;
    off.groupStepMinutes = 0;
    CHECK(LikingPolicy::GroupSteps(off, 120) == 0);
}
TEST(liking_the_score_stays_between_zero_and_one)
{
    Tuning tuning;
    tuning.dailyGainCap = 10.0;
    Pair pair = LikingPolicy::Fresh(tuning, NOW);
    pair = Apply(tuning, pair, Event::DungeonDone, 100, NOW);
    CHECK_NEAR(pair.value, 1.0, 1e-9);
    pair = Apply(tuning, pair, Event::Insulted, 100, NOW);
    CHECK_NEAR(pair.value, 0.0, 1e-9);
}
TEST(liking_good_moves_stop_at_the_daily_cap_but_rude_ones_do_not)
{
    Tuning const tuning;
    Pair pair = LikingPolicy::Fresh(tuning, NOW);
    for (int i = 0; i < 10; ++i)
        pair = Apply(tuning, pair, Event::DungeonDone, 1, NOW);
    CHECK_NEAR(pair.value, 0.30 + tuning.dailyGainCap, 1e-9);
    CHECK_NEAR(pair.gainToday, tuning.dailyGainCap, 1e-9);

    pair = Apply(tuning, pair, Event::Insulted, 1, NOW);
    CHECK_NEAR(pair.value, 0.30 + tuning.dailyGainCap - 0.10, 1e-9);
    // The insult gave no room back: the cap is about what was gained, not about the score.
    Pair const again = Apply(tuning, pair, Event::Traded, 1, NOW);
    CHECK_NEAR(again.value, pair.value, 1e-9);
}
TEST(liking_the_cap_opens_again_the_next_day)
{
    Tuning const tuning;
    Pair pair = LikingPolicy::Fresh(tuning, NOW);
    for (int i = 0; i < 10; ++i)
        pair = Apply(tuning, pair, Event::DungeonDone, 1, NOW);
    double const capped = pair.value;
    // The next morning: a day of decay first, then a new day's room.
    pair = Apply(tuning, pair, Event::Traded, 1, NOW + DAY);
    CHECK(pair.gainDay == (NOW + DAY) / DAY);
    CHECK_NEAR(pair.gainToday, tuning.traded, 1e-9);
    CHECK(pair.value > LikingPolicy::Decayed(tuning, capped, DAY));
}
TEST(liking_a_partial_step_is_cut_to_the_room_left)
{
    Tuning tuning;
    tuning.dailyGainCap = 0.07;
    Pair pair = LikingPolicy::Fresh(tuning, NOW);
    pair = Apply(tuning, pair, Event::DungeonDone, 1, NOW);
    pair = Apply(tuning, pair, Event::DungeonDone, 1, NOW);
    CHECK_NEAR(pair.value, 0.37, 1e-9);
}
TEST(liking_the_score_fades_toward_neutral_from_both_sides)
{
    Tuning const tuning;
    uint32_t const halfLife = uint32_t(tuning.decayHalfLifeDays) * DAY;
    CHECK_NEAR(LikingPolicy::Decayed(tuning, 0.90, halfLife), 0.60, 1e-9);
    CHECK_NEAR(LikingPolicy::Decayed(tuning, 0.00, halfLife), 0.15, 1e-9);
    CHECK_NEAR(LikingPolicy::Decayed(tuning, 0.90, 0), 0.90, 1e-9);
    CHECK_NEAR(LikingPolicy::Decayed(tuning, 0.90, 40 * halfLife), 0.30, 1e-6);
    Tuning never;
    never.decayHalfLifeDays = 0.0;
    CHECK_NEAR(LikingPolicy::Decayed(never, 0.90, 5), 0.30, 1e-9);
}
TEST(liking_reading_a_pair_does_not_change_it)
{
    Tuning const tuning;
    Pair pair;
    pair.value = 0.90;
    pair.updatedAt = NOW;
    double const later = LikingPolicy::ValueAt(tuning, pair, NOW + 21 * DAY);
    CHECK_NEAR(later, 0.60, 1e-9);
    CHECK_NEAR(pair.value, 0.90, 1e-9);
    // A clock set back counts as no time passed.
    CHECK_NEAR(LikingPolicy::ValueAt(tuning, pair, NOW - 5), 0.90, 1e-9);
}
TEST(liking_apply_decays_before_it_adds)
{
    Tuning const tuning;
    Pair pair;
    pair.value = 0.90;
    pair.updatedAt = NOW;
    Pair const next = Apply(tuning, pair, Event::Traded, 1, NOW + 21 * DAY);
    CHECK_NEAR(next.value, 0.62, 1e-9);
    CHECK(next.updatedAt == NOW + 21 * DAY);
}
TEST(liking_bands_follow_the_thresholds)
{
    Tuning const tuning;
    CHECK(LikingPolicy::BandOf(tuning, 0.0) == Band::Cold);
    CHECK(LikingPolicy::BandOf(tuning, 0.199) == Band::Cold);
    CHECK(LikingPolicy::BandOf(tuning, 0.20) == Band::Stranger);
    CHECK(LikingPolicy::BandOf(tuning, 0.549) == Band::Stranger);
    CHECK(LikingPolicy::BandOf(tuning, 0.55) == Band::Friendly);
    CHECK(LikingPolicy::BandOf(tuning, 0.75) == Band::Close);
    CHECK(std::string(LikingPolicy::BandName(Band::Close)) == "close");
    CHECK(std::string(LikingPolicy::BandName(Band::Cold)) == "cold");
    CHECK(LikingPolicy::Percent(0.554) == 55);
    CHECK(LikingPolicy::Percent(1.7) == 100);
    CHECK(LikingPolicy::Percent(-1.0) == 0);
}
TEST(liking_invite_chance_follows_the_score_around_neutral)
{
    Tuning const tuning;
    CHECK(LikingPolicy::InviteChanceDelta(tuning, 0.30) == 0);
    CHECK(LikingPolicy::InviteChanceDelta(tuning, 0.55) == 15);
    CHECK(LikingPolicy::InviteChanceDelta(tuning, 1.0) == 42);
    CHECK(LikingPolicy::InviteChanceDelta(tuning, 0.0) == -18);
}
TEST(liking_offers_wait_for_a_friend)
{
    Tuning const tuning;
    CHECK(!LikingPolicy::QuestOfferAllowed(tuning, 0.39));
    CHECK(LikingPolicy::QuestOfferAllowed(tuning, 0.40));
    CHECK(!LikingPolicy::GuildOfferAllowed(tuning, 0.54));
    CHECK(LikingPolicy::GuildOfferAllowed(tuning, 0.55));
}
TEST(liking_offers_per_human_are_capped_per_hour)
{
    LikingPolicy::OfferWindow window;
    CHECK(window.TryTake(NOW, 2));
    CHECK(window.TryTake(NOW + 600, 2));
    CHECK(!window.TryTake(NOW + 1200, 2));
    CHECK(!window.TryTake(NOW + 3599, 2));
    CHECK(window.TryTake(NOW + 3600, 2));
    CHECK(window.TryTake(NOW + 3700, 2));
    CHECK(!window.TryTake(NOW + 3800, 2));
}
TEST(liking_a_zero_cap_makes_no_offers_and_a_set_back_clock_opens_a_window)
{
    LikingPolicy::OfferWindow window;
    CHECK(!window.TryTake(NOW, 0));
    CHECK(window.TryTake(NOW, 1));
    CHECK(!window.TryTake(NOW + 10, 1));
    CHECK(window.TryTake(NOW - 100, 1));
}
TEST(liking_old_rows_near_neutral_are_forgettable)
{
    Tuning const tuning;
    Pair pair;
    pair.value = 0.90;
    pair.updatedAt = NOW;
    CHECK(!LikingPolicy::Forgettable(tuning, pair, NOW + 30 * DAY, 180));
    CHECK(!LikingPolicy::Forgettable(tuning, pair, NOW + 179 * DAY, 180));
    // Past the retention the score has decayed to within the dust of neutral (eight half-lives).
    CHECK(LikingPolicy::Forgettable(tuning, pair, NOW + 180 * DAY, 180));
    // A score that decay would keep far from neutral is kept.
    Tuning slow;
    slow.decayHalfLifeDays = 10000.0;
    CHECK(!LikingPolicy::Forgettable(slow, pair, NOW + 180 * DAY, 180));
}
