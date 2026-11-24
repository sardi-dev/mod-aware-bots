#include "TestHarness.h"
#include "LedgeWalkOffPolicy.h"

#include <cmath>

using namespace LedgeWalkOffPolicy;

TEST(ledge_walk_off_candidates_lie_on_their_rings)
{
    for (unsigned i = 0; i < CANDIDATE_COUNT; ++i)
    {
        float dx = 0.0f, dy = 0.0f;
        Offset(i, dx, dy);
        CHECK_NEAR(std::sqrt(dx * dx + dy * dy), RING_YARDS[i / DIRECTIONS], 0.01);
    }
}

TEST(ledge_walk_off_says_once_after_a_minute_stuck)
{
    Episode episode;
    CHECK(!ShouldSay(59, episode));
    CHECK(ShouldSay(60, episode));
    episode.said = true;
    CHECK(!ShouldSay(300, episode));
}

TEST(ledge_walk_off_landing_must_be_clear_and_within_the_drop_bounds)
{
    Candidate c;
    c.clear = true;
    Episode episode;
    c.drop = 16.0f;
    CHECK(Eligible(c, episode));
    c.drop = MAX_DROP_YARDS + 0.1f;
    CHECK(!Eligible(c, episode));
    c.drop = -MAX_RISE_YARDS - 0.1f;
    CHECK(!Eligible(c, episode));
    c.drop = 0.0f;
    c.clear = false;
    CHECK(!Eligible(c, episode));
}

TEST(ledge_walk_off_prefers_the_near_landing_and_charges_for_the_drop)
{
    Candidate list[4];
    for (Candidate& c : list)
        c.clear = true;
    // Ashbury: the courtyard is 16 yd below the wall walk; a landing on the next stair 6 yd away is cheaper than 5 yd
    // out over the drop, and a far dry landing loses to both.
    list[0].flat = 5.0f;
    list[0].drop = 16.0f;
    list[1].flat = 7.0f;
    list[1].drop = 2.0f;
    list[2].flat = 12.0f;
    list[2].drop = 0.0f;
    list[3].flat = 3.0f;
    list[3].drop = 30.0f;
    unsigned out[PATH_SEARCHES] = {};
    Episode episode;
    unsigned const found = Shortlist(list, 4, episode, out);
    CHECK(found == 3);
    CHECK(out[0] == 1);
    CHECK(out[1] == 0);
    CHECK(out[2] == 2);
}

TEST(ledge_walk_off_retry_skips_the_last_landing)
{
    Candidate list[2];
    for (Candidate& c : list)
    {
        c.clear = true;
        c.drop = 10.0f;
    }
    list[0].x = 10.0f;
    list[0].flat = 3.0f;
    list[1].x = 20.0f;
    list[1].flat = 5.0f;
    unsigned out[PATH_SEARCHES] = {};
    Episode episode;
    episode.hasLast = true;
    episode.lastX = 11.0f;
    episode.lastY = 0.0f;
    CHECK(Shortlist(list, 2, episode, out) == 1);
    CHECK(out[0] == 1);
}

TEST(ledge_walk_off_never_jumps_below_half_health)
{
    CHECK(!HealthOk(49.9f));
    CHECK(HealthOk(50.0f));
}

TEST(ledge_walk_off_tries_five_times_twenty_seconds_apart_after_a_minute)
{
    Episode episode;
    CHECK(!MayWalkOff(59, episode, 1000));
    CHECK(MayWalkOff(60, episode, 1000));
    NoteAttempt(episode, 1000);
    CHECK(!MayWalkOff(70, episode, 1010));
    CHECK(MayWalkOff(80, episode, 1020));
    NoteAttempt(episode, 1020);
    for (unsigned i = 2; i < MAX_ATTEMPTS; ++i)
        NoteAttempt(episode, 1000 + 20 * i);
    CHECK(episode.attempts == MAX_ATTEMPTS);
    CHECK(!MayWalkOff(500, episode, 2000));
}

TEST(ledge_walk_off_waits_for_the_landing_then_starts_the_clock_afresh)
{
    Episode episode;
    NoteAttempt(episode, 1000);
    NoteJump(episode, 3.0f, 4.0f, 1000);
    CHECK(!MayWalkOff(120, episode, 1030));
    bool landed = true;
    CHECK(Settling(episode, 1000 + SETTLE_SECONDS - 1, landed));
    CHECK(!landed);
    CHECK(!Settling(episode, 1000 + SETTLE_SECONDS, landed));
    CHECK(landed);
    CHECK(!Settling(episode, 1000 + SETTLE_SECONDS + 1, landed));
    CHECK(!landed);
    CHECK(episode.hasLast);
}

// Shadowfang Keep proof run 2026-11-23: on the Ashbury wall walk the cheapest spots by flat distance were on the walk itself
// (no drop), so the three path searches of each try went to spots with no path, and the floor 16 yd below was never read.
TEST(ledge_walk_off_real_drops_come_before_spots_on_the_bots_own_floor)
{
    Candidate list[5];
    for (Candidate& c : list)
        c.clear = true;
    // Four spots along the walk, 3 to 7 yd out, and one landing on the floor 12 yd out and 16 yd down (cost 20).
    for (unsigned i = 0; i < 4; ++i)
    {
        list[i].flat = 3.0f + float(i);
        list[i].drop = 0.2f;
    }
    list[4].flat = 12.0f;
    list[4].drop = 16.0f;
    Episode episode;
    unsigned out[PATH_SEARCHES] = {};
    CHECK(Shortlist(list, 5, episode, out) == 3);
    CHECK(out[0] == 4);
    CHECK(out[1] == 0);
}

