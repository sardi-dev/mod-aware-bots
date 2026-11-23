#include "TestHarness.h"
#include "PvpAwarenessPolicy.h"

// BT-520: the per-bot view of enemy state: what a tier notices, how long it remembers, the cooldown windows, the school
// locks, the diminishing returns and the last seen position of an enemy.

using namespace PvpAware;
using namespace PvpSpells;

namespace
{
constexpr std::uint64_t ENEMY = 0x1001;
constexpr std::uint64_t ENEMY2 = 0x1002;
constexpr std::uint64_t US = 0x2001;
constexpr std::uint32_t OBS = 777;
constexpr std::uint32_t T0 = 100000;

Sight Seen() { return { 20.0f, true, true }; }

Interest Key() { Interest i; i.actorKey = true; i.actorNearestKicker = true; i.ownCast = true; return i; }

Event Cast(std::uint32_t seq, std::uint64_t actor, std::uint32_t spell, std::uint32_t at, std::uint8_t cls = 0)
{
    Event e;
    e.seq = seq;
    e.atMs = at;
    e.actor = actor;
    e.spellId = spell;
    e.actorClass = cls;
    return e;
}

// A seq the book of this observer notices (rolls are fixed per event, so find one by scanning).
std::uint32_t NoticedSeq(Book& b, std::uint32_t spell, Interest const& i, std::uint32_t from = 1)
{
    PvpSpells::Entry const* entry = Find(spell);
    for (std::uint32_t s = from; s < from + 5000; ++s)
        if (Roll(b.ObserverGuid(), s, 1) < NoticePercent(b.Model(), entry->kind, i))
            return s;
    return 0;
}
}  // namespace

TEST(pvp_aware_tiers_track_less_from_expert_to_casual)
{
    TierModel const e = ModelOf(Tier::EXPERT);
    TierModel const g = ModelOf(Tier::GOOD);
    TierModel const c = ModelOf(Tier::CASUAL);
    CHECK(e.sightYards > g.sightYards && g.sightYards > c.sightYards);
    CHECK(e.trinketScope == Scope::ALL && g.trinketScope == Scope::KEY && c.trinketScope == Scope::NONE);
    CHECK(e.memoryMs == 0 && g.memoryMs == 60000 && c.memoryMs == 15000);
    CHECK(e.mistakePct == 5 && g.mistakePct == 15 && c.mistakePct == 30);
    CHECK(c.drIgnorePct == 50 && e.drIgnorePct == 0);
    // Only the expert keeps a kick on the cooldown for its whole length.
    CHECK(e.kickMemoryMs == 0 && g.kickMemoryMs == 5000 && c.kickScope == Scope::NONE);
}

TEST(pvp_aware_sight_needs_range_line_of_sight_and_a_detectable_enemy)
{
    TierModel const m = ModelOf(Tier::EXPERT);
    CHECK(CanSee(m, { 30.0f, true, true }));
    CHECK(!CanSee(m, { 30.0f, false, true }));   // behind a pillar
    CHECK(!CanSee(m, { 30.0f, true, false }));   // stealthed
    CHECK(!CanSee(m, { 90.0f, true, true }));    // too far
    CHECK(!CanSee(ModelOf(Tier::CASUAL), { 40.0f, true, true }));
    CHECK(CanSee(ModelOf(Tier::CASUAL), { 30.0f, true, true }));
}

TEST(pvp_aware_a_cast_out_of_sight_is_not_booked)
{
    Book b(OBS, Tier::EXPERT);
    std::uint32_t const seq = NoticedSeq(b, TRINKET_SPELL, Key());
    CHECK(seq != 0);
    CHECK(!b.ObserveCast(Cast(seq, ENEMY, TRINKET_SPELL, T0), { 20.0f, false, true }, Key(), T0));
    CHECK(!b.ObserveCast(Cast(seq, ENEMY, TRINKET_SPELL, T0), { 20.0f, true, false }, Key(), T0));
    CHECK(b.TrinketReading(ENEMY, T0).state == State::UNKNOWN);
    CHECK(b.ObserveCast(Cast(seq, ENEMY, TRINKET_SPELL, T0), Seen(), Key(), T0));
    CHECK(b.TrinketReading(ENEMY, T0 + 1).state == State::DOWN);
}

