#include "TestHarness.h"
#include "TradeOutcomePolicy.h"

using namespace TradeOutcomePolicy;

TEST(trade_outcome_money_or_items_that_moved_count)
{
    CHECK(Completed(100, 90, 0, 0, 0));  // the bot paid
    CHECK(Completed(100, 100, 1, 2, 0)); // one offered item left its bags
    CHECK(Completed(100, 100, 0, 0, 1)); // the other side's item arrived
}

TEST(trade_outcome_a_cancelled_trade_does_not_count)
{
    CHECK(!Completed(100, 100, 2, 2, 0)); // the window closed, nothing moved (a full bag cancel)
    CHECK(!Completed(0, 0, 0, 0, 0));
}
