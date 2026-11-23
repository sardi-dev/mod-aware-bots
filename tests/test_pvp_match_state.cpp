#include "TestHarness.h"
#include "PvpMatchState.h"

// BT-480: the per-match table: creation on the first touch, generations, the end hook, the sweep as a safety net and the
// inbox a session thread posts to.

using namespace PvpMatch;

namespace
{
PvpObs::Message Msg(std::uint32_t generation, std::uint8_t side, std::uint32_t sender)
{
    PvpObs::Message m;
    m.generation = generation;
    m.side = side;
    m.obs.senderLow = sender;
    return m;
}
}  // namespace

TEST(pvp_state_is_created_by_the_first_touch_and_found_afterwards)
{
    Clear();
    CHECK(!Find(7));
    Handle h = Touch(7, 489, 1000, false, 50000);
    CHECK(h);
    CHECK(Find(7) == h);
    CHECK(h->Key().instanceId == 7);
    CHECK(h->MapId() == 489);
    CHECK(Size() == 1);
    CHECK(Touch(7, 489, 3000, false, 52000) == h);
    CHECK(Size() == 1);
    Clear();
}

TEST(pvp_state_generations_differ_between_matches)
{
    Clear();
    Handle a = Touch(7, 489, 1000, false, 50000);
    Handle b = Touch(8, 489, 1000, false, 50000);
    CHECK(a->Key().generation != b->Key().generation);
    Clear();
}

TEST(pvp_state_a_reused_instance_gets_a_new_generation_and_empty_sides)
{
    Clear();
    Handle first = Touch(7, 489, 900000, false, 50000);
    first->Side(0).observations.items.push_back(PvpObs::Observation());
    Handle second = Touch(7, 489, 1000, false, 60000);
    CHECK(second != first);
    CHECK(second->Key().generation != first->Key().generation);
    CHECK(second->Side(0).observations.items.empty());
    CHECK(Find(7) == second);
    CHECK(Size() == 1);
    Clear();
}

TEST(pvp_state_end_drops_the_state_and_a_late_post_is_harmless)
{
    Clear();
    Handle h = Touch(7, 529, 1000, false, 50000);
    End(7);
    CHECK(!Find(7));
    CHECK(Size() == 0);
    // A session thread that found the handle just before the end can still post: the message dies with the state.
    CHECK(h->Post(Msg(h->Key().generation, 0, 5)));
    // End of an unknown instance does nothing.
    End(99);
    Clear();
}

TEST(pvp_state_drain_returns_valid_messages_in_order_and_empties_the_inbox)
{
    Clear();
    Handle h = Touch(7, 489, 1000, false, 50000);
    std::uint32_t const gen = h->Key().generation;
    CHECK(h->Post(Msg(gen, 0, 1)));
    CHECK(h->Post(Msg(gen + 1, 0, 2)));    // another generation
    CHECK(h->Post(Msg(gen, 5, 3)));        // not a side
    CHECK(h->Post(Msg(gen, 1, 4)));
    std::vector<PvpObs::Message> const out = h->Drain();
    CHECK(out.size() == 2);
    CHECK(out[0].obs.senderLow == 1);
    CHECK(out[1].obs.senderLow == 4);
    CHECK(h->Drain().empty());
    Clear();
}

TEST(pvp_state_inbox_is_bounded)
{
    Clear();
    Handle h = Touch(7, 489, 1000, false, 50000);
    std::uint32_t accepted = 0;
    for (std::uint32_t i = 0; i < PvpObs::INBOX_MAX + 20; ++i)
        if (h->Post(Msg(h->Key().generation, 0, i)))
            ++accepted;
    CHECK(accepted == PvpObs::INBOX_MAX);
    CHECK(h->Drain().size() == PvpObs::INBOX_MAX);
    Clear();
}

TEST(pvp_state_sides_are_isolated)
{
    Clear();
    Handle h = Touch(7, 489, 1000, false, 50000);
    PvpObs::Limits limits;
    PvpObs::Observation o;
    o.senderLow = 1;
    h->Side(0).observations.Add(o, 50000, limits);
    CHECK(h->Side(0).observations.items.size() == 1);
    CHECK(h->Side(1).observations.items.empty());
    Clear();
}

TEST(pvp_state_an_ended_match_is_swept_after_its_grace)
{
    Clear();
    std::uint32_t now = 100000;
    Handle live = Touch(1, 489, 1000, false, now);
    Handle over = Touch(2, 489, 1000, false, now);
    now += 1000;
    Touch(2, 489, 1500, true, now);      // a refresh saw the match over
    CHECK(over->ended);
    // Within the grace both stay; a later touch of the live match runs the sweep once the interval passed.
    now += 30000;
    Touch(1, 489, 31000, false, now);
    CHECK(Size() == 2);
    now += PvpObs::END_GRACE_MS + PvpObs::SWEEP_INTERVAL_MS;
    Touch(1, 489, 200000, false, now);
    CHECK(Find(1) == live);
    CHECK(!Find(2));
    Clear();
}

TEST(pvp_state_an_idle_match_is_swept_when_a_new_one_starts)
{
    Clear();
    std::uint32_t now = 100000;
    Touch(1, 489, 1000, false, now);
    now += PvpObs::IDLE_DROP_MS + 1000;
    Touch(2, 529, 1000, false, now);
    CHECK(!Find(1));
    CHECK(Find(2));
    CHECK(Size() == 1);
    Clear();
}

TEST(pvp_state_the_table_never_grows_past_its_size)
{
    Clear();
    std::uint32_t now = 100000;
    for (std::uint32_t i = 1; i <= PvpObs::MAX_MATCHES + 10; ++i)
    {
        Touch(i, 489, 1000, false, now);
        now += 10;
    }
    CHECK(Size() <= PvpObs::MAX_MATCHES);
    CHECK(Find(PvpObs::MAX_MATCHES + 10));     // the newest stays
    CHECK(!Find(1));                           // the oldest went
    Clear();
}

