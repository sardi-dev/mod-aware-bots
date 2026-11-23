/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "ArenaMath.h"
#include "TestHarness.h"

using ArenaMath::Outcome;
using ArenaMath::TeamView;
using BotSkillMath::Tier;

namespace
{
// Fixed instants (UTC epoch seconds), computed with an independent calendar tool.
constexpr std::int64_t NOV_12_2026_UTC_MIDNIGHT = 1794441600;  // Thursday
constexpr std::int64_t DST_2026_STARTS_UTC = 1772953200;       // Sunday 2026-03-08 07:00 UTC = 02:00 EST
constexpr std::int64_t DST_2026_ENDS_UTC = 1793512800;         // Sunday 2026-11-01 06:00 UTC = 02:00 EDT
constexpr std::int64_t JULY_4_2026_2000_UTC = 1783195200;      // Saturday
constexpr std::int64_t DST_2027_STARTS_UTC = 1805007600;       // Sunday 2027-03-14 07:00 UTC
constexpr std::int64_t DST_2027_ENDS_UTC = 1825567200;         // Sunday 2027-11-07 06:00 UTC

ArenaMath::LocalTime Eastern(std::int64_t utc, std::int32_t clockOffsetMinutes = 0)
{
    return ArenaMath::OwnerLocalTime(utc, ArenaMath::OWNER_UTC_OFFSET_HOURS, true, clockOffsetMinutes);
}

TeamView Team(std::uint32_t id, std::int32_t rating, std::uint32_t idle, std::uint8_t faction = 0, std::uint8_t band = 10)
{
    TeamView team;
    team.id = id;
    team.rating = rating;
    team.bracket = 2;
    team.band = band;
    team.faction = faction;
    team.idleSeconds = idle;
    team.failedPasses = 0;
    return team;
}
}  // namespace

// ---- rating -----------------------------------------------------------------------------------------------------

TEST(arena_rating_matches_the_worked_table)
{
    // 1500 v 1500, settled teams (K 24): +12 / -12
    CHECK(ArenaMath::RatingChange(1500, 1500, Outcome::WIN, 20) == 12);
    CHECK(ArenaMath::RatingChange(1500, 1500, Outcome::LOSS, 20) == -12);
    // expert (1750) against good (1500): the favourite gains 7 for a win and loses 17 for a loss
    CHECK(ArenaMath::RatingChange(1750, 1500, Outcome::WIN, 20) == 7);
    CHECK(ArenaMath::RatingChange(1750, 1500, Outcome::LOSS, 20) == -17);
    CHECK(ArenaMath::RatingChange(1500, 1750, Outcome::WIN, 20) == 17);
    CHECK(ArenaMath::RatingChange(1500, 1750, Outcome::LOSS, 20) == -7);
    // casual (1300) against expert (1750): +20 for a win, -4 for a loss
    CHECK(ArenaMath::RatingChange(1300, 1750, Outcome::WIN, 20) == 20);
    CHECK(ArenaMath::RatingChange(1300, 1750, Outcome::LOSS, 20) == -4);
}

TEST(arena_rating_expected_score_is_a_probability_pair)
{
    CHECK_NEAR(ArenaMath::ExpectedScore(1500, 1500), 0.5, 1e-12);
    for (std::int32_t gap = -800; gap <= 800; gap += 100)
        CHECK_NEAR(ArenaMath::ExpectedScore(1500 + gap, 1500) + ArenaMath::ExpectedScore(1500, 1500 + gap), 1.0, 1e-12);
    CHECK(ArenaMath::ExpectedScore(1800, 1500) > 0.5);
}

TEST(arena_rating_k_is_higher_for_the_first_ten_games)
{
    CHECK(ArenaMath::KFactor(0) == 32);
    CHECK(ArenaMath::KFactor(9) == 32);
    CHECK(ArenaMath::KFactor(10) == 24);
    CHECK(ArenaMath::RatingChange(1500, 1500, Outcome::WIN, 0) == 16);
    CHECK(ArenaMath::RatingChange(1500, 1500, Outcome::WIN, 10) == 12);
}

