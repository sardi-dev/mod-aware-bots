#include "TestHarness.h"
#include "CrossMapTripPolicy.h"

#include <string>
#include <vector>

using namespace CrossMapTripPolicy;

TEST(cross_map_hearth_needs_the_stone_no_cooldown_and_the_goal_continent)
{
    // Kalimdor (1) to the Eastern Kingdoms (0), bound in Undercity: the hearth is a leg.
    CHECK(HearthLegUsable(true, true, false, 0, 1, 0));
    // Bound on a third continent, or on the bot's own: no hearth leg.
    CHECK(!HearthLegUsable(true, true, false, 530, 1, 0));
    CHECK(!HearthLegUsable(true, true, false, 1, 1, 0));
    // No stone, on cooldown, or dead: route without it.
    CHECK(!HearthLegUsable(true, false, false, 0, 1, 0));
    CHECK(!HearthLegUsable(true, true, true, 0, 1, 0));
    CHECK(!HearthLegUsable(false, true, false, 0, 1, 0));
}

TEST(cross_map_hearth_leg_qualifies_only_for_another_continent)
{
    // The 2026-11-18 log: "points=4 transport=0 flight=0 portal=0 hearth=1" to another map was refused as no-leg.
    CHECK(QualifyingLegs(0, 0, 0, 1, true) == 1);
    CHECK(QualifyingLegs(0, 0, 0, 1, false) == 0);
    CHECK(QualifyingLegs(1, 2, 1, 0, false) == 4);
    CHECK(QualifyingLegs(1, 0, 0, 1, true) == 2);
}

TEST(cross_map_hearth_cast_waits_retries_and_gives_up)
{
    CHECK(NextHearthStep(false, false, false, 0, 0) == HearthStep::Cast);
    // Casting, or the cast just ended and the teleport is about to come.
    CHECK(NextHearthStep(true, false, true, 9000, 1) == HearthStep::Wait);
    CHECK(NextHearthStep(true, false, false, HEARTH_SETTLE_MS - 1, 1) == HearthStep::Wait);
    // Landed at the bind.
    CHECK(NextHearthStep(true, true, false, 12000, 1) == HearthStep::Arrived);
    CHECK(NextHearthStep(false, true, false, 0, 0) == HearthStep::Arrived);
    // Not casting and not landed: refused or interrupted, so cast again until the tries run out.
    CHECK(NextHearthStep(true, false, false, HEARTH_SETTLE_MS, 1) == HearthStep::Cast);
    CHECK(NextHearthStep(true, false, false, HEARTH_SETTLE_MS, HEARTH_MAX_TRIES) == HearthStep::GiveUp);
    CHECK(NextHearthStep(false, false, false, 0, HEARTH_MAX_TRIES) == HearthStep::GiveUp);
}

TEST(cross_map_start_candidates_skip_nodes_the_bot_cannot_walk_to)
{
    // Nearest first: 40 yd (walked as before), 191 yd unreachable (another level), 230 yd reachable, 260 yd reachable.
    std::vector<StartCandidate> const c = {{40.0f, false}, {191.0f, false}, {230.0f, true}, {260.0f, true}};
    std::vector<size_t> const picked = PickStartCandidates(c, 80.0f, 5);
    CHECK(picked.size() == 3);
    CHECK(picked[0] == 0);
    CHECK(picked[1] == 2);
    CHECK(picked[2] == 3);
    // At most `keep` of them.
    CHECK(PickStartCandidates(c, 80.0f, 2).size() == 2);
}

TEST(cross_map_start_candidates_keep_the_nearest_when_none_is_walkable)
{
    // GM Island, 2026-11-18: the nearest node 15,000 yd away. Only a hearth route can start here; the nearest node
    // still seeds it.
    std::vector<StartCandidate> const c = {{15040.0f, false}, {15057.0f, false}};
    std::vector<size_t> const picked = PickStartCandidates(c, 80.0f, 5);
    CHECK(picked.size() == 1);
    CHECK(picked[0] == 0);
    CHECK(PickStartCandidates({}, 80.0f, 5).empty());
}

