#include "TestHarness.h"
#include "QuestInteractPolicy.h"

using namespace QuestInteractPolicy;

namespace
{
Spot At(uint32_t spawnId, uint32_t map, float x, float y, bool phased = false)
{
    Spot s;
    s.entry = 37080;
    s.spawnId = spawnId;
    s.map = map;
    s.x = x;
    s.y = y;
    s.phased = phased;
    return s;
}

ShownOption Option(uint32_t key, uint32_t type, uint32_t money = 0, bool coded = false)
{
    ShownOption o;
    o.key = key;
    o.optionType = type;
    o.boxMoney = money;
    o.coded = coded;
    return o;
}
}  // namespace

TEST(interact_only_where_a_kill_cannot_give_the_credit)
{
    // Kill Credit Bunny - Wounded Coldridge Mountaineer: never spawned.
    CreditFacts bunny;
    CHECK(NeedsInteraction(bunny, true));
    CHECK(!NeedsInteraction(bunny, false));
    // Northwatch Marine: spawned and hostile, the grind earns it.
    CreditFacts marine;
    marine.spawned = true;
    marine.attackable = true;
    CHECK(KillEarnsCredit(marine));
    CHECK(!NeedsInteraction(marine, true));
    // Frankal Invisible Trigger: spawned, friendly and a trigger.
    CreditFacts trigger;
    trigger.spawned = true;
    trigger.trigger = true;
    trigger.attackable = true;
    CHECK(NeedsInteraction(trigger, true));
    CreditFacts friendly;
    friendly.spawned = true;
    CHECK(NeedsInteraction(friendly, true));
}

TEST(interact_skips_vehicles_and_escorts)
{
    CHECK(Classify(false, false, false) == Skip::None);
    CHECK(Classify(true, false, false) == Skip::Vehicle);
    CHECK(Classify(false, true, true) == Skip::Vehicle);
    CHECK(Classify(false, false, true) == Skip::Escort);
}

TEST(interact_link_chain_climbs_to_the_player_event)
{
    // Wounded Coldridge Mountaineer: row 2 (spell hit) links to row 3 (link) which calls the action list.
    std::vector<SmartRow> rows = {{1, 0, 11}, {2, 3, EVENT_SPELLHIT}, {3, 0, EVENT_LINK}};
    CHECK(RootRow(rows, 2) == 1);
    CHECK(RootRow(rows, 1) == 1);
    // A link nobody points at is broken; a loop ends.
    std::vector<SmartRow> broken = {{1, 0, EVENT_LINK}};
    CHECK(RootRow(broken, 0) == -1);
    std::vector<SmartRow> loop = {{1, 2, EVENT_LINK}, {2, 1, EVENT_LINK}};
    CHECK(RootRow(loop, 0) == -1);
    CHECK(RootRow(rows, 9) == -1);
}

TEST(interact_events_map_to_player_actions)
{
    Way way = Way::Click;
    CHECK(WayForEvent(EVENT_GOSSIP_SELECT, false, way) && way == Way::Gossip);
    CHECK(WayForEvent(EVENT_GOSSIP_HELLO, true, way) && way == Way::OpenObject);
    CHECK(WayForEvent(EVENT_ON_SPELLCLICK, false, way) && way == Way::Click);
    CHECK(WayForEvent(EVENT_GO_STATE_CHANGED, true, way) && way == Way::OpenObject);
    CHECK(!WayForEvent(EVENT_GO_STATE_CHANGED, false, way));
    CHECK(!WayForEvent(EVENT_ON_SPELLCLICK, true, way));
    // A death or a timer is no player action; a spell hit needs the item lookup.
    CHECK(!WayForEvent(6, false, way));
    CHECK(!WayForEvent(EVENT_SPELLHIT, false, way));
}

