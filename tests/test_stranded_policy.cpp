#include "TestHarness.h"
#include "StrandedPolicy.h"

#include <cmath>

using namespace StrandedPolicy;

namespace
{
// Reads the bot as closed every READ_GAP_MS from `from` up to and including `to` (quest clock, ms).
void ReadClosed(State& state, uint32_t from, uint32_t to)
{
    for (uint32_t t = from; t <= to; t += READ_GAP_MS)
        Observe(state, Reading::Closed, 941, t);
}
}  // namespace

TEST(stranded_flood_reading_classes)
{
    // The Thousand Needles barge: a complete flood of 941 polygons is a closed pocket.
    CHECK(Classify(true, true, 941) == Reading::Closed);
    // A flood that overflowed the buffer or the node pool saw the open mesh (or too much to say).
    CHECK(Classify(true, false, 941) == Reading::Open);
    CHECK(Classify(true, true, PROBE_MAX_POLYS) == Reading::Open);
    CHECK(Classify(true, false, PROBE_MAX_POLYS) == Reading::Open);
    // No polygon under the bot: not judged.
    CHECK(Classify(false, true, 0) == Reading::Skip);
    // The barge fits the buffer with room to spare.
    CHECK(941u < PROBE_MAX_POLYS);
}

TEST(stranded_clock_starts_at_the_first_closed_read)
{
    State state;
    Observe(state, Reading::Closed, 941, 50000);
    CHECK(state.closedSinceMs == 50000);
    CHECK(StuckFor(state, 50000) == 0);
    CHECK(!Stuck(state, 50000 + STUCK_MS - 1, STUCK_MS));
    CHECK(Stuck(state, 50000 + STUCK_MS, STUCK_MS));
}

TEST(stranded_any_open_or_skipped_read_ends_the_episode)
{
    State state;
    ReadClosed(state, 10000, 100000);
    state.getOutTries = 2;
    Observe(state, Reading::Open, 5000, 110000);
    CHECK(state.closedSinceMs == 0);
    CHECK(state.getOutTries == 0);
    CHECK(!Stuck(state, 400000, STUCK_MS));

    ReadClosed(state, 120000, 150000);
    CHECK(state.closedSinceMs == 120000);
    Observe(state, Reading::Skip, 0, 160000);
    CHECK(state.closedSinceMs == 0);
}

TEST(stranded_a_long_gap_between_reads_starts_over)
{
    State state;
    ReadClosed(state, 10000, 60000);
    state.getOutTries = 3;
    Observe(state, Reading::Closed, 941, 60000 + READ_RESET_MS + 1);
    CHECK(state.closedSinceMs == 60000 + READ_RESET_MS + 1);
    CHECK(state.getOutTries == 0);
}

TEST(stranded_nothing_before_two_minutes)
{
    State state;
    ReadClosed(state, 10000, 10000 + STUCK_MS - READ_GAP_MS);
    CHECK(Decide(state, 10000 + STUCK_MS - READ_GAP_MS, 0, true, STUCK_MS) == Step::None);
    ReadClosed(state, 10000 + STUCK_MS, 10000 + STUCK_MS);
    CHECK(Decide(state, 10000 + STUCK_MS, 0, true, STUCK_MS) == Step::GetOut);
}

TEST(stranded_walks_off_before_it_uses_the_hearthstone)
{
    State state;
    uint32_t now = 10000;
    ReadClosed(state, now, now + STUCK_MS);
    now += STUCK_MS;
    for (unsigned i = 0; i < MAX_GET_OUT_TRIES; ++i)
    {
        CHECK(Decide(state, now, 0, true, STUCK_MS) == Step::GetOut);
        NoteGetOut(state, Outcome::Walking, 0.0f, 0.0f, now);
        // The next step waits for the gap.
        CHECK(Decide(state, now + 1, 0, true, STUCK_MS) == Step::None);
        now += STEP_GAP_MS;
        ReadClosed(state, now - STEP_GAP_MS + READ_GAP_MS, now);
    }
    CHECK(GetOutExhausted(state));
    CHECK(Decide(state, now, 0, true, STUCK_MS) == Step::Hearth);
}

