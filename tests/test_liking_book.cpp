#include "TestHarness.h"
#include "LikingBook.h"

using LikingBook::Book;
using LikingBook::PairClock;
using LikingBook::RepeatGate;
using LikingBook::Row;
using LikingPolicy::Event;
using LikingPolicy::Pair;
using LikingPolicy::Tuning;

namespace
{
constexpr uint32_t DAY = LikingPolicy::SECONDS_PER_DAY;
constexpr uint32_t NOW = 20000 * DAY + 9 * 3600;
constexpr uint32_t HUMAN = 5001;
constexpr uint32_t BOT_A = 71;
constexpr uint32_t BOT_B = 72;

Row StoredRow(uint32_t bot, double value, uint32_t updatedAt)
{
    Row row;
    row.bot = bot;
    row.human = HUMAN;
    row.pair.value = value;
    row.pair.updatedAt = updatedAt;
    return row;
}
}  // namespace

// ---- who makes a pair, the kick grace, the offer cooldown --------------------------------------------------------------

TEST(liking_pair_needs_a_random_bot_and_a_human)
{
    CHECK(LikingPolicy::PairEligible(true, true, 71, 5001));
    CHECK(!LikingPolicy::PairEligible(false, true, 71, 5001));   // an alt bot or any other bot
    CHECK(!LikingPolicy::PairEligible(true, false, 71, 72));     // two bots
    CHECK(!LikingPolicy::PairEligible(true, true, 0, 5001));
    CHECK(!LikingPolicy::PairEligible(true, true, 71, 0));
    CHECK(!LikingPolicy::PairEligible(true, true, 71, 71));
}
TEST(liking_a_kick_inside_the_grace_counts_for_nothing)
{
    Tuning tuning;
    tuning.kickGraceSeconds = 120;
    CHECK(!LikingPolicy::KickCounts(tuning, NOW - 30, NOW));
    CHECK(!LikingPolicy::KickCounts(tuning, NOW - 119, NOW));
    CHECK(LikingPolicy::KickCounts(tuning, NOW - 120, NOW));
    CHECK(LikingPolicy::KickCounts(tuning, NOW - 3600, NOW));
}
TEST(liking_a_kick_of_a_bot_with_an_unknown_join_time_counts)
{
    Tuning const tuning;
    CHECK(LikingPolicy::KickCounts(tuning, 0, NOW));
    CHECK(LikingPolicy::KickCounts(tuning, NOW + 50, NOW));  // a clock that went back
}
TEST(liking_the_default_grace_is_two_minutes)
{
    CHECK(Tuning().kickGraceSeconds == 120);
}
TEST(liking_the_offer_cooldown_runs_from_the_last_offer)
{
    uint32_t const hour = 3600;
    CHECK(LikingPolicy::OfferCooldownPassed(0, NOW, 24 * hour));
    CHECK(!LikingPolicy::OfferCooldownPassed(NOW - hour, NOW, 24 * hour));
    CHECK(!LikingPolicy::OfferCooldownPassed(NOW - 24 * hour + 1, NOW, 24 * hour));
    CHECK(LikingPolicy::OfferCooldownPassed(NOW - 24 * hour, NOW, 24 * hour));
}
TEST(liking_a_stored_offer_time_far_in_the_future_does_not_block_for_good)
{
    uint32_t const hour = 3600;
    CHECK(!LikingPolicy::OfferCooldownPassed(NOW + hour, NOW, 24 * hour));
    CHECK(LikingPolicy::OfferCooldownPassed(NOW + 30 * 24 * hour, NOW, 24 * hour));
}

// ---- the book: loading, waiting events, dirty rows ----------------------------------------------------------------------