TEST(arena_rating_draw_moves_toward_the_other_team)
{
    CHECK(ArenaMath::RatingChange(1500, 1500, Outcome::DRAW, 20) == 0);
    CHECK(ArenaMath::RatingChange(1750, 1500, Outcome::DRAW, 20) < 0);  // the favourite loses a little
    CHECK(ArenaMath::RatingChange(1500, 1750, Outcome::DRAW, 20) > 0);
}

TEST(arena_rating_rounds_half_away_from_zero)
{
    CHECK(ArenaMath::RoundAway(2.5) == 3);
    CHECK(ArenaMath::RoundAway(-2.5) == -3);
    CHECK(ArenaMath::RoundAway(2.4) == 2);
    CHECK(ArenaMath::RoundAway(-2.4) == -2);
}

TEST(arena_rating_floor_is_zero)
{
    CHECK(ArenaMath::Apply(10, -16) == 0);
    CHECK(ArenaMath::Apply(1500, -12) == 1488);
    CHECK(ArenaMath::Apply(0, 5) == 5);
}

TEST(arena_rating_seed_follows_the_tier_within_the_jitter)
{
    for (std::uint32_t id = 1; id < 300; ++id)
    {
        CHECK(std::abs(ArenaMath::SeedRating(Tier::EXPERT, id) - 1750) <= ArenaMath::SEED_JITTER);
        CHECK(std::abs(ArenaMath::SeedRating(Tier::GOOD, id) - 1500) <= ArenaMath::SEED_JITTER);
        CHECK(std::abs(ArenaMath::SeedRating(Tier::CASUAL, id) - 1300) <= ArenaMath::SEED_JITTER);
        CHECK(ArenaMath::SeedRating(Tier::GOOD, id) == ArenaMath::SeedRating(Tier::GOOD, id));
    }

    // the jitter spreads: ten teams of one tier are not all on one number
    std::int32_t low = 9999, high = 0;
    for (std::uint32_t id = 1; id <= 10; ++id)
    {
        low = std::min(low, ArenaMath::SeedRating(Tier::GOOD, id));
        high = std::max(high, ArenaMath::SeedRating(Tier::GOOD, id));
    }
    CHECK(high - low >= 20);
}

TEST(arena_streak_counts_wins_losses_and_resets_on_a_draw)
{
    std::int32_t streak = 0;
    streak = ArenaMath::NextStreak(streak, Outcome::WIN);
    streak = ArenaMath::NextStreak(streak, Outcome::WIN);
    streak = ArenaMath::NextStreak(streak, Outcome::WIN);
    CHECK(streak == 3);
    streak = ArenaMath::NextStreak(streak, Outcome::LOSS);
    CHECK(streak == -1);
    streak = ArenaMath::NextStreak(streak, Outcome::LOSS);
    CHECK(streak == -2);
    streak = ArenaMath::NextStreak(streak, Outcome::DRAW);
    CHECK(streak == 0);
    CHECK(ArenaMath::NextStreak(0, Outcome::WIN) == 1);
    CHECK(ArenaMath::NextStreak(0, Outcome::LOSS) == -1);
}

TEST(arena_partner_replacement_pulls_a_quarter_back_to_the_tier_base)
{
    CHECK(ArenaMath::RatingAfterPartnerReplaced(1700, 1500) == 1650);
    CHECK(ArenaMath::RatingAfterPartnerReplaced(1300, 1500) == 1350);
    CHECK(ArenaMath::RatingAfterPartnerReplaced(1500, 1500) == 1500);
}

TEST(arena_flip_swaps_win_and_loss)
{
    CHECK(ArenaMath::Flip(Outcome::WIN) == Outcome::LOSS);
    CHECK(ArenaMath::Flip(Outcome::LOSS) == Outcome::WIN);
    CHECK(ArenaMath::Flip(Outcome::DRAW) == Outcome::DRAW);
}

// ---- the draw rule ----------------------------------------------------------------------------------------------

TEST(arena_bot_only_match_is_cut_as_a_draw_after_twelve_minutes)
{
    CHECK(!ArenaMath::IsCutAsDraw(719, false));
    CHECK(ArenaMath::IsCutAsDraw(720, false));
    CHECK(ArenaMath::IsCutAsDraw(3000, false));
}

