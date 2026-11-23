#include "TestHarness.h"
#include "TpMidLockPolicy.h"
#include "TpPlanLogic.h"
#include "WsgPlanLogic.h"

// BT-694: the Twin Peaks middle deadlock (docs/bg-sheets/twin-peaks.md section 6). Both teams fight in the middle for 60 s with
// no kill, no flag and nobody through: the idle home guards leave their posts and join. Live runs (builds 24 and 25) showed
// three bots per team locked in the valley, a healer on each side, two per team at their posts, and no pickup in seven minutes.

using namespace TpMidLock;

namespace
{
constexpr std::uint32_t T0 = 100000;
constexpr std::uint32_t SECOND = 1000;

Facts Fight(std::uint32_t nowMs)
{
    Facts f;
    f.nowMs = nowMs;
    f.inProgress = true;
    f.ownFighters = 3;
    f.enemyFighters = 3;
    return f;
}

// Reads once a second for `seconds` seconds from `from`, returns the last verdict.
Verdict Run(State& state, std::uint32_t from, std::uint32_t seconds, Facts base)
{
    Verdict v;
    for (std::uint32_t s = 0; s <= seconds; ++s)
    {
        base.nowMs = from + s * SECOND;
        v = Update(state, base);
    }
    return v;
}

HomeBot Guard(std::uint32_t guid, std::uint32_t index, bool good = true, bool inCombat = false, bool joined = false)
{
    HomeBot b;
    b.guid = guid;
    b.roleIndex = index;
    b.alive = true;
    b.goodTier = good;
    b.inCombat = inCombat;
    b.joined = joined;
    return b;
}
}  // namespace

TEST(midlock_zone_holds_the_lanes_and_the_graveyards_but_not_the_flag_rooms)
{
    BgFlagMap::MapData const& d = TpLogic::Data();
    // The live fight (x 1800 to 1860, y 180 to 220) is inside; the sheet's 70 yd "mid" word would have missed it.
    CHECK(InMidZone({ 1830.0f, 200.0f, -15.0f }, d.mid));
    CHECK(!(TpLogic::Dist2({ 1830.0f, 200.0f, 0.0f }, d.mid) <= TpLogic::PLACE_MID_YARDS));
    CHECK(InMidZone(d.sides[0].mainGraveyard, d.mid));
    CHECK(InMidZone(d.sides[1].mainGraveyard, d.mid));
    CHECK(!InMidZone(d.sides[0].standWalk, d.mid));
    CHECK(!InMidZone(d.sides[1].standWalk, d.mid));
}

TEST(midlock_the_guards_are_called_after_sixty_seconds_of_a_locked_fight)
{
    State state;
    Verdict v = Run(state, T0, 59, Fight(T0));
    CHECK(v.locked);
    CHECK(!v.join);
    CHECK(v.lockedForMs == 59 * SECOND);

    Facts f = Fight(T0 + 60 * SECOND);
    v = Update(state, f);
    CHECK(v.join);
    CHECK(v.joinStarted);

    // The call is announced once; the guards stay called while the lock stands.
    f.nowMs += SECOND;
    v = Update(state, f);
    CHECK(v.join);
    CHECK(!v.joinStarted);
}

TEST(midlock_one_side_alone_or_a_lone_skirmish_is_no_deadlock)
{
    State state;
    Facts f = Fight(T0);
    f.enemyFighters = 0;
    Verdict v = Run(state, T0, 120, f);
    CHECK(!v.locked);
    CHECK(!v.join);

    State skirmish;
    f = Fight(T0);
    f.ownFighters = 1;
    f.enemyFighters = 1;
    v = Run(skirmish, T0, 120, f);
    CHECK(!v.locked);
    CHECK(!v.join);
}

TEST(midlock_a_kill_ends_the_lock_at_once_and_restarts_the_clock)
{
    State state;
    Facts f = Fight(T0);
    Run(state, T0, 70, f);
    CHECK(state.joining);

    f.nowMs = T0 + 71 * SECOND;
    f.latestDeathMs = f.nowMs - 500;   // someone died
    Verdict v = Update(state, f);
    CHECK(v.released == Release::KILL);
    CHECK(!v.join);
    // The fight goes on: a new lock begins at this read and needs its own 60 s.
    CHECK(v.locked);
    CHECK(v.lockedForMs == 0);

    v = Run(state, f.nowMs, 59, f);
    CHECK(!v.join);
    f.nowMs += 60 * SECOND;
    v = Update(state, f);
    CHECK(v.join);
    CHECK(v.joinStarted);
}

TEST(midlock_a_death_before_the_lock_only_moves_its_start)
{
    State state;
    Facts f = Fight(T0);
    f.latestDeathMs = T0 - 5000;
    Verdict v = Update(state, f);
    // The death is new to the state (it never saw one): the lock starts at this read, no call was running to release.
    CHECK(v.released == Release::NONE);
    CHECK(v.locked);
    CHECK(v.lockedForMs == 0);
}

