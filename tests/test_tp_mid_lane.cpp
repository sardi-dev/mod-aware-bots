#include "TestHarness.h"
#include "TpMidLockPolicy.h"
#include "TpPlanLogic.h"
#include "WsgPlanLogic.h"

// BT-695: after a kill in a locked Twin Peaks middle fight the strike groups walk round by the other lane while the fight holds, and
// one of the guards that joined walks with the taker instead of going home. Live runs (build 26): the 60 s join and the kill
// release worked, but the re-formed strike groups walked back into the same fight and no flag was ever taken in 19 minutes.

using namespace TpMidLock;

namespace
{
constexpr std::uint32_t T0 = 100000;
constexpr std::uint32_t SECOND = 1000;
constexpr std::uint8_t LANE_A = static_cast<std::uint8_t>(Lane::A);
constexpr std::uint8_t LANE_H = static_cast<std::uint8_t>(Lane::H);

// The live fight stood 100 to 140 yd south of the river centre: lane H.
Facts Fight(std::uint32_t nowMs, std::uint8_t lane = LANE_H)
{
    Facts f;
    f.nowMs = nowMs;
    f.inProgress = true;
    f.ownFighters = 3;
    f.enemyFighters = 3;
    f.fightLane = lane;
    return f;
}

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

// A locked fight for 70 s, then a kill: the state after the first release.
Verdict LockThenKill(State& state, std::uint8_t lane = LANE_H)
{
    Facts f = Fight(T0, lane);
    Run(state, T0, 70, f);
    f.nowMs = T0 + 71 * SECOND;
    f.latestDeathMs = f.nowMs - 500;
    return Update(state, f);
}

HomeBot Guard(std::uint32_t guid, std::uint32_t index, bool joined)
{
    HomeBot b;
    b.guid = guid;
    b.roleIndex = index;
    b.alive = true;
    b.goodTier = true;
    b.joined = joined;
    return b;
}
}  // namespace

TEST(lane_a_point_stands_on_the_lane_of_its_nearer_middle_graveyard)
{
    BgFlagMap::MapData const& d = TpLogic::Data();
    Pos const a = d.sides[0].mainGraveyard;
    Pos const h = d.sides[1].mainGraveyard;
    CHECK(LaneOf({ 1830.0f, 200.0f, -15.0f }, a, h) == LANE_H);   // the live fight
    CHECK(LaneOf({ 1860.0f, 430.0f, -5.0f }, a, h) == LANE_A);
    CHECK(OtherLane(LANE_A) == LANE_H);
    CHECK(OtherLane(LANE_H) == LANE_A);
    CHECK(OtherLane(0) == 0);
}

TEST(lane_a_kill_in_a_locked_fight_names_the_fight_lane)
{
    State state;
    Verdict const v = LockThenKill(state);
    CHECK(v.released == Release::KILL);
    CHECK(v.avoidStarted);
    CHECK(v.avoidLane == LANE_H);
    CHECK(!v.avoidEnded);
    // The fight goes on: a new lock began at this read, the lane rule holds with it.
    CHECK(v.locked);
}

TEST(lane_a_kill_without_a_lock_names_no_lane)
{
    State state;
    Facts f = Fight(T0);
    Update(state, f);   // the lock begins at this read
    f.nowMs = T0 + 5 * SECOND;
    f.latestDeathMs = f.nowMs;   // the first read of a death, before 60 s, but the lock exists (a locked fight, not yet called)
    Verdict v = Update(state, f);
    CHECK(v.avoidStarted);   // a kill in a locked fight starts it even before the call (the maintainer's rule: re-forming strike groups)

    // A death with no fight at all (no lock) does not.
    State quiet;
    Facts calm;
    calm.nowMs = T0;
    calm.inProgress = true;
    calm.latestDeathMs = T0;
    calm.fightLane = LANE_H;
    v = Update(quiet, calm);
    CHECK(!v.avoidStarted);
    CHECK(v.avoidLane == 0);
}

