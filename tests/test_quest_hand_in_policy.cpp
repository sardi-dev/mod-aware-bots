#include "TestHarness.h"
#include "QuestHandInPolicy.h"

using namespace QuestHandInPolicy;

namespace
{
Spawn At(uint32_t spawnId, uint32_t map, float x, float y)
{
    Spawn s;
    s.spawnId = spawnId;
    s.entry = 1323;
    s.map = map;
    s.x = x;
    s.y = y;
    return s;
}
}  // namespace

TEST(hand_in_nearest_spawn_stays_on_the_bot_continent)
{
    std::vector<Spawn> spawns = {At(1, 1, 10.0f, 0.0f), At(2, 0, 900.0f, 0.0f), At(3, 0, -300.0f, 0.0f)};
    CHECK(NearestSpawn(spawns, 0, 0.0f, 0.0f) == 2);
    CHECK(NearestSpawn(spawns, 1, 0.0f, 0.0f) == 0);
    CHECK(NearestSpawn(spawns, 530, 0.0f, 0.0f) == -1);
    CHECK(NearestSpawn({}, 0, 0.0f, 0.0f) == -1);
}

TEST(hand_in_pick_is_the_nearest_ender_then_the_lower_quest)
{
    std::vector<Candidate> candidates(3);
    candidates[0].questId = 301;
    candidates[0].dist = 1800.0f;
    candidates[1].questId = 26394;
    candidates[1].dist = 1700.0f;
    candidates[2].questId = 6391;
    candidates[2].dist = 1700.0f;
    CHECK(PickCandidate(candidates) == 2);
    CHECK(PickCandidate({}) == -1);
}

TEST(hand_in_budget_grows_with_distance_within_bounds)
{
    CHECK(BudgetMs(0.0f) == MIN_BUDGET_MS);
    CHECK(BudgetMs(-5.0f) == MIN_BUDGET_MS);
    // Goldshire to the Stormwind ender, about 1,700 yd: 5 min plus 680 s.
    CHECK(BudgetMs(1700.0f) == MIN_BUDGET_MS + 680000u);
    CHECK(BudgetMs(1000000.0f) == MAX_BUDGET_MS);
    CHECK(BudgetMs(3000.0f) > BudgetMs(2000.0f));
}

TEST(hand_in_same_dest_only_for_the_active_trip_ender)
{
    Candidate c;
    c.questId = 26394;
    c.spawn = At(7, 0, -8702.0f, 382.0f);
    c.dist = 1700.0f;
    Trip trip = Start(c, 1519, 1000);
    CHECK(trip.active && trip.questId == 26394 && trip.zone == 1519);
    CHECK(SameDest(trip, 0, -8702.3f, 382.4f));
    CHECK(!SameDest(trip, 0, -8705.0f, 382.0f));
    CHECK(!SameDest(trip, 1, -8702.0f, 382.0f));
    Trip none;
    CHECK(!SameDest(none, 0, 0.0f, 0.0f));
}

TEST(hand_in_judge_travels_waits_and_gives_up)
{
    Candidate c;
    c.questId = 301;
    c.spawn = At(9, 0, 0.0f, 0.0f);
    c.dist = 0.0f;
    Trip trip = Start(c, 1537, 5000);
    CHECK(Judge(trip, 6000, 400.0f) == Step::Travel);
    CHECK(trip.arrivedMs == 0);
    CHECK(Judge(trip, 7000, 8.0f) == Step::Wait);
    CHECK(trip.arrivedMs == 7000);
    // Walking up to the ender from the spawn point keeps the arrival clock.
    CHECK(Judge(trip, 7000 + ARRIVAL_WAIT_MS - 1, 40.0f) == Step::Wait);
    CHECK(Judge(trip, 7000 + ARRIVAL_WAIT_MS, 5.0f) == Step::NotThere);
}

TEST(hand_in_judge_expires_and_survives_the_clock_wrap)
{
    Candidate c;
    c.questId = 6391;
    c.spawn = At(4, 0, 0.0f, 0.0f);
    c.dist = 0.0f;
    Trip trip = Start(c, 1, 0xfffff000u);
    CHECK(!Expired(trip, 0xfffff000u + 1000u));
    CHECK(Judge(trip, 100, 500.0f) == Step::Travel);
    CHECK(Expired(trip, 0xfffff000u + MIN_BUDGET_MS));
    CHECK(Judge(trip, 0xfffff000u + MIN_BUDGET_MS, 500.0f) == Step::Expired);
    // An arrival exactly at tick 0 still starts the arrival clock.
    Trip at0 = Start(c, 1, 0xffffff00u);
    CHECK(Judge(at0, 0, 1.0f) == Step::Wait);
    CHECK(at0.arrivedMs == 1);
    Trip idle;
    CHECK(!Expired(idle, 123456));
}

