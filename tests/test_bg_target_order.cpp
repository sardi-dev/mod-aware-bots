#include "TestHarness.h"
#include "BgTargetOrderPolicy.h"

// BT-553: the target order of a fight at a node (owner decision 2026-11-18).

using namespace BgTargetOrder;

namespace
{
Foe MakeFoe(std::uint64_t guid, bool healer, bool squishy, float health, float distance = 10.0f)
{
    Foe f;
    f.guid = guid;
    f.healer = healer;
    f.squishy = squishy;
    f.healthPercent = health;
    f.distance = distance;
    return f;
}

std::vector<Foe> Team()
{
    return { MakeFoe(1, true, false, 90.0f), MakeFoe(2, false, true, 60.0f), MakeFoe(3, false, false, 30.0f),
             MakeFoe(4, false, true, 80.0f) };
}
}  // namespace

TEST(target_order_casual_takes_the_lowest_health)
{
    CHECK(Pick(Tier::CASUAL, true, Team()) == 3);
    CHECK(Pick(Tier::CASUAL, false, Team()) == 3);
}

TEST(target_order_good_takes_the_healer_first)
{
    CHECK(Pick(Tier::GOOD, false, Team()) == 1);
    std::vector<Foe> noHealer = { MakeFoe(2, false, true, 60.0f), MakeFoe(3, false, false, 30.0f) };
    CHECK(Pick(Tier::GOOD, false, noHealer) == 3);   // no healer in reach: the lowest health
}

TEST(target_order_expert_kills_the_healer_with_an_edge_else_drains_the_squishy)
{
    CHECK(Pick(Tier::EXPERT, true, Team()) == 1);    // a numbers edge: the healer
    CHECK(Pick(Tier::EXPERT, false, Team()) == 2);   // no edge: the lowest-health squishy damage dealer
    std::vector<Foe> noSquishy = { MakeFoe(1, true, false, 90.0f), MakeFoe(3, false, false, 30.0f) };
    CHECK(Pick(Tier::EXPERT, false, noSquishy) == 1);   // no squishy target: the healer
    std::vector<Foe> none = { MakeFoe(3, false, false, 30.0f), MakeFoe(5, false, false, 20.0f) };
    CHECK(Pick(Tier::EXPERT, false, none) == 5);
}

TEST(target_order_ignores_foes_out_of_reach_and_breaks_ties_by_guid)
{
    std::vector<Foe> far = { MakeFoe(1, true, false, 10.0f, 45.0f), MakeFoe(2, false, false, 90.0f, 12.0f) };
    CHECK(Pick(Tier::GOOD, false, far) == 2);
    CHECK(Pick(Tier::GOOD, false, { MakeFoe(1, true, false, 10.0f, 45.0f) }) == 0);
    std::vector<Foe> twins = { MakeFoe(9, false, false, 50.0f), MakeFoe(7, false, false, 50.0f) };
    CHECK(Pick(Tier::CASUAL, false, twins) == 7);
    CHECK(Pick(Tier::CASUAL, false, {}) == 0);
}

TEST(target_order_has_an_edge_by_the_numbers)
{
    CHECK(HasEdge(5, 4));
    CHECK(!HasEdge(4, 4));
    CHECK(!HasEdge(3, 6));
}

TEST(target_order_keeps_a_target_as_good_as_the_pick_and_waits_between_switches)
{
    std::vector<Foe> team = Team();
    // The bot hits foe 3 (not a healer); a good bot should switch to the healer, but not before the wait is over.
    CHECK(!ShouldSwitch(Tier::GOOD, false, team, 3, 1, 1000));
    CHECK(ShouldSwitch(Tier::GOOD, false, team, 3, 1, 4000));
    // Two squishy foes: an expert already on one keeps it even though the pick is the other.
    CHECK(!ShouldSwitch(Tier::EXPERT, false, team, 4, 2, 9000));
    // A casual bot never ranks one above another: it keeps its target.
    CHECK(!ShouldSwitch(Tier::CASUAL, false, team, 1, 3, 9000));
    // The current target is not among the foes (a pet, out of reach): take the pick.
    CHECK(ShouldSwitch(Tier::GOOD, false, team, 99, 1, 9000));
    CHECK(!ShouldSwitch(Tier::GOOD, false, team, 1, 1, 9000));
    CHECK(!ShouldSwitch(Tier::GOOD, false, team, 3, 0, 9000));
}
