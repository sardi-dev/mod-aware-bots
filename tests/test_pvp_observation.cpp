#include "TestHarness.h"
#include "PvpObservationMath.h"

// BT-480: the pure part of the shared PvP match state: time, identity, the observation store, the weighing of reports, the
// inbox and the match table's sweep.

using namespace PvpObs;

namespace
{
Observation Obs(Kind kind, std::uint8_t place, std::uint32_t sender, std::uint8_t count = 0,
                Provenance provenance = Provenance::HUMAN_CALL)
{
    Observation o;
    o.kind = kind;
    o.place = place;
    o.senderLow = sender;
    o.count = count;
    o.provenance = provenance;
    return o;
}

constexpr std::uint8_t FR = 1;
constexpr std::uint8_t TUN = 2;
}  // namespace

TEST(pvp_obs_time_compares_across_the_clock_wrap)
{
    CHECK(Reached(100, 100));
    CHECK(Reached(101, 100));
    CHECK(!Reached(99, 100));
    // A deadline set just before the 32 bit wrap is reached just after it.
    CHECK(!Reached(0xFFFFFF00u, 0xFFFFFF00u + 1000u));
    CHECK(Reached(0x00000100u, 0xFFFFFF00u + 0x100u));
    CHECK(Stamp(0) == 1);
    CHECK(Stamp(7) == 7);
}

TEST(pvp_obs_match_key_compares_instance_and_generation)
{
    MatchKey a{ 5, 1 };
    MatchKey b{ 5, 2 };
    CHECK(a == a);
    CHECK(!(a == b));
    CHECK(!(a == MatchKey{ 6, 1 }));
}

TEST(pvp_obs_a_clock_that_runs_back_is_a_reused_instance)
{
    CHECK(!IsRestart(60000, 62000));
    CHECK(!IsRestart(60000, 60000));
    CHECK(!IsRestart(60000, 56000));      // within the slack: a reading taken a moment apart
    CHECK(IsRestart(900000, 1000));
    CHECK(!IsRestart(0, 0));
}

TEST(pvp_obs_store_adds_with_provenance_and_expiry)
{
    SideStore store;
    Limits limits;
    CHECK(store.Add(Obs(Kind::INCOMING, FR, 11, 3), 10000, limits) == AddResult::ADDED);
    CHECK(store.items.size() == 1);
    CHECK(store.items[0].receivedMs == 10000);
    CHECK(store.items[0].expiresMs == 10000 + limits.ttlMs);
    CHECK(store.items[0].provenance == Provenance::HUMAN_CALL);
    CHECK(store.Fresh(10000 + limits.ttlMs - 1) == 1);
    CHECK(store.Fresh(10000 + limits.ttlMs) == 0);
}

TEST(pvp_obs_store_sweeps_what_expired)
{
    SideStore store;
    Limits limits;
    store.Add(Obs(Kind::INCOMING, FR, 11), 1000, limits);
    store.Add(Obs(Kind::HELP, TUN, 12), 9000, limits);
    store.Sweep(1000 + limits.ttlMs);
    CHECK(store.items.size() == 1);
    CHECK(store.items[0].senderLow == 12);
    store.Sweep(9000 + limits.ttlMs);
    CHECK(store.items.empty());
}

TEST(pvp_obs_store_limits_one_sender_to_one_line_per_gap)
{
    SideStore store;
    Limits limits;
    CHECK(store.Add(Obs(Kind::INCOMING, FR, 11), 5000, limits) == AddResult::ADDED);
    CHECK(store.Add(Obs(Kind::HELP, TUN, 11), 6000, limits) == AddResult::RATE_LIMITED);
    CHECK(store.Add(Obs(Kind::HELP, TUN, 11), 7000, limits) == AddResult::ADDED);
    // Another sender is not held back by the first.
    CHECK(store.Add(Obs(Kind::HELP, TUN, 12), 7001, limits) == AddResult::ADDED);
}

