#include "TestHarness.h"
#include "BgCalloutPolicy.h"
#include "BgScoutPolicy.h"

// BT-483: what an expert bot calls out beyond the plans' events: an enemy group on our half of the map, and the early warning
// at an Arathi Basin node.

using namespace BgScout;

namespace
{
Enemy At(std::uint8_t place, float x = 0.0f, float y = 0.0f)
{
    Enemy e;
    e.place = place;
    e.x = x;
    e.y = y;
    return e;
}
}  // namespace

TEST(scout_a_group_of_three_in_a_zone_is_a_call_and_two_are_not)
{
    Zone zone;
    CHECK(BestZone({ At(3, 10, 0), At(3, 20, 0), At(3, 30, 0) }, 1, zone));
    CHECK(zone.place == 3);
    CHECK(zone.count == 3);
    CHECK_NEAR(zone.x, 20.0, 0.001);

    CHECK(!BestZone({ At(3), At(3) }, 1, zone));
    CHECK(!BestZone({}, 1, zone));
}

TEST(scout_the_flag_room_and_unplaced_enemies_are_left_to_other_calls)
{
    Zone zone;
    CHECK(!BestZone({ At(1), At(1), At(1), At(1) }, 1, zone));
    CHECK(!BestZone({ At(0xFF), At(0xFF), At(0xFF) }, 1, zone));
    // Three in a zone and four in the flag room: the zone is still called.
    CHECK(BestZone({ At(1), At(1), At(1), At(1), At(2), At(2), At(2) }, 1, zone));
    CHECK(zone.place == 2);
}

TEST(scout_the_largest_group_wins_and_a_tie_goes_to_the_lower_place)
{
    Zone zone;
    CHECK(BestZone({ At(4), At(4), At(4), At(2), At(2), At(2), At(2) }, 1, zone));
    CHECK(zone.place == 2 && zone.count == 4);
    CHECK(BestZone({ At(4), At(4), At(4), At(2), At(2), At(2) }, 1, zone));
    CHECK(zone.place == 2);
}

TEST(scout_only_enemies_on_our_half_count)
{
    // Our stand at x = 0, theirs at x = 100.
    CHECK(OnOurHalf(20, 0, 0, 0, 100, 0));
    CHECK(!OnOurHalf(80, 0, 0, 0, 100, 0));
    CHECK(!OnOurHalf(50, 0, 0, 0, 100, 0));     // dead centre is not ours
}

TEST(scout_a_zone_is_called_again_only_after_the_gap)
{
    CHECK(RepeatDue(0, 1000));
    CHECK(!RepeatDue(5000, 5000 + SCOUT_REPEAT_MS - 1));
    CHECK(RepeatDue(5000, 5000 + SCOUT_REPEAT_MS));
    // Across the clock wrap.
    CHECK(!RepeatDue(0xFFFFFF00u, 0xFFFFFF00u + 1000u));
    CHECK(RepeatDue(0xFFFFFF00u, 0xFFFFFF00u + SCOUT_REPEAT_MS));
}

TEST(scout_the_node_warning_fires_between_sixty_and_fifty_yards)
{
    CHECK(EarlyWarning(0, 3, 3));
    CHECK(EarlyWarning(2, 3, 3));
    CHECK(!EarlyWarning(3, 4, 3));      // the ordinary call is already due
    CHECK(!EarlyWarning(0, 2, 3));      // too few
    CHECK(EarlyWarning(0, 2, 2));       // the lower threshold of a low bracket
}

TEST(scout_the_lines_a_bot_says_are_lines_a_human_could_write)
{
    // "3 inc mid" and "3 inc lane h" read back to the zone they name.
    BgCallout::Report r = BgCallout::Parse("3 inc mid", BgCallout::MapKind::WSG);
    CHECK(r.valid && r.count == 3 && r.place == static_cast<std::uint8_t>(WsgLogic::Place::MID));
    r = BgCallout::Parse("4 inc lane h", BgCallout::MapKind::TP);
    CHECK(r.valid && r.count == 4 && r.place == static_cast<std::uint8_t>(WsgLogic::Place::LANE_H));
}