TEST(book_an_unknown_human_takes_no_events_and_sits_at_neutral)
{
    Tuning const tuning;
    Book book;
    CHECK(!book.Note(tuning, HUMAN, BOT_A, Event::DungeonDone, 1, NOW));
    CHECK_NEAR(book.Value(tuning, HUMAN, BOT_A, NOW), 0.30, 1e-9);
    CHECK(book.TakeDirty(0, 100).empty());
}
TEST(book_events_before_the_rows_arrive_wait_and_replay_on_them)
{
    Tuning const tuning;
    Book book;
    book.Begin(HUMAN);
    CHECK(book.Has(HUMAN) && !book.Loaded(HUMAN));
    CHECK(book.Note(tuning, HUMAN, BOT_A, Event::DungeonDone, 1, NOW));
    // Waiting events do not show in the score yet and nothing is dirty.
    CHECK_NEAR(book.Value(tuning, HUMAN, BOT_A, NOW), 0.30, 1e-9);
    CHECK(book.DirtyCount() == 0);

    book.Load(tuning, HUMAN, { StoredRow(BOT_A, 0.50, NOW) }, 0);
    CHECK(book.Loaded(HUMAN));
    CHECK_NEAR(book.Value(tuning, HUMAN, BOT_A, NOW), 0.56, 1e-9);
    CHECK(book.DirtyCount() == 1);
}
TEST(book_rows_that_come_from_the_database_are_clean)
{
    Tuning const tuning;
    Book book;
    book.Begin(HUMAN);
    book.Load(tuning, HUMAN, { StoredRow(BOT_A, 0.50, NOW), StoredRow(BOT_B, 0.40, NOW) }, 0);
    CHECK(book.RowCount(HUMAN) == 2);
    CHECK(book.DirtyCount() == 0);
    CHECK_NEAR(book.Value(tuning, HUMAN, BOT_B, NOW), 0.40, 1e-9);
}
TEST(book_a_change_marks_only_that_row_dirty_and_taking_it_marks_it_clean)
{
    Tuning const tuning;
    Book book;
    book.Begin(HUMAN);
    book.Load(tuning, HUMAN, { StoredRow(BOT_A, 0.50, NOW), StoredRow(BOT_B, 0.40, NOW) }, 0);
    CHECK(book.Note(tuning, HUMAN, BOT_B, Event::Traded, 1, NOW));
    CHECK(book.DirtyCount() == 1);

    std::vector<Row> const taken = book.TakeDirty(0, 100);
    CHECK(taken.size() == 1);
    CHECK(taken[0].bot == BOT_B && taken[0].human == HUMAN);
    CHECK_NEAR(taken[0].pair.value, 0.42, 1e-9);
    CHECK(book.DirtyCount() == 0);
    CHECK(book.TakeDirty(0, 100).empty());
}
TEST(book_a_new_pair_starts_at_neutral_and_is_dirty)
{
    Tuning const tuning;
    Book book;
    book.Begin(HUMAN);
    book.Load(tuning, HUMAN, {}, 0);
    book.Note(tuning, HUMAN, BOT_A, Event::Insulted, 1, NOW);
    CHECK_NEAR(book.Value(tuning, HUMAN, BOT_A, NOW), 0.20, 1e-9);
    CHECK(book.DirtyCount() == 1);
}
TEST(book_take_dirty_stops_at_the_limit_and_the_rest_comes_next_time)
{
    Tuning const tuning;
    Book book;
    book.Begin(HUMAN);
    book.Load(tuning, HUMAN, {}, 0);
    for (uint32_t bot = 100; bot < 105; ++bot)
        book.Note(tuning, HUMAN, bot, Event::DungeonDone, 1, NOW);
    CHECK(book.TakeDirty(0, 2).size() == 2);
    CHECK(book.DirtyCount() == 3);
    CHECK(book.TakeDirty(0, 100).size() == 3);
}
TEST(book_take_dirty_for_one_human_leaves_the_others)
{
    Tuning const tuning;
    Book book;
    book.Begin(HUMAN);
    book.Begin(HUMAN + 1);
    book.Load(tuning, HUMAN, {}, 0);
    book.Load(tuning, HUMAN + 1, {}, 0);
    book.Note(tuning, HUMAN, BOT_A, Event::DungeonDone, 1, NOW);
    book.Note(tuning, HUMAN + 1, BOT_A, Event::DungeonDone, 1, NOW);
    CHECK(book.TakeDirty(HUMAN, 100).size() == 1);
    CHECK(book.DirtyCount() == 1);
}
TEST(book_the_daily_cap_holds_across_events_in_the_book)
{
    Tuning const tuning;
    Book book;
    book.Begin(HUMAN);
    book.Load(tuning, HUMAN, {}, 0);
    for (int i = 0; i < 5; ++i)
        book.Note(tuning, HUMAN, BOT_A, Event::DungeonDone, 1, NOW);
    CHECK_NEAR(book.Value(tuning, HUMAN, BOT_A, NOW), 0.45, 1e-9);
}
TEST(book_logout_hands_out_the_changed_rows_and_forgets_the_human)
{
    Tuning const tuning;
    Book book;
    book.Begin(HUMAN);
    book.Load(tuning, HUMAN, {}, 0);
    book.Note(tuning, HUMAN, BOT_A, Event::DungeonDone, 1, NOW);
    std::vector<Row> const rows = book.End(HUMAN);
    CHECK(rows.size() == 1);
    CHECK(!book.Has(HUMAN));
    CHECK(book.HumanCount() == 0);
}
TEST(book_a_wipe_clears_one_human_but_keeps_it_loaded_and_the_others)
{
    Tuning const tuning;
    Book book;
    book.Begin(HUMAN);
    book.Begin(HUMAN + 1);
    book.Load(tuning, HUMAN, { StoredRow(BOT_A, 0.8, NOW) }, 0);
    book.Load(tuning, HUMAN + 1, { StoredRow(BOT_A, 0.8, NOW) }, 0);
    book.Wipe(HUMAN);
    CHECK(book.Loaded(HUMAN));
    CHECK(book.RowCount(HUMAN) == 0);
    CHECK_NEAR(book.Value(tuning, HUMAN, BOT_A, NOW), 0.30, 1e-9);
    CHECK_NEAR(book.Value(tuning, HUMAN + 1, BOT_A, NOW), 0.80, 1e-9);
    book.WipeAll();
    CHECK(book.RowCount(HUMAN + 1) == 0);
}
TEST(book_a_wipe_before_the_load_drops_the_waiting_events)
{
    Tuning const tuning;
    Book book;
    book.Begin(HUMAN);
    book.Note(tuning, HUMAN, BOT_A, Event::DungeonDone, 1, NOW);
    book.Wipe(HUMAN);
    book.Load(tuning, HUMAN, {}, 0);
    CHECK(book.DirtyCount() == 0);
}
TEST(book_a_deleted_bot_leaves_every_humans_memory)
{
    Tuning const tuning;
    Book book;
    book.Begin(HUMAN);
    book.Begin(HUMAN + 1);
    book.Load(tuning, HUMAN, { StoredRow(BOT_A, 0.8, NOW), StoredRow(BOT_B, 0.7, NOW) }, 0);
    book.Note(tuning, HUMAN + 1, BOT_A, Event::DungeonDone, 1, NOW);
    book.ForgetBot(BOT_A);
    CHECK(book.RowCount(HUMAN) == 1);
    book.Load(tuning, HUMAN + 1, {}, 0);
    CHECK(book.DirtyCount() == 0);
}
TEST(book_the_waiting_list_is_bounded)
{
    Tuning const tuning;
    Book book;
    book.Begin(HUMAN);
    for (uint32_t i = 0; i < 500; ++i)
        book.Note(tuning, HUMAN, 100 + i, Event::GroupTime, 1, NOW);
    book.Load(tuning, HUMAN, {}, 0);
    CHECK(book.RowCount(HUMAN) == LikingBook::MAX_WAITING);
}

