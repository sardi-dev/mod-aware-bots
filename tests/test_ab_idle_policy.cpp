#include "TestHarness.h"
#include "AbPlanLogic.h"

using namespace AbLogic;

TEST(ab_surplus_guard_does_not_keep_a_delayed_role)
{
    CHECK(SurplusGuard({Job::GUARD, NODE_BLACKSMITH}, 3, 2));
    CHECK(SurplusGuard({Job::GUARD, NODE_BLACKSMITH}, 1, 0));
    CHECK(!SurplusGuard({Job::GUARD, NODE_BLACKSMITH}, 2, 2));
    CHECK(!SurplusGuard({Job::MOBILE, NODE_NONE}, 3, 2));
}

TEST(ab_opening_assault_moves_on_after_capture_hold)
{
    DecisionContext c;
    c.job = Job::ASSAULT;
    c.slotNode = NODE_BLACKSMITH;
    c.assaultNode = NODE_GOLD_MINE;
    c.sit[NODE_BLACKSMITH] = NodeSituation::CAPPING;
    c.captureAgeMs[NODE_BLACKSMITH] = STAY_AFTER_CAST_MS - 1;
    CHECK(Options(c).front().kind == Kind::HOLD_CAPTURE);
    c.captureAgeMs[NODE_BLACKSMITH] = STAY_AFTER_CAST_MS;
    CHECK(Options(c).front().kind == Kind::TAKE);
    CHECK(Options(c).front().node == NODE_GOLD_MINE);
}