TEST(interact_gossip_route_takes_the_shortest_walk)
{
    std::vector<MenuEdge> edges = {{100, 0, 200}, {100, 1, 300}, {200, 0, 400}, {300, 2, 400}, {400, 5, 0}};
    std::vector<GossipStep> route = GossipRoute(100, 400, 5, edges);
    CHECK(route.size() == 3);
    CHECK(route[0].menu == 100 && route[0].option == 0);
    CHECK(route[1].menu == 200 && route[1].option == 0);
    CHECK(route[2].menu == 400 && route[2].option == 5);
    // The option is on the first menu.
    std::vector<GossipStep> direct = GossipRoute(400, 400, 5, edges);
    CHECK(direct.size() == 1 && direct[0].option == 5);
    // A menu only a script opens is not reachable.
    CHECK(GossipRoute(100, 999, 1, edges).empty());
    CHECK(GossipRoute(0, 400, 5, edges).empty());
}

TEST(interact_gossip_route_is_bounded)
{
    std::vector<MenuEdge> chain;
    for (uint32_t m = 1; m < 10; ++m)
        chain.push_back({m, 0, m + 1});
    CHECK(!GossipRoute(1, 3, 0, chain).empty());
    CHECK(GossipRoute(1, 9, 0, chain).empty());
}

TEST(interact_route_option_needs_the_page_and_the_offer)
{
    std::vector<GossipStep> route = {{100, 0}, {400, 5}};
    std::vector<ShownOption> page = {Option(0, GOSSIP_OPTION_GOSSIP), Option(1, 3)};
    CHECK(PickRouteOption(route, 100, page) == 0);
    CHECK(PickRouteOption(route, 400, page) == -1);
    std::vector<ShownOption> last = {Option(5, GOSSIP_OPTION_GOSSIP)};
    CHECK(PickRouteOption(route, 400, last) == 5);
    CHECK(PickRouteOption(route, 777, last) == -1);
}

TEST(interact_chat_option_never_buys_or_trains)
{
    // Rutgar's chain: script actions from 1000.
    std::vector<ShownOption> rutgar = {Option(0, SCRIPT_ACTION_BASE + 1)};
    CHECK(PickChatOption(rutgar) == 0);
    // Vendor (3) and trainer (5) options, a paid option and a coded one are passed over.
    std::vector<ShownOption> services = {Option(0, 3), Option(1, 5), Option(2, GOSSIP_OPTION_GOSSIP, 100),
                                         Option(3, GOSSIP_OPTION_GOSSIP, 0, true), Option(4, GOSSIP_OPTION_GOSSIP)};
    CHECK(PickChatOption(services) == 4);
    CHECK(PickChatOption({Option(0, 3)}) == -1);
}

TEST(interact_spot_prefers_near_and_unphased)
{
    std::vector<Spot> spots = {At(1, 0, 500, 0), At(2, 0, 100, 0, true), At(3, 1, 10, 0), At(4, 0, 350, 0)};
    float score = 0.0f;
    // Spot 2 is 100 yd away but phased: 400 against 350.
    CHECK(PickSpot(spots, 0, 0, 0, {}, &score) == 3);
    CHECK(score == 350.0f);
    CHECK(PickSpot(spots, 0, 0, 0, {4}) == 1);
    CHECK(PickSpot(spots, 1, 0, 0, {}) == 2);
    std::vector<Spot> far = {At(1, 0, 3500, 0)};
    CHECK(PickSpot(far, 0, 0, 0, {}) == -1);
    std::vector<Spot> tie = {At(9, 0, 100, 0), At(5, 0, -100, 0)};
    CHECK(PickSpot(tie, 0, 0, 0, {}) == 1);
}

TEST(interact_trip_travels_acts_waits_and_finishes)
{
    Trip trip = Start(24471, 253754, 37079, 4, 0, At(1, 0, 0, 0), 600.0f, 1000);
    CHECK(trip.active);
    CHECK(trip.budgetMs == MIN_BUDGET_MS + 240000);
    CHECK(Judge(trip, 2000, 50.0f, 0) == Step::Travel);
    CHECK(Judge(trip, 3000, 5.0f, 0) == Step::Act);
    NoteAct(trip, 3000, 77, Way::UseItem);
    CHECK(Used(trip, 77));
    CHECK(Judge(trip, 4000, 5.0f, 0) == Step::Wait);
    CHECK(Judge(trip, 3000 + ACT_GAP_MS, 5.0f, 0) == Step::Act);
    // The credit arrives: the act count starts over.
    NoteAct(trip, 8000, 78, Way::UseItem);
    CHECK(trip.actsWithoutCredit == 2);
    CHECK(Judge(trip, 8000 + ACT_GAP_MS, 5.0f, 1) == Step::Act);
    CHECK(trip.actsWithoutCredit == 0);
    CHECK(Judge(trip, 20000, 5.0f, 4) == Step::Done);
}

