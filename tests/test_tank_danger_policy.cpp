/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "TestHarness.h"

#include "TankDangerPolicy.h"

using namespace TankDangerPolicy;

namespace
{
Reading Calm()
{
    Reading r;
    r.attackersOnTank = 1;
    return r;
}
}

TEST(tank_danger_calm_fight_is_level_zero)
{
    CHECK(Level(Calm()) == 0u);
    Reading r = Calm();
    r.attackersOnTank = MANY_ATTACKERS - 1;
    r.healthDropPct = FAST_DROP_PCT - 0.1f;
    CHECK(Level(r) == 0u);
}

TEST(tank_danger_each_fact_adds_points)
{
    Reading r = Calm();
    r.attackersOnTank = MANY_ATTACKERS;
    CHECK(Level(r) == 1u);
    r.attackersOnTank = SWARM_ATTACKERS;
    CHECK(Level(r) == 2u);
    r = Calm();
    r.healthDropPct = FAST_DROP_PCT;
    CHECK(Level(r) == 1u);
    r.healthDropPct = BURST_DROP_PCT;
    CHECK(Level(r) == 2u);
    r = Calm();
    r.healerAvailable = false;
    CHECK(Level(r) == 1u);
    r.bossOnTank = true;
    CHECK(Level(r) == 2u);
}

TEST(tank_danger_level_is_capped_at_three)
{
    Reading r;
    r.attackersOnTank = SWARM_ATTACKERS + 3;
    r.healthDropPct = 80.0f;
    r.healerAvailable = false;
    r.bossOnTank = true;
    CHECK(Level(r) == MAX_DANGER_LEVEL);
}

TEST(tank_danger_small_defensive_comes_earlier_with_danger)
{
    // Calm: the class rules decide alone.
    CHECK(!SmallDefensive(10.0f, 0));
    CHECK(SmallDefensive(SMALL_AT_LEVEL1_PCT - 0.1f, 1));
    CHECK(!SmallDefensive(SMALL_AT_LEVEL1_PCT, 1));
    CHECK(SmallDefensive(SMALL_AT_LEVEL2_PCT - 0.1f, 2));
    CHECK(!SmallDefensive(SMALL_AT_LEVEL2_PCT, 2));
    CHECK(SmallDefensive(100.0f, 3));
}

TEST(tank_danger_big_defensive_needs_a_dangerous_fight)
{
    CHECK(!BigDefensive(10.0f, 0));
    CHECK(!BigDefensive(10.0f, 1));
    CHECK(BigDefensive(BIG_AT_LEVEL2_PCT - 0.1f, 2));
    CHECK(!BigDefensive(BIG_AT_LEVEL2_PCT, 2));
    CHECK(BigDefensive(BIG_AT_LEVEL3_PCT - 0.1f, 3));
    CHECK(!BigDefensive(BIG_AT_LEVEL3_PCT, 3));
    // The big one always waits for a lower bar than the small one at the same level.
    CHECK(BIG_AT_LEVEL2_PCT < SMALL_AT_LEVEL2_PCT);
}

TEST(tank_danger_two_big_defensives_are_spaced_unless_deadly)
{
    CHECK(BigAllowed(0, 5000, 2));
    CHECK(!BigAllowed(1000, 1000 + BIG_GAP_MS - 1, 2));
    CHECK(BigAllowed(1000, 1000 + BIG_GAP_MS, 2));
    CHECK(BigAllowed(1000, 1001, MAX_DANGER_LEVEL));
}

TEST(tank_danger_healer_external_by_level)
{
    CHECK(!ExternalOnTank(20.0f, 0));
    CHECK(!ExternalOnTank(20.0f, 1));
    CHECK(ExternalOnTank(EXTERNAL_AT_LEVEL2_PCT - 0.1f, 2));
    CHECK(!ExternalOnTank(EXTERNAL_AT_LEVEL2_PCT, 2));
    CHECK(ExternalOnTank(EXTERNAL_AT_LEVEL3_PCT - 0.1f, 3));
    CHECK(!ReadingFresh(0, 100));
    CHECK(ReadingFresh(1000, 1000 + READING_FRESH_MS));
    CHECK(!ReadingFresh(1000, 1001 + READING_FRESH_MS));
}

TEST(tank_danger_health_drop_reads_peak_minus_now_in_the_window)
{
    HealthDropWindow window;
    CHECK_NEAR(window.Observe(100.0f, 1000), 0.0f, 0.001f);
    CHECK_NEAR(window.Observe(90.0f, 2000), 10.0f, 0.001f);
    CHECK_NEAR(window.Observe(60.0f, 3000), 40.0f, 0.001f);
    // A heal back up lowers the drop.
    CHECK_NEAR(window.Observe(80.0f, 4000), 20.0f, 0.001f);
    // The 100 at 1000 ms leaves the window after DROP_WINDOW_MS; the peak is then the 90 read at 2000 ms.
    CHECK_NEAR(window.Observe(80.0f, 1000 + DROP_WINDOW_MS + 1), 10.0f, 0.001f);
    window.Reset();
    CHECK_NEAR(window.Observe(50.0f, 20000), 0.0f, 0.001f);
}

TEST(tank_danger_health_drop_folds_close_readings_and_keeps_the_peak)
{
    HealthDropWindow window;
    window.Observe(100.0f, 1000);
    // Many readings within the gap fold into one sample, keeping its highest health.
    for (std::uint32_t t = 1001; t < 1000 + HealthDropWindow::MIN_GAP_MS; ++t)
        window.Observe(70.0f, t);
    CHECK_NEAR(window.Observe(70.0f, 1000 + HealthDropWindow::MIN_GAP_MS - 1), 30.0f, 0.001f);
    // Wrapping the ring with readings across the window still finds the peak inside it.
    for (std::uint32_t i = 1; i <= HealthDropWindow::CAPACITY * 2; ++i)
        window.Observe(i == HealthDropWindow::CAPACITY * 2 ? 50.0f : 90.0f, 1000 + i * HealthDropWindow::MIN_GAP_MS);
    CHECK_NEAR(window.Observe(50.0f, 1000 + HealthDropWindow::CAPACITY * 2 * HealthDropWindow::MIN_GAP_MS), 40.0f, 0.001f);
}

TEST(tank_danger_hand_of_sacrifice_needs_the_paladin_healthy)
{
    CHECK(HandOfSacrificeSafe(SACRIFICE_OWN_HEALTH_PCT, false));
    CHECK(!HandOfSacrificeSafe(SACRIFICE_OWN_HEALTH_PCT - 1.0f, false));
    CHECK(!HandOfSacrificeSafe(100.0f, true));
}
