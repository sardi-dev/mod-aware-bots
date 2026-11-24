#include "TestHarness.h"
#include "SocialSoakPolicy.h"

using SocialSoak::ClassifyMove;
using SocialSoak::Counter;
using SocialSoak::Move;
using SocialSoak::Scope;
using SocialSoak::Tally;

TEST(social_soak_an_empty_list_limits_nothing)
{
    Scope const scope = Scope::Parse("");
    CHECK(!scope.Limited());
    CHECK(scope.Allows("Anybody"));
    CHECK(scope.Allows(""));
    CHECK(scope.Size() == 0);
    CHECK(scope.Describe() == "all bots");
    // Only separators and blanks is still empty.
    CHECK(!Scope::Parse(" , ;, \t ").Limited());
}
TEST(social_soak_a_named_list_allows_only_those_bots_in_any_letter_case)
{
    Scope const scope = Scope::Parse("Botone, bottwo;BOTTHREE");
    CHECK(scope.Limited());
    CHECK(scope.Size() == 3);
    CHECK(scope.Allows("Botone"));
    CHECK(scope.Allows("botone"));
    CHECK(scope.Allows("BOTTWO"));
    CHECK(scope.Allows("Botthree"));
    CHECK(!scope.Allows("Botfour"));
    CHECK(!scope.Allows(""));
    // A name that only starts like a listed one is not listed.
    CHECK(!scope.Allows("Botonee"));
    CHECK(!scope.Allows("Boto"));
    CHECK(scope.Describe() == "3 named bots");
}
TEST(social_soak_repeats_and_blank_items_are_dropped)
{
    Scope const scope = Scope::Parse(",Aaa,,aaa, AAA ,Bbb,");
    CHECK(scope.Size() == 2);
    CHECK(scope.Allows("aaa"));
    CHECK(scope.Allows("bbb"));
}
TEST(social_soak_the_tally_counts_and_prints_every_counter_in_order)
{
    Tally tally;
    CHECK(tally.Total() == 0);
    tally.Add(Counter::EmoteReply);
    tally.Add(Counter::EmoteReply);
    tally.Add(Counter::LikingUp, 5);
    CHECK(tally.Get(Counter::EmoteReply) == 2);
    CHECK(tally.Get(Counter::LikingUp) == 5);
    CHECK(tally.Get(Counter::QuestOffer) == 0);
    CHECK(tally.Total() == 7);

    std::string const fields = tally.Fields();
    CHECK(fields.find("liking_up=5") != std::string::npos);
    CHECK(fields.find("emote_reply=2") != std::string::npos);
    CHECK(fields.find("quest_offer=0") != std::string::npos);
    CHECK(fields.find("out_of_scope=0") != std::string::npos);
    CHECK(fields.rfind("liking_up=5", 0) == 0);

    Tally other;
    CHECK(!(tally == other));
    other.Add(Counter::EmoteReply, 2);
    other.Add(Counter::LikingUp, 5);
    CHECK(tally == other);
}
TEST(social_soak_every_counter_has_a_distinct_name)
{
    for (size_t i = 0; i < SocialSoak::COUNTER_COUNT; ++i)
    {
        std::string const name = SocialSoak::CounterName(Counter(i));
        CHECK(name != "unknown");
        for (size_t j = i + 1; j < SocialSoak::COUNTER_COUNT; ++j)
            CHECK(name != SocialSoak::CounterName(Counter(j)));
    }
}
TEST(social_soak_the_summary_line_is_due_only_when_something_changed_after_the_interval)
{
    using SocialSoak::SummaryDue;
    CHECK(SummaryDue(1000, 1900, 900, true));
    CHECK(!SummaryDue(1000, 1899, 900, true));
    CHECK(SummaryDue(1000, 5000, 900, false));
    // An interval of 0 turns the periodic line off.
    CHECK(!SummaryDue(1000, 99999, 0, true));
    // A clock set back still lets the line out.
    CHECK(SummaryDue(5000, 1000, 900, true));
}
TEST(social_soak_the_summary_line_names_the_scope_the_window_and_the_totals)
{
    Tally tally;
    tally.Add(Counter::GuildOffer);
    std::string const line = SocialSoak::SummaryLine(Scope::Parse("A,B"), tally, 900, { 4, 3, 2, 1 }, 8, 9);
    CHECK(line.rfind("[SOCIAL] summary (interval): scope=2 named bots window=900s ", 0) == 0);
    CHECK(line.find("guild_offer=1") != std::string::npos);
    CHECK(line.find("pairs_cold_or_above=4 pairs_stranger_or_above=3 pairs_friendly_or_above=2 pairs_close=1") != std::string::npos);
    CHECK(line.find("total_quest_offer=8 total_db_writes=9") != std::string::npos);
}
TEST(social_soak_an_emote_verdict_feeds_its_counter_and_the_silent_ones_feed_none)
{
    using EmoteReplyPolicy::Verdict;
    CHECK(SocialSoak::EmoteCounter(Verdict::Reply) == Counter::EmoteReply);
    CHECK(SocialSoak::EmoteCounter(Verdict::Chance) == Counter::EmoteChance);
    CHECK(SocialSoak::EmoteCounter(Verdict::Limited) == Counter::EmoteLimited);
    CHECK(SocialSoak::EmoteCounter(Verdict::Cold) == Counter::EmoteCold);
    CHECK(SocialSoak::EmoteCounter(Verdict::Busy) == Counter::EmoteBusy);
    CHECK(SocialSoak::EmoteCounter(Verdict::OutOfScope) == Counter::OutOfScope);
    CHECK(!SocialSoak::EmoteCounter(Verdict::NotAimed).has_value());
    CHECK(!SocialSoak::EmoteCounter(Verdict::TooFar).has_value());
}
TEST(social_soak_a_score_move_is_up_down_capped_or_queued)
{
    CHECK(ClassifyMove(true, 0.02, 0.30, 0.32) == Move::Up);
    CHECK(ClassifyMove(true, -0.10, 0.50, 0.40) == Move::Down);
    // A positive request that left the score alone was cut by the daily cap.
    CHECK(ClassifyMove(true, 0.06, 0.45, 0.45) == Move::Capped);
    // A negative request at the floor stays a "down" step (nothing left to lose).
    CHECK(ClassifyMove(true, -0.10, 0.0, 0.0) == Move::Down);
    // Rows still loading: nothing is known about the score.
    CHECK(ClassifyMove(false, 0.02, 0.30, 0.30) == Move::Queued);
    CHECK(SocialSoak::MoveCounter(Move::Up) == Counter::LikingUp);
    CHECK(SocialSoak::MoveCounter(Move::Down) == Counter::LikingDown);
    CHECK(SocialSoak::MoveCounter(Move::Capped) == Counter::LikingCapped);
    CHECK(SocialSoak::MoveCounter(Move::Queued) == Counter::LikingQueued);
}
TEST(social_soak_the_log_lines_are_greppable_and_carry_a_guid_never_a_human_name)
{
    std::string const liking = SocialSoak::LikingLine("Botone", 4242, LikingPolicy::Event::Thanked, 1, Move::Up, 0.30, 0.32);
    CHECK(liking == "[SOCIAL] liking bot=Botone human=4242 event=thanked steps=1 result=up before=30 after=32");

    std::string const queued = SocialSoak::LikingLine("Botone", 7, LikingPolicy::Event::Kicked, 1, Move::Queued, 0.30, 0.30);
    CHECK(queued.find("result=queued") != std::string::npos);

    std::string const emote = SocialSoak::EmoteLine("Bottwo", 4242, 78, true, 3.4f);
    CHECK(emote == "[SOCIAL] emote bot=Bottwo human=4242 text_emote=78 aim=bot yards=3");
    CHECK(SocialSoak::EmoteLine("Bottwo", 4242, 78, false, 7.6f).find("aim=nobody yards=8") != std::string::npos);

    CHECK(SocialSoak::OfferLine("quest", "Botone", 4242, 1234, true) ==
        "[SOCIAL] offer kind=quest bot=Botone human=4242 quest=1234 result=made");
    CHECK(SocialSoak::OfferLine("guild", "Botone", 4242, 0, false) ==
        "[SOCIAL] offer kind=guild bot=Botone human=4242 quest=0 result=refused");
}
TEST(social_soak_every_liking_event_has_a_log_name)
{
    using LikingPolicy::Event;
    for (Event const event : { Event::GroupTime, Event::DungeonDone, Event::Traded, Event::Dueled, Event::GuildTime, Event::Thanked,
             Event::Insulted, Event::Kicked, Event::LootNinja })
        CHECK(std::string(SocialSoak::EventName(event)) != "unknown");
}

TEST(social_soak_cause_counters_follow_event_order)
{
    CHECK(SocialSoak::CauseCounter(LikingPolicy::Event::Traded, true) == Counter::UpTraded);
    CHECK(SocialSoak::CauseCounter(LikingPolicy::Event::LootNinja, false) == Counter::DownLootNinja);
}