TEST(lane_the_lane_survives_a_kill_read_with_no_fighter_left_in_the_zone)
{
    State state;
    Facts f = Fight(T0, LANE_A);
    Run(state, T0, 30, f);
    // The read of the kill: the dead bot was our last one in the zone (no lane told), the lane read earlier stands.
    f.nowMs = T0 + 31 * SECOND;
    f.ownFighters = 0;
    f.fightLane = 0;
    f.latestDeathMs = f.nowMs;
    Verdict const v = Update(state, f);
    CHECK(v.avoidStarted);
    CHECK(v.avoidLane == LANE_A);
}

TEST(lane_a_second_kill_on_the_same_lane_is_not_announced_again)
{
    State state;
    LockThenKill(state);
    Facts f = Fight(T0 + 100 * SECOND);
    f.latestDeathMs = T0 + 100 * SECOND;
    Verdict const v = Update(state, f);
    CHECK(!v.avoidStarted);
    CHECK(v.avoidLane == LANE_H);
}

TEST(lane_the_rule_holds_while_the_fight_holds_and_ends_with_it)
{
    State state;
    LockThenKill(state);
    Facts f = Fight(T0 + 72 * SECOND);
    Verdict v = Run(state, f.nowMs, 120, f);
    CHECK(v.avoidLane == LANE_H);

    // The fight collapses: a flicker of 5 s changes nothing, 10 s of it ends the rule with the lock.
    f.nowMs = T0 + 200 * SECOND;
    f.ownFighters = 0;
    f.enemyFighters = 0;
    v = Update(state, f);
    CHECK(v.avoidLane == LANE_H);
    f.nowMs += 5 * SECOND;
    v = Update(state, f);
    CHECK(v.avoidLane == LANE_H);
    f.nowMs += 5 * SECOND;
    v = Update(state, f);
    CHECK(v.avoidEnded);
    CHECK(v.avoidEndedWhy == Release::FIGHT_OVER);
    CHECK(v.avoidLane == 0);
}

TEST(lane_the_rule_ends_after_its_window_even_while_the_fight_goes_on)
{
    State state;
    LockThenKill(state);
    Facts f = Fight(T0 + 72 * SECOND);
    Verdict v = Run(state, f.nowMs, 170, f);   // 72 s to 242 s: 171 s after the kill read at 71 s
    CHECK(v.avoidLane == LANE_H);
    f.nowMs = T0 + 71 * SECOND + LANE_AVOID_MAX_MS;
    v = Update(state, f);
    CHECK(v.avoidEnded);
    CHECK(v.avoidEndedWhy == Release::NONE);
    CHECK(v.avoidLane == 0);
}

TEST(lane_a_flag_out_the_takers_through_or_the_end_of_the_match_end_the_rule)
{
    {
        State state;
        LockThenKill(state);
        Facts f = Fight(T0 + 80 * SECOND);
        f.flagOut = true;
        Verdict const v = Update(state, f);
        CHECK(v.avoidEnded);
        CHECK(v.avoidEndedWhy == Release::FLAG);
        CHECK(v.avoidLane == 0);
    }
    {
        State state;
        LockThenKill(state);
        Facts f = Fight(T0 + 80 * SECOND);
        f.takersThrough = true;
        Verdict const v = Update(state, f);
        CHECK(v.avoidEnded);
        CHECK(v.avoidEndedWhy == Release::THROUGH);
    }
    {
        State state;
        LockThenKill(state);
        Facts f = Fight(T0 + 80 * SECOND);
        f.inProgress = false;
        Verdict const v = Update(state, f);
        CHECK(v.avoidEnded);
        CHECK(v.avoidEndedWhy == Release::MATCH);
    }
}

TEST(lane_the_first_kill_read_keeps_the_old_release_semantics)
{
    // The lock rule of BT-694 is unchanged: a kill releases a call, the next 60 s start again.
    State state;
    Verdict const v = LockThenKill(state);
    CHECK(v.released == Release::KILL);
    CHECK(!v.join);
    CHECK(v.lockedForMs == 0);
}

TEST(lane_the_route_choice_goes_round_when_the_fight_stands_on_the_takers_lane)
{
    // Both teams' takers walk out by lane H (live run, build 28: the Horde taker, backup and escort stood at lane H with the
    // Alliance's; the sheet's lane A for the Horde is the carrier's way home).
    CHECK(OutboundLane() == LANE_H);

    CHECK(RouteChoices(LANE_H, true, false).front().id == RouteChoice::OTHER_LANE);
    // The fight on the lane the takers would not walk anyway: nothing changes.
    CHECK(RouteChoices(LANE_A, true, false).front().id == RouteChoice::OWN_ROUTE);
    // No avoidance: the route the plan always had.
    CHECK(RouteChoices(0, true, false).front().id == RouteChoice::OWN_ROUTE);
}