TEST(ledge_walk_off_own_floor_spots_are_kept_for_a_bot_off_the_mesh)
{
    Candidate list[1];
    list[0].clear = true;
    list[0].flat = 3.0f;
    list[0].drop = 0.0f;
    Episode episode;
    unsigned out[PATH_SEARCHES] = {};
    CHECK(Shortlist(list, 1, episode, out) == 1);
}

TEST(ledge_walk_off_a_retry_searches_spots_the_last_try_did_not)
{
    // Six landings 10 yd apart, all the same cost class: every try takes the three cheapest not yet searched.
    Candidate list[6];
    for (unsigned i = 0; i < 6; ++i)
    {
        list[i].clear = true;
        list[i].drop = 10.0f;
        list[i].x = 10.0f * float(i);
        list[i].flat = 3.0f + float(i);
    }
    Episode episode;
    unsigned out[PATH_SEARCHES] = {};
    unsigned found = Shortlist(list, 6, episode, out);
    CHECK(found == 3);
    CHECK(out[0] == 0 && out[1] == 1 && out[2] == 2);
    for (unsigned k = 0; k < found; ++k)
        NoteSearched(episode, list[out[k]].x, list[out[k]].y);

    found = Shortlist(list, 6, episode, out);
    CHECK(found == 3);
    CHECK(out[0] == 3 && out[1] == 4 && out[2] == 5);
    for (unsigned k = 0; k < found; ++k)
        NoteSearched(episode, list[out[k]].x, list[out[k]].y);

    CHECK(Shortlist(list, 6, episode, out) == 0);
}

TEST(ledge_walk_off_remembers_a_searched_spot_within_four_yards)
{
    Episode episode;
    NoteSearched(episode, 10.0f, 10.0f);
    CHECK(NearSearched(episode, 12.0f, 10.0f));
    CHECK(!NearSearched(episode, 14.1f, 10.0f));
}

TEST(ledge_walk_off_the_searched_list_never_overflows)
{
    Episode episode;
    for (unsigned i = 0; i < SEARCHED_CAPACITY + 5; ++i)
        NoteSearched(episode, 100.0f * float(i), 0.0f);
    CHECK(episode.searchedCount == SEARCHED_CAPACITY);
    CHECK(SEARCHED_CAPACITY >= MAX_ATTEMPTS * PATH_SEARCHES);
}

// Shadowfang Keep 2026-11-23: the bot stood on the Ashbury wall walk with no path, the leader 16 yd below and 40 to 63 yd away;
// the 12 yd rings found no landing that had a path to the leader.
TEST(ledge_walk_off_wide_search_needs_no_path_and_a_leader_far_below)
{
    CHECK(WantsLeaderFloor(true, 16.0f));
    CHECK(WantsLeaderFloor(true, LEADER_BELOW_YARDS + 0.1f));
    CHECK(!WantsLeaderFloor(true, LEADER_BELOW_YARDS));
    CHECK(!WantsLeaderFloor(true, -5.0f));
    CHECK(!WantsLeaderFloor(false, 16.0f));
}

TEST(ledge_walk_off_wide_search_reads_rings_out_to_thirty_yards)
{
    CHECK(CandidateCount(false) == NEAR_RINGS * DIRECTIONS);
    CHECK(CandidateCount(true) == CANDIDATE_COUNT);
    CHECK_NEAR(RING_YARDS[NEAR_RINGS - 1], 12.0f, 0.01);
    CHECK_NEAR(RING_YARDS[RINGS - 1], 30.0f, 0.01);
    float dx = 0.0f, dy = 0.0f;
    Offset(CANDIDATE_COUNT - DIRECTIONS, dx, dy);
    CHECK_NEAR(std::sqrt(dx * dx + dy * dy), 30.0f, 0.01);
}

TEST(ledge_walk_off_wide_search_allows_a_longer_drop)
{
    Candidate c;
    c.clear = true;
    c.drop = 25.0f;
    Episode narrow;
    CHECK(!Eligible(c, narrow));
    Episode wide;
    wide.wide = true;
    CHECK(Eligible(c, wide));
    c.drop = WIDE_MAX_DROP_YARDS + 0.1f;
    CHECK(!Eligible(c, wide));
}

TEST(ledge_walk_off_a_landing_on_the_leaders_floor_comes_first)
{
    CHECK(OnLeaderFloor(80.0f, 80.0f));
    CHECK(OnLeaderFloor(83.9f, 80.0f));
    CHECK(OnLeaderFloor(76.1f, 80.0f));
    CHECK(!OnLeaderFloor(84.1f, 80.0f));
    CHECK(!OnLeaderFloor(75.9f, 80.0f));

    Candidate list[3];
    for (Candidate& c : list)
        c.clear = true;
    // A stair 5 yd out and 6 yd down, the wall walk's own floor, and the leader's floor 30 yd out and 16 yd down.
    list[0].flat = 5.0f;
    list[0].drop = 6.0f;
    list[1].flat = 3.0f;
    list[1].drop = 0.0f;
    list[2].flat = 30.0f;
    list[2].drop = 16.0f;
    list[2].onLeaderFloor = true;
    unsigned out[PATH_SEARCHES] = {};
    Episode episode;
    episode.wide = true;
    CHECK(Shortlist(list, 3, episode, out) == 3);
    CHECK(out[0] == 2);
    CHECK(out[1] == 0);
}
