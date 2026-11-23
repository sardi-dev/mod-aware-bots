#include "TestHarness.h"
#include "EmoteReplyPolicy.h"

using EmoteReplyPolicy::Aim;
using EmoteReplyPolicy::Decide;
using EmoteReplyPolicy::Facts;
using EmoteReplyPolicy::Limiter;
using EmoteReplyPolicy::Tuning;
using EmoteReplyPolicy::Verdict;

namespace
{
Facts Targeted(float yards)
{
    Facts facts;
    facts.aim = Aim::AtBot;
    facts.yards = yards;
    return facts;
}

Facts Untargeted(float yards, uint32_t roll)
{
    Facts facts;
    facts.aim = Aim::AtNobody;
    facts.yards = yards;
    facts.roll = roll;
    return facts;
}
}  // namespace

TEST(emote_reply_a_targeted_emote_in_range_is_answered)
{
    Tuning const tuning;
    CHECK(Decide(tuning, Targeted(3.0f)) == Verdict::Reply);
    CHECK(Decide(tuning, Targeted(20.0f)) == Verdict::Reply);
    CHECK(Decide(tuning, Targeted(20.1f)) == Verdict::TooFar);
}
TEST(emote_reply_an_emote_aimed_at_someone_else_is_not_ours)
{
    Tuning const tuning;
    Facts facts = Targeted(2.0f);
    facts.aim = Aim::AtOther;
    CHECK(Decide(tuning, facts) == Verdict::NotAimed);
    // Even when it is the nearest bot and not busy: the first test is whom the emote was for.
    facts.yards = 100.0f;
    CHECK(Decide(tuning, facts) == Verdict::NotAimed);
}
TEST(emote_reply_an_untargeted_emote_needs_a_close_bot_and_a_lucky_roll)
{
    Tuning const tuning;
    CHECK(Decide(tuning, Untargeted(8.0f, 35)) == Verdict::Reply);
    CHECK(Decide(tuning, Untargeted(8.1f, 1)) == Verdict::TooFar);
    CHECK(Decide(tuning, Untargeted(5.0f, 36)) == Verdict::Chance);
    CHECK(Decide(tuning, Untargeted(5.0f, 100)) == Verdict::Chance);
    Tuning always = tuning;
    always.untargetedChancePct = 100;
    CHECK(Decide(always, Untargeted(5.0f, 100)) == Verdict::Reply);
    Tuning never = tuning;
    never.untargetedChancePct = 0;
    CHECK(Decide(never, Untargeted(0.0f, 1)) == Verdict::Chance);
}
TEST(emote_reply_a_busy_bot_stays_out_of_it)
{
    Tuning const tuning;
    Facts facts = Targeted(4.0f);
    facts.busy = true;
    CHECK(Decide(tuning, facts) == Verdict::Busy);
}
TEST(emote_reply_a_cold_bot_ignores_the_human)
{
    Tuning const tuning;
    Facts facts = Targeted(4.0f);
    facts.cold = true;
    CHECK(Decide(tuning, facts) == Verdict::Cold);
    // A busy bot reports busy first: the cheaper reason wins, nothing else changes.
    facts.busy = true;
    CHECK(Decide(tuning, facts) == Verdict::Busy);
}
TEST(emote_reply_the_per_human_limit_is_the_last_gate)
{
    Tuning const tuning;
    Facts facts = Targeted(4.0f);
    facts.limited = true;
    CHECK(Decide(tuning, facts) == Verdict::Limited);
    // An untargeted emote that lost its roll does not report the limit: the roll is checked first.
    Facts lost = Untargeted(4.0f, 90);
    lost.limited = true;
    CHECK(Decide(tuning, lost) == Verdict::Chance);
}
TEST(emote_reply_limiter_allows_one_reply_per_human_per_minute)
{
    Limiter limiter;
    uint32_t const now = 1000000;
    CHECK(!limiter.Limited(7, now, 60));
    limiter.Record(7, now, 60);
    CHECK(limiter.Limited(7, now, 60));
    CHECK(limiter.Limited(7, now + 59, 60));
    CHECK(!limiter.Limited(7, now + 60, 60));
    // Another human has their own minute.
    CHECK(!limiter.Limited(8, now + 1, 60));
}
TEST(emote_reply_limiter_keeps_the_minute_when_nothing_was_recorded)
{
    // The gesture table may have no answer for an emote: the caller then never records, so the human is not locked out.
    Limiter limiter;
    CHECK(!limiter.Limited(7, 5000, 60));
    CHECK(!limiter.Limited(7, 5001, 60));
    CHECK(limiter.Size() == 0);
}
TEST(emote_reply_limiter_survives_a_clock_set_back_and_a_zero_gap)
{
    Limiter limiter;
    limiter.Record(7, 90000, 60);
    CHECK(!limiter.Limited(7, 10, 60));
    CHECK(!limiter.Limited(7, 90000, 0));
}
TEST(emote_reply_limiter_does_not_grow_for_good)
{
    Limiter limiter;
    for (uint32_t human = 1; human <= 1000; ++human)
        limiter.Record(human, 1000 + human, 60);
    CHECK(limiter.Size() <= 256);
    // Old entries are dropped first: the latest human is still limited.
    CHECK(limiter.Limited(1000, 2000, 60));
}

TEST(emote_reply_a_bot_outside_the_only_bots_list_stays_out_after_the_range_checks)
{
    Tuning const tuning;
    Facts facts = Targeted(3.0f);
    facts.inScope = false;
    CHECK(Decide(tuning, facts) == Verdict::OutOfScope);
    // Someone else's emote and a far bot are not worth counting as held back by the scope.
    facts.aim = Aim::AtOther;
    CHECK(Decide(tuning, facts) == Verdict::NotAimed);
    facts = Targeted(25.0f);
    facts.inScope = false;
    CHECK(Decide(tuning, facts) == Verdict::TooFar);
    // In scope is the default, so nothing changes with the list empty.
    CHECK(Decide(tuning, Targeted(3.0f)) == Verdict::Reply);
}