TEST(lane_a_casual_bot_and_a_bot_that_went_round_keep_their_route)
{
    // The tier mix: the casual bot walks the old route and makes the mistake; the 30 percent still bring a human texture.
    CHECK(RouteChoices(LANE_H, false, false).front().id == RouteChoice::OWN_ROUTE);
    // Already at the other lane's post for this avoidance: it walks on to the stand.
    CHECK(RouteChoices(LANE_H, true, true).front().id == RouteChoice::OWN_ROUTE);
    // Both options are always listed, the rule plays the first and a decision layer may pick the other.
    CHECK(RouteChoices(LANE_H, true, false).size() == 2);
}

TEST(lane_the_other_lane_posts_stand_on_that_lane_for_both_teams)
{
    BgFlagMap::MapData const& d = TpLogic::Data();
    Pos const a = d.sides[0].mainGraveyard;
    Pos const h = d.sides[1].mainGraveyard;
    TpLogic::LanePosts const ally = TpLogic::LanePostsOf(0);
    TpLogic::LanePosts const horde = TpLogic::LanePostsOf(1);
    for (bool alliance : { true, false })
        for (std::uint8_t lane : { LANE_A, LANE_H })
        {
            Pos const via = ViaPost(lane, alliance, ally, horde);
            CHECK(LaneOf(via, a, h) == lane);
        }
    // The walkers meet the enemy's half of the valley: the Alliance's posts are the Horde team's, the Horde's the Alliance's.
    Pos const toLaneA = ViaPost(LANE_A, true, ally, horde);
    CHECK(toLaneA.x == horde.cut.x && toLaneA.y == horde.cut.y);
    Pos const toLaneH = ViaPost(LANE_H, false, ally, horde);
    CHECK(toLaneH.x == ally.cut.x && toLaneH.y == ally.cut.y);
}

TEST(lane_a_taker_close_to_the_post_has_gone_round)
{
    Pos const via = ViaPost(LANE_A, true, TpLogic::LanePostsOf(0), TpLogic::LanePostsOf(1));
    CHECK(ViaReached({ via.x + 20.0f, via.y, via.z }, via));
    CHECK(!ViaReached({ via.x + 80.0f, via.y, via.z }, via));
}

TEST(lane_the_decision_puts_the_detour_before_the_walk_to_the_enemy_base_only_with_nothing_out)
{
    using namespace WsgLogic;
    DecisionContext c;
    c.role = Role::TAKER;
    c.tier = Tier::GOOD;
    c.strikeGathered = true;
    c.viaLane = true;
    std::vector<Option> options = Options(c);
    CHECK(options.front().kind == Kind::VIA_LANE);
    bool take = false;
    for (Option const& o : options)
        take = take || o.kind == Kind::TAKE_ENEMY_FLAG;
    CHECK(take);   // the old walk stays the fallback

    c.viaLane = false;
    CHECK(Options(c).front().kind == Kind::TAKE_ENEMY_FLAG);

    // Our carrier out: the escort duty outranks the detour (the plan clears it, the second lock is here).
    DecisionContext carried;
    carried.role = Role::BACKUP;
    carried.viaLane = true;
    carried.theirFlag = FLAG_ON_PLAYER;
    for (Option const& o : Options(carried))
        CHECK(o.kind != Kind::VIA_LANE);

    // A bot that carries is never sent round.
    DecisionContext carrier;
    carrier.role = Role::TAKER;
    carrier.hasFlag = true;
    carrier.viaLane = true;
    for (Option const& o : Options(carrier))
        CHECK(o.kind != Kind::VIA_LANE);

    // Mid and home do not read it.
    DecisionContext home;
    home.role = Role::HOME;
    home.viaLane = true;
    for (Option const& o : Options(home))
        CHECK(o.kind != Kind::VIA_LANE);
}

