#include "TestHarness.h"
#include "BgFillPolicy.h"

TEST(bg_fill_requires_a_waiting_human)
{
    CHECK(!BgFillPolicy::Wanted(0, 0, 0, 10));
    CHECK(BgFillPolicy::Wanted(1, 0, 0, 10));
}

TEST(bg_fill_obeys_each_team_quota)
{
    CHECK(BgFillPolicy::Wanted(1, 1, 8, 10));
    CHECK(!BgFillPolicy::Wanted(1, 1, 9, 10));
    CHECK(!BgFillPolicy::Wanted(1, 11, 0, 10));
    CHECK(!BgFillPolicy::Wanted(1, 0, 0, 0));
}

TEST(bg_fill_bots_leave_a_queue_no_human_waits_in_unless_invited)
{
    CHECK(BgFillPolicy::ShouldLeaveQueue(0, false));
    CHECK(!BgFillPolicy::ShouldLeaveQueue(0, true));
    CHECK(!BgFillPolicy::ShouldLeaveQueue(1, false));
    CHECK(!BgFillPolicy::ShouldLeaveQueue(2, true));
}

// BT-430: the team sizes of every battleground the core queues, from battleground_template (min, max a side).
TEST(bg_fill_quota_is_the_map_team_size)
{
    CHECK((BgFillPolicy::TeamQuota(5, 10, false)) == (10u));   // Warsong Gulch
    CHECK((BgFillPolicy::TeamQuota(8, 15, false)) == (15u));   // Arathi Basin, Eye of the Storm, Strand of the Ancients
    CHECK((BgFillPolicy::TeamQuota(5, 10, false)) == (10u));   // Twin Peaks, Battle for Gilneas, Temple of Kotmogu
    CHECK((BgFillPolicy::TeamQuota(10, 40, false)) == (40u));  // Alterac Valley, Isle of Conquest
}

TEST(bg_fill_quota_of_the_random_queue_ignores_its_template)
{
    // the random template says 5 to 40, the map is chosen when the match is made
    CHECK((BgFillPolicy::TeamQuota(5, 40, true)) == (BgFillPolicy::RANDOM_QUEUE_FILL_PER_TEAM));
    CHECK((BgFillPolicy::TeamQuota(0, 0, true)) == (BgFillPolicy::RANDOM_QUEUE_FILL_PER_TEAM));
}

TEST(bg_fill_quota_is_bounded)
{
    CHECK((BgFillPolicy::TeamQuota(0, 0, false)) == (0u));                                  // no battleground behind the queue
    CHECK((BgFillPolicy::TeamQuota(12, 8, false)) == (12u));                                // never below what starts the match
    CHECK((BgFillPolicy::TeamQuota(10, 80, false)) == (BgFillPolicy::MAX_FILL_PER_TEAM));  // never above the biggest map
}

TEST(bg_fill_booking_counts_the_bot_at_once)
{
    unsigned bots = 0;
    CHECK(BgFillPolicy::TryBook(1, 0, bots, 10));
    CHECK((bots) == (1u));
    CHECK(!BgFillPolicy::TryBook(0, 0, bots, 10));  // nobody waits: no booking, no change
    CHECK((bots) == (1u));
}

TEST(bg_fill_booking_stops_at_the_quota_inside_one_census_window)
{
    // the 2026-11-17 live run: 29 bots looked in one census window and queued for a Warsong quota of 10 a side (12 and 17
    // arrived). With the booking each bot sees the ones before it, whatever the order.
    unsigned allianceBots = 0, hordeBots = 0;
    unsigned const humanOnHorde = 1;
    unsigned queued = 0;
    for (unsigned i = 0; i < 29; ++i)
    {
        bool const alliance = i % 2 == 0;
        bool const booked = alliance ? BgFillPolicy::TryBook(1, 0, allianceBots, 10)
                                     : BgFillPolicy::TryBook(1, humanOnHorde, hordeBots, 10);
        queued += booked ? 1 : 0;
    }
    CHECK((allianceBots) == (10u));
    CHECK((hordeBots) == (9u));  // the human holds one of the ten Horde places
    CHECK((queued) == (19u));
}

TEST(bg_fill_booking_fills_the_big_maps_to_forty_a_side)
{
    unsigned bots = 0;
    unsigned const quota = BgFillPolicy::TeamQuota(10, 40, false);
    unsigned queued = 0;
    for (unsigned i = 0; i < 100; ++i)
        queued += BgFillPolicy::TryBook(1, 0, bots, quota) ? 1 : 0;
    CHECK((queued) == (40u));
}

TEST(bg_fill_a_full_human_side_takes_no_bot)
{
    unsigned bots = 0;
    CHECK(!BgFillPolicy::TryBook(1, 10, bots, 10));
    CHECK((bots) == (0u));
}
