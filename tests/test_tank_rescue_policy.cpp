#include "TestHarness.h"
#include "TankRescuePolicy.h"

#include <string>
#include <vector>
TEST(tank_rescue_priority_and_area)
{
    using namespace TankRescuePolicy;
    CHECK(Better({true, 100, false, 20}, {false, 1, true, 1}));
    CHECK(Better({false, 20, false, 20}, {false, 21, true, 1}));
    CHECK(Better({false, 20, true, 20}, {false, 20, false, 1}));
    CHECK(Better({true, 100, true, 20}, {true, 1, false, 1}));
    CHECK(Better({false, 20, true, 1}, {false, 20, true, 2}));
    CHECK(!UseArea(1, true));
    CHECK(UseArea(2, true));
    CHECK(!UseArea(2, false));
}

namespace
{
using namespace TankRescuePolicy;

struct Candidate
{
    uint64_t attacker;
    uint64_t victim;
    bool healer;
    float health;
    float distance;
    bool inSight;
};

// The same pick as AttackedMemberRescue::FindAttacker: skip what the book sets aside, rank the rest. 0 = none.
uint64_t Pick(GiveUpBook const& book, std::vector<Candidate> const& all, time_t now)
{
    uint64_t pick = 0;
    Priority best = {};
    for (Candidate const& c : all)
    {
        bool unreached = false;
        if (!Consider(book, c.attacker, c.victim, now, [&] { return c.inSight; }, unreached))
            continue;
        Priority const p = { c.healer, c.health, false, c.distance, unreached };
        if (!pick || Better(p, best))
        {
            pick = c.attacker;
            best = p;
        }
    }
    return pick;
}

void GiveUp(GiveUpBook& book, std::vector<Candidate> const& all, uint64_t attacker, time_t now)
{
    for (Candidate const& c : all)
        if (c.attacker == attacker)
        {
            RecordGiveUp(book.attackers, c.attacker, now);
            RecordGiveUp(book.victims, c.victim, now);
        }
}

constexpr uint64_t HEALER = 100;
constexpr uint64_t LEADER = 200;
constexpr uint64_t OFFICER_1 = 1001;
constexpr uint64_t OFFICER_2 = 1002;
constexpr uint64_t OFFICER_3 = 1003;
constexpr uint64_t GUARD = 2001;
}  // namespace

// Shadowfang Keep run 7: three Officers on a healer the tank cannot reach, one Guardsman on the leader.
TEST(tank_rescue_unreachable_healer_attacker_does_not_hide_the_leaders)
{
    std::vector<Candidate> const all = {
        { OFFICER_1, HEALER, true, 100.0f, 90.0f, false },
        { OFFICER_2, HEALER, true, 100.0f, 91.0f, false },
        { OFFICER_3, HEALER, true, 100.0f, 92.0f, false },
        { GUARD, LEADER, false, 60.0f, 20.0f, true },
    };
    GiveUpBook book;
    time_t now = 1000;
    // Before any give-up the healer's attackers come first, as before.
    CHECK(Pick(book, all, now) == OFFICER_1);
    GiveUp(book, all, OFFICER_1, now + WalkSeconds);
    now += WalkSeconds;
    // One give-up is enough for the next pick to be the leader's attacker, not the healer's other two.
    CHECK(Pick(book, all, now) == GUARD);
    CHECK(Pick(book, all, now + 5) == GUARD);
}

TEST(tank_rescue_leader_attacker_kept_when_only_unreached_ones_remain)
{
    std::vector<Candidate> const all = {
        { OFFICER_2, HEALER, true, 100.0f, 91.0f, false },
        { OFFICER_3, HEALER, true, 100.0f, 92.0f, false },
    };
    GiveUpBook book;
    GiveUp(book, { { OFFICER_1, HEALER, true, 100.0f, 90.0f, false } }, OFFICER_1, 1000);
    // Nothing else needs the tank: the healer's other attackers are still tried (the walk may find a way).
    CHECK(Pick(book, all, 1008) == OFFICER_2);
}

