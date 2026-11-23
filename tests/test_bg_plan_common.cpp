#include "TestHarness.h"
#include "BgPlanCommon.h"

// BT-156: the battleground plan pieces shared by Warsong Gulch, Arathi Basin and the maps that follow.

using namespace BgPlan;

namespace
{
enum TestCall : std::uint8_t
{
    CALL_A = 1,
    CALL_B = 2
};

struct TestCandidate : CandidateBase
{
};

TestCandidate Cand(std::uint32_t guid, std::uint8_t level = 20)
{
    TestCandidate c;
    c.guidLow = guid;
    c.level = level;
    return c;
}

struct TestRecord : RecordBase
{
    int payload = 0;
};
}  // namespace

TEST(bgplan_draw_is_20_60_20_and_matches_the_strategy_numbers)
{
    std::uint32_t offense = 0, balanced = 0, defense = 0;
    for (std::uint32_t roll = 0; roll < 100; ++roll)
    {
        Plan const plan = DrawPlan(roll);
        offense += plan == Plan::OFFENSE;
        balanced += plan == Plan::BALANCED;
        defense += plan == Plan::DEFENSE;
    }
    CHECK(offense == 20);
    CHECK(balanced == 60);
    CHECK(defense == 20);

    // The same numbers as WSBotStrategy / ABBotStrategy: balanced 0, offensive 1, defensive 2.
    CHECK(static_cast<int>(Plan::BALANCED) == 0);
    CHECK(static_cast<int>(Plan::OFFENSE) == 1);
    CHECK(static_cast<int>(Plan::DEFENSE) == 2);
    CHECK(PlanFromStrategy(1) == Plan::OFFENSE);
    CHECK(PlanFromStrategy(2) == Plan::DEFENSE);
    CHECK(PlanFromStrategy(9) == Plan::BALANCED);
}

TEST(bgplan_tier_lag_and_call_chance)
{
    CHECK(AdoptLagMs(Tier::EXPERT) == 0);
    CHECK(AdoptLagMs(Tier::GOOD) == 1000);
    CHECK(AdoptLagMs(Tier::CASUAL) == 3000);
    CHECK(CallChancePercent(Tier::EXPERT) == 100);
    CHECK(CallChancePercent(Tier::GOOD) == 70);
    CHECK(CallChancePercent(Tier::CASUAL) == 30);
    CHECK(CallDelayMs(Tier::EXPERT, 999) == 0);
    CHECK(CallDelayMs(Tier::GOOD, 0) == 150);
    CHECK(CallDelayMs(Tier::GOOD, 999) < 400);
    CHECK(CallDelayMs(Tier::CASUAL, 0) == 400);
    CHECK(CallDelayMs(Tier::CASUAL, 999) < 900);
}

TEST(bgplan_call_draw_is_deterministic_and_follows_the_chance)
{
    std::uint32_t expertPass = 0, goodPass = 0, casualPass = 0;
    for (std::uint32_t seq = 1; seq <= 1000; ++seq)
    {
        CallDraw const a = DrawCall(77, seq, Tier::GOOD);
        CallDraw const b = DrawCall(77, seq, Tier::GOOD);
        CHECK(a.pass == b.pass);
        CHECK(a.delayMs == b.delayMs);
        expertPass += DrawCall(5, seq, Tier::EXPERT).pass;
        goodPass += DrawCall(6, seq, Tier::GOOD).pass;
        casualPass += DrawCall(7, seq, Tier::CASUAL).pass;
    }
    CHECK(expertPass == 1000);
    CHECK(goodPass > 600 && goodPass < 800);
    CHECK(casualPass > 220 && casualPass < 380);
}