TEST(pvp_aware_a_spell_outside_the_catalog_is_ignored)
{
    Book b(OBS, Tier::EXPERT);
    CHECK(!b.ObserveCast(Cast(1, ENEMY, 116, T0), Seen(), Key(), T0));  // Frostbolt
    CHECK(b.ActorCount() == 0);
}

TEST(pvp_aware_the_notice_roll_is_stable_per_event_and_rates_follow_the_tier)
{
    Book e(OBS, Tier::EXPERT);
    int noticed = 0;
    for (std::uint32_t s = 1; s <= 1000; ++s)
        if (Roll(OBS, s, 1) < NoticePercent(e.Model(), Kind::TRINKET, Key()))
            ++noticed;
    CHECK(noticed > 950 && noticed <= 1000);

    Book g(OBS, Tier::GOOD);
    noticed = 0;
    for (std::uint32_t s = 1; s <= 1000; ++s)
        if (Roll(OBS, s, 1) < NoticePercent(g.Model(), Kind::TRINKET, Key()))
            ++noticed;
    CHECK(noticed > 850 && noticed < 950);

    // Stable: the same seq answers the same way every time.
    for (std::uint32_t s = 1; s <= 50; ++s)
        CHECK(Roll(OBS, s, 1) == Roll(OBS, s, 1));
    // Different observers do not all notice the same events.
    int differ = 0;
    for (std::uint32_t s = 1; s <= 200; ++s)
        if ((Roll(1, s, 1) < 90) != (Roll(2, s, 1) < 90))
            ++differ;
    CHECK(differ > 5);
}

TEST(pvp_aware_a_good_bot_tracks_only_the_key_enemies_and_a_casual_bot_no_trinket_at_all)
{
    Interest other;  // not the healer, not the focus
    TierModel const g = ModelOf(Tier::GOOD);
    CHECK(NoticePercent(g, Kind::TRINKET, other) == 0);
    CHECK(NoticePercent(g, Kind::TRINKET, Key()) == 90);
    // Immunities only: a tempo defensive is not tracked, an immunity of a key enemy is.
    CHECK(NoticePercent(g, Kind::DEFENSIVE_TEMPO, Key()) == 0);
    CHECK(NoticePercent(g, Kind::DEFENSIVE_IMMUNITY, Key()) == 85);
    CHECK(NoticePercent(g, Kind::DEFENSIVE_IMMUNITY, other) == 0);
    // The nearest kicker, and only it.
    Interest kicker;
    kicker.actorNearestKicker = true;
    CHECK(NoticePercent(g, Kind::INTERRUPT, kicker) == 80);
    CHECK(NoticePercent(g, Kind::INTERRUPT, other) == 0);
    TierModel const c = ModelOf(Tier::CASUAL);
    CHECK(NoticePercent(c, Kind::TRINKET, Key()) == 0);
    CHECK(NoticePercent(c, Kind::INTERRUPT, Key()) == 0);
    CHECK(NoticePercent(c, Kind::DEFENSIVE_TEMPO, other) == 50);
}

TEST(pvp_aware_trinket_is_down_maybe_then_ready_over_the_two_minutes)
{
    Book b(OBS, Tier::EXPERT);
    std::uint32_t const seq = NoticedSeq(b, TRINKET_SPELL, Key());
    CHECK(b.ObserveCast(Cast(seq, ENEMY, TRINKET_SPELL, T0), Seen(), Key(), T0));
    Reading r = b.TrinketReading(ENEMY, T0 + 10000);
    CHECK(r.state == State::DOWN);
    CHECK(r.remainingMs == 110000);
    CHECK(!AssumeReady(r));
    // 85 percent of 120 s is 102 s: a talent may have ended it after that.
    CHECK(b.TrinketReading(ENEMY, T0 + 101000).state == State::DOWN);
    CHECK(b.TrinketReading(ENEMY, T0 + 103000).state == State::MAYBE);
    CHECK(AssumeReady(b.TrinketReading(ENEMY, T0 + 103000)));
    CHECK(!KnownReady(b.TrinketReading(ENEMY, T0 + 103000)));
    CHECK(b.TrinketReading(ENEMY, T0 + 121000).state == State::READY);
    CHECK(KnownReady(b.TrinketReading(ENEMY, T0 + 121000)));
    // An enemy never seen using it is UNKNOWN, not ready.
    CHECK(b.TrinketReading(ENEMY2, T0).state == State::UNKNOWN);
    CHECK(AssumeReady(b.TrinketReading(ENEMY2, T0)));
}