// ---- the guild offer gate ----------------------------------------------------------------------------------------------

TEST(book_a_guild_offer_needs_the_rows_loaded)
{
    Book book;
    book.Begin(HUMAN);
    CHECK(!book.TakeGuildOffer(HUMAN, NOW, 1));
    CHECK(!book.TakeGuildOffer(HUMAN + 9, NOW, 1));
}
TEST(book_one_guild_offer_per_human_per_hour_and_the_time_is_kept)
{
    Tuning const tuning;
    Book book;
    book.Begin(HUMAN);
    book.Load(tuning, HUMAN, {}, 0);
    CHECK(book.TakeGuildOffer(HUMAN, NOW, 1));
    CHECK(book.GuildOfferAt(HUMAN) == NOW);
    CHECK(!book.TakeGuildOffer(HUMAN, NOW + 600, 1));
    CHECK(book.TakeGuildOffer(HUMAN, NOW + 3600, 1));
}
TEST(book_the_stored_offer_time_is_loaded_and_a_new_one_is_written_back)
{
    Tuning const tuning;
    Book book;
    book.Begin(HUMAN);
    book.Load(tuning, HUMAN, {}, NOW - 100);
    CHECK(book.GuildOfferAt(HUMAN) == NOW - 100);

    std::vector<std::pair<uint32_t, uint32_t>> offers;
    CHECK(book.TakeDirty(0, 10, &offers).empty());
    CHECK(offers.empty());  // what was loaded is not written again

    book.TakeGuildOffer(HUMAN, NOW, 1);
    CHECK(book.TakeDirty(0, 10, &offers).empty());
    CHECK(offers.size() == 1 && offers[0].first == HUMAN && offers[0].second == NOW);
    offers.clear();
    book.TakeDirty(0, 10, &offers);
    CHECK(offers.empty());
}
TEST(book_logout_writes_a_pending_offer_time)
{
    Tuning const tuning;
    Book book;
    book.Begin(HUMAN);
    book.Load(tuning, HUMAN, {}, 0);
    book.TakeGuildOffer(HUMAN, NOW, 1);
    std::vector<std::pair<uint32_t, uint32_t>> offers;
    book.End(HUMAN, &offers);
    CHECK(offers.size() == 1);
}