TEST(escort_the_release_replies_play_the_escort_only_when_it_is_safe)
{
    CHECK(ReleaseReplies(true, true, false, false).front().id == ReleaseReply::ESCORT_TAKER);
    CHECK(ReleaseReplies(true, true, true, false).front().id == ReleaseReply::WALK_HOME);    // a flag is out
    CHECK(ReleaseReplies(true, true, false, true).front().id == ReleaseReply::WALK_HOME);    // the base is threatened
    CHECK(ReleaseReplies(false, true, false, false).front().id == ReleaseReply::WALK_HOME);  // nobody to walk with
    CHECK(ReleaseReplies(true, false, false, false).front().id == ReleaseReply::WALK_HOME);  // no guard joined
    CHECK(ReleaseReplies(true, true, false, false).size() == 2);
}

TEST(escort_the_guard_is_the_lowest_joined_one_and_the_taker_the_best_ranked_alive)
{
    std::vector<HomeBot> const home = { Guard(30, 2, true), Guard(20, 1, true), Guard(10, 0, false) };
    std::vector<TakerBot> const takers = { { 7, 2, true }, { 5, 0, false }, { 6, 1, true } };
    EscortPick const pick = PickEscort(home, takers);
    CHECK(pick.guard == 20);   // guard 10 did not join, 20 has the lower index of the two that did
    CHECK(pick.taker == 6);    // the taker (rank 0) is dead, the backup is next

    // Nobody joined, or nobody to walk with: no pick.
    CHECK(PickEscort({ Guard(10, 0, false) }, takers).guard == 0);
    CHECK(PickEscort(home, { { 5, 0, false } }).taker == 0);
    HomeBot dead = Guard(10, 0, true);
    dead.alive = false;
    CHECK(PickEscort({ dead }, takers).guard == 0);
}

TEST(escort_it_starts_on_a_pick_and_ends_for_each_reason)
{
    EscortFacts safe;
    safe.nowMs = T0;
    safe.inProgress = true;
    safe.takerAlive = true;
    safe.guardAlive = true;
    EscortPick const pick = { 20, 6 };

    EscortState state;
    EscortVerdict v = EscortStep(state, safe, pick);
    CHECK(v.started && v.active);
    CHECK(state.guard == 20 && state.taker == 6);

    // The pick is only read to start: a later read with no pick goes on.
    safe.nowMs = T0 + 10 * SECOND;
    v = EscortStep(state, safe, EscortPick());
    CHECK(v.active && !v.started);

    struct Case { bool EscortFacts::*field; bool value; EscortEnd why; };
    Case const cases[] = {
        { &EscortFacts::inProgress, false, EscortEnd::MATCH },
        { &EscortFacts::flagOut, true, EscortEnd::FLAG },
        { &EscortFacts::baseThreatened, true, EscortEnd::BASE_THREAT },
        { &EscortFacts::takersThrough, true, EscortEnd::THROUGH },
        { &EscortFacts::takerAlive, false, EscortEnd::TAKER_GONE },
        { &EscortFacts::guardAlive, false, EscortEnd::GUARD_GONE },
    };
    for (Case const& c : cases)
    {
        EscortState s;
        EscortFacts f = safe;
        f.nowMs = T0;
        CHECK(EscortStep(s, f, pick).started);
        f.nowMs = T0 + 5 * SECOND;
        f.*(c.field) = c.value;
        EscortVerdict const out = EscortStep(s, f, EscortPick());
        CHECK(out.ended == c.why);
        CHECK(!out.active);
        CHECK(s.guard == 0);
    }

    // The window.
    EscortState s;
    EscortFacts f = safe;
    f.nowMs = T0;
    EscortStep(s, f, pick);
    f.nowMs = T0 + ESCORT_MAX_MS;
    CHECK(EscortStep(s, f, EscortPick()).ended == EscortEnd::WINDOW);
}