TEST(bgplan_limiter_one_line_per_six_seconds_twenty_per_bot_eight_a_minute)
{
    CallLimiter limiter;
    CHECK(limiter.Allow(1, CALL_A, 10000));
    limiter.Commit(1, CALL_A, 10000);
    CHECK(!limiter.Allow(2, CALL_B, 15999));
    CHECK(limiter.Allow(2, CALL_B, 16000));
    CHECK(!limiter.Allow(1, CALL_A, 29999));
    CHECK(limiter.Allow(1, CALL_A, 30000));

    CallLimiter busy;
    std::uint32_t sent = 0;
    for (std::uint32_t i = 0; i < 40; ++i)
    {
        std::uint32_t const now = 100000 + i * 1500;
        if (busy.Allow(100 + i, CALL_A, now))
        {
            busy.Commit(100 + i, CALL_A, now);
            ++sent;
        }
    }
    CHECK(sent == 8);  // the 40 calls span 58.5 s: one minute window, eight lines
}

TEST(bgplan_limiter_kind_gap_is_asked_for_by_the_caller)
{
    CallLimiter limiter;
    limiter.Commit(1, CALL_A, 10000);
    KindGap const gap = { true, CALL_A, 10000 };
    CHECK(!limiter.Allow(2, CALL_B, 16500, gap));   // team gap passed, kind gap not
    CHECK(limiter.Allow(2, CALL_B, 20000, gap));
    CHECK(limiter.Allow(2, CALL_B, 16500));         // no gap asked for
}

TEST(bgplan_call_words_and_parse)
{
    static WordId const verbs[] = { { "inc", 1 }, { "help", 1 }, { "take", 2 } };
    static WordId const places[] = { { "bs", 10 }, { "blacksmith", 10 }, { "lm", 11 } };

    ParsedCall call = ParseCall("INC BS 3", verbs, std::size(verbs), places, std::size(places));
    CHECK(call.verb == 1);
    CHECK(call.place == 10);
    CHECK(call.number == 3);

    call = ParseCall("take lm", verbs, std::size(verbs), places, std::size(places));
    CHECK(call.verb == 2);
    CHECK(call.place == 11);
    CHECK(call.number == 0);

    call = ParseCall("nothing to see", verbs, std::size(verbs), places, std::size(places));
    CHECK(call.verb == WORD_NONE);
    CHECK(call.place == WORD_NONE);

    // A line longer than a short call is not read.
    call = ParseCall("inc bs this is a very long sentence about nothing at all", verbs, std::size(verbs), places,
                     std::size(places));
    CHECK(call.verb == WORD_NONE);
    CHECK(CallWords("").empty());
    CHECK(CallWords("a,b;c").size() == 3);
}

TEST(bgplan_pick_best_breaks_ties_by_hash_then_guid_and_skips_the_ineligible)
{
    std::vector<TestCandidate> list = { Cand(1), Cand(2), Cand(3) };
    std::vector<bool> taken(3, false);
    auto const flat = [](TestCandidate const&) { return 10; };
    std::size_t const first = PickBest(list, taken, flat);
    CHECK(first < 3);

    // The tie-break does not depend on the order of the list.
    std::vector<TestCandidate> reversed(list.rbegin(), list.rend());
    std::size_t const again = PickBest(reversed, taken, flat);
    CHECK(reversed[again].guidLow == list[first].guidLow);

    taken[first] = true;
    std::size_t const second = PickBest(list, taken, flat);
    CHECK(second != first && second < 3);

    auto const onlyTwo = [](TestCandidate const& c) { return c.guidLow == 2 ? 5 : NOT_ELIGIBLE; };
    std::vector<bool> none(3, false);
    CHECK(list[PickBest(list, none, onlyTwo)].guidLow == 2);
    none[1] = true;
    CHECK(PickBest(list, none, onlyTwo) == list.size());

    LevelSpan const span = SpanOf(std::vector<TestCandidate>{ Cand(1, 15), Cand(2, 19) });
    CHECK(span.minLevel == 15 && span.maxLevel == 19);
}