// ---- time together, repeated tone, pass credit -------------------------------------------------------------------------

TEST(clock_ten_minutes_of_group_time_make_one_step_and_the_rest_carries_over)
{
    PairClock clock;
    uint64_t const key = PairClock::KeyOf(BOT_A, HUMAN);
    uint32_t steps = 0;
    for (int pass = 0; pass < 9; ++pass)
        steps += clock.Add(key, 60, 600);
    CHECK(steps == 0);
    CHECK(clock.Add(key, 60, 600) == 1);
    CHECK(clock.Add(key, 1260, 600) == 2);  // a long credit makes whole steps and keeps the remainder (60 left)
    CHECK(clock.Add(key, 540, 600) == 1);
}
TEST(clock_a_pair_not_seen_in_a_pass_is_dropped)
{
    PairClock clock;
    clock.Add(PairClock::KeyOf(BOT_A, HUMAN), 60, 600);
    clock.Add(PairClock::KeyOf(BOT_B, HUMAN), 60, 600);
    clock.EndPass();
    CHECK(clock.Size() == 2);
    clock.Add(PairClock::KeyOf(BOT_A, HUMAN), 60, 600);
    clock.EndPass();
    CHECK(clock.Size() == 1);
    clock.EndPass();
    CHECK(clock.Size() == 0);
}
TEST(clock_guild_hours_make_a_step_per_full_hour)
{
    PairClock clock;
    uint64_t const key = PairClock::KeyOf(BOT_A, HUMAN);
    uint32_t steps = 0;
    for (int pass = 0; pass < 120; ++pass)
        steps += clock.Add(key, 60, LikingPolicy::SECONDS_PER_HOUR);
    CHECK(steps == 2);
}
TEST(clock_a_zero_step_length_makes_nothing)
{
    PairClock clock;
    CHECK(clock.Add(PairClock::KeyOf(BOT_A, HUMAN), 60, 0) == 0);
}
TEST(clock_keys_of_different_pairs_differ)
{
    CHECK(PairClock::KeyOf(1, 2) != PairClock::KeyOf(2, 1));
    CHECK(PairClock::KeyOf(1, 2) != PairClock::KeyOf(1, 3));
}
TEST(gate_the_same_line_counts_once_in_a_window)
{
    RepeatGate gate;
    CHECK(gate.Allow(7, NOW, 60));
    CHECK(!gate.Allow(7, NOW + 10, 60));
    CHECK(!gate.Allow(7, NOW + 59, 60));
    CHECK(gate.Allow(7, NOW + 60, 60));
    CHECK(gate.Allow(8, NOW + 61, 60));
}
TEST(gate_a_repeat_does_not_extend_the_window)
{
    RepeatGate gate;
    gate.Allow(7, NOW, 60);
    gate.Allow(7, NOW + 50, 60);  // blocked, and must not restart the window
    CHECK(gate.Allow(7, NOW + 61, 60));
}
TEST(credit_a_pass_credits_the_time_since_the_last_one_but_never_more_than_three_minutes)
{
    CHECK(LikingBook::PassCredit(0, NOW) == 0);
    CHECK(LikingBook::PassCredit(NOW, NOW) == 0);
    CHECK(LikingBook::PassCredit(NOW + 5, NOW) == 0);
    CHECK(LikingBook::PassCredit(NOW - 60, NOW) == 60);
    CHECK(LikingBook::PassCredit(NOW - 7200, NOW) == LikingBook::MAX_PASS_CREDIT_SECONDS);
}