TEST(tank_rescue_gives_up_twice_ignores_sixty_seconds)
{
    std::vector<Candidate> const all = { { OFFICER_1, HEALER, true, 100.0f, 90.0f, false } };
    GiveUpBook book;
    CHECK(!RecordGiveUp(book.attackers, OFFICER_1, 1000));
    CHECK(SetAside(book.attackers, OFFICER_1, 1000 + RetrySeconds - 1));
    CHECK(!SetAside(book.attackers, OFFICER_1, 1000 + RetrySeconds));
    // The second give-up starts the 60 s ignore, counted from that give-up.
    CHECK(RecordGiveUp(book.attackers, OFFICER_1, 1018));
    CHECK(SetAside(book.attackers, OFFICER_1, 1018 + IgnoreSeconds - 1));
    CHECK(!SetAside(book.attackers, OFFICER_1, 1018 + IgnoreSeconds));
    // After the ignore ran out the attacker is a candidate again, and needs two new give-ups.
    CHECK(Pick(book, all, 1018 + IgnoreSeconds) == OFFICER_1);
    CHECK(!RecordGiveUp(book.attackers, OFFICER_1, 1018 + IgnoreSeconds + 8));
    CHECK(RecordGiveUp(book.attackers, OFFICER_1, 1018 + IgnoreSeconds + 26));
}

TEST(tank_rescue_old_give_up_does_not_count_toward_an_ignore)
{
    GiveUpBook book;
    CHECK(!RecordGiveUp(book.attackers, OFFICER_1, 1000));
    // The next give-up comes a minute later: the first one expired, so this is a first give-up again.
    CHECK(!RecordGiveUp(book.attackers, OFFICER_1, 1000 + IgnoreSeconds));
    CHECK(!SetAside(book.attackers, OFFICER_1, 1000 + IgnoreSeconds + RetrySeconds));
}

TEST(tank_rescue_sight_beats_the_book)
{
    std::vector<Candidate> all = {
        { OFFICER_1, HEALER, true, 100.0f, 10.0f, false },
        { GUARD, LEADER, false, 60.0f, 20.0f, true },
    };
    GiveUpBook book;
    GiveUp(book, all, OFFICER_1, 1000);
    GiveUp(book, all, OFFICER_1, 1018);
    CHECK(Pick(book, all, 1019) == GUARD);
    // The healer ran back into the tank's sight: she is rescued first again, ignore or not.
    all[0].inSight = true;
    CHECK(Pick(book, all, 1019) == OFFICER_1);
}

TEST(tank_rescue_sight_is_asked_only_for_listed_units)
{
    GiveUpBook book;
    bool asked = false;
    bool unreached = true;
    CHECK(Consider(book, OFFICER_1, HEALER, 1000, [&] { asked = true; return false; }, unreached));
    CHECK(!asked);
    CHECK(!unreached);
}

TEST(tank_rescue_ledger_makes_room_by_dropping_the_oldest)
{
    GiveUpLedger ledger;
    for (unsigned i = 0; i < LedgerSlots; ++i)
        RecordGiveUp(ledger, 500 + i, 1000 + i);
    RecordGiveUp(ledger, 900, 1100);
    CHECK(Find(ledger, 900) != nullptr);
    CHECK(Find(ledger, 500) == nullptr);
    CHECK(Find(ledger, 501) != nullptr);
}

TEST(tank_rescue_give_up_reason_names_the_cause)
{
    using namespace TankRescuePolicy;
    CHECK(ClassifyGiveUp(true, false, 90.0f, 12.0f) == GiveUpReason::NO_PATH);
    CHECK(ClassifyGiveUp(false, true, 20.0f, 0.0f) == GiveUpReason::INCOMPLETE_PATH);
    CHECK(ClassifyGiveUp(false, false, 20.0f, 6.0f) == GiveUpReason::FLOOR_DIFFERENCE);
    CHECK(ClassifyGiveUp(false, false, 20.0f, -6.0f) == GiveUpReason::FLOOR_DIFFERENCE);
    CHECK(ClassifyGiveUp(false, false, 90.0f, 1.0f) == GiveUpReason::TOO_FAR);
    CHECK(ClassifyGiveUp(false, false, 20.0f, 1.0f) == GiveUpReason::NO_SIGHT);
    CHECK(std::string(ReasonName(GiveUpReason::NO_PATH)) == "no-path");
    CHECK(std::string(ReasonName(GiveUpReason::TOO_FAR)) == "distance");
}
