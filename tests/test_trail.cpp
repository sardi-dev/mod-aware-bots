/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "TestHarness.h"
#include "TrailMath.h"

namespace
{
// The values Formations.cpp uses.
TrailMath::Limits const LIMITS = { 2.0f, 6.0f, 3.0f, 16 };
size_t const CATCH_UP_POINTS = 3;
size_t const CATCH_UP_STEP = 2;

// The leader walks from (0, 0) along +x in one yard steps and is recorded after every step.
std::deque<TrailMath::Point> WalkEast(float yards)
{
    std::deque<TrailMath::Point> trail;
    for (float x = 0.0f; x <= yards; x += 1.0f)
        TrailMath::Record(trail, { x, 0.0f, 0.0f }, LIMITS);
    return trail;
}
}  // namespace

TEST(trail_starts_with_the_first_reading)
{
    std::deque<TrailMath::Point> trail;
    CHECK(TrailMath::Record(trail, { 5.0f, 5.0f, 0.0f }, LIMITS));
    CHECK(trail.size() == 1);
}

TEST(trail_ignores_a_leader_who_stands_still)
{
    std::deque<TrailMath::Point> trail;
    TrailMath::Record(trail, { 0.0f, 0.0f, 0.0f }, LIMITS);
    for (int i = 0; i < 50; ++i)
        CHECK(!TrailMath::Record(trail, { 0.5f, 0.5f, 0.0f }, LIMITS));
    CHECK(trail.size() == 1);
}

TEST(trail_adds_a_point_every_step)
{
    std::deque<TrailMath::Point> trail = WalkEast(10.0f);
    // readings at 0, 2, 4, 6, 8 and 10 yards
    CHECK(trail.size() == 6);
    CHECK_NEAR(trail.front().x, 10.0, 0.001);
    CHECK_NEAR(trail.back().x, 0.0, 0.001);
}

TEST(trail_restarts_after_a_teleport)
{
    std::deque<TrailMath::Point> trail = WalkEast(10.0f);
    CHECK(TrailMath::Record(trail, { 100.0f, 0.0f, 0.0f }, LIMITS));
    CHECK(trail.size() == 1);
    CHECK_NEAR(trail.front().x, 100.0, 0.001);
}

TEST(trail_restarts_on_another_floor)
{
    std::deque<TrailMath::Point> trail = WalkEast(10.0f);
    CHECK(TrailMath::Record(trail, { 10.5f, 0.0f, 8.0f }, LIMITS));
    CHECK(trail.size() == 1);
}

TEST(trail_keeps_at_most_the_maximum)
{
    std::deque<TrailMath::Point> trail = WalkEast(100.0f);
    CHECK(trail.size() == LIMITS.maxPoints);
    CHECK_NEAR(trail.front().x, 100.0, 0.001);
}

TEST(trail_spot_on_a_straight_walk)
{
    std::deque<TrailMath::Point> trail = WalkEast(20.0f);
    TrailMath::Point const head = { 20.0f, 0.0f, 0.0f };
    // a bot standing near its place, five yards behind the leader
    TrailMath::Place const place = TrailMath::Find(trail, head, 5.0f, 14.0f, 0.5f, CATCH_UP_POINTS, CATCH_UP_STEP);
    CHECK(place.kind == TrailMath::Place::SPOT);
    CHECK_NEAR(place.point.x, 15.0, 0.01);
    CHECK_NEAR(place.point.y, 0.0, 0.01);
    // the side direction is across the way walked, one yard long
    CHECK_NEAR(place.sideX, 0.0, 0.01);
    CHECK_NEAR(std::fabs(place.sideY), 1.0, 0.01);
}

TEST(trail_spot_follows_a_corner)
{
    // ten yards east, then ten yards north
    std::deque<TrailMath::Point> trail;
    for (float x = 0.0f; x <= 10.0f; x += 1.0f)
        TrailMath::Record(trail, { x, 0.0f, 0.0f }, LIMITS);
    for (float y = 1.0f; y <= 10.0f; y += 1.0f)
        TrailMath::Record(trail, { 10.0f, y, 0.0f }, LIMITS);

    TrailMath::Point const head = { 10.0f, 10.0f, 0.0f };
    // fourteen yards back along the way is four yards before the corner, not on the diagonal
    TrailMath::Place const place = TrailMath::Find(trail, head, 14.0f, 6.0f, 0.0f, CATCH_UP_POINTS, CATCH_UP_STEP);
    CHECK(place.kind == TrailMath::Place::SPOT);
    CHECK_NEAR(place.point.x, 6.0, 0.01);
    CHECK_NEAR(place.point.y, 0.0, 0.01);
}

TEST(trail_too_short_gives_no_place)
{
    std::deque<TrailMath::Point> trail = WalkEast(2.0f);
    TrailMath::Place const place = TrailMath::Find(trail, { 2.0f, 0.0f, 0.0f }, 9.0f, 0.0f, 0.0f, CATCH_UP_POINTS,
                                                   CATCH_UP_STEP);
    CHECK(place.kind == TrailMath::Place::NONE);
}

TEST(trail_bot_far_behind_walks_the_corner)
{
    std::deque<TrailMath::Point> trail;
    for (float x = 0.0f; x <= 10.0f; x += 1.0f)
        TrailMath::Record(trail, { x, 0.0f, 0.0f }, LIMITS);
    for (float y = 1.0f; y <= 10.0f; y += 1.0f)
        TrailMath::Record(trail, { 10.0f, y, 0.0f }, LIMITS);

    TrailMath::Point const head = { 10.0f, 10.0f, 0.0f };
    // the bot's place is three yards behind the leader (10, 7), but the bot still stands at the start (0, 0)
    TrailMath::Place const place = TrailMath::Find(trail, head, 3.0f, 0.0f, 0.0f, CATCH_UP_POINTS, CATCH_UP_STEP);
    CHECK(place.kind == TrailMath::Place::CATCH_UP);
    // it is sent along the first leg (y stays 0), not across the corner towards (10, 7)
    CHECK_NEAR(place.point.y, 0.0, 0.01);
    CHECK(place.point.x > 0.0f && place.point.x <= 10.0f);
}

TEST(trail_bot_near_its_place_is_not_sent_back)
{
    std::deque<TrailMath::Point> trail = WalkEast(20.0f);
    TrailMath::Place const place = TrailMath::Find(trail, { 20.0f, 0.0f, 0.0f }, 3.0f, 16.0f, 0.0f, CATCH_UP_POINTS,
                                                   CATCH_UP_STEP);
    CHECK(place.kind == TrailMath::Place::SPOT);
    CHECK_NEAR(place.point.x, 17.0, 0.01);
}
