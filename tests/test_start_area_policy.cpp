#include "StartAreaPolicy.h"
#include "TestHarness.h"

using StartAreaPolicy::InForeignStartArea;
using StartAreaPolicy::StartPoint;

namespace
{
// Tauren at Camp Narache, orc in the Valley of Trials (map 1); human in Northshire (map 0).
std::vector<StartPoint> const STARTS = {{6, 1, -2916.0f, -257.0f}, {2, 1, -619.0f, -4252.0f}, {1, 0, -8915.0f, -134.0f}};
constexpr std::uint32_t PANDAREN_HORDE = 26;
constexpr std::uint32_t TAUREN = 6;
}

TEST(start_area_other_race_start_is_refused)
{
    CHECK(InForeignStartArea(STARTS, PANDAREN_HORDE, 1, -2900.0f, -250.0f));
}

TEST(start_area_own_start_and_open_world_are_allowed)
{
    CHECK(!InForeignStartArea(STARTS, TAUREN, 1, -2900.0f, -250.0f));
    CHECK(!InForeignStartArea(STARTS, PANDAREN_HORDE, 1, -2000.0f, -500.0f));
    // Same coordinates on another map are not the start area.
    CHECK(!InForeignStartArea(STARTS, PANDAREN_HORDE, 0, -2900.0f, -250.0f));
}

TEST(start_area_shared_start_counts_as_own)
{
    std::vector<StartPoint> shared = STARTS;
    shared.push_back({3, 0, -8915.0f, -134.0f});
    CHECK(!InForeignStartArea(shared, 3, 0, -8900.0f, -140.0f));
}

TEST(start_area_pandaren_faction_pick_waits_for_a_new_fate)
{
    CHECK(!StartAreaPolicy::ShouldPickPandarenFaction(false, 5, 15));
    CHECK(StartAreaPolicy::ShouldPickPandarenFaction(true, 9, 15));
    CHECK(StartAreaPolicy::ShouldPickPandarenFaction(false, 15, 15));
}