TEST(pvp_obs_store_drops_a_repeat_of_the_same_line_while_it_is_fresh)
{
    SideStore store;
    Limits limits;
    store.Add(Obs(Kind::INCOMING, FR, 11, 3), 5000, limits);
    CHECK(store.Add(Obs(Kind::INCOMING, FR, 11, 3), 8000, limits) == AddResult::DUPLICATE);
    CHECK(store.items.size() == 1);
    // After the first line expired the same words are news again.
    CHECK(store.Add(Obs(Kind::INCOMING, FR, 11, 3), 5000 + limits.ttlMs + 1, limits) == AddResult::ADDED);
    // A different place is not a repeat.
    CHECK(store.Add(Obs(Kind::INCOMING, TUN, 11, 3), 5000 + limits.ttlMs + 3000, limits) == AddResult::ADDED);
}

TEST(pvp_obs_store_keeps_a_human_and_a_bot_of_the_same_guid_apart)
{
    // The sender id is a low GUID: a human and a bot can share the number, the provenance tells them apart.
    SideStore store;
    Limits limits;
    store.Add(Obs(Kind::INCOMING, FR, 11, 0, Provenance::HUMAN_CALL), 5000, limits);
    CHECK(store.Add(Obs(Kind::INCOMING, FR, 11, 0, Provenance::BOT_CALL), 5001, limits) == AddResult::ADDED);
}

TEST(pvp_obs_store_is_bounded_and_drops_the_oldest)
{
    SideStore store;
    Limits limits;
    limits.maxPerSide = 4;
    for (std::uint32_t i = 0; i < 10; ++i)
        CHECK(store.Add(Obs(Kind::INCOMING, std::uint8_t(i), 100 + i), 1000 + i * 10, limits) == AddResult::ADDED);
    CHECK(store.items.size() == 4);
    CHECK(store.items.front().senderLow == 106);
    CHECK(store.items.back().senderLow == 109);
}

TEST(pvp_obs_confidence_falls_to_zero_at_the_expiry_and_a_human_weighs_less_than_a_bot)
{
    SideStore store;
    Limits limits;
    store.Add(Obs(Kind::INCOMING, FR, 1, 0, Provenance::HUMAN_CALL), 1000, limits);
    store.Add(Obs(Kind::INCOMING, FR, 2, 0, Provenance::BOT_CALL), 1000, limits);
    Observation const human = store.items[0];
    Observation const bot = store.items[1];
    CHECK(ConfidencePercent(human, 1000) == HUMAN_WEIGHT_PERCENT);
    CHECK(ConfidencePercent(bot, 1000) == BOT_WEIGHT_PERCENT);
    CHECK(ConfidencePercent(human, 1000 + limits.ttlMs / 2) < ConfidencePercent(human, 1000));
    CHECK(ConfidencePercent(human, 1000 + limits.ttlMs / 2) > 0);
    CHECK(ConfidencePercent(human, 1000 + limits.ttlMs) == 0);
    CHECK(ConfidencePercent(human, 1000 + limits.ttlMs + 5000) == 0);
}

TEST(pvp_obs_reported_enemies_weigh_the_count_by_confidence)
{
    SideStore store;
    Limits limits;
    store.Add(Obs(Kind::INCOMING, FR, 1, 4), 1000, limits);
    // 4 enemies at 70 percent: 2.8, rounded to 3.
    CHECK(ReportedEnemies(store.items, FR, 1000) == 3);
    // Half way through its life the line is worth half: 4 * 35 percent = 1.4, rounded to 1.
    CHECK(ReportedEnemies(store.items, FR, 1000 + limits.ttlMs / 2) == 1);
    CHECK(ReportedEnemies(store.items, FR, 1000 + limits.ttlMs) == 0);
    CHECK(ReportedEnemies(store.items, TUN, 1000) == 0);
}

TEST(pvp_obs_a_call_without_a_number_counts_as_a_small_group_and_help_counts_like_inc)
{
    SideStore store;
    Limits limits;
    store.Add(Obs(Kind::HELP, FR, 1, 0, Provenance::BOT_CALL), 1000, limits);
    CHECK(ReportedEnemies(store.items, FR, 1000) == (UNNUMBERED_ENEMIES * BOT_WEIGHT_PERCENT + 50) / 100);
}