TEST(arena_match_with_a_human_is_never_cut)
{
    CHECK(!ArenaMath::IsCutAsDraw(720, true));
    CHECK(!ArenaMath::IsCutAsDraw(100000, true));
}

TEST(arena_cut_can_be_switched_off_with_zero)
{
    CHECK(!ArenaMath::IsCutAsDraw(100000, false, 0));
}

// ---- the owner's clock ------------------------------------------------------------------------------------------

TEST(arena_clock_standard_time_in_november)
{
    // 2026-11-12 00:00 UTC is Wednesday 19:00 EST
    ArenaMath::LocalTime const t = Eastern(NOV_12_2026_UTC_MIDNIGHT);
    CHECK(t.year == 2026 && t.month == 11 && t.day == 11);
    CHECK(t.hour == 19 && t.minute == 0);
    CHECK(t.weekday == 3);  // Wednesday
    CHECK(!t.daylight);
}

TEST(arena_clock_daylight_time_in_july)
{
    // 2026-07-04 20:00 UTC is Saturday 16:00 EDT
    ArenaMath::LocalTime const t = Eastern(JULY_4_2026_2000_UTC);
    CHECK(t.daylight);
    CHECK(t.hour == 16 && t.day == 4 && t.month == 7);
    CHECK(t.weekday == 6);
}

TEST(arena_clock_daylight_edges_2026)
{
    // spring: one second before 02:00 EST is 01:59:59 EST, then it is 03:00 EDT
    ArenaMath::LocalTime before = Eastern(DST_2026_STARTS_UTC - 1);
    CHECK(!before.daylight && before.hour == 1 && before.minute == 59);
    ArenaMath::LocalTime after = Eastern(DST_2026_STARTS_UTC);
    CHECK(after.daylight && after.hour == 3 && after.minute == 0);
    CHECK(after.weekday == 0);  // Sunday

    // autumn: one second before 02:00 EDT is 01:59:59 EDT, then it is 01:00 EST again
    before = Eastern(DST_2026_ENDS_UTC - 1);
    CHECK(before.daylight && before.hour == 1 && before.minute == 59);
    after = Eastern(DST_2026_ENDS_UTC);
    CHECK(!after.daylight && after.hour == 1 && after.minute == 0);
}

TEST(arena_clock_daylight_edges_2027)
{
    CHECK(!Eastern(DST_2027_STARTS_UTC - 1).daylight);
    CHECK(Eastern(DST_2027_STARTS_UTC).daylight);
    CHECK(Eastern(DST_2027_ENDS_UTC - 1).daylight);
    CHECK(!Eastern(DST_2027_ENDS_UTC).daylight);
}

TEST(arena_clock_daylight_can_be_switched_off)
{
    ArenaMath::LocalTime const t = ArenaMath::OwnerLocalTime(JULY_4_2026_2000_UTC, -5, false);
    CHECK(!t.daylight && t.hour == 15);
}

TEST(arena_clock_offset_hook_moves_the_clock)
{
    // Wednesday 19:00 EST plus 60 minutes
    ArenaMath::LocalTime const t = Eastern(NOV_12_2026_UTC_MIDNIGHT, 60);
    CHECK(t.hour == 20 && t.day == 11);
    // and back to 03:00 the next morning: 19:00 + 8 h = 03:00 Thursday
    ArenaMath::LocalTime const night = Eastern(NOV_12_2026_UTC_MIDNIGHT, 8 * 60);
    CHECK(night.hour == 3 && night.day == 12 && night.weekday == 4);
}

TEST(arena_clock_epoch_and_negative_times_do_not_break)
{
    ArenaMath::LocalTime const t = Eastern(0);
    CHECK(t.year == 1969 && t.month == 12 && t.day == 31 && t.hour == 19);
    CHECK(ArenaMath::WeekdayOfDays(0) == 4);  // 1970-01-01 was a Thursday
}

// ---- the schedule -----------------------------------------------------------------------------------------------

TEST(arena_schedule_is_quiet_from_23_to_11)
{
    for (std::uint32_t weekday = 0; weekday < 7; ++weekday)
        for (std::uint32_t hour = 0; hour < 24; ++hour)
        {
            ArenaMath::LocalTime time{};
            time.weekday = weekday;
            time.hour = hour;
            bool const quiet = hour >= 23 || hour < 11;
            CHECK((ArenaMath::ActivityWeightPercent(time) == 0) == quiet);
            CHECK(ArenaMath::IsQuietHour(hour) == quiet);
        }
}