TEST(midlock_a_flag_pickup_ends_the_lock_and_no_new_one_starts_while_it_is_out)
{
    State state;
    Facts f = Fight(T0);
    Run(state, T0, 65, f);
    CHECK(state.joining);

    f.nowMs = T0 + 66 * SECOND;
    f.flagOut = true;
    Verdict v = Update(state, f);
    CHECK(v.released == Release::FLAG);
    CHECK(!v.locked);
    CHECK(!v.join);

    // Still out a minute later: no lock, no call.
    v = Run(state, f.nowMs, 70, f);
    CHECK(!v.locked);
    CHECK(!v.join);
}

TEST(midlock_takers_through_ends_the_lock_and_holds_it_off)
{
    State state;
    Facts f = Fight(T0);
    Run(state, T0, 65, f);
    f.nowMs = T0 + 66 * SECOND;
    f.takersThrough = true;
    Verdict v = Update(state, f);
    CHECK(v.released == Release::THROUGH);
    CHECK(!v.locked);
    v = Run(state, f.nowMs, 90, f);
    CHECK(!v.locked);
}

TEST(midlock_the_enemy_collapsing_ends_it_after_the_grace_not_at_a_flicker)
{
    State state;
    Facts f = Fight(T0);
    Run(state, T0, 65, f);

    // One read with the enemy out of combat (a healer between casts) changes nothing.
    f.nowMs = T0 + 66 * SECOND;
    f.enemyFighters = 0;
    Verdict v = Update(state, f);
    CHECK(v.join);
    f.nowMs += (LOCK_BREAK_GRACE_MS / 2);
    v = Update(state, f);
    CHECK(v.join);

    // Back in the fight inside the grace: the quiet clock starts over.
    f.enemyFighters = 3;
    f.nowMs += SECOND;
    v = Update(state, f);
    CHECK(v.join);
    CHECK(state.quietSinceMs == 0);

    // Gone for the whole grace: released, the guards return.
    f.enemyFighters = 0;
    f.nowMs += SECOND;
    v = Update(state, f);
    CHECK(v.join);
    f.nowMs += LOCK_BREAK_GRACE_MS;
    v = Update(state, f);
    CHECK(!v.join);
    CHECK(!v.locked);
    CHECK(v.released == Release::FIGHT_OVER);
}

TEST(midlock_a_lock_that_never_called_anyone_ends_silently)
{
    State state;
    Facts f = Fight(T0);
    Run(state, T0, 20, f);
    f.nowMs = T0 + 21 * SECOND;
    f.enemyFighters = 0;
    Verdict v = Run(state, f.nowMs, 12, f);
    CHECK(!v.locked);
    CHECK(v.released == Release::NONE);
}

TEST(midlock_a_match_that_is_not_running_clears_everything)
{
    State state;
    Facts f = Fight(T0);
    Run(state, T0, 65, f);
    f.inProgress = false;
    f.nowMs = T0 + 66 * SECOND;
    Verdict v = Update(state, f);
    CHECK(v.released == Release::MATCH);
    CHECK(!v.locked);
    CHECK(!state.joining);
}

TEST(midlock_the_idle_guards_join_and_casual_ones_keep_their_post)
{
    std::vector<HomeBot> home = { Guard(11, 0), Guard(12, 1, false), Guard(13, 2) };
    std::vector<std::uint32_t> const joiners = Joiners(home, true, false, false);
    CHECK(joiners.size() == 2);
    CHECK(joiners[0] == 11);
    CHECK(joiners[1] == 13);
}

TEST(midlock_nobody_leaves_before_the_call)
{
    std::vector<HomeBot> home = { Guard(11, 0), Guard(12, 1) };
    CHECK(Joiners(home, false, false, false).empty());
}

TEST(midlock_the_defence_is_never_stripped_while_our_flag_is_carried_or_the_base_is_threatened)
{
    std::vector<HomeBot> home = { Guard(11, 0), Guard(12, 1), Guard(13, 2, true, false, true) };
    // Even a guard that already left is sent back (it is not in the list): the carrier is at the door.
    CHECK(Joiners(home, true, true, false).empty());
    CHECK(Joiners(home, true, false, true).empty());
    CHECK(Joiners(home, true, true, true).empty());
    CHECK(Joiners(home, true, false, false).size() == 3);
}

TEST(midlock_a_guard_in_combat_at_the_base_is_not_idle_but_one_already_in_the_fight_stays)
{
    std::vector<HomeBot> home = { Guard(11, 0, true, true, false), Guard(12, 1, true, true, true), Guard(13, 2) };
    std::vector<std::uint32_t> const joiners = Joiners(home, true, false, false);
    CHECK(joiners.size() == 2);
    CHECK(joiners[0] == 12);
    CHECK(joiners[1] == 13);
}

TEST(midlock_dead_guards_never_join_and_the_order_is_by_role_index)
{
    HomeBot dead = Guard(21, 0);
    dead.alive = false;
    std::vector<HomeBot> home = { Guard(23, 2), dead, Guard(22, 1) };
    std::vector<std::uint32_t> const joiners = Joiners(home, true, false, false);
    CHECK(joiners.size() == 2);
    CHECK(joiners[0] == 22);
    CHECK(joiners[1] == 23);
}

