#include "TestHarness.h"
#include "TokPlanLogic.h"
#include "AbPlanLogic.h"
#include "BgCalloutPolicy.h"
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

TEST(tok_projection_and_stance)
{
    Projection p = Project(500, 400, 400, 350, 20000, 200000);
    CHECK(p.ours == 1500 && p.theirs == 900);
    CHECK(p.ourFinish < p.theirFinish);
    CHECK(ChooseStance(p, 500, 400, 200000, Stance::BALANCED) == Stance::HOLD);
    Projection q = Project(400, 500, 350, 400, 20000, 200000);
    CHECK(ChooseStance(q, 400, 500, 200000, Stance::BALANCED) == Stance::PUSH);
    CHECK(ChooseStance(q, 400, 500, 100000, Stance::PUSH) == Stance::ALL_IN);
    Projection near{550, 500, 100, 200};
    CHECK(ChooseStance(near, 500, 450, 300000, Stance::BALANCED) == Stance::BALANCED);
    CHECK(ChooseStance(near, 500, 450, 300000, Stance::HOLD) == Stance::HOLD);
    CHECK(ChooseStance(Project(0, 0, 0, 0, 0, 1000), 0, 0, 1000, Stance::BALANCED) == Stance::BALANCED);
    Split h = WithStance(Opening(10), Stance::HOLD), a = WithStance(Opening(10), Stance::ALL_IN);
    CHECK(h.support == 4 && h.hunt == 1);
    CHECK(a.support == 2 && a.hunt == 4 && a.flex == 0);
}
TEST(tok_call_rules)
{
    CHECK(Fresh(16000, 1001)); CHECK(!Fresh(16001, 1001));
    CHECK(Fresh(10, 0xfffffff0u));
    CHECK(CallAllowed(30001, 10001, 10001, 0));
    CHECK(!CallAllowed(30000, 10001, 10001, 0));
    CHECK(!CallAllowed(30001, 10001, 10001, 2));
    auto call = BgCallout::Parse("enemy green carrier mid", BgCallout::MapKindOf(998));
    CHECK(call.valid && call.kind == PvpObs::Kind::ENEMY_CARRIER && call.place == 2);
    CHECK(!BgCallout::Parse("enemy green blue carrier", BgCallout::MapKind::TOK).valid);
    CHECK(!BgCallout::Parse("enemy green carrier?", BgCallout::MapKind::TOK).valid);
}
