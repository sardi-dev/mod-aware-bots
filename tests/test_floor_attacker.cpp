#include "TestHarness.h"
#include "FloorAttackerPolicy.h"
#include "TankRescuePolicy.h"

#include <cmath>

using namespace FloorAttackerPolicy;

TEST(floor_attacker_needs_a_floor_difference_and_a_close_victim)
{
    // Stockade run: Hogger 5 yd above, on his victim. Shadowfang Keep: a Frantic Geist 9.6 and 16 yd below.
    CHECK(NeedsStepAway(5.0f, 1.4f, 8.0f));
    CHECK(NeedsStepAway(-9.6f, 3.0f, 21.0f));
    CHECK(NeedsStepAway(-16.0f, 6.0f, 13.6f));
    // The same floor, or a flight of stairs less than the limit.
    CHECK(!NeedsStepAway(2.9f, 1.0f, 8.0f));
    CHECK(NeedsStepAway(3.0f, 1.0f, 8.0f));
    // A caster far from its victim is no overlap.
    CHECK(!NeedsStepAway(-9.6f, 6.1f, 8.0f));
    // A bot that walked the rescue limit and is still far is a distance problem.
    CHECK(!NeedsStepAway(-9.6f, 3.0f, 40.1f));
}

TEST(floor_attacker_spots_lie_on_their_rings_all_round_the_bot)
{
    for (unsigned i = 0; i < SPOT_COUNT; ++i)
    {
        float dx = 0.0f, dy = 0.0f;
        Offset(i, dx, dy);
        float const length = std::sqrt(dx * dx + dy * dy);
        CHECK(length >= 12.0f - 0.01f && length <= 16.0f + 0.01f);
        CHECK_NEAR(length, RING_YARDS[i / SPOTS_PER_RING], 0.01);
    }
    // The shared ring math keeps the pull sight rings as they were.
    float dx = 0.0f, dy = 0.0f;
    PullSightPolicy::Offset(PullSightPolicy::SPOTS_PER_RING + 3, dx, dy);
    CHECK_NEAR(std::sqrt(dx * dx + dy * dy), PullSightPolicy::RING_YARDS[1], 0.01);
}

TEST(floor_attacker_shortlist_prefers_sight_then_the_nearest)
{
    Spot spots[6];
    for (Spot& spot : spots)
        spot.awayGain = 8.0f;
    spots[0].distance = 12.0f;
    spots[1].distance = 13.0f;
    spots[1].sees = true;
    spots[2].distance = 11.0f;
    spots[3].distance = 12.5f;
    spots[3].sees = true;
    spots[4].distance = 15.0f;
    spots[5].distance = 14.0f;
    unsigned out[PATH_SEARCHES] = {};
    unsigned const found = Shortlist(spots, 6, false, 0.0f, 0.0f, out);
    CHECK(found == PATH_SEARCHES);
    CHECK(out[0] == 3);
    CHECK(out[1] == 1);
    CHECK(out[2] == 2);
    CHECK(out[3] == 0);
}

TEST(floor_attacker_shortlist_drops_spots_that_do_not_step_away_or_wake_a_pack)
{
    Spot spots[4];
    for (Spot& spot : spots)
    {
        spot.awayGain = 8.0f;
        spot.distance = 12.0f;
    }
    spots[0].awayGain = 5.9f;
    spots[1].nearOtherMob = true;
    spots[2].distance = 14.0f;
    unsigned out[PATH_SEARCHES] = {};
    unsigned const found = Shortlist(spots, 4, false, 0.0f, 0.0f, out);
    CHECK(found == 2);
    CHECK(out[0] == 3);
    CHECK(out[1] == 2);
    CHECK(Shortlist(spots, 0, false, 0.0f, 0.0f, out) == 0);
}