TEST(arena_schedule_weekday_weights)
{
    ArenaMath::LocalTime time{};
    time.weekday = 2;  // Tuesday
    time.hour = 12;
    CHECK(ArenaMath::ActivityWeightPercent(time) == 25);
    time.hour = 16;
    CHECK(ArenaMath::ActivityWeightPercent(time) == 25);
    time.hour = 17;
    CHECK(ArenaMath::ActivityWeightPercent(time) == 60);
    time.hour = 18;
    CHECK(ArenaMath::ActivityWeightPercent(time) == 60);
    time.hour = 19;
    CHECK(ArenaMath::ActivityWeightPercent(time) == 100);
    time.hour = 22;
    CHECK(ArenaMath::ActivityWeightPercent(time) == 100);
}

TEST(arena_schedule_weekend_weights_are_busier)
{
    for (std::uint32_t weekday : { 0u, 6u })
    {
        ArenaMath::LocalTime time{};
        time.weekday = weekday;
        time.hour = 13;
        CHECK(ArenaMath::ActivityWeightPercent(time) == 60);
        time.hour = 18;
        CHECK(ArenaMath::ActivityWeightPercent(time) == 80);
        time.hour = 20;
        CHECK(ArenaMath::ActivityWeightPercent(time) == 100);
        time.hour = 2;
        CHECK(ArenaMath::ActivityWeightPercent(time) == 0);
    }
}

TEST(arena_schedule_expected_matches_per_day_follow_the_design)
{
    // Expected matches per day = sum over the 24 hours of weight * peak (4 per hour at full weight).
    auto perDay = [](bool weekend)
    {
        double total = 0.0;
        for (std::uint32_t hour = 0; hour < 24; ++hour)
        {
            ArenaMath::LocalTime time{};
            time.weekday = weekend ? 6 : 2;
            time.hour = hour;
            total += ArenaMath::StartChance(ArenaMath::ActivityWeightPercent(time), 4) * 60.0;
        }
        return total;
    };
    CHECK_NEAR(perDay(false), 27.0, 1.0);
    CHECK_NEAR(perDay(true), 37.0, 1.5);
}

TEST(arena_schedule_start_chance_and_roll)
{
    CHECK_NEAR(ArenaMath::StartChance(100, 4), 4.0 / 60.0, 1e-12);
    CHECK_NEAR(ArenaMath::StartChance(25, 4), 1.0 / 60.0, 1e-12);
    CHECK_NEAR(ArenaMath::StartChance(0, 4), 0.0, 1e-12);
    CHECK_NEAR(ArenaMath::StartChance(100, 120), 1.0, 1e-12);  // capped
    CHECK(ArenaMath::ShouldStart(100, 4, 0.05));
    CHECK(!ArenaMath::ShouldStart(100, 4, 0.07));
    CHECK(!ArenaMath::ShouldStart(0, 4, 0.0));  // quiet: never, even for the lowest roll
}

TEST(arena_schedule_skips_while_the_world_is_slow)
{
    CHECK(ArenaMath::WorldLoadAllows(80, 80));
    CHECK(ArenaMath::WorldLoadAllows(12, 80));
    CHECK(!ArenaMath::WorldLoadAllows(81, 80));
}

TEST(arena_schedule_owner_evening_vs_night_with_the_clock_hook)
{
    // 2026-11-12 00:00 UTC is Wednesday 19:00 EST: prime time. +8 h is 03:00 Thursday: quiet.
    CHECK(ArenaMath::ActivityWeightPercent(Eastern(NOV_12_2026_UTC_MIDNIGHT)) == 100);
    CHECK(ArenaMath::ActivityWeightPercent(Eastern(NOV_12_2026_UTC_MIDNIGHT, 8 * 60)) == 0);
    CHECK(ArenaMath::ActivityWeightPercent(Eastern(NOV_12_2026_UTC_MIDNIGHT, 60)) == 100);
}

// ---- levels and bands -------------------------------------------------------------------------------------------