// ---- the SQL text -----------------------------------------------------------------------------------------------------

TEST(sql_a_replace_holds_the_rows_in_the_column_order)
{
    std::vector<Row> rows;
    Row row = StoredRow(71, 0.5, NOW);
    row.pair.gainDay = 20000;
    row.pair.gainToday = 0.04;
    rows.push_back(row);
    rows.push_back(StoredRow(72, 0.25, NOW));
    std::string const sql = LikingBook::ReplaceSql(rows, 0, rows.size());
    CHECK(sql.find("REPLACE INTO `playerbots_social_liking` (`bot_guid`, `player_guid`, `value`, `updated_at`, `gain_day`, `gain_today`) VALUES (71, 5001, 0.500000, ") == 0);
    CHECK(sql.find("), (72, 5001, 0.250000, ") != std::string::npos);
    CHECK(sql.find(", 20000, 0.040000)") != std::string::npos);
    CHECK(sql.back() == ')');
}
TEST(sql_a_replace_of_a_slice_holds_only_that_slice)
{
    std::vector<Row> rows;
    for (uint32_t bot = 1; bot <= 5; ++bot)
        rows.push_back(StoredRow(bot, 0.5, NOW));
    std::string const sql = LikingBook::ReplaceSql(rows, 2, 4);
    CHECK(sql.find("(3, 5001") != std::string::npos);
    CHECK(sql.find("(4, 5001") != std::string::npos);
    CHECK(sql.find("(2, 5001") == std::string::npos);
    CHECK(sql.find("(5, 5001") == std::string::npos);
}
TEST(sql_the_offer_replace_names_the_guild_kind)
{
    CHECK(LikingBook::OfferReplaceSql(5001, 1234) ==
        "REPLACE INTO `playerbots_social_offer` (`player_guid`, `kind`, `last_at`) VALUES (5001, 1, 1234)");
}
TEST(sql_retention_deletes_old_rows_near_neutral_and_old_offers)
{
    Tuning const tuning;
    std::vector<std::string> const sql = LikingBook::RetentionSql(tuning, NOW, 180);
    CHECK(sql.size() == 2);
    uint32_t const cutoff = NOW - 180 * DAY;
    CHECK(sql[0].find("DELETE FROM `playerbots_social_liking` WHERE `updated_at` < " + std::to_string(cutoff)) == 0);
    CHECK(sql[0].find("ABS(`value` - 0.300000)") != std::string::npos);
    CHECK(sql[0].find("<= 0.0200") != std::string::npos);
    CHECK(sql[1] == "DELETE FROM `playerbots_social_offer` WHERE `last_at` < " + std::to_string(cutoff));
}
TEST(sql_retention_without_decay_drops_every_old_row)
{
    Tuning tuning;
    tuning.decayHalfLifeDays = 0.0;
    std::vector<std::string> const sql = LikingBook::RetentionSql(tuning, NOW, 180);
    CHECK(sql[0].find("ABS") == std::string::npos);
}
TEST(sql_retention_agrees_with_the_policys_forgettable_rule)
{
    // The SQL keeps a row while |value - neutral| * 2^(-age / halfLife) is above the dust: the same test as Forgettable.
    Tuning const tuning;
    Pair pair;
    pair.value = 0.95;
    pair.updatedAt = NOW - 180 * DAY;
    CHECK(LikingPolicy::Forgettable(tuning, pair, NOW, 180));
    pair.updatedAt = NOW - 100 * DAY;
    CHECK(!LikingPolicy::Forgettable(tuning, pair, NOW, 180));
}
TEST(sql_forgetting_a_character_covers_both_roles)
{
    std::vector<std::string> const sql = LikingBook::ForgetPlayerSql(5001, true);
    CHECK(sql.size() == 5);
    CHECK(sql[0] == "DELETE FROM `playerbots_social_liking` WHERE `player_guid` = 5001");
    CHECK(sql[1] == "DELETE FROM `playerbots_social_offer` WHERE `player_guid` = 5001");
    CHECK(sql[2] == "DELETE FROM `playerbots_social_quest_offer` WHERE `player_guid` = 5001");
    CHECK(sql[3] == "DELETE FROM `playerbots_social_liking` WHERE `bot_guid` = 5001");
    CHECK(sql[4] == "DELETE FROM `playerbots_social_quest_offer` WHERE `bot_guid` = 5001");
    CHECK(LikingBook::ForgetPlayerSql(5001, false).size() == 3);
}
TEST(quest_offer_needs_the_rows_loaded_and_enough_liking)
{
    Tuning const tuning;
    uint32_t const cooldown = 6 * 3600;
    Book book;
    // An unknown human, then one whose rows are still on their way.
    CHECK(!book.TakeQuestOffer(tuning, HUMAN, BOT_A, NOW, cooldown));
    book.Begin(HUMAN);
    CHECK(!book.TakeQuestOffer(tuning, HUMAN, BOT_A, NOW, cooldown));

    book.Load(tuning, HUMAN, { StoredRow(BOT_A, 0.45, NOW), StoredRow(BOT_B, 0.39, NOW) }, 0);
    // 39 percent is below the gate of 40, 45 is above it; a bot with no row sits at neutral 30.
    CHECK(!book.TakeQuestOffer(tuning, HUMAN, BOT_B, NOW, cooldown));
    CHECK(!book.TakeQuestOffer(tuning, HUMAN, 999, NOW, cooldown));
    CHECK(book.TakeQuestOffer(tuning, HUMAN, BOT_A, NOW, cooldown));
}
TEST(quest_offer_the_same_bot_waits_out_the_pair_cooldown)
{
    Tuning tuning;
    tuning.questOffersPerHour = 50;
    uint32_t const cooldown = 6 * 3600;
    Book book;
    book.Begin(HUMAN);
    book.Load(tuning, HUMAN, { StoredRow(BOT_A, 0.6, NOW) }, 0);
    CHECK(book.TakeQuestOffer(tuning, HUMAN, BOT_A, NOW, cooldown));
    CHECK(!book.TakeQuestOffer(tuning, HUMAN, BOT_A, NOW + cooldown - 1, cooldown));
    CHECK(book.TakeQuestOffer(tuning, HUMAN, BOT_A, NOW + cooldown, cooldown));
}
TEST(quest_offer_a_human_gets_at_most_the_hourly_cap_from_all_bots_together)
{
    Tuning const tuning;
    uint32_t const cooldown = 6 * 3600;
    Book book;
    book.Begin(HUMAN);
    book.Load(tuning, HUMAN, { StoredRow(BOT_A, 0.6, NOW), StoredRow(BOT_B, 0.6, NOW), StoredRow(73, 0.6, NOW) }, 0);
    CHECK(book.TakeQuestOffer(tuning, HUMAN, BOT_A, NOW, cooldown));
    CHECK(book.TakeQuestOffer(tuning, HUMAN, BOT_B, NOW + 60, cooldown));
    // The default cap is two an hour: a third bot is refused, and the refusal costs nothing.
    CHECK(!book.TakeQuestOffer(tuning, HUMAN, 73, NOW + 120, cooldown));
    CHECK(!book.TakeQuestOffer(tuning, HUMAN, 73, NOW + 3599, cooldown));
    CHECK(book.TakeQuestOffer(tuning, HUMAN, 73, NOW + 3600, cooldown));
}
TEST(quest_offer_a_cap_of_zero_never_offers_and_another_human_has_their_own_window)
{
    Tuning off;
    off.questOffersPerHour = 0;
    Book book;
    book.Begin(HUMAN);
    book.Load(off, HUMAN, { StoredRow(BOT_A, 0.9, NOW) }, 0);
    CHECK(!book.TakeQuestOffer(off, HUMAN, BOT_A, NOW, 3600));

    Tuning const tuning;
    uint32_t const other = HUMAN + 1;
    book.Begin(other);
    Row row = StoredRow(BOT_A, 0.9, NOW);
    row.human = other;
    book.Load(tuning, other, { row }, 0);
    CHECK(book.TakeQuestOffer(tuning, other, BOT_A, NOW, 3600));
}
TEST(quest_offer_a_wipe_forgets_the_offers_and_the_liking_that_allowed_them)
{
    Tuning const tuning;
    Book book;
    book.Begin(HUMAN);
    book.Load(tuning, HUMAN, { StoredRow(BOT_A, 0.9, NOW) }, 0);
    CHECK(book.TakeQuestOffer(tuning, HUMAN, BOT_A, NOW, 3600));
    book.Wipe(HUMAN);
    // The human stays loaded but the row is gone: the bot is back at neutral and offers nothing.
    CHECK(!book.TakeQuestOffer(tuning, HUMAN, BOT_A, NOW + 10 * 3600, 3600));
}
TEST(quest_offer_the_pair_list_does_not_grow_for_good)
{
    Tuning tuning;
    tuning.questOffersPerHour = 100000;
    Book book;
    book.Begin(HUMAN);
    std::vector<Row> rows;
    for (uint32_t bot = 1; bot <= 200; ++bot)
        rows.push_back(StoredRow(bot, 0.9, NOW));
    book.Load(tuning, HUMAN, rows, 0);
    // Old offers (past the cooldown) are dropped once the list is at its limit, so every bot can offer again later.
    for (uint32_t bot = 1; bot <= 200; ++bot)
        CHECK(book.TakeQuestOffer(tuning, HUMAN, bot, NOW + bot * 4 * 3600, 3600));
}