TEST(floor_attacker_retry_keeps_away_from_the_last_spot)
{
    Spot spots[2];
    for (Spot& spot : spots)
        spot.awayGain = 8.0f;
    spots[0].x = 10.0f;
    spots[0].distance = 12.0f;
    spots[1].x = 20.0f;
    spots[1].distance = 14.0f;
    unsigned out[PATH_SEARCHES] = {};
    CHECK(Shortlist(spots, 2, true, 12.0f, 0.0f, out) == 1);
    CHECK(out[0] == 1);
    CHECK(Shortlist(spots, 2, true, 15.0f, 0.0f, out) == 2);
}

TEST(floor_attacker_path_must_be_whole_and_short)
{
    CHECK(PathAccepted(true, 20.0f, 14.0f));
    CHECK(!PathAccepted(false, 20.0f, 14.0f));
    CHECK(!PathAccepted(true, 80.0f, 14.0f));
}

TEST(floor_attacker_order_runs_for_its_seconds_then_ends)
{
    Order order;
    CHECK(!Live(order, 1000));
    Begin(order, 77, 1.0f, 2.0f, 3.0f, 1000);
    CHECK(Live(order, 1000));
    CHECK(Live(order, 1000 + ORDER_SECONDS - 1));
    CHECK(!Live(order, 1000 + ORDER_SECONDS));
    Begin(order, 77, 1.0f, 2.0f, 3.0f, 2000);
    End(order);
    CHECK(!Live(order, 2001));
}

TEST(floor_attacker_arrives_within_the_arrival_radius_flat)
{
    Order order;
    Begin(order, 77, 10.0f, 10.0f, 5.0f, 1000);
    CHECK(Arrived(order, 10.0f, 12.4f));
    CHECK(!Arrived(order, 10.0f, 12.6f));
}

TEST(floor_attacker_search_waits_between_tries_and_stops_after_three_steps)
{
    Order order;
    CHECK(MaySearch(order, 77, 1000));
    NoteSearch(order, 1000);
    CHECK(!MaySearch(order, 77, 1000 + RETRY_SECONDS - 1));
    // A failed search (no order begun) allows the next after the gap.
    CHECK(MaySearch(order, 77, 1000 + RETRY_SECONDS));

    time_t now = 2000;
    for (unsigned step = 0; step < MAX_STEPS; ++step)
    {
        CHECK(MaySearch(order, 77, now));
        NoteSearch(order, now);
        Begin(order, 77, float(step), 0.0f, 0.0f, now);
        now += ORDER_SECONDS;
        CHECK(!MaySearch(order, 77, now - 1));
        now += RETRY_SECONDS;
    }
    // Three steps for this attacker inside the window: no fourth until the window is over.
    CHECK(!MaySearch(order, 77, now));
    // Another attacker has its own budget.
    CHECK(MaySearch(order, 78, now));
    CHECK(MaySearch(order, 77, 2000 + STEP_WINDOW_SECONDS));
}

TEST(floor_attacker_remembers_the_last_spot_of_the_same_attacker_only)
{
    Order order;
    float x = 0.0f, y = 0.0f;
    CHECK(!PreviousSpot(order, 77, 1000, x, y));
    Begin(order, 77, 4.0f, 5.0f, 0.0f, 1000);
    CHECK(PreviousSpot(order, 77, 1001, x, y));
    CHECK_NEAR(x, 4.0, 0.001);
    CHECK_NEAR(y, 5.0, 0.001);
    CHECK(!PreviousSpot(order, 78, 1001, x, y));
    CHECK(!PreviousSpot(order, 77, 1000 + STEP_WINDOW_SECONDS, x, y));
}

TEST(floor_attacker_limits_fit_the_rescue_policy)
{
    // The step starts at a give-up and must end before the rescue forgets the give-up (IgnoreSeconds).
    CHECK(ORDER_SECONDS < TankRescuePolicy::IgnoreSeconds);
    CHECK(MAX_BOT_ATTACKER_YARDS == TankRescuePolicy::TooFarYards);
}