TEST(interact_gossip_reuses_its_creature)
{
    Trip trip = Start(8304, 257225, 15222, 1, 0, At(1, 1, 0, 0), 0.0f, 0);
    NoteAct(trip, 10, 15170, Way::Gossip);
    CHECK(!Used(trip, 15170));
}

TEST(interact_defers_a_credit_whose_target_offers_nothing)
{
    // Dearest Natalia: Frankal (15221) talks only after Rutgar (15222).
    Trip frankal = Start(8304, 257224, 15221, 1, 0, At(1, 1, 0, 0), 0.0f, 0);
    std::vector<uint32_t> deferred = Defer(frankal);
    CHECK(Deferred(deferred, 15221));
    CHECK(!Deferred(deferred, 15222));
    Trip rutgar = Start(8304, 257225, 15222, 1, 0, At(2, 1, 0, 0), 0.0f, 0);
    rutgar.deferred = deferred;
    std::vector<uint32_t> both = Defer(rutgar);
    CHECK(both.size() == 2);
    // Deferring the same credit twice keeps one entry.
    rutgar.deferred = both;
    CHECK(Defer(rutgar).size() == 2);
}

TEST(interact_trip_moves_on_after_acts_without_credit)
{
    Trip trip = Start(14098, 267179, 35830, 3, 0, At(1, 654, 0, 0), 0.0f, 0);
    CHECK(Judge(trip, 0, 0.0f, 0) == Step::Act);
    CHECK(trip.spotMs == 1);
    uint32_t now = 10;
    for (uint32_t i = 0; i < MAX_ACTS_WITHOUT_CREDIT; ++i)
    {
        NoteAct(trip, now, 100 + i, Way::OpenObject);
        now += ACT_GAP_MS;
    }
    CHECK(Judge(trip, now, 0.0f, 0) == Step::NextSpot);
    CHECK(!LeaveSpot(trip));
    CHECK(trip.visited.size() == 1 && trip.spotMs == 0 && trip.actsWithoutCredit == 0);
    // Doors already opened stay used at the next spot.
    CHECK(Used(trip, 100));
    for (uint32_t i = 1; i < MAX_SPOTS_WITHOUT_CREDIT - 1; ++i)
        CHECK(!LeaveSpot(trip));
    CHECK(LeaveSpot(trip));
}

TEST(interact_trip_spot_wait_and_budget)
{
    Trip trip = Start(1, 1, 1, 1, 0, At(1, 0, 0, 0), 0.0f, 0);
    CHECK(Judge(trip, 5, 0.0f, 0) == Step::Act);
    CHECK(Judge(trip, 5 + SPOT_WAIT_MS, 0.0f, 0) == Step::NextSpot);
    CHECK(Judge(trip, MIN_BUDGET_MS, 0.0f, 0) == Step::Expired);
    CHECK(BudgetMs(1.0e7f) == MAX_BUDGET_MS);
    CHECK(BudgetMs(-5.0f) == MIN_BUDGET_MS);
}

TEST(interact_trip_owns_only_its_spot)
{
    Trip trip = Start(1, 1, 1, 1, 0, At(1, 0, 100.0f, 200.0f), 0.0f, 0);
    CHECK(SameDest(trip, 0, 100.2f, 200.2f));
    CHECK(!SameDest(trip, 1, 100.0f, 200.0f));
    CHECK(!SameDest(trip, 0, 110.0f, 200.0f));
    trip.active = false;
    CHECK(!SameDest(trip, 0, 100.0f, 200.0f));
}

TEST(interact_log_verbs)
{
    CHECK(std::string(Verb(Way::Gossip)) == "gossips");
    CHECK(std::string(Verb(Way::Click)) == "clicks");
    CHECK(std::string(Verb(Way::UseItem)) == "uses item on");
    CHECK(std::string(Verb(Way::OpenObject)) == "opens");
}