TEST(quest_offer_history_survives_logout_and_restart)
{
    Tuning const tuning;
    uint32_t const cooldown = 6 * 3600;
    std::vector<Row> const rows = { StoredRow(BOT_A, 0.9, NOW), StoredRow(BOT_B, 0.9, NOW), StoredRow(73, 0.9, NOW) };
    Book first;
    first.Begin(HUMAN);
    first.Load(tuning, HUMAN, rows, 0);
    CHECK(first.TakeQuestOffer(tuning, HUMAN, BOT_A, NOW, cooldown));
    CHECK(first.TakeQuestOffer(tuning, HUMAN, BOT_B, NOW + 1800, cooldown));
    std::vector<LikingBook::QuestOffer> const saved = first.TakeQuestDirty(HUMAN);
    CHECK(saved.size() == 2);
    first.End(HUMAN);

    Book reloaded;
    reloaded.Begin(HUMAN);
    reloaded.Load(tuning, HUMAN, rows, 0, saved);
    CHECK(!reloaded.TakeQuestOffer(tuning, HUMAN, 73, NOW + 3599, cooldown));
    CHECK(reloaded.TakeQuestOffer(tuning, HUMAN, 73, NOW + 3600, cooldown));
    CHECK(!reloaded.TakeQuestOffer(tuning, HUMAN, BOT_A, NOW + 3601, cooldown));
    CHECK(!reloaded.TakeQuestOffer(tuning, HUMAN, BOT_A, NOW + cooldown - 1, cooldown));
    CHECK(reloaded.TakeQuestOffer(tuning, HUMAN, BOT_A, NOW + cooldown, cooldown));
}
TEST(quest_offer_rolling_hour_counts_across_window_boundaries)
{
    Tuning const tuning;
    Book book;
    book.Begin(HUMAN);
    book.Load(tuning, HUMAN, { StoredRow(BOT_A, 0.9, NOW), StoredRow(BOT_B, 0.9, NOW), StoredRow(73, 0.9, NOW) }, 0);
    CHECK(book.TakeQuestOffer(tuning, HUMAN, BOT_A, NOW, 0));
    CHECK(book.TakeQuestOffer(tuning, HUMAN, BOT_B, NOW + 3599, 0));
    CHECK(book.TakeQuestOffer(tuning, HUMAN, 73, NOW + 3600, 0));
    CHECK(!book.TakeQuestOffer(tuning, HUMAN, BOT_A, NOW + 3601, 0));
}
TEST(sql_quest_offer_history_and_retention)
{
    CHECK(LikingBook::QuestOfferInsertSql({ 5001, 72, 1234 }) ==
        "INSERT INTO `playerbots_social_quest_offer` (`player_guid`, `bot_guid`, `offered_at`) VALUES (5001, 72, 1234)");
    CHECK(LikingBook::QuestRetentionSql(NOW, 1, 2 * DAY) ==
        "DELETE FROM `playerbots_social_quest_offer` WHERE `offered_at` < " + std::to_string(NOW - 2 * DAY));
}