TEST(pvp_obs_many_senders_saying_the_same_thing_are_one_report)
{
    SideStore store;
    Limits limits;
    for (std::uint32_t i = 0; i < 5; ++i)
        store.Add(Obs(Kind::INCOMING, FR, 20 + i, 3), 1000, limits);
    CHECK(store.items.size() == 5);
    CHECK(ReportedEnemies(store.items, FR, 1000) == 2);   // 3 at 70 percent, once
}

TEST(pvp_obs_a_clear_after_an_inc_cancels_it_but_not_the_other_way_round)
{
    SideStore store;
    Limits limits;
    store.Add(Obs(Kind::INCOMING, FR, 1, 3), 1000, limits);
    store.Add(Obs(Kind::CLEAR, FR, 2), 4000, limits);
    CHECK(ReportedEnemies(store.items, FR, 4500) == 0);

    SideStore later;
    later.Add(Obs(Kind::CLEAR, FR, 2), 1000, limits);
    later.Add(Obs(Kind::INCOMING, FR, 1, 3), 4000, limits);
    CHECK(ReportedEnemies(later.items, FR, 4500) > 0);
}

TEST(pvp_obs_other_kinds_are_not_pressure)
{
    SideStore store;
    Limits limits;
    store.Add(Obs(Kind::ENEMY_CARRIER, FR, 1, 5), 1000, limits);
    store.Add(Obs(Kind::CLAIM, FR, 2, 5), 1000, limits);
    CHECK(ReportedEnemies(store.items, FR, 1000) == 0);
}

TEST(pvp_obs_a_report_never_lowers_what_the_plan_sees)
{
    CHECK(ReconciledEnemies(5, 2) == 5);
    CHECK(ReconciledEnemies(0, 3) == 3);
    CHECK(ReconciledEnemies(3, 3) == 3);
    CHECK(ReconciledEnemies(0, 0) == 0);
}

TEST(pvp_obs_inbox_is_bounded_and_drains_in_order)
{
    Inbox inbox;
    for (std::uint32_t i = 0; i < 5; ++i)
    {
        Message m;
        m.generation = 1;
        m.side = 0;
        m.obs.senderLow = i;
        CHECK(inbox.Push(m, 5));
    }
    Message extra;
    CHECK(!inbox.Push(extra, 5));

    std::vector<Message> const out = inbox.Drain();
    CHECK(out.size() == 5);
    CHECK(out.front().obs.senderLow == 0);
    CHECK(out.back().obs.senderLow == 4);
    CHECK(inbox.queue.empty());
    CHECK(inbox.Push(extra, 5));
}

TEST(pvp_obs_a_message_of_another_generation_or_side_is_not_valid)
{
    Message m;
    m.generation = 3;
    m.side = 1;
    CHECK(MessageValid(m, 3));
    CHECK(!MessageValid(m, 4));
    m.side = 2;
    CHECK(!MessageValid(m, 3));
}

TEST(pvp_obs_sweep_drops_ended_matches_after_the_grace_and_idle_matches)
{
    std::uint32_t const now = 5000000;
    std::vector<MatchStamp> stamps = {
        { now - 1000, false, 0 },                              // live
        { now - 1000, true, now - 60000 },                     // ended a minute ago: within the grace
        { now - 1000, true, now - END_GRACE_MS - 1 },          // ended long ago
        { now - IDLE_DROP_MS - 1, false, 0 },                  // nobody touched it for ten minutes
    };
    std::vector<std::size_t> const drop = SweepSelect(stamps, now);
    CHECK(drop.size() == 2);
    CHECK(drop[0] == 2);
    CHECK(drop[1] == 3);
}

TEST(pvp_obs_sweep_over_the_size_drops_the_least_recently_touched)
{
    std::uint32_t const now = 5000000;
    std::vector<MatchStamp> stamps;
    for (std::uint32_t i = 0; i < 6; ++i)
        stamps.push_back({ now - 100 * (i + 1), false, 0 });
    // Index 0 is the newest, index 5 the oldest.
    std::vector<std::size_t> const drop = SweepSelect(stamps, now, 4);
    CHECK(drop.size() == 2);
    CHECK(drop[0] == 4);
    CHECK(drop[1] == 5);
    CHECK(SweepSelect(stamps, now, 6).empty());
}

TEST(pvp_obs_sweep_of_nothing_drops_nothing)
{
    CHECK(SweepSelect({}, 12345).empty());
}