TEST(escort_it_never_starts_with_a_flag_out_or_a_threatened_base)
{
    EscortFacts f;
    f.nowMs = T0;
    f.inProgress = true;
    f.takerAlive = true;
    f.guardAlive = true;
    EscortPick const pick = { 20, 6 };

    EscortState a;
    f.flagOut = true;
    CHECK(!EscortStep(a, f, pick).started);
    CHECK(a.guard == 0);

    EscortState b;
    f.flagOut = false;
    f.baseThreatened = true;
    CHECK(!EscortStep(b, f, pick).started);

    EscortState c;
    f.baseThreatened = false;
    f.inProgress = false;
    CHECK(!EscortStep(c, f, pick).started);

    EscortState d;
    f.inProgress = true;
    CHECK(!EscortStep(d, f, EscortPick()).started);
    CHECK(EscortStep(d, f, pick).started);
}

TEST(escort_the_decision_puts_the_escort_before_the_post_but_not_before_a_flag_event)
{
    using namespace WsgLogic;
    DecisionContext c;
    c.role = Role::HOME;
    c.midEscort = true;
    CHECK(Options(c).front().kind == Kind::ESCORT_TAKER);

    bool post = false;
    for (Option const& o : Options(c))
        post = post || o.kind == Kind::HOLD_HOME_POST || o.kind == Kind::HOLD_HOME_LOOSE;
    CHECK(post);

    // The escort beats the join (it is the guard's answer to a kill release).
    c.midJoin = true;
    CHECK(Options(c).front().kind == Kind::ESCORT_TAKER);

    // A dropped flag of ours is returned first.
    DecisionContext ground = c;
    ground.ourFlag = FLAG_ON_GROUND;
    ground.ownGroundResponder = true;
    CHECK(Options(ground).front().kind == Kind::RETURN_OWN_FLAG);

    // Never while our flag is carried or while we carry theirs.
    DecisionContext theyCarry;
    theyCarry.role = Role::HOME;
    theyCarry.midEscort = true;
    theyCarry.ourFlag = FLAG_ON_PLAYER;
    for (Option const& o : Options(theyCarry))
        CHECK(o.kind != Kind::ESCORT_TAKER);
    DecisionContext weCarry;
    weCarry.role = Role::HOME;
    weCarry.midEscort = true;
    weCarry.theirFlag = FLAG_ON_PLAYER;
    for (Option const& o : Options(weCarry))
        CHECK(o.kind != Kind::ESCORT_TAKER);

    // The other roles do not read it.
    DecisionContext mid;
    mid.role = Role::MID;
    mid.midEscort = true;
    for (Option const& o : Options(mid))
        CHECK(o.kind != Kind::ESCORT_TAKER);
}

TEST(lane_the_whole_cycle_lock_kill_detour_escort_and_the_return_to_the_plan)
{
    State lock;
    EscortState escort;
    Facts f = Fight(T0);
    Run(lock, T0, 70, f);   // the call is on at 60 s: the guards joined
    CHECK(lock.joining);

    // The kill read: the lock releases, the lane is named and the escort starts.
    f.nowMs = T0 + 71 * SECOND;
    f.latestDeathMs = f.nowMs;
    Verdict const v = Update(lock, f);
    CHECK(v.released == Release::KILL);
    CHECK(v.avoidStarted && v.avoidLane == LANE_H);

    EscortFacts ef;
    ef.nowMs = f.nowMs;
    ef.inProgress = true;
    ef.takerAlive = true;
    ef.guardAlive = true;
    std::vector<HomeBot> const home = { Guard(20, 0, true), Guard(21, 1, true) };
    EscortPick const pick = PickEscort(home, { { 6, 0, true }, { 7, 1, true } });
    CHECK(EscortStep(escort, ef, pick).started);
    CHECK(escort.guard == 20 && escort.taker == 6);

    // A taker of either team: the fight is on lane H, the takers' lane, so it goes round by lane A.
    CHECK(RouteChoices(v.avoidLane, true, false).front().id == RouteChoice::OTHER_LANE);

    // The fight is over for 10 s: the lane rule ends, the escort goes on to its own end (the taker gone).
    f.nowMs += 20 * SECOND;
    f.ownFighters = 0;
    f.enemyFighters = 0;
    f.fightLane = 0;
    Update(lock, f);
    f.nowMs += 11 * SECOND;
    Verdict const over = Update(lock, f);
    CHECK(over.avoidEnded);
    ef.nowMs = f.nowMs;
    ef.takerAlive = false;
    CHECK(EscortStep(escort, ef, EscortPick()).ended == EscortEnd::TAKER_GONE);
}
