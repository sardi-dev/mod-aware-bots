#include "TestHarness.h"
#include "ActivityScalePolicy.h"

#include <cstdint>

using namespace ActivityScale;

TEST(activity_scale_keeps_the_rotation_below_the_floor)
{
    CHECK(Scale(10, 0, 50, 200) == 10);
    CHECK(Scale(10, 50, 50, 200) == 10);
}

TEST(activity_scale_thins_between_floor_and_ceiling)
{
    CHECK(Scale(10, 125, 50, 200) == 5);
    CHECK(Scale(10, 53, 50, 200) == 9);
    CHECK(Scale(10, 199, 50, 200) == 0);
}

TEST(activity_scale_stops_above_the_ceiling)
{
    CHECK(Scale(10, 201, 50, 200) == 0);
    CHECK(Scale(10, 500, 200, 200) == 0);
    CHECK(Scale(10, 150, 200, 200) == 10);
}

TEST(activity_table_reads_the_mean_not_the_worst_tick)
{
    // The live pattern of 2026-11-17: ticks of about 50 ms with one stall of 7.8 s in a table. The mean stays near
    // the floor, so the rotation keeps most of its share; the longest tick would have stopped it.
    Table table;
    for (uint32_t i = 0; i < TABLE_TICKS - 1; ++i)
        table.Add(50);
    table.Add(7800);
    CHECK(table.full);
    CHECK(table.Lag() == (50 * (TABLE_TICKS - 1) + 7800) / TABLE_TICKS);
    CHECK(Scale(10, table.Lag(), 50, 200) == 9);
}

TEST(activity_table_follows_a_real_overload)
{
    Table table;
    for (uint32_t i = 0; i < TABLE_TICKS; ++i)
        table.Add(250);
    CHECK(Scale(10, table.Lag(), 50, 200) == 0);
    // The next table is not read until it is full.
    for (uint32_t i = 0; i < TABLE_TICKS - 1; ++i)
        table.Add(40);
    CHECK(table.Lag() == 250);
    table.Add(40);
    CHECK(table.Lag() == 40);
}

TEST(activity_table_before_the_first_full_table)
{
    Table table;
    CHECK(table.Lag() == 0);
    table.Add(60);
    table.Add(100);
    CHECK(!table.full);
    CHECK(table.Lag() == 80);
}