TEST(bgplan_tier_fit_profiles)
{
    CHECK(TierFitCritical(Tier::EXPERT) > TierFitCritical(Tier::GOOD));
    CHECK(TierFitCritical(Tier::GOOD) > TierFitCritical(Tier::CASUAL));
    CHECK(TierFitPost(Tier::CASUAL) > TierFitPost(Tier::GOOD));
    CHECK(TierFitPost(Tier::GOOD) > TierFitPost(Tier::EXPERT));

    CandidateBase rogue;
    rogue.cls = CLS_ROGUE;
    CandidateBase cat;
    cat.cls = CLS_DRUID;
    cat.feral = true;
    CandidateBase healerDruid;
    healerDruid.cls = CLS_DRUID;
    CHECK(IsStealthClass(rogue));
    CHECK(IsStealthClass(cat));
    CHECK(!IsStealthClass(healerDruid));
}

TEST(bgplan_apportion_adds_up_and_honours_the_minimums)
{
    // 15 players over home 2, centre 4, 4, 4 and one raider.
    std::vector<GroupWish> const wishes = { { 2, 1 }, { 4, 2 }, { 4, 2 }, { 4, 2 }, { 1, 0 } };
    for (std::uint32_t total = 5; total <= 15; ++total)
    {
        std::vector<std::uint32_t> const parts = Apportion(wishes, total);
        std::uint32_t sum = 0;
        for (std::uint32_t p : parts)
            sum += p;
        CHECK(sum == total);
        CHECK(parts[0] >= 1);
    }

    std::vector<std::uint32_t> const full = Apportion(wishes, 15);
    CHECK(full[0] == 2 && full[1] == 4 && full[2] == 4 && full[3] == 4 && full[4] == 1);

    // The defense table of 15 comes out as its weights: 2 / 5 / 5 / 0 / 3.
    std::vector<std::uint32_t> const defense = Apportion({ { 2, 1 }, { 5, 2 }, { 5, 2 }, { 0, 2 }, { 3, 0 } }, 15);
    CHECK(defense[0] == 2 && defense[1] == 5 && defense[2] == 5 && defense[3] == 0 && defense[4] == 3);

    // A group with weight zero stays closed; a total smaller than the minimums feeds the groups in list order.
    std::vector<std::uint32_t> const closed = Apportion({ { 2, 1 }, { 0, 2 }, { 4, 2 } }, 10);
    CHECK(closed[1] == 0);
    CHECK(closed[0] + closed[2] == 10);
    std::vector<std::uint32_t> const tiny = Apportion({ { 2, 1 }, { 4, 2 }, { 4, 2 } }, 2);
    CHECK(tiny[0] == 1 && tiny[1] == 1 && tiny[2] == 0);
    CHECK(Apportion({ { 0, 0 } }, 5)[0] == 0);
}

TEST(bgplan_record_table_creates_touches_and_drops_old_records)
{
    RecordTable<TestRecord> table;
    int dropped = 0;
    auto const onDrop = [&dropped](TestRecord const&) { ++dropped; };

    TestRecord& a = table.Of(10, 1000, onDrop);
    CHECK(a.instanceId == 10);
    CHECK(a.createdMs == 1000);
    a.payload = 7;

    TestRecord& same = table.Of(10, 5000, onDrop);
    CHECK(same.payload == 7);
    CHECK(same.lastTouchMs == 5000);

    // A new match, much later: the first record is older than the limit and goes.
    table.Of(11, 5000 + RECORD_MAX_AGE_MS + 1, onDrop);
    CHECK(dropped == 1);
    CHECK(table.records.size() == 1);
    CHECK(table.records.count(10) == 0);

    // The refresh and the epoch idiom.
    RecordBase base;
    CHECK(base.RefreshDue(100));
    base.lastRefreshMs = 100;
    CHECK(!base.RefreshDue(100 + REFRESH_MS - 1));
    CHECK(base.RefreshDue(100 + REFRESH_MS));
    std::uint32_t const before = base.epoch;
    base.BumpEpoch(4000);
    CHECK(base.epoch == before + 1);
    CHECK(base.epochChangedMs == 4000);
}