TEST(pvp_aware_every_man_for_himself_is_the_medallion_for_planning)
{
    Book b(OBS, Tier::EXPERT);
    std::uint32_t const seq = NoticedSeq(b, EVERY_MAN_FOR_HIMSELF, Key());
    CHECK(b.ObserveCast(Cast(seq, ENEMY, EVERY_MAN_FOR_HIMSELF, T0), Seen(), Key(), T0));
    CHECK(b.TrinketReading(ENEMY, T0 + 1000).state == State::DOWN);
    // Will of the Forsaken has its own two minutes and does not touch the medallion reading.
    Book c(OBS, Tier::EXPERT);
    std::uint32_t const seq2 = NoticedSeq(c, 7744, Key());
    CHECK(c.ObserveCast(Cast(seq2, ENEMY, 7744, T0), Seen(), Key(), T0));
    CHECK(c.TrinketReading(ENEMY, T0 + 1000).state == State::UNKNOWN);
    CHECK(c.CooldownOf(ENEMY, 7744, T0 + 1000).state == State::DOWN);
}

TEST(pvp_aware_a_casual_bot_never_books_a_trinket_and_a_good_bot_forgets_it_after_a_minute)
{
    Book c(OBS, Tier::CASUAL);
    for (std::uint32_t s = 1; s <= 200; ++s)
        CHECK(!c.ObserveCast(Cast(s, ENEMY, TRINKET_SPELL, T0), Seen(), Key(), T0));
    CHECK(c.TrinketReading(ENEMY, T0 + 1000).state == State::UNKNOWN);

    Book g(OBS, Tier::GOOD);
    std::uint32_t const seq = NoticedSeq(g, TRINKET_SPELL, Key());
    CHECK(g.ObserveCast(Cast(seq, ENEMY, TRINKET_SPELL, T0), { 20.0f, true, true }, Key(), T0));
    CHECK(g.TrinketReading(ENEMY, T0 + 30000).state == State::DOWN);
    // Past its minute of memory the good bot no longer knows, and assumes the enemy has it again.
    CHECK(g.TrinketReading(ENEMY, T0 + 61000).state == State::UNKNOWN);
    CHECK(AssumeReady(g.TrinketReading(ENEMY, T0 + 61000)));
}

TEST(pvp_aware_the_expert_remembers_the_whole_cooldown)
{
    Book b(OBS, Tier::EXPERT);
    std::uint32_t const seq = NoticedSeq(b, 45438, Key());
    CHECK(b.ObserveCast(Cast(seq, ENEMY, 45438, T0, CLS_MAGE), Seen(), Key(), T0));  // Ice Block, 5 minutes
    CHECK(b.CooldownOf(ENEMY, 45438, T0 + 200000).state == State::DOWN);
    CHECK(b.CooldownOf(ENEMY, 45438, T0 + 200000).remainingMs == 100000);
    CHECK(b.CooldownOf(ENEMY, 45438, T0 + 300000).state == State::READY);
}

TEST(pvp_aware_a_kick_seen_used_puts_the_class_kick_down_until_it_is_back)
{
    Book b(OBS, Tier::EXPERT);
    CHECK(Book::HasStopper(CLS_WARRIOR));
    CHECK(b.KickReading(ENEMY, CLS_WARRIOR, T0).state == State::UNKNOWN);  // never seen: assume it has it
    std::uint32_t const seq = NoticedSeq(b, 6552, Key());                  // Pummel
    CHECK(b.ObserveCast(Cast(seq, ENEMY, 6552, T0, CLS_WARRIOR), Seen(), Key(), T0));
    Reading r = b.KickReading(ENEMY, CLS_WARRIOR, T0 + 5000);
    CHECK(r.state == State::DOWN);
    CHECK(r.remainingMs == 10000);
    CHECK(b.KickReading(ENEMY, CLS_WARRIOR, T0 + 14000).state == State::MAYBE);
    CHECK(b.KickReading(ENEMY, CLS_WARRIOR, T0 + 16000).state == State::READY);
}

