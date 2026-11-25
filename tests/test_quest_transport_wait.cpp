#include "TestHarness.h"
#include "QuestRetryPolicy.h"

TEST(quest_transport_wait_timeout_uses_long_retry_after_repeat)
{
    using namespace QuestRetryPolicy;
    Failure f;
    CHECK(IsStuckReason(TRANSPORT_WAIT_TIMED_OUT));
    CHECK(!RecordFailure(f, 100, 1, 40, true, IsStuckReason(TRANSPORT_WAIT_TIMED_OUT)));
    CHECK(Delay(f) == 10u * 60 * 1000);
    CHECK(!RecordFailure(f, 200, 1, 40, true, IsStuckReason(TRANSPORT_WAIT_TIMED_OUT)));
    CHECK(Delay(f) == STUCK_DELAY_MS);
    CHECK(Wait(f, 201, 2, 40));
    CHECK(!Wait(f, 201, 2, 41));
    CHECK(!Wait(f, 200 + STUCK_DELAY_MS, 2, 40));
}