TEST(liking_book_tiers_and_quest_refusal_reasons)
{
    LikingPolicy::Tuning tuning;
    LikingBook::Book book;
    LikingBook::QuestResult result;
    CHECK(!book.TakeQuestOffer(tuning, 1, 2, 100, 60, &result));
    CHECK(result == LikingBook::QuestResult::NotReady);
    book.Begin(1);
    LikingBook::Row row;
    row.human = 1; row.bot = 2; row.pair.value = 0.8; row.pair.updatedAt = 100;
    book.Load(tuning, 1, {row}, 0);
    auto counts = book.TierCounts(tuning, 100);
    CHECK(counts[0] == 1 && counts[1] == 1 && counts[2] == 1 && counts[3] == 1);
    CHECK(!book.TakeQuestOffer(tuning, 1, 3, 100, 60, &result));
    CHECK(result == LikingBook::QuestResult::Liking);
    CHECK(book.TakeQuestOffer(tuning, 1, 2, 100, 60, &result));
    CHECK(result == LikingBook::QuestResult::Made);
    CHECK(!book.TakeQuestOffer(tuning, 1, 2, 101, 60, &result));
    CHECK(result == LikingBook::QuestResult::PairCooldown);
    tuning.questOffersPerHour = 1;
    CHECK(!book.TakeQuestOffer(tuning, 1, 2, 161, 60, &result));
    CHECK(result == LikingBook::QuestResult::RollingLimit);
}