TEST(pvp_aware_a_secondary_stopper_never_seen_does_not_keep_the_kick_up)
{
    Book b(OBS, Tier::EXPERT);
    // A death knight: Mind Freeze is sure, Strangulate is a spec. Mind Freeze seen down: the kick reads DOWN.
    std::uint32_t const seq = NoticedSeq(b, 47528, Key());
    CHECK(b.ObserveCast(Cast(seq, ENEMY, 47528, T0, CLS_DEATH_KNIGHT), Seen(), Key(), T0));
    CHECK(b.KickReading(ENEMY, CLS_DEATH_KNIGHT, T0 + 2000).state == State::DOWN);
    // Strangulate seen next: it is up, so the death knight can still stop a cast.
    std::uint32_t const seq2 = NoticedSeq(b, 47476, Key(), seq + 1);
    CHECK(b.ObserveCast(Cast(seq2, ENEMY, 47476, T0 + 3000, CLS_DEATH_KNIGHT), Seen(), Key(), T0 + 3000));
    CHECK(b.KickReading(ENEMY, CLS_DEATH_KNIGHT, T0 + 4000).state == State::DOWN);  // both seen, both down
    CHECK(b.KickReading(ENEMY, CLS_DEATH_KNIGHT, T0 + 17000).state == State::READY); // Mind Freeze is back
}

TEST(pvp_aware_a_good_bot_only_knows_a_kick_for_five_seconds)
{
    Book g(OBS, Tier::GOOD);
    Interest kicker;
    kicker.actorNearestKicker = true;
    std::uint32_t const seq = NoticedSeq(g, 2139, kicker);  // Counterspell, 24 s
    CHECK(g.ObserveCast(Cast(seq, ENEMY, 2139, T0, CLS_MAGE), Seen(), kicker, T0));
    CHECK(g.KickReading(ENEMY, CLS_MAGE, T0 + 3000).state == State::DOWN);
    CHECK(g.KickReading(ENEMY, CLS_MAGE, T0 + 8000).state == State::UNKNOWN);  // forgot: assumes it is up
    // A casual bot never tracks kicks.
    Book c(OBS, Tier::CASUAL);
    for (std::uint32_t s = 1; s <= 100; ++s)
        CHECK(!c.ObserveCast(Cast(s, ENEMY, 2139, T0, CLS_MAGE), Seen(), kicker, T0));
}

TEST(pvp_aware_a_kick_that_landed_locks_the_school_for_its_lockout)
{
    Book b(OBS, Tier::EXPERT);
    Event e = Cast(1, ENEMY, 2139, T0, CLS_MAGE);  // Counterspell on the enemy healer
    e.target = ENEMY2;
    e.castSchool = SCHOOL_HOLY;
    std::uint32_t const seq = NoticedSeq(b, 2139, Key());
    e.seq = seq;
    CHECK(b.ObserveCast(e, Seen(), Key(), T0));
    CHECK(b.SchoolLocked(ENEMY2, SCHOOL_HOLY, T0 + 5000));
    CHECK(!b.SchoolLocked(ENEMY2, SCHOOL_HOLY, T0 + 6500));   // 6 s lockout
    CHECK(!b.SchoolLocked(ENEMY2, SCHOOL_NATURE, T0 + 1000)); // another school is open
    CHECK(!b.SchoolLocked(ENEMY, SCHOOL_HOLY, T0 + 1000));    // the kicker itself is not locked
    // A kick that stopped nothing locks nothing.
    Book c(OBS, Tier::EXPERT);
    Event miss = Cast(NoticedSeq(c, 2139, Key()), ENEMY, 2139, T0, CLS_MAGE);
    miss.target = ENEMY2;
    CHECK(c.ObserveCast(miss, Seen(), Key(), T0));
    CHECK(c.LockCount() == 0);
}