TEST(hand_in_far_trip_only_for_a_quest_at_level)
{
    CHECK(WorthTheTrip(5000.0f, false));
    CHECK(WorthTheTrip(ROUTE_DIST - 1.0f, true));
    CHECK(!WorthTheTrip(ROUTE_DIST, true));
    CHECK(!WorthTheTrip(12773.0f, true));
}

TEST(hand_in_trip_pauses_while_the_bot_is_away)
{
    // The proof-run pattern of 2026-11-17: a trip of 725 yd (590 s budget) starts, the bot is called into a dungeon for
    // 15 active minutes and comes back; the trip goes on with its budget instead of failing "out of time".
    QuestHandInPolicy::Candidate c;
    c.questId = 13974;
    c.dist = 725.0f;
    QuestHandInPolicy::Trip trip = QuestHandInPolicy::Start(c, 331, 1000);
    QuestHandInPolicy::Resume(trip, 31000);
    CHECK(trip.startMs == 1000);
    uint32_t const back = 31000 + 15 * 60 * 1000;
    QuestHandInPolicy::Resume(trip, back);
    CHECK(trip.startMs == 1000 + 15 * 60 * 1000);
    CHECK(!QuestHandInPolicy::Expired(trip, back));
    CHECK(QuestHandInPolicy::Expired(trip, back + trip.budgetMs));
}

TEST(hand_in_trip_short_gaps_still_count)
{
    QuestHandInPolicy::Candidate c;
    c.dist = 100.0f;
    QuestHandInPolicy::Trip trip = QuestHandInPolicy::Start(c, 1, 5000);
    QuestHandInPolicy::Resume(trip, 5000 + QuestHandInPolicy::PAUSE_MS);
    CHECK(trip.startMs == 5000);
    trip.arrivedMs = 9000;
    QuestHandInPolicy::Resume(trip, 5000 + 3 * QuestHandInPolicy::PAUSE_MS);
    CHECK(trip.startMs == 5000 + 2 * QuestHandInPolicy::PAUSE_MS);
    CHECK(trip.arrivedMs == 9000 + 2 * QuestHandInPolicy::PAUSE_MS);
    QuestHandInPolicy::Trip idle;
    QuestHandInPolicy::Resume(idle, 99999);
    CHECK(!idle.active && idle.workedMs == 0);
}

TEST(hand_in_cross_map_trip_takes_the_lowest_quest_and_the_long_budget)
{
    std::vector<QuestHandInPolicy::Candidate> candidates(3);
    candidates[0].questId = 28708;
    candidates[1].questId = 9416;
    candidates[2].questId = 28630;
    CHECK(QuestHandInPolicy::PickCrossMap(candidates) == 1);
    CHECK(QuestHandInPolicy::PickCrossMap({}) == -1);

    candidates[1].dist = -1.0f;
    candidates[1].spawn.map = 1;
    QuestHandInPolicy::Trip const trip = QuestHandInPolicy::StartCrossMap(candidates[1], 1377, 4000);
    CHECK(trip.active && trip.crossMap);
    CHECK(trip.budgetMs == QuestHandInPolicy::CROSS_MAP_BUDGET_MS);
    CHECK(trip.ender.map == 1 && trip.questId == 9416 && trip.workedMs == 4000);
    CHECK(!QuestHandInPolicy::Start(candidates[0], 1, 4000).crossMap);
}

TEST(hand_in_cross_map_trip_takes_the_first_ender_the_route_test_accepts)
{
    std::vector<Spawn> spawns = {At(1, 530, 0.0f, 0.0f), At(2, 1, 5.0f, 5.0f), At(3, 1, 9.0f, 9.0f)};
    auto onKalimdor = [](Spawn const& s) { return s.map == 1; };
    CHECK(FirstReachable(spawns, onKalimdor) == 1);
    CHECK(FirstReachable(spawns, [](Spawn const&) { return false; }) == -1);
    CHECK(FirstReachable({}, onKalimdor) == -1);
}