TEST(arena_bands_follow_the_core_brackets)
{
    CHECK(ArenaMath::BandOfLevel(10) == 0);
    CHECK(ArenaMath::BandOfLevel(14) == 0);
    CHECK(ArenaMath::BandOfLevel(15) == 1);
    CHECK(ArenaMath::BandOfLevel(59) == 9);
    CHECK(ArenaMath::BandOfLevel(60) == 10);
    CHECK(ArenaMath::BandOfLevel(64) == 10);
    CHECK(ArenaMath::BandOfLevel(65) == 11);
    CHECK(ArenaMath::BandOfLevel(89) == 15);
    CHECK(ArenaMath::BandOfLevel(90) == 16);
    CHECK(ArenaMath::BandMinLevel(10) == 60 && ArenaMath::BandMaxLevel(10) == 64);
    CHECK(ArenaMath::BandMinLevel(16) == 90 && ArenaMath::BandMaxLevel(16) == 90);
}

TEST(arena_teams_only_from_level_sixty)
{
    CHECK(!ArenaMath::BandAllowed(9, 60));   // 55 to 59
    CHECK(ArenaMath::BandAllowed(10, 60));   // 60 to 64
    CHECK(ArenaMath::BandAllowed(16, 60));
    CHECK(ArenaMath::BandAllowed(4, 30));    // the minimum level is a setting
}

TEST(arena_weakest_member_sets_the_team_tier)
{
    CHECK(ArenaMath::WeakestTier(Tier::EXPERT, Tier::GOOD) == Tier::GOOD);
    CHECK(ArenaMath::WeakestTier(Tier::CASUAL, Tier::EXPERT) == Tier::CASUAL);
    CHECK(ArenaMath::WeakestTier(Tier::GOOD, Tier::GOOD) == Tier::GOOD);
    CHECK(ArenaMath::WeakestTier(Tier::EXPERT, Tier::EXPERT) == Tier::EXPERT);
}

// ---- pairing ----------------------------------------------------------------------------------------------------

TEST(arena_pairing_takes_the_longest_idle_team_first)
{
    std::vector<TeamView> teams = { Team(1, 1500, 100), Team(2, 1500, 900), Team(3, 1500, 500) };
    ArenaMath::PairChoice const pick = ArenaMath::PickPair(teams);
    CHECK(pick.found);
    CHECK(pick.teamA == 2);  // idle longest
}

TEST(arena_pairing_prefers_the_closest_rating)
{
    std::vector<TeamView> teams = { Team(1, 1500, 900), Team(2, 1700, 10), Team(3, 1540, 10) };
    ArenaMath::PairChoice const pick = ArenaMath::PickPair(teams);
    CHECK(pick.found && pick.teamA == 1 && pick.teamB == 3);
}

TEST(arena_pairing_prefers_the_opposing_faction_on_a_tie)
{
    std::vector<TeamView> teams = { Team(1, 1500, 900, 0), Team(2, 1500, 10, 0), Team(3, 1500, 10, 1) };
    ArenaMath::PairChoice const pick = ArenaMath::PickPair(teams);
    CHECK(pick.found && pick.teamB == 3);
}

TEST(arena_pairing_penalises_a_rematch)
{
    std::vector<TeamView> teams = { Team(1, 1500, 900), Team(2, 1500, 10), Team(3, 1520, 10) };
    teams[0].recentOpponents = { 2 };  // 1 just played 2: the 20 point rating gap loses to the repeat penalty
    ArenaMath::PairChoice const pick = ArenaMath::PickPair(teams);
    CHECK(pick.found && pick.teamB == 3);
}

TEST(arena_pairing_stays_in_one_band)
{
    std::vector<TeamView> teams = { Team(1, 1500, 900, 0, 10), Team(2, 1500, 10, 1, 11) };
    ArenaMath::PairChoice const pick = ArenaMath::PickPair(teams);
    CHECK(!pick.found);
    CHECK(pick.unmatched.size() == 2);
}