TEST(cross_map_precheck_memo_keeps_a_yes_briefly_and_a_no_for_the_gate)
{
    PlanMemo memo;
    CHECK(AskMemo(memo, 1000, 1, 0, 0, 0, 0) == MemoAnswer::Unknown);
    memo.valid = true;
    memo.ok = true;
    memo.atMs = 1000;
    memo.fromMap = 1;
    memo.destMap = 0;
    // A yes holds for a minute on the spot, for that continent only.
    CHECK(AskMemo(memo, 1000 + PLAN_OK_TTL_MS - 1, 1, 3, 4, 0, 0) == MemoAnswer::Yes);
    CHECK(AskMemo(memo, 1000 + PLAN_OK_TTL_MS, 1, 0, 0, 0, 0) == MemoAnswer::Unknown);
    CHECK(AskMemo(memo, 2000, 1, PLAN_OK_RADIUS, 0, 0, 0) == MemoAnswer::Unknown);
    CHECK(AskMemo(memo, 2000, 1, 0, 0, 0, 530) == MemoAnswer::Unknown);
    CHECK(AskMemo(memo, 2000, 0, 0, 0, 0, 0) == MemoAnswer::Unknown);
    // A no holds for the retry gate's ten minutes while the bot stays in the area.
    memo.ok = false;
    CHECK(AskMemo(memo, 1000 + PLAN_FAIL_TTL_MS - 1, 1, 150, 0, 0, 0) == MemoAnswer::No);
    CHECK(AskMemo(memo, 1000 + PLAN_FAIL_TTL_MS, 1, 0, 0, 0, 0) == MemoAnswer::Unknown);
    CHECK(AskMemo(memo, 2000, 1, PLAN_FAIL_RADIUS, 0, 0, 0) == MemoAnswer::Unknown);
    // The clock wraps.
    memo.atMs = 0xFFFFF000u;
    CHECK(AskMemo(memo, 0x00000100u, 1, 0, 0, 0, 0) == MemoAnswer::No);
}

TEST(cross_map_precheck_respects_the_trip_retry_gate)
{
    uint32_t const gate = 10 * 60 * 1000;
    // Dylo, 2026-11-18: a second quest to the same continent 6 s after the first trip failed hit the gate.
    CHECK(RetryGateClosed(1000, 7000, gate, 0, 0));
    CHECK(!RetryGateClosed(1000, 7000, gate, 0, 530));
    CHECK(!RetryGateClosed(1000, 1000 + gate, gate, 0, 0));
    CHECK(!RetryGateClosed(0, 7000, gate, 0, 0));
}

TEST(cross_map_outcome_is_arrived_on_the_destination_continent_only)
{
    CHECK(EndOutcome(0, 0) == Outcome::Arrived);
    CHECK(EndOutcome(1, 0) == Outcome::GaveUp);
    CHECK(std::string(OutcomeName(Outcome::Arrived)) == "arrived");
    CHECK(std::string(OutcomeName(Outcome::GaveUp)) == "gave-up");
    CHECK(std::string(OutcomeName(Outcome::NoRoute)) == "noroute");
}

TEST(cross_map_noroute_outcome_skips_the_retry_gate_and_rides)
{
    CHECK(NoRouteIsOutcome("start-unreachable"));
    CHECK(NoRouteIsOutcome("empty"));
    CHECK(NoRouteIsOutcome("no-graph"));
    CHECK(!NoRouteIsOutcome("retry-gate"));
    CHECK(!NoRouteIsOutcome("busy"));
    CHECK(!NoRouteIsOutcome(nullptr));
}

TEST(cross_map_route_search_ends_on_the_goal_continent_for_a_roam_trip)
{
    // Stormwind (0) to Kalimdor (1): a walkable node in the Deeprun Tram (369) ends AC's search, not a roam trip's.
    CHECK(SearchMayEndOn(369, 0, 1, false));
    CHECK(!SearchMayEndOn(369, 0, 1, true));
    CHECK(SearchMayEndOn(1, 0, 1, true));
    CHECK(SearchMayEndOn(1, 0, 1, false));
    // Same map: only the goal node ends the search, for a roam trip and for AC's search alike.
    CHECK(!SearchMayEndOn(0, 0, 0, true));
    CHECK(!SearchMayEndOn(0, 0, 0, false));
}
