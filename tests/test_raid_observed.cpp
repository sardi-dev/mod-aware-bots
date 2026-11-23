#include "TestHarness.h"
#include "RaidGeometry.h"
#include "StaggerClock.h"

#include <cmath>
#include <vector>

using RaidGeometry::Point3;

TEST(raid_heading_cosine_is_one_straight_ahead_and_negative_behind)
{
    Point3 const from{ 0.0f, 0.0f, 0.0f };
    Point3 const east{ 1.0f, 0.0f, 0.0f };
    CHECK_NEAR(RaidGeometry::HeadingCosine(from, east, { 10.0f, 0.0f, 0.0f }), 1.0, 1e-5);
    CHECK(RaidGeometry::HeadingCosine(from, east, { -10.0f, 0.0f, 0.0f }) < 0.0f);
    // Nothing lies ahead on the unit itself.
    CHECK(RaidGeometry::HeadingCosine(from, east, from) < 0.0f);
}

TEST(raid_heading_match_tells_the_high_point_from_the_low_one_below_it)
{
    // Al'ar's quill point (high) and the middle (low) share their map position; a climbing flight goes to the first.
    std::vector<Point3> const targets = {
        { 388.09f, 31.54f, 20.18f },   // the next platform
        { 332.0f, 0.01f, 39.0f },      // quill point
        { 331.0f, 0.01f, -2.39f },     // middle
    };
    Point3 const platform{ 340.15f, 58.65f, 17.71f };

    auto headingTo = [&](Point3 const& to)
    {
        float const dx = to.x - platform.x, dy = to.y - platform.y, dz = to.z - platform.z;
        float const length = std::sqrt(dx * dx + dy * dy + dz * dz);
        return Point3{ dx / length, dy / length, dz / length };
    };

    CHECK((RaidGeometry::BestHeadingMatch(platform, headingTo(targets[0]), targets, 0.97f)) == 0);
    CHECK((RaidGeometry::BestHeadingMatch(platform, headingTo(targets[1]), targets, 0.97f)) == 1);
    CHECK((RaidGeometry::BestHeadingMatch(platform, headingTo(targets[2]), targets, 0.97f)) == 2);
    // Flying level towards the middle of the room matches neither the climb nor the descent.
    CHECK((RaidGeometry::BestHeadingMatch(platform, { -0.15f, -0.99f, 0.0f }, targets, 0.97f)) == -1);
}

TEST(raid_lane_match_needs_the_lane_line_and_its_direction)
{
    std::vector<RaidGeometry::Lane> const lanes = {
        { { 710.8815f, 306.4028f }, { 660.3492f, 345.5749f } },
        { { 652.105f, 259.5127f }, { 701.6755f, 297.3358f } },
        { { 642.7164f, 305.2436f }, { 706.7507f, 269.4593f } },
    };

    // Halfway along the second lane, moving from its start to its end.
    RaidGeometry::Point const mid{ 676.9f, 278.4f };
    RaidGeometry::Point const along{ 701.6755f - 652.105f, 297.3358f - 259.5127f };
    CHECK((RaidGeometry::MatchLane(mid, along, lanes, 5.0f, 0.9f)) == 1);
    // The same spot, moving the other way, is no beam sweep of that lane.
    CHECK((RaidGeometry::MatchLane(mid, { -along.x, -along.y }, lanes, 5.0f, 0.9f)) == -1);
    // Far off every lane line.
    CHECK((RaidGeometry::MatchLane({ 600.0f, 200.0f }, along, lanes, 5.0f, 0.9f)) == -1);
    // Standing still tells nothing.
    CHECK((RaidGeometry::MatchLane(mid, { 0.0f, 0.0f }, lanes, 5.0f, 0.9f)) == -1);
}

TEST(raid_stagger_clock_waits_its_delay_then_fires_once)
{
    StaggerClock clock;
    CHECK(!clock.Ready(7, 1000, 500));   // arms
    CHECK((clock.RemainingMs(1200)) == 300u);
    CHECK(!clock.Ready(7, 1400, 900));   // still waiting; the first delay holds
    CHECK(clock.Ready(7, 1500, 900));    // fires
    CHECK(!clock.Ready(7, 1600, 400));   // a new step arms again
    CHECK(clock.Ready(7, 2000, 400));
}

TEST(raid_stagger_clock_rearms_for_a_new_key_or_a_stale_delay)
{
    StaggerClock clock;
    CHECK(!clock.Ready(1, 0, 500));
    CHECK(!clock.Ready(2, 600, 500));    // another target: armed anew
    CHECK(clock.Ready(2, 1100, 500));
    CHECK(!clock.Ready(3, 2000, 500));
    // Nobody asked for far longer than the delay plus the stale window: the step was abandoned, so it waits again.
    CHECK(!clock.Ready(3, 2000 + 500 + StaggerClock::STALE_AFTER_MS + 1, 500));
}

TEST(raid_stagger_clock_survives_the_millisecond_clock_wrapping)
{
    StaggerClock clock;
    uint32_t const nearWrap = 0xFFFFFF00u;
    CHECK(!clock.Ready(9, nearWrap, 0x200));
    CHECK(!clock.Ready(9, nearWrap + 0x100, 0x200));
    CHECK(clock.Ready(9, nearWrap + 0x200, 0x200));
}