TEST(pvp_aware_a_silence_closes_every_school)
{
    Book b(OBS, Tier::EXPERT);
    Event e = Cast(NoticedSeq(b, 47476, Key()), ENEMY, 47476, T0, CLS_DEATH_KNIGHT);  // Strangulate
    e.target = ENEMY2;
    CHECK(b.ObserveCast(e, Seen(), Key(), T0));
    CHECK(b.SchoolLocked(ENEMY2, SCHOOL_FIRE, T0 + 4000));
    CHECK(b.SchoolLocked(ENEMY2, SCHOOL_HOLY, T0 + 4000));
    CHECK(!b.SchoolLocked(ENEMY2, SCHOOL_HOLY, T0 + 5100));
}

TEST(pvp_aware_diminishing_returns_step_from_full_to_immune)
{
    Book b(OBS, Tier::EXPERT);
    Event e;
    e.target = US;
    std::uint32_t at = T0;
    std::uint32_t seq = 1;
    for (std::uint32_t expectStep = 0; expectStep < 3; ++expectStep)
    {
        CHECK(b.DrStepExact(US, DrCategory::STUN, at) == expectStep);
        e.atMs = at;
        e.seq = NoticedSeq(b, 408, Key(), seq);
        seq = e.seq + 1;
        CHECK(b.ObserveCcLanded(e, DrCategory::STUN, 4000, Seen(), Key(), at));
        at += 5000;  // the effect ended 1 s ago
    }
    CHECK(b.DrStepExact(US, DrCategory::STUN, at) == 3);  // immune
    CHECK(DrPercent(0) == 100 && DrPercent(1) == 50 && DrPercent(2) == 25 && DrPercent(3) == 0);
    // Another category on the same target is untouched.
    CHECK(b.DrStepExact(US, DrCategory::FEAR, at) == 0);
    // Another target too.
    CHECK(b.DrStepExact(ENEMY, DrCategory::STUN, at) == 0);
}

TEST(pvp_aware_diminishing_returns_reset_eighteen_seconds_after_the_last_effect_ends)
{
    Book b(OBS, Tier::EXPERT);
    Event e;
    e.target = US;
    e.atMs = T0;
    e.seq = NoticedSeq(b, 118, Key());
    CHECK(b.ObserveCcLanded(e, DrCategory::INCAPACITATE, 8000, Seen(), Key(), T0));  // ends at T0 + 8 s
    CHECK(b.DrStepExact(US, DrCategory::INCAPACITATE, T0 + 5000) == 1);     // still running
    CHECK(b.DrStepExact(US, DrCategory::INCAPACITATE, T0 + 8000 + 17000) == 1);
    CHECK(b.DrStepExact(US, DrCategory::INCAPACITATE, T0 + 8000 + 18000) == 0);
    CHECK(b.DrResetsIn(US, DrCategory::INCAPACITATE, T0 + 8000) == 18000);
    CHECK(b.DrResetsIn(US, DrCategory::INCAPACITATE, T0 + 8000 + 18000) == 0);
    // After the reset a new effect starts a fresh count.
    Event again = e;
    again.atMs = T0 + 30000;
    again.seq = NoticedSeq(b, 118, Key(), e.seq + 1);
    CHECK(b.ObserveCcLanded(again, DrCategory::INCAPACITATE, 8000, Seen(), Key(), again.atMs));
    CHECK(b.DrStepExact(US, DrCategory::INCAPACITATE, again.atMs + 1000) == 1);
}

