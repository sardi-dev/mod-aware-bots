/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "ArenaChatMath.h"
#include "TestHarness.h"

using ArenaChatMath::ChatLimiter;
using ArenaChatMath::Moment;
using ArenaChatMath::ResultFacts;

TEST(arena_chat_streak_moments)
{
    ResultFacts facts;
    facts.oldRank = facts.newRank = 5;
    facts.oldStreak = 2;
    facts.newStreak = 3;
    CHECK(ArenaChatMath::ChooseMoment(facts, true) == Moment::STREAK3);
    facts.oldStreak = 4;
    facts.newStreak = 5;
    CHECK(ArenaChatMath::ChooseMoment(facts, true) == Moment::STREAK5);
    facts.oldStreak = 5;
    facts.newStreak = 6;
    CHECK(ArenaChatMath::ChooseMoment(facts, true) == Moment::NONE);
    facts.oldStreak = 0;
    facts.newStreak = 1;
    CHECK(ArenaChatMath::ChooseMoment(facts, true) == Moment::NONE);
}

TEST(arena_chat_a_loss_never_speaks_for_its_own_team)
{
    ResultFacts facts;
    facts.newStreak = -1;
    CHECK(ArenaChatMath::ChooseMoment(facts, false) == Moment::NONE);
}

TEST(arena_chat_climb_beats_a_streak)
{
    ResultFacts facts;
    facts.oldRank = 4;
    facts.newRank = 3;
    facts.oldStreak = 2;
    facts.newStreak = 3;
    CHECK(ArenaChatMath::ChooseMoment(facts, true) == Moment::CLIMB);
    facts.oldRank = 3;
    facts.newRank = 3;
    CHECK(ArenaChatMath::ChooseMoment(facts, true) == Moment::STREAK3);
    facts.oldRank = 2;
    facts.newRank = 1;
    facts.newStreak = 1;
    CHECK(ArenaChatMath::ChooseMoment(facts, true) == Moment::CLIMB);
    facts.oldRank = 1;
    facts.newRank = 1;
    CHECK(ArenaChatMath::ChooseMoment(facts, true) == Moment::NONE);
    facts.oldRank = 0;  // newly ranked into the top three
    facts.newRank = 2;
    CHECK(ArenaChatMath::ChooseMoment(facts, true) == Moment::CLIMB);
}

TEST(arena_chat_rivalry_needs_four_meetings_and_a_close_record)
{
    ResultFacts facts;
    facts.oldRank = facts.newRank = 6;
    facts.meetings = 4;
    facts.winsOfTeam = 2;
    facts.winsOfOpponent = 2;
    CHECK(ArenaChatMath::IsRivalry(facts));
    CHECK(ArenaChatMath::ChooseMoment(facts, true) == Moment::RIVAL);
    facts.winsOfTeam = 3;
    facts.winsOfOpponent = 1;
    CHECK(!ArenaChatMath::IsRivalry(facts));  // two wins apart
    facts.meetings = 3;
    facts.winsOfTeam = 2;
    facts.winsOfOpponent = 1;
    CHECK(!ArenaChatMath::IsRivalry(facts));
}

TEST(arena_chat_a_broken_streak_is_worth_a_line_from_three)
{
    CHECK(!ArenaChatMath::BrokeAStreak(2));
    CHECK(ArenaChatMath::BrokeAStreak(3));
    CHECK(ArenaChatMath::BrokeAStreak(8));
    CHECK(!ArenaChatMath::BrokeAStreak(-4));
}

TEST(arena_chat_limiter_ladder_gap_and_team_gap)
{
    ChatLimiter limiter;
    CHECK(limiter.TryTake(1000, false, 1));
    CHECK(!limiter.TryTake(1000 + 899, false, 2));  // inside the 15 minute ladder gap
    CHECK(limiter.TryTake(1000 + 900, false, 2));
    CHECK(!limiter.TryTake(1000 + 1800, false, 1));  // team 1 spoke 30 minutes ago, under the 45 minute gap
    CHECK(limiter.TryTake(1000 + 2700, false, 1));
}

TEST(arena_chat_limiter_is_silent_in_quiet_hours_and_records_nothing)
{
    ChatLimiter limiter;
    CHECK(!limiter.TryTake(5000, true, 1));
    CHECK(limiter.TryTake(5001, false, 1));  // the refused try left no trace
}

TEST(arena_chat_limiter_caps_a_day)
{
    ArenaChatMath::ChatLimits limits;
    limits.ladderGapSeconds = 0;
    limits.teamGapSeconds = 0;
    limits.perDay = 3;
    ChatLimiter limiter(limits);
    CHECK(limiter.TryTake(100, false, 1));
    CHECK(limiter.TryTake(200, false, 1));
    CHECK(limiter.TryTake(300, false, 1));
    CHECK(!limiter.TryTake(400, false, 1));
    CHECK(!limiter.TryTake(100 + 86399, false, 1));
    CHECK(limiter.TryTake(100 + 86400, false, 1));  // the first line has left the 24 hour window
}
