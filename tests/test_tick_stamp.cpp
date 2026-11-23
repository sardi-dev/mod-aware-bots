#include "TestHarness.h"
#include "TickStamp.h"

// BT-699: facts read once per AI tick are reused inside that tick only.

TEST(tick_stamp_renews_on_the_first_read_and_holds_within_the_tick)
{
    TickStamp stamp;
    CHECK(stamp.Renew(7));
    CHECK(!stamp.Renew(7));
    CHECK(!stamp.Renew(7));
}

TEST(tick_stamp_renews_on_the_next_tick)
{
    TickStamp stamp;
    CHECK(stamp.Renew(7));
    CHECK(stamp.Renew(8));
    CHECK(!stamp.Renew(8));
}

TEST(tick_stamp_serial_zero_is_no_tick_and_never_matches)
{
    TickStamp stamp;
    CHECK(stamp.Renew(0));
    CHECK(stamp.Renew(0));
    CHECK(stamp.Renew(5));
    CHECK(stamp.Renew(0));
    CHECK(stamp.Renew(5));
}
