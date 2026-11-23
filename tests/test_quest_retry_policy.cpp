#include "TestHarness.h"
#include "QuestRetryPolicy.h"

TEST(quest_retry_progress_is_an_increase)
{
    using namespace QuestRetryPolicy;
    CHECK(!Advanced({3, 0}, {3, 0}));
    CHECK(!Advanced({3, 2}, {2, 2}));
    CHECK(Advanced({3, 0}, {3, 1}));
    CHECK(Advanced({1, 4}, {2, 3}));
}

TEST(quest_retry_delay_and_changed_context)
{
    using namespace QuestRetryPolicy;
    Failure f;
    CHECK(!RecordFailure(f, 100, 7, 20, false));
    CHECK(Wait(f, 100 + 599999, 7, 20));
    CHECK(!Wait(f, 100 + 600000, 7, 20));
    CHECK(!Wait(f, 101, 8, 20));
    CHECK(!Wait(f, 101, 7, 21));
    CHECK(!RecordFailure(f, 200, 7, 20, false));
    CHECK(Wait(f, 200 + 1799999, 7, 20));
    CHECK(!Wait(f, 200 + 1800000, 7, 20));
    CHECK(RecordFailure(f, 300, 7, 20, false));
    CHECK(Wait(f, 300 + 7199999, 7, 20));
    CHECK(!Wait(f, 300 + 7200000, 7, 20));
}

TEST(quest_retry_complete_is_never_abandoned_and_clock_wraps)
{
    using namespace QuestRetryPolicy;
    Failure f;
    for (unsigned i = 0; i < 5; ++i)
        CHECK(!RecordFailure(f, 0xfffffff0u, 1, 1, true));
    CHECK(f.tries == 3);
    CHECK(Wait(f, 20, 1, 1));
    CHECK(!Wait(f, 7200000, 1, 1));
}

TEST(quest_retry_stuck_reasons_are_the_two_texts_of_a_bot_that_got_nowhere)
{
    using namespace QuestRetryPolicy;
    CHECK(IsStuckReason("stuck on the way"));
    CHECK(IsStuckReason(STOOD_STILL));
    CHECK(!IsStuckReason("out of time"));
    CHECK(!IsStuckReason("no route to the ender's continent"));
    CHECK(!IsStuckReason(nullptr));
}

TEST(quest_retry_second_stuck_failure_waits_three_hours_and_ignores_a_zone_change)
{
    using namespace QuestRetryPolicy;
    Failure f;
    // the first failure waits as before (10 minutes), a changed zone releases the quest
    CHECK(!RecordFailure(f, 1000, 7, 20, true, true));
    CHECK(!Unreachable(f));
    CHECK(Wait(f, 1000 + 599999, 7, 20));
    CHECK(!Wait(f, 1000 + 599999, 8, 20));
    // the second one is no accident of the path
    CHECK(!RecordFailure(f, 2000, 7, 20, true, true));
    CHECK(Unreachable(f));
    CHECK(Wait(f, 2000 + STUCK_DELAY_MS - 1, 7, 20));
    CHECK(Wait(f, 2000 + STUCK_DELAY_MS - 1, 8, 20));
    CHECK(!Wait(f, 2000 + STUCK_DELAY_MS, 7, 20));
    // a new level is a new chance
    CHECK(!Wait(f, 2001, 7, 21));
}

TEST(quest_retry_failures_that_are_not_stuck_keep_the_old_delays)
{
    using namespace QuestRetryPolicy;
    Failure f;
    RecordFailure(f, 0, 3, 10, true);
    RecordFailure(f, 0, 3, 10, true);
    CHECK(f.stuck == 0 && !Unreachable(f));
    CHECK(Delay(f) == Delay(2u));
    RecordFailure(f, 0, 3, 10, true);
    CHECK(Delay(f) == Delay(3u));
    // a long third-try delay is never cut short by the stuck rule
    f.stuck = STUCK_LONG_AFTER;
    CHECK(Delay(f) == STUCK_DELAY_MS);
    f.tries = 3;
    CHECK(Delay(f) >= Delay(3u));
}
