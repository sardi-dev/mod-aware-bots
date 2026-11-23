#include "TestHarness.h"
#include "QuestClockPolicy.h"
#include "QuestHandInPolicy.h"

#include <cstdint>

using namespace QuestClock;

TEST(quest_clock_runs_only_while_active)
{
    uint32_t clock = 1;
    clock = Advance(clock, 50, true);
    CHECK(clock == 51);
    clock = Advance(clock, 50, false);
    CHECK(clock == 51);
    clock = Advance(clock, 120, true);
    CHECK(clock == 171);
}

TEST(quest_clock_caps_a_stalled_tick)
{
    CHECK(Advance(1, 30000, true) == 1 + MAX_TICK_MS);
    CHECK(Advance(1, MAX_TICK_MS, true) == 1 + MAX_TICK_MS);
}

TEST(quest_clock_never_reads_zero)
{
    CHECK(Advance(0, 0, false) == 1);
    CHECK(Advance(0xFFFFFFFFu, 1, true) == 1);
    CHECK(Advance(0xFFFFFFF0u, 100, true) == 0x54u);
}

TEST(quest_clock_gives_a_rotating_bot_its_planned_walk)
{
    // A bot active one 30 s window in ten: ten minutes of wall time are one minute of play, so a hand-in trip of
    // 1,000 yd (5 min plus 400 s) is not out of time after 15 wall minutes, as it was on the wall clock.
    QuestHandInPolicy::Candidate c;
    c.questId = 26011;
    c.dist = 1000.0f;
    uint32_t clock = 1;
    QuestHandInPolicy::Trip trip = QuestHandInPolicy::Start(c, 406, clock);
    for (uint32_t ms = 0; ms < 15u * 60u * 1000u; ms += 100)
        clock = Advance(clock, 100, (ms / 30000u) % 10u == 0);
    CHECK(!QuestHandInPolicy::Expired(trip, clock));
    CHECK(QuestHandInPolicy::Expired(trip, 15u * 60u * 1000u));
}

TEST(status_watch_measures_a_status_in_active_time)
{
    // A rotating bot: the clock reads 1,000 when DO_QUEST starts (serial 4) and 181,000 three active minutes later,
    // after 30 wall minutes; the 30-minute quest pick has 27 minutes of play left.
    StatusWatch watch;
    CHECK(watch.Elapsed(4, 1000) == 0);
    CHECK(watch.Elapsed(4, 181000) == 180000);
    CHECK(watch.Elapsed(4, 181000) < 30u * 60u * 1000u);
}

TEST(status_watch_restarts_on_a_status_change)
{
    StatusWatch watch;
    watch.Elapsed(1, 5000);
    CHECK(watch.Elapsed(1, 9000) == 4000);
    CHECK(watch.Elapsed(2, 9500) == 0);
    CHECK(watch.Elapsed(2, 10500) == 1000);
}

TEST(status_watch_after_a_reset_info)
{
    // A fresh NewRpgInfo has serial 0, as a fresh watch: the first look still stamps the start.
    StatusWatch watch;
    CHECK(watch.Elapsed(0, 70000) == 0);
    CHECK(watch.Elapsed(0, 71000) == 1000);
}