TEST(pvp_aware_an_effect_that_broke_early_starts_the_reset_clock_early)
{
    Book b(OBS, Tier::EXPERT);
    Event e;
    e.target = US;
    e.atMs = T0;
    e.seq = NoticedSeq(b, 118, Key());
    CHECK(b.ObserveCcLanded(e, DrCategory::INCAPACITATE, 8000, Seen(), Key(), T0));
    b.NoteCcEnded(US, DrCategory::INCAPACITATE, T0 + 2000);  // a hit broke it after 2 s
    CHECK(b.DrStepExact(US, DrCategory::INCAPACITATE, T0 + 2000 + 17000) == 1);
    CHECK(b.DrStepExact(US, DrCategory::INCAPACITATE, T0 + 2000 + 18000) == 0);
    // Ending an effect that already ended does not move the clock back.
    b.NoteCcEnded(US, DrCategory::INCAPACITATE, T0 + 30000);
    CHECK(b.DrStepExact(US, DrCategory::INCAPACITATE, T0 + 2000 + 18000) == 0);
}

TEST(pvp_aware_a_casual_bot_disregards_half_of_the_diminishing_returns)
{
    int ignored = 0;
    int considered = 0;
    for (std::uint32_t target = 1; target <= 400; ++target)
    {
        Book b(OBS, Tier::CASUAL);
        Interest own;
        own.ownCast = true;
        Event e;
        e.target = target;
        e.atMs = T0;
        for (std::uint32_t s = 1; s < 500 && !b.ObserveCcLanded([&] { e.seq = s; return e; }(), DrCategory::STUN, 4000, Seen(), own, T0); ++s)
        {
        }
        if (b.DrStepExact(target, DrCategory::STUN, T0 + 1000) != 1)
            continue;  // the book did not notice this one
        if (b.DrStep(target, DrCategory::STUN, T0 + 1000) == 0)
            ++ignored;
        else
            ++considered;
        // Stable: asking again answers the same way.
        CHECK(b.DrStep(target, DrCategory::STUN, T0 + 1000) == b.DrStep(target, DrCategory::STUN, T0 + 1000));
    }
    CHECK(ignored > 140 && ignored < 260);
    CHECK(considered > 140 && considered < 260);
}

TEST(pvp_aware_a_good_bot_books_only_its_own_crowd_control_and_the_key_targets)
{
    TierModel const g = ModelOf(Tier::GOOD);
    Interest other;
    CHECK(NoticePercent(g, Kind::CROWD_CONTROL, other) == 0);
    Interest own;
    own.ownCast = true;
    CHECK(NoticePercent(g, Kind::CROWD_CONTROL, own) == 90);
    TierModel const e = ModelOf(Tier::EXPERT);
    CHECK(NoticePercent(e, Kind::CROWD_CONTROL, other) == 98);
}

TEST(pvp_aware_last_seen_position_expires_by_tier_for_a_stealth_enemy)
{
    Book e(OBS, Tier::EXPERT);
    e.NoteSeen(ENEMY, CLS_ROGUE, 10.0f, 20.0f, 3.0f, T0);
    Position p;
    CHECK(e.LastSeen(ENEMY, T0 + 7000, p));
    CHECK_NEAR(p.x, 10.0f, 0.001);
    CHECK(p.ageMs == 7000);
    CHECK(!e.LastSeen(ENEMY, T0 + 9000, p));
    Book c(OBS, Tier::CASUAL);
    c.NoteSeen(ENEMY, CLS_ROGUE, 10.0f, 20.0f, 3.0f, T0);
    CHECK(c.LastSeen(ENEMY, T0 + 2000, p));
    CHECK(!c.LastSeen(ENEMY, T0 + 3000, p));
    CHECK(!c.LastSeen(ENEMY2, T0, p));
}

TEST(pvp_aware_the_book_is_bounded_and_drops_the_least_recently_touched)
{
    Book b(OBS, Tier::EXPERT);
    for (std::uint32_t i = 0; i < MAX_ACTORS + 10; ++i)
        b.NoteSeen(1000 + i, CLS_MAGE, 0, 0, 0, T0 + i * 10);
    CHECK(b.ActorCount() == MAX_ACTORS);
    Position p;
    CHECK(!b.LastSeen(1000, T0 + 400, p));                  // the oldest went
    CHECK(b.LastSeen(1000 + MAX_ACTORS + 9, T0 + 400, p));  // the newest stayed
}

