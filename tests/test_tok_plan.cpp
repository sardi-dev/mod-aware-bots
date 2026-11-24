#include "TestHarness.h"
#include "TokPlanLogic.h"
#include "AbPlanLogic.h"
#include <vector>
using namespace TokPlan;
TEST(tok_opening_sizes)
{
    Split a = Opening(10), b = Opening(5);
    CHECK(a.pickup == 4 && a.support == 3 && a.hunt == 2 && a.flex == 1);
    CHECK(b.pickup == 2 && b.support == 1 && b.hunt == 1 && b.flex == 1);
    for (unsigned n = 0; n <= 10; ++n) { Split s = Opening(n); CHECK(s.pickup + s.support + s.hunt + s.flex == n); CHECK(s.pickup <= 4); }
}
TEST(tok_zone_hysteresis)
{
    CHECK(CarrierZone(Zone::CENTRE, 90, 2, 1, true, 10000) == Zone::CENTRE);
    CHECK(CarrierZone(Zone::CENTRE, 39, 1, 2, false, 0) == Zone::OUTER);
    CHECK(CarrierZone(Zone::CENTRE, 90, 0, 0, true, 10000) == Zone::RING);
    CHECK(CarrierZone(Zone::RING, 65, 2, 0, true, 20000) == Zone::RING);
    CHECK(CarrierZone(Zone::RING, 66, 2, 0, true, 9000) == Zone::RING);
    CHECK(CarrierZone(Zone::RING, 66, 2, 0, true, 10000) == Zone::CENTRE);
}
TEST(tok_seen_counts)
{
    std::vector<int> observations{0, 1, 0, 1};
    CHECK(CountSeen(observations, [](int v) { return v != 0; }) == 2);
}
TEST(tok_seen_by_living_teammates)
{
    struct Observer { bool alive, inRange, detects; };
    std::vector<Observer> observers{{false, true, true}, {true, false, true}, {true, true, false}};
    auto seen = [](std::vector<Observer> const& team) {
        return AbLogic::SeenByTeam(true, team, [](Observer const& p) { return p.alive; },
            [](Observer const& p) { return p.inRange; }, [](Observer const& p) { return p.detects; });
    };
    CHECK(!seen(observers));
    observers.push_back({true, true, true});
    CHECK(seen(observers));
}
