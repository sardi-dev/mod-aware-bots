#include "TestHarness.h"
#include "PvpBurstPolicy.h"
#include "PvpTeamPolicy.h"

// BT-628 to BT-631: the pieces together over time: a healer lock chain against the diminishing returns of an awareness book, the
// executor choice across two bots, the window and the burst reasons for each tier, and the match timing of the go. Deterministic
// (no clock, no random numbers): the tests pass the times in.

using namespace PvpTeam;
using PvpAware::Book;

namespace
{
constexpr std::uint64_t HEALER = 0x3001;
constexpr std::uint32_t HEALER_LOW = 0x3001;
constexpr std::uint32_t MAGE = 11;
constexpr std::uint32_t ROGUE = 12;
constexpr std::uint32_t T0 = 200000;

PvpAware::Sight Seen() { return { 20.0f, true, true }; }
PvpAware::Interest Key() { PvpAware::Interest i; i.actorKey = true; i.ownCast = true; return i; }

// Books a crowd control the observer saw land: scans for an event number the observer notices (the notice roll is fixed per event).
void BookCc(Book& b, DrCategory category, std::uint32_t spell, std::uint32_t at, std::uint32_t durationMs, std::uint32_t& seq)
{
    PvpAware::Event e;
    e.target = HEALER;
    e.atMs = at;
    for (std::uint32_t s = seq; s < seq + 5000; ++s)
    {
        e.seq = s;
        e.spellId = spell;
        if (b.ObserveCcLanded(e, category, durationMs, Seen(), Key(), at))
        {
            seq = s + 1;
            return;
        }
    }
    CHECK(false);
}

ControlCandidate FromKit(std::uint32_t member, std::uint32_t spell, DrCategory category, std::uint32_t baseMs, std::uint32_t castMs,
    bool breakable, Book& book, std::uint32_t now)
{
    ControlCandidate c;
    c.member = member;
    c.spellId = spell;
    c.category = category;
    c.baseMs = baseMs;
    c.castMs = castMs;
    c.breakable = breakable;
    c.drStep = book.DrStep(HEALER, category, now);
    return c;
}
}  // namespace

TEST(scenario_expert_chains_sheep_then_stun_with_no_gap_and_no_double_cast)
{
    Book book(MAGE, Tier::EXPERT);
    ControlBook team;
    std::uint32_t seq = 1;
    std::uint32_t now = T0;

    // t = 0: the healer is free. The mage (Polymorph) and the rogue (Kidney Shot) both publish; the better offer executes.
    ControlTarget free;
    std::int32_t const sheepScore = ScoreControl(FromKit(MAGE, 118, DrCategory::INCAPACITATE, 8000, 1700, true, book, now), free, Tier::EXPERT);
    std::int32_t const kidneyScore = ScoreControl(FromKit(ROGUE, 408, DrCategory::STUN, 6000, 0, false, book, now), free, Tier::EXPERT);
    CHECK(sheepScore > 0 && kidneyScore > 0);
    team.Publish({ MAGE, HEALER_LOW, 118, DrCategory::INCAPACITATE, sheepScore, now }, now);
    team.Publish({ ROGUE, HEALER_LOW, 408, DrCategory::STUN, kidneyScore, now }, now);
    bool const mageFirst = sheepScore >= kidneyScore;
    CHECK(team.IsExecutor(mageFirst ? MAGE : ROGUE, HEALER_LOW, now));
    CHECK(!team.IsExecutor(mageFirst ? ROGUE : MAGE, HEALER_LOW, now));

    // The executor casts the sheep: the cast is pending, so the other bot is blocked for a hard control for 2.5 s.
    team.NoteCast(MAGE, HEALER_LOW, DrCategory::INCAPACITATE, true, now);
    CHECK(team.Blocked(ROGUE, HEALER_LOW, DrCategory::STUN, true, now + 1000));
    BookCc(book, DrCategory::INCAPACITATE, 118, now + 1700, 8000, seq);   // lands after the cast time, 8 s
    now += 1700;

    // While the sheep runs the stun is not due; at the lead of the stun (instant) the link goes out, before the sheep ends.
    ControlTarget running;
    running.lockedMs = 8000;
    running.lockCategory = DrCategory::INCAPACITATE;
    CHECK(ScoreControl(FromKit(ROGUE, 408, DrCategory::STUN, 6000, 0, false, book, now), running, Tier::EXPERT) == NOT_DUE);
    running.lockedMs = LeadMs(0, Tier::EXPERT);
    now += 8000 - running.lockedMs;
    std::int32_t const chain = ScoreControl(FromKit(ROGUE, 408, DrCategory::STUN, 6000, 0, false, book, now), running, Tier::EXPERT);
    CHECK(chain > 0);
    // The stun lands before the sheep ends: no gap in which the healer could act.
    CHECK(running.lockedMs > 0);

    // A second sheep into the same category would be half length (the book has seen one): not the best link.
    std::int32_t const sheepAgain = ScoreControl(FromKit(MAGE, 118, DrCategory::INCAPACITATE, 8000, 1700, true, book, now), running, Tier::EXPERT);
    CHECK(sheepAgain == NOT_DUE || sheepAgain < chain);
}