namespace
{
PvpObs::Message Say(std::uint32_t generation, std::uint8_t side, std::uint32_t sender, PvpObs::Kind kind, std::uint8_t place,
                    std::uint8_t count = 0)
{
    PvpObs::Message m = Msg(generation, side, sender);
    m.obs.kind = kind;
    m.obs.place = place;
    m.obs.count = count;
    return m;
}
}  // namespace

TEST(pvp_state_ingest_puts_a_call_into_the_store_of_its_side_with_its_life_from_now)
{
    Clear();
    Handle h = Touch(7, 489, 1000, false, 50000);
    PvpObs::Limits limits;
    h->Post(Say(h->Key().generation, 1, 11, PvpObs::Kind::INCOMING, 3, 4));
    std::vector<State::Ingested> const in = h->IngestInbox(52000, limits);
    CHECK(in.size() == 1);
    CHECK(in[0].side == 1);
    CHECK(in[0].obs.count == 4);
    CHECK(in[0].obs.receivedMs == 52000);
    CHECK(in[0].obs.expiresMs == 52000 + limits.ttlMs);
    CHECK(h->Side(1).observations.items.size() == 1);
    CHECK(h->Side(0).observations.items.empty());
    Clear();
}

TEST(pvp_state_ingest_drops_repeats_floods_and_lines_of_another_match)
{
    Clear();
    Handle h = Touch(7, 489, 1000, false, 50000);
    PvpObs::Limits limits;
    std::uint32_t const gen = h->Key().generation;

    h->Post(Say(gen, 0, 11, PvpObs::Kind::INCOMING, 3));
    h->Post(Say(gen, 0, 11, PvpObs::Kind::HELP, 4));               // same sender inside the gap
    h->Post(Say(gen + 1, 0, 12, PvpObs::Kind::INCOMING, 3));       // another generation
    h->Post(Say(gen, 0, 13, PvpObs::Kind::INCOMING, 3));
    std::vector<State::Ingested> in = h->IngestInbox(52000, limits);
    CHECK(in.size() == 2);
    CHECK(in[0].obs.senderLow == 11);
    CHECK(in[1].obs.senderLow == 13);

    // The same line again from the same sender a few seconds later, while the first is fresh: a repeat.
    h->Post(Say(gen, 0, 11, PvpObs::Kind::INCOMING, 3));
    CHECK(h->IngestInbox(56000, limits).empty());
    // A lot of different senders: the inbox and the store both stay bounded.
    for (std::uint32_t i = 0; i < 200; ++i)
        h->Post(Say(gen, 0, 100 + i, PvpObs::Kind::INCOMING, std::uint8_t(i % 5)));
    h->IngestInbox(58000, limits);
    CHECK(h->Side(0).observations.items.size() <= limits.maxPerSide);
    Clear();
}

// --- BT-502: the answers of the leader's asks -------------------------------------------------------------------------------

namespace
{
AiPvp::RawAnswer Answer(std::uint8_t side, std::uint32_t requestId)
{
    AiPvp::RawAnswer a;
    a.side = side;
    a.requestId = requestId;
    return a;
}
}  // namespace

TEST(pvp_state_answers_are_queued_by_the_io_thread_and_taken_by_side_in_order)
{
    Clear();
    Handle h = Touch(7, 489, 1000, false, 50000);
    CHECK(h->PostAnswer(Answer(0, 1)));
    CHECK(h->PostAnswer(Answer(1, 2)));
    CHECK(h->PostAnswer(Answer(0, 3)));

    std::vector<AiPvp::RawAnswer> const alliance = h->TakeAnswers(0);
    CHECK(alliance.size() == 2);
    CHECK(alliance[0].requestId == 1 && alliance[1].requestId == 3);
    // The other side's answer stays queued until its own refresh takes it, and nothing is taken twice.
    CHECK(h->TakeAnswers(0).empty());
    std::vector<AiPvp::RawAnswer> const horde = h->TakeAnswers(1);
    CHECK(horde.size() == 1 && horde[0].requestId == 2);
    Clear();
}

TEST(pvp_state_the_answer_queue_is_bounded)
{
    Clear();
    Handle h = Touch(7, 489, 1000, false, 50000);
    for (std::size_t i = 0; i < AiPvp::MAX_QUEUED_ANSWERS; ++i)
        CHECK(h->PostAnswer(Answer(0, std::uint32_t(i + 1))));
    CHECK(!h->PostAnswer(Answer(0, 99)));
    CHECK(h->TakeAnswers(0).size() == AiPvp::MAX_QUEUED_ANSWERS);
    CHECK(h->PostAnswer(Answer(0, 100)));
    Clear();
}

TEST(pvp_state_a_late_answer_to_a_dropped_state_is_harmless)
{
    Clear();
    Handle h = Touch(7, 489, 1000, false, 50000);
    Handle held = h;      // what the IO callback holds
    End(7);
    CHECK(!Find(7));
    CHECK(held->PostAnswer(Answer(0, 1)));
    CHECK(held->TakeAnswers(0).size() == 1);
    Clear();
}

TEST(pvp_state_each_side_has_its_own_asks)
{
    Clear();
    Handle h = Touch(7, 489, 100000, false, 50000);
    h->Side(0).ai.pacing.requests = 5;
    CHECK(h->Side(1).ai.pacing.requests == 0);
    // A new generation of the same instance starts with no asks at all.
    Handle next = Touch(7, 489, 100, false, 90000);
    CHECK(next != h);
    CHECK(next->Side(0).ai.pacing.requests == 0);
    Clear();
}
