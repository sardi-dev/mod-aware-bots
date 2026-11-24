#include "TestHarness.h"
#include "LfgTeleportPolicy.h"

using namespace LfgTeleportPolicy;

TEST(lfg_teleport_first_ask_is_made_at_once)
{
    CHECK(Next(0, false, 0) == Step::Ask);
}

TEST(lfg_teleport_asks_are_spaced)
{
    CHECK(Next(1, true, ASK_GAP_MS - 1) == Step::Wait);
    CHECK(Next(1, true, ASK_GAP_MS) == Step::Ask);
}

TEST(lfg_teleport_the_bot_leaves_after_three_asks)
{
    CHECK(MAX_ASKS == 3);
    CHECK(Next(MAX_ASKS - 1, true, ASK_GAP_MS) == Step::Ask);
    CHECK(Next(MAX_ASKS, true, ASK_GAP_MS) == Step::Leave);
}

TEST(lfg_teleport_the_last_ask_is_given_time_before_the_bot_leaves)
{
    CHECK(Next(MAX_ASKS, true, 1000) == Step::Wait);
}