TEST(scenario_diminishing_returns_wear_the_category_out_and_reset_after_eighteen_seconds)
{
    Book book(MAGE, Tier::EXPERT);
    std::uint32_t seq = 1;
    std::uint32_t at = T0;
    ControlTarget free;

    CHECK(ScoreControl(FromKit(MAGE, 118, DrCategory::INCAPACITATE, 8000, 1700, true, book, at), free, Tier::EXPERT) > 0);
    BookCc(book, DrCategory::INCAPACITATE, 118, at, 8000, seq);
    at += 9000;   // the first sheep ended a second ago: step 1, half length (4 s)
    CHECK(book.DrStepExact(HEALER, DrCategory::INCAPACITATE, at) == 1);
    CHECK(ScoreControl(FromKit(MAGE, 118, DrCategory::INCAPACITATE, 8000, 1700, true, book, at), free, Tier::EXPERT) > 0);
    BookCc(book, DrCategory::INCAPACITATE, 118, at, 4000, seq);
    at += 5000;   // step 2, a quarter of 8 s is 2 s: under the useful length
    CHECK(book.DrStepExact(HEALER, DrCategory::INCAPACITATE, at) == 2);
    CHECK(ScoreControl(FromKit(MAGE, 118, DrCategory::INCAPACITATE, 8000, 1700, true, book, at), free, Tier::EXPERT) == NOT_DUE);
    // A stun is untouched by the sheep's category.
    CHECK(ScoreControl(FromKit(ROGUE, 408, DrCategory::STUN, 6000, 0, false, book, at), free, Tier::EXPERT) > 0);
    // The reset: 18 s after the last effect ended the sheep is full again.
    at += 20000;
    CHECK(book.DrStepExact(HEALER, DrCategory::INCAPACITATE, at) == 0);
    CHECK(ScoreControl(FromKit(MAGE, 118, DrCategory::INCAPACITATE, 8000, 1700, true, book, at), free, Tier::EXPERT) > 0);
}

TEST(scenario_window_then_go_then_partner_burst_in_time)
{
    // The healer is polymorphed for 8 s: the window opens for good and expert bots, the go is shared, a partner that bursts 1.2 s
    // later is aligned, and the go has run out 12 s after it opened.
    Go go;
    WindowInputs in;
    in.healerPresent = true;
    in.healerLockedMs = 8000;
    WindowVerdict const v = EvaluateWindow(in, Tier::GOOD);
    CHECK(v.open);
    CHECK(OpenGo(go, 77, T0, v.reason));
    CHECK(GoActive(go, 77, T0 + 3000));
    CHECK(Aligned(T0 + 100, T0 + 1300));
    CHECK(!Aligned(T0 + 100, T0 + 2000));
    CHECK(!GoActive(go, 77, T0 + GO_HOLD_MS));

    // The same state for a casual bot: no window.
    CHECK(!EvaluateWindow(in, Tier::CASUAL).open);
}

TEST(scenario_burst_reasons_follow_the_team_window_for_each_tier)
{
    PvpBurst::Inputs in;
    in.targetIsEnemyPlayer = true;
    in.fightMs = 2000;
    // Nothing yet: expert and good hold, casual goes.
    CHECK(!PvpBurst::Decide(in, Tier::EXPERT).open);
    CHECK(!PvpBurst::Decide(in, Tier::GOOD).open);
    CHECK(PvpBurst::Decide(in, Tier::CASUAL).open);
    // The team window opens: expert and good go at once with the team's reason.
    in.teamWindow = true;
    CHECK(PvpBurst::Decide(in, Tier::EXPERT).reason == PvpBurst::Reason::TEAM_WINDOW);
    CHECK(PvpBurst::Decide(in, Tier::GOOD).reason == PvpBurst::Reason::TEAM_WINDOW);
    // The kill target pops an immunity: expert and good hold again.
    in.immune = true;
    CHECK(!PvpBurst::Decide(in, Tier::EXPERT).open);
    CHECK(!PvpBurst::Decide(in, Tier::GOOD).open);
    // A partner already went in: the same answer through the other reason.
    in.immune = false;
    in.teamWindow = false;
    in.partnerBurst = true;
    CHECK(PvpBurst::Decide(in, Tier::GOOD).reason == PvpBurst::Reason::PARTNER_BURST);
}

TEST(scenario_nobody_breaks_the_sheep)
{
    // The healer is in a sheep with 6 s left: damage is held for good and expert bots, allowed when it is nearly over and for
    // a casual bot; the burst window is closed on that target for the tiers that read it.
    CHECK(HoldDamage(true, 6000, true, Tier::EXPERT));
    CHECK(!HoldDamage(true, 500, true, Tier::EXPERT));
    CHECK(!HoldDamage(true, 6000, true, Tier::CASUAL));
    PvpBurst::Inputs in;
    in.targetIsEnemyPlayer = true;
    in.breakableCc = true;
    in.teamWindow = true;
    CHECK(!PvpBurst::Decide(in, Tier::EXPERT).open);
    CHECK(PvpBurst::Decide(in, Tier::CASUAL).open);
}