TEST(stranded_no_landing_goes_to_the_hearthstone_after_the_side_step)
{
    State state;
    uint32_t now = 10000;
    ReadClosed(state, now, now + STUCK_MS);
    now += STUCK_MS;
    CHECK(Decide(state, now, 0, true, STUCK_MS) == Step::GetOut);
    NoteGetOut(state, Outcome::NoLanding, 0.0f, 0.0f, now);
    CHECK(state.noLanding);
    // The side step gets its gap before the stone is cast.
    CHECK(Decide(state, now + 1, 0, true, STUCK_MS) == Step::None);
    now += STEP_GAP_MS;
    ReadClosed(state, now - STEP_GAP_MS + READ_GAP_MS, now);
    CHECK(Decide(state, now, 0, true, STUCK_MS) == Step::Hearth);
}

TEST(stranded_a_jump_gets_time_to_land_and_remembers_its_spot)
{
    State state;
    uint32_t now = 10000;
    ReadClosed(state, now, now + STUCK_MS);
    now += STUCK_MS;
    NoteGetOut(state, Outcome::Jumped, 12.0f, -30.0f, now);
    CHECK(state.hasLast);
    CHECK_NEAR(state.lastX, 12.0, 0.001);
    CHECK_NEAR(state.lastY, -30.0, 0.001);
    CHECK(Decide(state, now + JUMP_SETTLE_MS - 1, 0, true, STUCK_MS) == Step::None);
    // Settled and the gap is over: the bot is still closed, so it tries again.
    ReadClosed(state, now + READ_GAP_MS, now + STEP_GAP_MS);
    CHECK(Decide(state, now + STEP_GAP_MS, 0, true, STUCK_MS) == Step::GetOut);
}

TEST(stranded_hearthstone_cast_is_waited_for_and_tried_twice)
{
    State state;
    uint32_t now = 10000;
    ReadClosed(state, now, now + STUCK_MS);
    now += STUCK_MS;
    state.noLanding = true;
    CHECK(Decide(state, now, 500, true, STUCK_MS) == Step::Hearth);
    NoteHearth(state, 500);
    CHECK(state.hearthTries == 1);
    // Casting (10 s) and landing: nothing else runs meanwhile.
    CHECK(Decide(state, now, 500 + HEARTH_WAIT_MS - 1, true, STUCK_MS) == Step::None);
    // The cast was interrupted: a second one.
    CHECK(Decide(state, now, 500 + HEARTH_WAIT_MS, true, STUCK_MS) == Step::Hearth);
    NoteHearth(state, 500 + HEARTH_WAIT_MS);
    CHECK(state.hearthTries == HEARTH_MAX_TRIES);
    CHECK(Decide(state, now, 500 + 2 * HEARTH_WAIT_MS, true, STUCK_MS) == Step::NoHearth);
}

TEST(stranded_a_bot_that_lands_on_the_open_mesh_starts_clean)
{
    State state;
    uint32_t now = 10000;
    ReadClosed(state, now, now + STUCK_MS);
    NoteGetOut(state, Outcome::Jumped, 1.0f, 2.0f, now + STUCK_MS);
    NoteHearth(state, 900);
    Observe(state, Reading::Open, 1000, now + STUCK_MS + READ_GAP_MS);
    CHECK(state.getOutTries == 0);
    CHECK(state.hearthTries == 0);
    CHECK(!state.hasLast);
    CHECK(Decide(state, now + 2 * STUCK_MS, 0, true, STUCK_MS) == Step::None);
}

TEST(stranded_hearthstone_on_cooldown_leaves_it_to_the_unseen_teleport_rules)
{
    State state;
    uint32_t now = 10000;
    ReadClosed(state, now, now + STUCK_MS);
    now += STUCK_MS;
    state.noLanding = true;
    CHECK(Decide(state, now, 0, false, STUCK_MS) == Step::NoHearth);
    // The line is written once per episode.
    CHECK(NoteNoHearth(state, now));
    CHECK(Decide(state, now + 1, 0, false, STUCK_MS) == Step::None);
    // It looks again a minute later; the stone is still not ready, and the line is not written twice.
    uint32_t later = now + NO_HEARTH_RETRY_MS;
    ReadClosed(state, now + READ_GAP_MS, later);
    CHECK(Decide(state, later, 0, false, STUCK_MS) == Step::NoHearth);
    CHECK(!NoteNoHearth(state, later));
    // Once the stone is ready the bot casts it.
    uint32_t const ready = later + NO_HEARTH_RETRY_MS;
    ReadClosed(state, later + READ_GAP_MS, ready);
    CHECK(Decide(state, ready, 0, true, STUCK_MS) == Step::Hearth);
}