TEST(pvp_aware_prune_drops_what_is_over_and_keeps_what_runs)
{
    Book b(OBS, Tier::EXPERT);
    Event e = Cast(NoticedSeq(b, 6552, Key()), ENEMY, 6552, T0, CLS_WARRIOR);
    CHECK(b.ObserveCast(e, Seen(), Key(), T0));
    b.NoteSeen(ENEMY, CLS_WARRIOR, 1, 2, 3, T0);
    b.Prune(T0 + 5000);
    CHECK(b.KickReading(ENEMY, CLS_WARRIOR, T0 + 5000).state == State::DOWN);
    b.Prune(T0 + 200000);
    CHECK(b.ActorCount() == 0);
    CHECK(b.KickReading(ENEMY, CLS_WARRIOR, T0 + 200000).state == State::UNKNOWN);
}

TEST(pvp_aware_time_arithmetic_survives_the_millisecond_clock_wrap)
{
    Book b(OBS, Tier::EXPERT);
    std::uint32_t const near = 0xFFFFF000u;
    Event e = Cast(NoticedSeq(b, TRINKET_SPELL, Key()), ENEMY, TRINKET_SPELL, near);
    CHECK(b.ObserveCast(e, Seen(), Key(), near));
    std::uint32_t const later = near + 50000;  // wrapped past zero
    CHECK(later < near);
    CHECK(b.TrinketReading(ENEMY, later).state == State::DOWN);
    CHECK(b.TrinketReading(ENEMY, later).remainingMs == 70000);
    CHECK(b.TrinketReading(ENEMY, near + 125000).state == State::READY);
}

TEST(pvp_aware_a_follow_up_control_needs_a_fresh_category_and_a_cooldown_not_seen_running)
{
    Book b(OBS, Tier::EXPERT);
    // A rogue that just used Kidney Shot on us: the stun category is the one to leave out. Gouge, Blind, Sap remain.
    CHECK(b.FollowUpControlPossible(ENEMY, CLS_ROGUE, US, DrCategory::STUN, T0));
    // A mage (Polymorph, Deep Freeze, Dragon's Breath, Ring of Frost, Frost Nova): leaving out incapacitates still has stuns.
    CHECK(b.FollowUpControlPossible(ENEMY, CLS_MAGE, US, DrCategory::INCAPACITATE, T0));
    // A class with no catalog crowd control has no follow-up (a death knight has Asphyxiate, Gnaw).
    CHECK(!b.FollowUpControlPossible(ENEMY, CLS_DEATH_KNIGHT, US, DrCategory::STUN, T0));
    // Immune on every other category: no follow-up from a priest (Psychic Horror, Chastise, Psychic Scream, Silence).
    Book d(OBS, Tier::EXPERT);
    Event e;
    e.target = US;
    e.atMs = T0;
    std::uint32_t seq = 1;
    for (DrCategory cat : { DrCategory::INCAPACITATE, DrCategory::DISORIENT, DrCategory::FEAR, DrCategory::HORROR,
                             DrCategory::MIND_CONTROL })
        for (int n = 0; n < 3; ++n)
        {
            e.seq = NoticedSeq(d, 408, Key(), seq);
            seq = e.seq + 1;
            e.atMs = T0 + 100 * seq;
            CHECK(d.ObserveCcLanded(e, cat, 1000, Seen(), Key(), e.atMs));
        }
    CHECK(!d.FollowUpControlPossible(ENEMY, CLS_PRIEST, US, DrCategory::STUN, T0 + 3000));
}

TEST(pvp_aware_mistakes_on_purpose_follow_the_tier_rate_and_are_stable)
{
    int expert = 0, casual = 0;
    for (std::uint32_t key = 1; key <= 2000; ++key)
    {
        if (MakesMistake(Tier::EXPERT, 5, key))
            ++expert;
        if (MakesMistake(Tier::CASUAL, 5, key))
            ++casual;
        CHECK(MakesMistake(Tier::GOOD, 5, key) == MakesMistake(Tier::GOOD, 5, key));
    }
    CHECK(expert > 60 && expert < 140);     // about 5 percent
    CHECK(casual > 520 && casual < 680);    // about 30 percent
}