TEST(midlock_the_candidate_list_joins_first_only_when_called_and_the_base_is_quiet)
{
    auto const first = [](bool called, bool threatened) { return Replies(called, threatened).front().id; };
    CHECK(first(true, false) == Reply::JOIN_FIGHT);
    CHECK(first(false, false) == Reply::HOLD_POSTS);
    CHECK(first(true, true) == Reply::HOLD_POSTS);
    CHECK(Replies(true, false).size() == 2);
}

TEST(midlock_the_join_point_stays_out_of_the_enemy_graveyard_zone)
{
    BgFlagMap::Pos const gy = { 1816.87f, 160.083f, 1.8f };
    // The live fight stood 40 yd from the Horde graveyard: an Alliance guard is sent to its edge, towards the fight.
    BgFlagMap::Pos const fight = { 1830.0f, 198.0f, -15.0f };
    BgFlagMap::Pos const out = KeepOut(fight, gy, GY_KEEP_OUT_YARDS);
    CHECK_NEAR(TpLogic::Dist2(out, gy), GY_KEEP_OUT_YARDS, 0.01);
    CHECK(out.y > fight.y);
    CHECK_NEAR(out.z, fight.z, 0.001);

    // A fight outside the zone is left where it is; one on the graveyard point itself still gets a point outside.
    BgFlagMap::Pos const far = { 1840.0f, 320.0f, -24.9f };
    CHECK_NEAR(KeepOut(far, gy, GY_KEEP_OUT_YARDS).x, far.x, 0.001);
    CHECK_NEAR(TpLogic::Dist2(KeepOut(gy, gy, GY_KEEP_OUT_YARDS), gy), GY_KEEP_OUT_YARDS, 0.01);
}

TEST(midlock_the_decision_prefers_the_join_over_the_post_but_not_over_a_flag_event)
{
    using namespace WsgLogic;
    DecisionContext c;
    c.role = Role::HOME;
    c.midJoin = true;
    std::vector<Option> options = Options(c);
    CHECK(options.front().kind == Kind::JOIN_MID_FIGHT);

    // The post stays the fallback behind it.
    bool post = false;
    for (Option const& o : options)
        post = post || o.kind == Kind::HOLD_HOME_POST;
    CHECK(post);

    // A dropped flag of ours is returned first.
    c.ourFlag = FLAG_ON_GROUND;
    c.ownGroundResponder = true;
    CHECK(Options(c).front().kind == Kind::RETURN_OWN_FLAG);

    // With our flag carried the guard is never offered the join, even if a stale read still marks it.
    DecisionContext carried;
    carried.role = Role::HOME;
    carried.midJoin = true;
    carried.ourFlag = FLAG_ON_PLAYER;
    for (Option const& o : Options(carried))
        CHECK(o.kind != Kind::JOIN_MID_FIGHT);

    // The other roles do not read it.
    DecisionContext mid;
    mid.role = Role::MID;
    mid.midJoin = true;
    for (Option const& o : Options(mid))
        CHECK(o.kind != Kind::JOIN_MID_FIGHT);
}

TEST(midlock_the_whole_cycle_in_the_live_shape_three_fighters_a_side_two_guards)
{
    // Seven minutes of the live run: 3 against 3 in the valley, nothing else moves. The call comes at 60 s; a kill at 150 s
    // sends the guards back; the fight goes on and the next call comes 60 s after the kill.
    State state;
    Facts f = Fight(T0);
    std::vector<HomeBot> home = { Guard(31, 0), Guard(32, 1) };
    std::uint32_t calledAt = 0;
    std::uint32_t releasedAt = 0;
    std::uint32_t calledAgainAt = 0;
    for (std::uint32_t s = 0; s <= 400; ++s)
    {
        f.nowMs = T0 + s * SECOND;
        if (s == 150)
            f.latestDeathMs = f.nowMs;
        Verdict const v = Update(state, f);
        if (v.joinStarted)
        {
            if (!calledAt)
                calledAt = s;
            else if (!calledAgainAt)
                calledAgainAt = s;
        }
        if (v.released == Release::KILL && !releasedAt)
            releasedAt = s;
        if (s == 100)
            CHECK(Joiners(home, v.join, false, false).size() == 2);
        if (s == 155)
            CHECK(Joiners(home, v.join, false, false).empty());
    }
    CHECK(calledAt == 60);
    CHECK(releasedAt == 150);
    CHECK(calledAgainAt == 210);
}

TEST(midlock_a_player_leaving_the_match_is_no_kill)
{
    State state;
    Facts f = Fight(T0);
    f.latestDeathMs = T0 - 20 * SECOND;
    Run(state, T0, 65, f);
    CHECK(state.joining);

    // The newest death time drops to an older one (the player who died last left): the lock stands.
    f.nowMs = T0 + 66 * SECOND;
    f.latestDeathMs = T0 - 40 * SECOND;
    Verdict const v = Update(state, f);
    CHECK(v.join);
    CHECK(v.released == Release::NONE);
}