TEST(stranded_steps_on_foot_start_again_after_the_cycle)
{
    State state;
    uint32_t now = 10000;
    ReadClosed(state, now, now + STUCK_MS);
    now += STUCK_MS;
    for (unsigned i = 0; i < MAX_GET_OUT_TRIES; ++i)
        NoteGetOut(state, Outcome::Failed, 0.0f, 0.0f, now);
    CHECK(GetOutExhausted(state));
    uint32_t const exhausted = state.exhaustedAtMs;
    CHECK(exhausted == now);
    NoteNoHearth(state, now + GET_OUT_CYCLE_MS - 1);
    CHECK(GetOutExhausted(state));
    NoteNoHearth(state, now + GET_OUT_CYCLE_MS);
    CHECK(!GetOutExhausted(state));
    CHECK(state.getOutTries == 0);
}

TEST(stranded_candidates_lie_on_their_rings)
{
    for (unsigned i = 0; i < CANDIDATE_COUNT; ++i)
    {
        float dx = 0.0f, dy = 0.0f;
        Offset(i, dx, dy);
        CHECK_NEAR(std::sqrt(dx * dx + dy * dy), RING_YARDS[i / DIRECTIONS], 0.01);
    }
}

TEST(stranded_landing_must_be_clear_and_within_the_drop_bounds)
{
    Candidate c;
    c.clear = true;
    c.drop = 6.2f;
    c.water = true;
    CHECK(Eligible(c, false, 0.0f, 0.0f));
    c.drop = MAX_DROP_YARDS + 0.1f;
    CHECK(!Eligible(c, false, 0.0f, 0.0f));
    c.drop = -MAX_RISE_YARDS - 0.1f;
    CHECK(!Eligible(c, false, 0.0f, 0.0f));
    c.drop = 6.2f;
    c.clear = false;
    CHECK(!Eligible(c, false, 0.0f, 0.0f));
}

TEST(stranded_landing_near_the_last_jump_is_skipped)
{
    Candidate c;
    c.clear = true;
    c.x = 10.0f;
    c.y = 10.0f;
    CHECK(!Eligible(c, true, 12.0f, 10.0f));
    CHECK(Eligible(c, true, 20.0f, 10.0f));
    CHECK(Eligible(c, false, 10.0f, 10.0f));
}

TEST(stranded_prefers_the_near_rail_and_charges_for_the_drop)
{
    Candidate list[5];
    for (Candidate& c : list)
        c.clear = true;
    // A deck 6 yd over the water: the water 14 yd out beats a landing 10 yd away that is 22 yd down, and the far
    // landing loses to both.
    list[0].flat = 14.0f;
    list[0].drop = 6.2f;
    list[0].water = true;
    list[1].flat = 10.0f;
    list[1].drop = 22.0f;
    list[2].flat = 40.0f;
    list[2].drop = 6.2f;
    list[2].water = true;
    list[3].flat = 7.0f;
    list[3].drop = 6.2f;
    list[3].clear = false;
    list[4].flat = 19.0f;
    list[4].drop = MAX_DROP_YARDS + 5.0f;
    unsigned picks[PROBES_PER_TRY];
    unsigned const found = Shortlist(list, 5, false, 0.0f, 0.0f, picks);
    CHECK(found == 3);
    CHECK(picks[0] == 0);
    CHECK(picks[1] == 1);
    CHECK(picks[2] == 2);
}

TEST(stranded_shortlist_keeps_only_the_best_few)
{
    Candidate list[CANDIDATE_COUNT];
    for (unsigned i = 0; i < CANDIDATE_COUNT; ++i)
    {
        list[i].clear = true;
        list[i].flat = float(CANDIDATE_COUNT - i);
    }
    unsigned picks[PROBES_PER_TRY];
    unsigned const found = Shortlist(list, CANDIDATE_COUNT, false, 0.0f, 0.0f, picks);
    CHECK(found == PROBES_PER_TRY);
    CHECK(picks[0] == CANDIDATE_COUNT - 1);
    CHECK(picks[1] == CANDIDATE_COUNT - 2);
}

TEST(stranded_jump_reach_splits_walk_from_jump)
{
    CHECK(InJumpReach(JUMP_REACH_YARDS));
    CHECK(!InJumpReach(JUMP_REACH_YARDS + 0.1f));
}