TEST(arena_pairing_window_widens_with_failed_passes)
{
    CHECK(ArenaMath::WindowFor(0) == 150);
    CHECK(ArenaMath::WindowFor(1) == 200);
    CHECK(ArenaMath::WindowFor(5) == 400);
    CHECK(ArenaMath::WindowFor(50) == 400);  // capped

    std::vector<TeamView> teams = { Team(1, 1750, 900), Team(2, 1300, 10) };  // 450 apart
    CHECK(!ArenaMath::PickPair(teams).found);
    teams[0].failedPasses = 5;
    teams[1].failedPasses = 5;
    CHECK(!ArenaMath::PickPair(teams).found);  // cost 450 - 20 stays above the capped window of 400
    teams[1].rating = 1400;                    // 350 apart
    CHECK(ArenaMath::PickPair(teams).found);
}

TEST(arena_pairing_an_unmatched_first_team_does_not_block_the_next)
{
    // team 1 has waited longest but only a team of another band exists for it; teams 2 and 3 can play each other
    std::vector<TeamView> teams = { Team(1, 1500, 900, 0, 11), Team(2, 1500, 100, 0, 10), Team(3, 1500, 50, 1, 10) };
    ArenaMath::PairChoice const pick = ArenaMath::PickPair(teams);
    CHECK(pick.found && pick.teamA == 2 && pick.teamB == 3);
    CHECK(pick.unmatched.size() == 1 && pick.unmatched[0] == 1);
}

TEST(arena_pairing_with_fewer_than_two_teams_finds_nobody)
{
    CHECK(!ArenaMath::PickPair({}).found);
    CHECK(!ArenaMath::PickPair({ Team(1, 1500, 10) }).found);
}

TEST(arena_pairing_is_deterministic_in_ties)
{
    std::vector<TeamView> a = { Team(4, 1500, 100), Team(2, 1500, 100), Team(9, 1500, 100) };
    std::vector<TeamView> b = { Team(9, 1500, 100), Team(4, 1500, 100), Team(2, 1500, 100) };
    ArenaMath::PairChoice const x = ArenaMath::PickPair(a);
    ArenaMath::PairChoice const y = ArenaMath::PickPair(b);
    CHECK(x.teamA == y.teamA && x.teamB == y.teamB);
    CHECK(x.teamA == 2 && x.teamB == 4);
}

// ---- the periodic check of a team --------------------------------------------------------------------------------

TEST(arena_team_check_keeps_a_team_in_step)
{
    ArenaMath::ValidationResult r = ArenaMath::ValidateTeam(10, 61, 63, 60, 0, 12);
    CHECK(r.action == ArenaMath::Validation::OK && r.band == 10 && r.failedChecks == 0);
}

TEST(arena_team_moves_up_with_its_members)
{
    ArenaMath::ValidationResult const r = ArenaMath::ValidateTeam(10, 65, 67, 60, 5, 12);
    CHECK(r.action == ArenaMath::Validation::MOVE_BAND);
    CHECK(r.band == 11);
    CHECK(r.failedChecks == 0);  // a good check clears earlier failures
}

TEST(arena_team_out_of_step_counts_failures_and_retires)
{
    ArenaMath::ValidationResult r = ArenaMath::ValidateTeam(10, 64, 65, 60, 0, 3);
    CHECK(r.action == ArenaMath::Validation::OUT_OF_STEP && r.failedChecks == 1 && r.band == 10);
    r = ArenaMath::ValidateTeam(10, 64, 65, 60, r.failedChecks, 3);
    CHECK(r.action == ArenaMath::Validation::OUT_OF_STEP && r.failedChecks == 2);
    r = ArenaMath::ValidateTeam(10, 64, 65, 60, r.failedChecks, 3);
    CHECK(r.action == ArenaMath::Validation::RETIRE);
}

TEST(arena_team_below_the_minimum_level_is_a_failed_check)
{
    ArenaMath::ValidationResult const r = ArenaMath::ValidateTeam(10, 12, 13, 60, 0, 12);
    CHECK(r.action == ArenaMath::Validation::BELOW_MINIMUM && r.failedChecks == 1);
}

TEST(arena_team_back_in_step_clears_the_count)
{
    ArenaMath::ValidationResult r = ArenaMath::ValidateTeam(10, 64, 65, 60, 0, 12);
    r = ArenaMath::ValidateTeam(10, 65, 65, 60, r.failedChecks, 12);  // the partner caught up: both moved up
    CHECK(r.action == ArenaMath::Validation::MOVE_BAND && r.failedChecks == 0);
}
