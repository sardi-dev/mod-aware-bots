#include "TestHarness.h"
#include "WsgPlanLogic.h"
#include "BgCalloutPolicy.h"

// BT-153: Warsong Gulch playbook logic (docs/bg-sheets/warsong-gulch.md).

using namespace WsgLogic;

namespace
{
Candidate Bot(std::uint32_t guid, std::uint8_t cls, std::uint8_t level = 20, Tier tier = Tier::GOOD)
{
    Candidate c;
    c.guidLow = guid;
    c.cls = cls;
    c.level = level;
    c.tier = tier;
    c.melee = cls == CLS_WARRIOR || cls == CLS_ROGUE || cls == CLS_PALADIN || cls == CLS_DEATH_KNIGHT ||
              cls == CLS_MONK || cls == CLS_DRUID;
    return c;
}

Candidate Healer(std::uint32_t guid, std::uint8_t cls = CLS_PRIEST, std::uint8_t level = 20)
{
    Candidate c = Bot(guid, cls, level);
    c.healer = true;
    c.melee = false;
    return c;
}

Role RoleOfGuid(std::vector<Assignment> const& list, std::uint32_t guid)
{
    for (Assignment const& a : list)
        if (a.guidLow == guid)
            return a.role;
    return Role::NONE;
}

std::uint32_t CountRole(std::vector<Assignment> const& list, Role role)
{
    std::uint32_t n = 0;
    for (Assignment const& a : list)
        if (a.role == role)
            ++n;
    return n;
}

std::vector<Candidate> MixedTeam()
{
    return {
        Bot(101, CLS_ROGUE, 23),   Bot(102, CLS_WARRIOR, 24), Bot(103, CLS_MAGE, 22),   Bot(104, CLS_HUNTER, 21),
        Healer(105),               Bot(106, CLS_PALADIN, 20), Bot(107, CLS_WARLOCK, 22), Bot(108, CLS_SHAMAN, 23),
        Bot(109, CLS_DRUID, 21),   Bot(110, CLS_MONK, 20)
    };
}

TeamMix FullMix(bool healer = true, bool low = false)
{
    TeamMix mix;
    mix.size = 10;
    mix.hasHealer = healer;
    mix.lowBracket = low;
    return mix;
}
}  // namespace

// --- plan draw ---------------------------------------------------------------------------------------------------------

TEST(wsg_plan_draw_weights_are_20_60_20)
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
}

TEST(wsg_plan_enum_matches_the_strategy_numbers)
{
    // WSBotStrategy: balanced 0, offensive 1, defensive 2 (BattleGroundTactics.h); the harness pins these numbers.
    CHECK(static_cast<int>(Plan::BALANCED) == 0);
    CHECK(static_cast<int>(Plan::OFFENSE) == 1);
    CHECK(static_cast<int>(Plan::DEFENSE) == 2);
}

// --- role counts -------------------------------------------------------------------------------------------------------

TEST(wsg_counts_follow_the_sheet_table_for_ten)
{
    RoleCounts b = BaseCounts(Plan::BALANCED, 10);
    CHECK(b[Role::TAKER] == 1 && b[Role::BACKUP] == 1 && b[Role::ESCORT] == 2 && b[Role::MID] == 3 && b[Role::HOME] == 3);
    RoleCounts o = BaseCounts(Plan::OFFENSE, 10);
    CHECK(o[Role::TAKER] == 1 && o[Role::BACKUP] == 1 && o[Role::ESCORT] == 4 && o[Role::MID] == 3 && o[Role::HOME] == 1);
    RoleCounts d = BaseCounts(Plan::DEFENSE, 10);
    CHECK(d[Role::TAKER] == 1 && d[Role::BACKUP] == 0 && d[Role::ESCORT] == 2 && d[Role::MID] == 2 && d[Role::HOME] == 5);
}

TEST(wsg_counts_always_add_up_for_five_to_ten)
{
    for (std::uint32_t size = 1; size <= 12; ++size)
    {
        for (Plan plan : { Plan::BALANCED, Plan::OFFENSE, Plan::DEFENSE })
        {
            RoleCounts const counts = BaseCounts(plan, size);
            CHECK(counts.Total() == size);
            CHECK(counts[Role::TAKER] == 1);
        }
    }
}

TEST(wsg_small_teams_have_one_home_one_mid_and_the_rest_strike)
{
    for (std::uint32_t size = 5; size <= 6; ++size)
    {
        RoleCounts const counts = BaseCounts(Plan::BALANCED, size);
        CHECK(counts[Role::HOME] == 1);
        CHECK(counts[Role::MID] == 1);
        CHECK(counts[Role::TAKER] + counts[Role::BACKUP] + counts[Role::ESCORT] == size - 2);
    }
}

TEST(wsg_home_never_vanishes_in_scaled_teams)
{
    for (std::uint32_t size = 7; size <= 9; ++size)
    {
        CHECK(BaseCounts(Plan::BALANCED, size)[Role::HOME] >= 1);
        CHECK(BaseCounts(Plan::DEFENSE, size)[Role::HOME] >= 3);
    }
}

TEST(wsg_low_bracket_and_no_healer_give_three_escorts)
{
    CHECK(CountsFor(Plan::BALANCED, Situation::NORMAL, FullMix(true, true))[Role::ESCORT] == 3);
    CHECK(CountsFor(Plan::BALANCED, Situation::NORMAL, FullMix(false, false))[Role::ESCORT] == 3);
    CHECK(CountsFor(Plan::BALANCED, Situation::NORMAL, FullMix(true, false))[Role::ESCORT] == 2);
    // The total stays the team size, and an offense plan keeps its four.
    CHECK(CountsFor(Plan::BALANCED, Situation::NORMAL, FullMix(true, true)).Total() == 10);
    CHECK(CountsFor(Plan::OFFENSE, Situation::NORMAL, FullMix(true, true))[Role::ESCORT] == 4);
}

TEST(wsg_humans_take_mid_slots_first_and_never_the_takers)
{
    TeamMix mix = FullMix();
    mix.humans = 2;
    RoleCounts const counts = CountsFor(Plan::BALANCED, Situation::NORMAL, mix);
    CHECK(counts.Total() == 8);
    CHECK(counts[Role::MID] == 1);
    CHECK(counts[Role::TAKER] == 1 && counts[Role::BACKUP] == 1);

    mix.humans = 9;
    RoleCounts const crowd = CountsFor(Plan::BALANCED, Situation::NORMAL, mix);
    CHECK(crowd[Role::TAKER] == 1);
    CHECK(crowd[Role::BACKUP] == 1);
}

// --- score and clock rules ---------------------------------------------------------------------------------------------

TEST(wsg_two_nil_lead_switches_to_the_defense_plan)
{
    CHECK(SituationOf(2, 0, LeftMs(5 * 60 * 1000)) == Situation::LEAD2);
    CHECK(SituationOf(2, 1, LeftMs(5 * 60 * 1000)) == Situation::NORMAL);
    RoleCounts const counts = CountsFor(Plan::OFFENSE, Situation::LEAD2, FullMix());
    CHECK(counts[Role::HOME] == 5);
    CHECK(counts[Role::TAKER] + counts[Role::BACKUP] + counts[Role::ESCORT] == 3);
}

TEST(wsg_clock_rules_use_the_27_minute_limit)
{
    CHECK(LeftMs(0) == 27 * 60 * 1000);
    CHECK(LeftMs(27 * 60 * 1000) == 0);
    CHECK(LeftMs(28 * 60 * 1000) < 0);
}

TEST(wsg_behind_with_under_five_minutes_goes_all_in)
{
    CHECK(SituationOf(0, 1, 6 * 60 * 1000) == Situation::NORMAL);
    CHECK(SituationOf(1, 2, 4 * 60 * 1000) == Situation::ALL_IN);
    RoleCounts const counts = CountsFor(Plan::BALANCED, Situation::ALL_IN, FullMix());
    CHECK(counts[Role::HOME] == 1);
    CHECK(counts.Total() == 10);
}

TEST(wsg_level_with_under_four_minutes_strengthens_both_ends)
{
    CHECK(SituationOf(1, 1, 5 * 60 * 1000) == Situation::NORMAL);
    CHECK(SituationOf(1, 1, 3 * 60 * 1000) == Situation::LEVEL_LATE);
    RoleCounts const counts = CountsFor(Plan::DEFENSE, Situation::LEVEL_LATE, FullMix());
    CHECK(counts[Role::TAKER] + counts[Role::BACKUP] + counts[Role::ESCORT] == 5);
    CHECK(counts[Role::HOME] == 3);
    CHECK(counts.Total() == 10);
}

TEST(wsg_one_up_with_under_three_minutes_defends)
{
    CHECK(SituationOf(2, 1, 4 * 60 * 1000) == Situation::NORMAL);
    CHECK(SituationOf(2, 1, 2 * 60 * 1000) == Situation::LEAD1_LATE);
    RoleCounts const counts = CountsFor(Plan::OFFENSE, Situation::LEAD1_LATE, FullMix());
    CHECK(counts[Role::HOME] == 5);
    CHECK(counts[Role::TAKER] + counts[Role::BACKUP] + counts[Role::ESCORT] == 2);
    CHECK(counts[Role::MID] == 3);
}

// --- role assignment ---------------------------------------------------------------------------------------------------

TEST(wsg_assignment_fills_every_slot_once)
{
    std::vector<Assignment> const list = Assign(MixedTeam(), CountsFor(Plan::BALANCED, Situation::NORMAL, FullMix()));
    CHECK(list.size() == 10);
    CHECK(CountRole(list, Role::TAKER) == 1);
    CHECK(CountRole(list, Role::BACKUP) == 1);
    CHECK(CountRole(list, Role::ESCORT) == 2);
    CHECK(CountRole(list, Role::HOME) == 3);
    CHECK(CountRole(list, Role::MID) == 3);
}

TEST(wsg_rogue_and_feral_druid_take_the_flag_taker_slots)
{
    std::vector<Candidate> team = MixedTeam();
    Candidate feral = Bot(109, CLS_DRUID, 15);   // the lowest level, still a stealth class
    feral.feral = true;
    team[8] = feral;
    team[0] = Bot(101, CLS_ROGUE, 15);
    std::vector<Assignment> const list = Assign(team, CountsFor(Plan::BALANCED, Situation::NORMAL, FullMix()));
    Role const rogue = RoleOfGuid(list, 101);
    Role const druid = RoleOfGuid(list, 109);
    CHECK((rogue == Role::TAKER && druid == Role::BACKUP) || (rogue == Role::BACKUP && druid == Role::TAKER));
}

TEST(wsg_without_stealth_the_highest_level_takes_the_flag)
{
    std::vector<Candidate> team;
    for (std::uint32_t i = 0; i < 10; ++i)
        team.push_back(Bot(200 + i, CLS_WARRIOR, 20 + (i == 6 ? 4 : 0)));
    std::vector<Assignment> const list = Assign(team, CountsFor(Plan::BALANCED, Situation::NORMAL, FullMix()));
    CHECK(RoleOfGuid(list, 206) == Role::TAKER);
}

TEST(wsg_a_carrier_is_always_the_taker)
{
    std::vector<Candidate> team = MixedTeam();
    team[4].carrying = true;   // the priest carries
    std::vector<Assignment> const list = Assign(team, CountsFor(Plan::BALANCED, Situation::NORMAL, FullMix()));
    CHECK(RoleOfGuid(list, 105) == Role::TAKER);
}

TEST(wsg_first_healer_escorts_the_second_stays_with_mid)
{
    std::vector<Candidate> team = MixedTeam();
    team[5] = Healer(106, CLS_PALADIN);
    std::vector<Assignment> const list = Assign(team, CountsFor(Plan::BALANCED, Situation::NORMAL, FullMix()));
    std::uint32_t healersInEscort = 0;
    for (Assignment const& a : list)
        if ((a.guidLow == 105 || a.guidLow == 106) && a.role == Role::ESCORT)
            ++healersInEscort;
    CHECK(healersInEscort == 1);
}

TEST(wsg_assignment_is_deterministic_and_ignores_input_order)
{
    std::vector<Candidate> team = MixedTeam();
    std::vector<Assignment> const first = Assign(team, CountsFor(Plan::BALANCED, Situation::NORMAL, FullMix()));
    std::reverse(team.begin(), team.end());
    std::vector<Assignment> const second = Assign(team, CountsFor(Plan::BALANCED, Situation::NORMAL, FullMix()));
    for (Assignment const& a : first)
        CHECK(RoleOfGuid(second, a.guidLow) == a.role);
}

TEST(wsg_roles_are_stable_when_nothing_happens)
{
    // No re-roll: feeding the previous roles back changes nothing.
    std::vector<Candidate> team = MixedTeam();
    RoleCounts const counts = CountsFor(Plan::BALANCED, Situation::NORMAL, FullMix());
    std::vector<Assignment> const first = Assign(team, counts);
    for (Candidate& c : team)
        c.previous = RoleOfGuid(first, c.guidLow);
    std::vector<Assignment> const second = Assign(team, counts);
    for (Assignment const& a : first)
        CHECK(RoleOfGuid(second, a.guidLow) == a.role);
}

TEST(wsg_a_death_changes_only_what_it_has_to)
{
    std::vector<Candidate> team = MixedTeam();
    RoleCounts const counts = CountsFor(Plan::BALANCED, Situation::NORMAL, FullMix());
    std::vector<Assignment> const first = Assign(team, counts);
    for (Candidate& c : team)
        c.previous = RoleOfGuid(first, c.guidLow);

    // The escort dies: it leaves the candidates, the rest keep their roles except for what the empty slot pulls.
    std::uint32_t deadGuid = 0;
    for (Assignment const& a : first)
        if (a.role == Role::ESCORT)
        {
            deadGuid = a.guidLow;
            break;
        }
    std::vector<Candidate> alive;
    for (Candidate const& c : team)
        if (c.guidLow != deadGuid)
            alive.push_back(c);

    std::vector<Assignment> const after = Assign(alive, counts);
    CHECK(after.size() == 9);
    std::uint32_t changed = 0;
    for (Assignment const& a : after)
        if (RoleOfGuid(first, a.guidLow) != a.role)
            ++changed;
    CHECK(changed <= 2);
    // Taker and backup are untouched by an escort's death.
    for (Assignment const& a : first)
        if ((a.role == Role::TAKER || a.role == Role::BACKUP) && a.guidLow != deadGuid)
            CHECK(RoleOfGuid(after, a.guidLow) == a.role);
}

TEST(wsg_a_dead_taker_hands_over_to_the_backup)
{
    std::vector<Candidate> team = MixedTeam();
    RoleCounts const counts = CountsFor(Plan::BALANCED, Situation::NORMAL, FullMix());
    std::vector<Assignment> const first = Assign(team, counts);
    std::uint32_t taker = 0, backup = 0;
    for (Assignment const& a : first)
    {
        if (a.role == Role::TAKER)
            taker = a.guidLow;
        if (a.role == Role::BACKUP)
            backup = a.guidLow;
    }
    for (Candidate& c : team)
        c.previous = RoleOfGuid(first, c.guidLow);

    std::vector<Candidate> alive;
    for (Candidate const& c : team)
        if (c.guidLow != taker)
            alive.push_back(c);
    std::vector<Assignment> const after = Assign(alive, counts);
    CHECK(RoleOfGuid(after, backup) == Role::TAKER);
}

TEST(wsg_expert_bots_take_the_roles_where_late_reactions_cost_most)
{
    std::vector<Candidate> team;
    for (std::uint32_t i = 0; i < 10; ++i)
        team.push_back(Bot(300 + i, CLS_WARRIOR, 20, i == 3 ? Tier::EXPERT : i < 6 ? Tier::GOOD : Tier::CASUAL));
    std::vector<Assignment> const list = Assign(team, CountsFor(Plan::BALANCED, Situation::NORMAL, FullMix()));
    CHECK(RoleOfGuid(list, 303) == Role::TAKER);
    // The casual bots land at the posts and escorts, not in the taker slots.
    for (std::uint32_t i = 6; i < 10; ++i)
        CHECK(RoleOfGuid(list, 300 + i) != Role::TAKER);
}

// --- flag phases -------------------------------------------------------------------------------------------------------

TEST(wsg_phases_follow_the_flag_states)
{
    CHECK(PhaseOf(FLAG_ON_BASE, FLAG_ON_BASE) == Phase::BOTH_HOME);
    CHECK(PhaseOf(FLAG_ON_BASE, FLAG_ON_PLAYER) == Phase::WE_CARRY);
    CHECK(PhaseOf(FLAG_ON_PLAYER, FLAG_ON_BASE) == Phase::THEY_CARRY);
    CHECK(PhaseOf(FLAG_ON_PLAYER, FLAG_ON_PLAYER) == Phase::BOTH_CARRIED);
    CHECK(PhaseOf(FLAG_ON_GROUND, FLAG_ON_BASE) == Phase::FLAG_DOWN);
    CHECK(PhaseOf(FLAG_ON_BASE, FLAG_ON_GROUND) == Phase::FLAG_DOWN);
    CHECK(PhaseOf(FLAG_WAIT_RESPAWN, FLAG_WAIT_RESPAWN) == Phase::POST_CAPTURE);
    CHECK(PhaseOf(FLAG_WAIT_RESPAWN, FLAG_ON_BASE) == Phase::POST_CAPTURE);
}

TEST(wsg_both_held_clock_runs_only_while_both_are_carried)
{
    std::uint32_t start = 0;
    CHECK(BothHeldMs(FLAG_ON_PLAYER, FLAG_ON_PLAYER, 1000, start) == 0);
    CHECK(BothHeldMs(FLAG_ON_PLAYER, FLAG_ON_PLAYER, 61000, start) == 60000);
    CHECK(BothHeldMs(FLAG_ON_PLAYER, FLAG_ON_BASE, 70000, start) == 0);
    CHECK(start == 0);
    CHECK(BothHeldMs(FLAG_ON_PLAYER, FLAG_ON_PLAYER, 80000, start) == 0);
}

// --- objective options -------------------------------------------------------------------------------------------------

namespace
{
DecisionContext Ctx(Role role, Tier tier = Tier::GOOD)
{
    DecisionContext c;
    c.role = role;
    c.tier = tier;
    return c;
}
}  // namespace

TEST(wsg_options_are_sorted_and_deterministic)
{
    DecisionContext c = Ctx(Role::MID);
    c.powerupAvailable = true;
    std::vector<Option> const first = Options(c);
    std::vector<Option> const second = Options(c);
    CHECK(first.size() == second.size());
    for (std::size_t i = 0; i + 1 < first.size(); ++i)
        CHECK(first[i].score >= first[i + 1].score);
    for (std::size_t i = 0; i < first.size(); ++i)
        CHECK(first[i].kind == second[i].kind && first[i].score == second[i].score);
}

TEST(wsg_carrier_runs_home_and_hides_only_when_ours_is_taken)
{
    DecisionContext c = Ctx(Role::TAKER);
    c.hasFlag = true;
    CHECK(Options(c).front().kind == Kind::CARRY_HOME);

    c.ourFlag = FLAG_ON_PLAYER;
    c.theirFlag = FLAG_ON_PLAYER;
    CHECK(Options(c).front().kind == Kind::CARRY_HIDE);

    // A casual carrier has no hide rule: it runs home.
    c.tier = Tier::CASUAL;
    CHECK(Options(c).front().kind == Kind::CARRY_HOME);
}

TEST(wsg_stalemate_sends_the_carrier_home_at_nine_minutes)
{
    DecisionContext c = Ctx(Role::TAKER);
    c.hasFlag = true;
    c.ourFlag = FLAG_ON_PLAYER;
    c.theirFlag = FLAG_ON_PLAYER;
    c.bothHeldMs = STALEMATE_RUN_MS - 1;
    CHECK(Options(c).front().kind == Kind::CARRY_HIDE);
    c.bothHeldMs = STALEMATE_RUN_MS;
    CHECK(Options(c).front().kind == Kind::CARRY_HOME);
}

TEST(wsg_last_two_and_a_half_minutes_an_expert_carrier_runs)
{
    DecisionContext c = Ctx(Role::TAKER, Tier::EXPERT);
    c.hasFlag = true;
    c.ourFlag = FLAG_ON_PLAYER;
    c.theirFlag = FLAG_ON_PLAYER;
    c.leftMs = RUN_ONLY_LEFT_MS + 1000;
    CHECK(Options(c).front().kind == Kind::CARRY_HIDE);
    c.leftMs = RUN_ONLY_LEFT_MS;
    CHECK(Options(c).front().kind == Kind::CARRY_HOME);
    c.tier = Tier::GOOD;
    CHECK(Options(c).front().kind == Kind::CARRY_HIDE);
}

TEST(wsg_opening_strike_gathers_then_takes_the_flag)
{
    DecisionContext c = Ctx(Role::TAKER);
    CHECK(Options(c).front().kind == Kind::GATHER_STRIKE);
    c.strikeGathered = true;
    CHECK(Options(c).front().kind == Kind::TAKE_ENEMY_FLAG);

    // A casual bot walks alone to the stand.
    DecisionContext casual = Ctx(Role::ESCORT, Tier::CASUAL);
    CHECK(Options(casual).front().kind == Kind::TAKE_ENEMY_FLAG);
}

TEST(wsg_home_holds_the_flag_room_and_chases_only_near_the_stand)
{
    DecisionContext c = Ctx(Role::HOME);
    c.ourFlag = FLAG_ON_PLAYER;
    c.efcKnown = true;
    c.efcHunter = true;
    c.efcToOurStand = 120.0f;
    CHECK(Options(c).front().kind == Kind::HOLD_HOME_POST);
    c.efcToOurStand = 30.0f;
    CHECK(Options(c).front().kind == Kind::HUNT_EFC);
    c.efcHunter = false;
    CHECK(Options(c).front().kind == Kind::HOLD_HOME_POST);
    // Casual home guards hold loosely.
    CHECK(Options(Ctx(Role::HOME, Tier::CASUAL)).front().kind == Kind::HOLD_HOME_LOOSE);
}

TEST(wsg_when_they_carry_ours_backup_takes_theirs_at_once)
{
    DecisionContext c = Ctx(Role::BACKUP);
    c.ourFlag = FLAG_ON_PLAYER;
    c.efcKnown = true;
    CHECK(Options(c).front().kind == Kind::BLOCK_STEAL);

    DecisionContext escort0 = Ctx(Role::ESCORT);
    escort0.ourFlag = FLAG_ON_PLAYER;
    escort0.efcKnown = true;
    CHECK(Options(escort0).front().kind == Kind::BLOCK_STEAL);

    DecisionContext escort1 = escort0;
    escort1.roleIndex = 1;
    CHECK(Options(escort1).front().kind == Kind::HUNT_EFC);
}

TEST(wsg_mid_hunts_with_the_two_nearest_and_cuts_the_lane_otherwise)
{
    DecisionContext c = Ctx(Role::MID);
    c.ourFlag = FLAG_ON_PLAYER;
    c.efcKnown = true;
    c.efcHunter = true;
    CHECK(Options(c).front().kind == Kind::HUNT_EFC);
    c.efcHunter = false;
    CHECK(Options(c).front().kind == Kind::CUT_EFC_LANE);
}

TEST(wsg_a_dropped_flag_is_answered_by_the_nearest_two)
{
    DecisionContext c = Ctx(Role::MID);
    c.ourFlag = FLAG_ON_GROUND;
    c.ownGroundResponder = true;
    CHECK(Options(c).front().kind == Kind::RETURN_OWN_FLAG);
    c.ownGroundResponder = false;
    CHECK(Options(c).front().kind == Kind::HOLD_MID);

    DecisionContext striker = Ctx(Role::TAKER);
    striker.theirFlag = FLAG_ON_GROUND;
    striker.theirGroundResponder = true;
    CHECK(Options(striker).front().kind == Kind::PICKUP_DROPPED_FLAG);
}

TEST(wsg_two_nil_lead_holds_the_enemy_exit_with_three_at_most)
{
    for (std::uint32_t index = 0; index < 5; ++index)
    {
        DecisionContext c = Ctx(Role::MID);
        c.situation = Situation::LEAD2;
        c.roleIndex = index;
        Kind const kind = Options(c).front().kind;
        CHECK((index < LEAD_TUNNEL_HOLDERS) == (kind == Kind::HOLD_ENEMY_EXIT));
    }
}

TEST(wsg_both_carried_stalemate_hunts_from_five_minutes)
{
    DecisionContext c = Ctx(Role::ESCORT);
    c.roleIndex = 1;
    c.ourFlag = FLAG_ON_PLAYER;
    c.theirFlag = FLAG_ON_PLAYER;
    c.efcKnown = true;
    c.bothHeldMs = STALEMATE_HUNT_MS - 1;
    CHECK(Options(c).front().kind == Kind::ESCORT_CARRIER);
    c.bothHeldMs = STALEMATE_HUNT_MS;
    CHECK(Options(c).front().kind == Kind::HUNT_EFC);
    // One escort keeps guarding the carrier.
    c.roleIndex = 0;
    CHECK(Options(c).front().kind == Kind::ESCORT_CARRIER);
    // A casual bot does not play the stalemate rule.
    c.roleIndex = 1;
    c.tier = Tier::CASUAL;
    CHECK(Options(c).front().kind == Kind::ESCORT_CARRIER);
}

TEST(wsg_the_first_mid_takes_a_power_up_in_the_opening_only)
{
    DecisionContext c = Ctx(Role::MID);
    c.powerupAvailable = true;
    c.playMs = 10000;
    CHECK(Options(c).front().kind == Kind::TAKE_POWERUP);
    c.roleIndex = 1;
    CHECK(Options(c).front().kind == Kind::HOLD_MID);
    c.roleIndex = 0;
    c.playMs = OPENING_MS;
    CHECK(Options(c).front().kind == Kind::HOLD_MID);
}

TEST(wsg_human_call_is_answered_by_good_bots_and_up)
{
    DecisionContext c = Ctx(Role::HOME);
    c.answerCall = true;
    CHECK(Options(c).front().kind == Kind::ANSWER_CALL);
    c.tier = Tier::CASUAL;
    CHECK(Options(c).front().kind != Kind::ANSWER_CALL);
}

// --- graveyard rule ----------------------------------------------------------------------------------------------------

TEST(wsg_no_bot_stays_at_the_enemy_graveyard_past_twenty_seconds)
{
    DwellTracker tracker;
    CHECK(tracker.Update(true, 1000) == 0);
    CHECK(!GyMustLeave(tracker.Update(true, 15000), false));
    CHECK(GyMustLeave(tracker.Update(true, 21001), false));
    // The exception: the enemy carries our flag on that lane.
    CHECK(!GyMustLeave(tracker.Update(true, 30000), true));
    // Leaving resets the clock.
    CHECK(tracker.Update(false, 31000) == 0);
    CHECK(tracker.Update(true, 40000) == 0);
}

// --- call-outs ---------------------------------------------------------------------------------------------------------

TEST(wsg_call_limiter_keeps_one_line_per_six_seconds_per_team)
{
    CallLimiter limiter;
    CHECK(limiter.Allow(1, Call::EFC_TAKEN, 10000));
    limiter.Commit(1, Call::EFC_TAKEN, 10000);
    CHECK(!limiter.Allow(2, Call::FLAG_DOWN, 15999));
    CHECK(limiter.Allow(2, Call::FLAG_DOWN, 16000));
}

TEST(wsg_call_limiter_keeps_twenty_seconds_per_bot)
{
    CallLimiter limiter;
    limiter.Commit(1, Call::EFC_TAKEN, 10000);
    CHECK(!limiter.Allow(1, Call::CAPTURE, 29000));
    CHECK(limiter.Allow(1, Call::CAPTURE, 30000));
    CHECK(limiter.Allow(2, Call::CAPTURE, 17000));
}

TEST(wsg_call_limiter_allows_eight_lines_a_minute)
{
    CallLimiter limiter;
    std::uint32_t now = 1000;
    std::uint32_t sent = 0;
    // Different bots every 6.5 s for a minute and a half: the window caps at eight.
    for (std::uint32_t i = 0; i < 14; ++i)
    {
        if (limiter.Allow(100 + i, Call::INCOMING, now))
        {
            limiter.Commit(100 + i, Call::INCOMING, now);
            if (now < 61000)
                ++sent;
        }
        now += 6500;
    }
    CHECK(sent <= CALL_MAX_PER_WINDOW);
    CHECK(sent >= 8);
}

TEST(wsg_efc_position_calls_come_at_most_every_ten_seconds)
{
    CallLimiter limiter;
    limiter.Commit(1, Call::EFC_MOVED, 10000);
    CHECK(!limiter.Allow(2, Call::EFC_MOVED, 16500));
    CHECK(limiter.Allow(2, Call::EFC_MOVED, 20000));
    CHECK(limiter.Allow(2, Call::FLAG_DOWN, 16500));
}

TEST(wsg_call_chance_and_delay_follow_the_tier)
{
    CHECK(CallChancePercent(Tier::EXPERT) == 100);
    CHECK(CallChancePercent(Tier::GOOD) == 70);
    CHECK(CallChancePercent(Tier::CASUAL) == 30);
    CHECK(CallDelayMs(Tier::EXPERT, 999) == 0);
    CHECK(CallDelayMs(Tier::GOOD, 0) >= 150 && CallDelayMs(Tier::GOOD, 999) <= 400);
    CHECK(CallDelayMs(Tier::CASUAL, 0) >= 400 && CallDelayMs(Tier::CASUAL, 999) <= 900);
    CHECK(AdoptLagMs(Tier::EXPERT) < AdoptLagMs(Tier::GOOD));
    CHECK(AdoptLagMs(Tier::GOOD) < AdoptLagMs(Tier::CASUAL));
}

// --- human calls and places --------------------------------------------------------------------------------------------

TEST(wsg_human_calls_are_read)
{
    using PvpObs::Kind;
    BgCallout::Report call = BgCallout::Parse("EFC mid", BgCallout::MapKind::WSG);
    CHECK(call.valid && call.kind == Kind::ENEMY_CARRIER && call.place == static_cast<std::uint8_t>(Place::MID));
    call = BgCallout::Parse("efc at tun!", BgCallout::MapKind::WSG);
    CHECK(call.valid && call.kind == Kind::ENEMY_CARRIER && call.place == static_cast<std::uint8_t>(Place::TUNNEL));
    call = BgCallout::Parse("inc fr", BgCallout::MapKind::WSG);
    CHECK(call.valid && call.kind == Kind::INCOMING && call.place == static_cast<std::uint8_t>(Place::FLAG_ROOM));
    call = BgCallout::Parse("Incoming roof", BgCallout::MapKind::WSG);
    CHECK(call.valid && call.kind == Kind::INCOMING && call.place == static_cast<std::uint8_t>(Place::ROOF));
    // A bare "help" has no place: the plan reads the sender's own position (it was a guessed flag room before).
    call = BgCallout::Parse("help", BgCallout::MapKind::WSG);
    CHECK(call.valid && call.kind == Kind::HELP && call.place == PvpObs::PLACE_UNKNOWN);
    call = BgCallout::Parse("inc gy", BgCallout::MapKind::WSG);
    CHECK(call.valid && call.kind == Kind::INCOMING && call.place == static_cast<std::uint8_t>(Place::GRAVEYARD));
}

TEST(wsg_other_chat_is_not_a_call)
{
    CHECK(!BgCallout::Parse("", BgCallout::MapKind::WSG).valid);
    CHECK(!BgCallout::Parse("lol nice one", BgCallout::MapKind::WSG).valid);
    CHECK(!BgCallout::Parse("we should really talk about what the efc did on the ramp earlier today", BgCallout::MapKind::WSG).valid);
}

TEST(wsg_place_classification_takes_the_nearest_anchor_in_radius)
{
    std::vector<PlaceAnchor> const anchors = {
        { Place::FLAG_ROOM, 1540.0f, 1481.0f, 40.0f },
        { Place::TUNNEL, 1348.0f, 1461.0f, 50.0f },
        { Place::MID, 1227.0f, 1476.0f, 70.0f },
    };
    CHECK(ClassifyPlace(anchors, 1530.0f, 1480.0f) == Place::FLAG_ROOM);
    CHECK(ClassifyPlace(anchors, 1350.0f, 1470.0f) == Place::TUNNEL);
    CHECK(ClassifyPlace(anchors, 1230.0f, 1480.0f) == Place::MID);
    CHECK(ClassifyPlace(anchors, 1000.0f, 1000.0f) == Place::NONE);
}

TEST(wsg_nearest_n_breaks_ties_by_guid)
{
    std::vector<std::pair<std::uint32_t, float>> list = { { 9, 10.0f }, { 3, 10.0f }, { 5, 4.0f }, { 7, 30.0f } };
    std::vector<std::uint32_t> const two = NearestN(list, 2);
    CHECK(two.size() == 2);
    CHECK(two[0] == 5 && two[1] == 3);
    CHECK(NearestN(list, 10).size() == 4);
}

TEST(wsg_assignment_with_dead_bots_fills_the_important_slots_first)
{
    // Four alive of a team of ten: the taker, backup, escorts and home guards come before mid.
    std::vector<Candidate> team = { Bot(401, CLS_ROGUE), Bot(402, CLS_WARRIOR), Bot(403, CLS_PALADIN), Bot(404, CLS_MAGE) };
    std::vector<Assignment> const list = Assign(team, CountsFor(Plan::BALANCED, Situation::NORMAL, FullMix()));
    CHECK(list.size() == 4);
    CHECK(CountRole(list, Role::TAKER) == 1);
    CHECK(CountRole(list, Role::BACKUP) == 1);
    CHECK(CountRole(list, Role::MID) == 0);
}

TEST(wsg_extra_bots_beyond_the_counts_become_mid)
{
    std::vector<Candidate> team;
    for (std::uint32_t i = 0; i < 6; ++i)
        team.push_back(Bot(500 + i, CLS_MAGE));
    RoleCounts counts;
    counts[Role::TAKER] = 1;
    counts[Role::HOME] = 1;
    std::vector<Assignment> const list = Assign(team, counts);
    CHECK(list.size() == 6);
    CHECK(CountRole(list, Role::MID) == 4);
}

TEST(wsg_small_team_lead_keeps_one_taker_and_every_bot_a_role)
{
    TeamMix mix;
    mix.size = 5;
    RoleCounts const counts = CountsFor(Plan::OFFENSE, Situation::LEAD2, mix);
    CHECK(counts.Total() == 5);
    CHECK(counts[Role::TAKER] == 1);
    CHECK(counts[Role::HOME] == 1);
}

TEST(wsg_every_decision_has_at_least_one_option)
{
    for (Role role : { Role::TAKER, Role::BACKUP, Role::ESCORT, Role::HOME, Role::MID, Role::NONE })
        for (Tier tier : { Tier::EXPERT, Tier::GOOD, Tier::CASUAL })
            for (std::uint8_t our : { FLAG_ON_BASE, FLAG_WAIT_RESPAWN, FLAG_ON_PLAYER, FLAG_ON_GROUND })
                for (std::uint8_t their : { FLAG_ON_BASE, FLAG_WAIT_RESPAWN, FLAG_ON_PLAYER, FLAG_ON_GROUND })
                {
                    DecisionContext c = Ctx(role, tier);
                    c.ourFlag = our;
                    c.theirFlag = their;
                    c.efcKnown = our == FLAG_ON_PLAYER;
                    CHECK(!Options(c).empty());
                    c.hasFlag = true;
                    CHECK(!Options(c).empty());
                }
}

// --- own graveyard exit (BT-386) --------------------------------------------------------------------------------

TEST(wsg_graveyard_hop_sends_a_bot_at_its_graveyard_to_the_exit_first)
{
    // At the graveyard, a target in the base 140 yd away and far from the exit: hop.
    CHECK(NeedsGraveyardExitHop(2.0f, 140.0f, 190.0f, false));
    // The edge of the graveyard zone still counts, outside it does not.
    CHECK(NeedsGraveyardExitHop(GY_EXIT_HOP_RADIUS, 140.0f, 190.0f, false));
    CHECK(!NeedsGraveyardExitHop(GY_EXIT_HOP_RADIUS + 1.0f, 140.0f, 190.0f, false));
}

TEST(wsg_graveyard_hop_leaves_near_targets_carriers_and_the_exit_itself_alone)
{
    // A fight at the graveyard: the enemy carrier next to the bot.
    CHECK(!NeedsGraveyardExitHop(2.0f, GY_EXIT_HOP_MIN_TARGET, 190.0f, false));
    CHECK(NeedsGraveyardExitHop(2.0f, GY_EXIT_HOP_MIN_TARGET + 1.0f, 190.0f, false));
    // A carrier never respawns at the graveyard, but never hops either.
    CHECK(!NeedsGraveyardExitHop(2.0f, 140.0f, 190.0f, true));
    // The target already is the exit: nothing to replace.
    CHECK(!NeedsGraveyardExitHop(2.0f, 119.0f, GY_EXIT_HOP_AT_EXIT, false));
    CHECK(NeedsGraveyardExitHop(2.0f, 119.0f, GY_EXIT_HOP_AT_EXIT + 1.0f, false));
}

// --- the carrier runs home (owner rule 2026-11-17) ----------------------------------------------------------------------

namespace
{
CarrierFacts Chased(std::uint32_t attackers, std::uint32_t enemiesNear)
{
    CarrierFacts f;
    f.attackers = attackers;
    f.enemiesNear = enemiesNear;
    return f;
}
}  // namespace

TEST(wsg_carrier_with_nobody_on_it_runs)
{
    CarrierOption const o = ChooseCarrier(Chased(0, 0));
    CHECK(o.choice == CarrierChoice::RUN);
    CHECK(std::string(o.why) == "no-attacker");
}

TEST(wsg_carrier_at_the_enemy_stand_runs_from_two_attackers_and_from_one_with_friends_of_theirs_near)
{
    // The first live match: one defender hits the taker, three more stand at the stand. The old rule fought the one.
    CarrierOption const two = ChooseCarrier(Chased(2, 3));
    CHECK(two.choice == CarrierChoice::RUN);
    CHECK(std::string(two.why) == "outnumbered");

    CarrierOption const oneOfFour = ChooseCarrier(Chased(1, 4));
    CHECK(oneOfFour.choice == CarrierChoice::RUN);
    CHECK(std::string(oneOfFour.why) == "outnumbered");
}

TEST(wsg_carrier_caught_alone_fights_back)
{
    CarrierOption const o = ChooseCarrier(Chased(1, 1));
    CHECK(o.choice == CarrierChoice::FIGHT_ALONE);
    CHECK(CarrierFights(o.choice));
}

TEST(wsg_carrier_with_an_escort_beside_it_leaves_the_single_attacker_to_the_escort)
{
    CarrierFacts f = Chased(1, 1);
    f.escortsNear = 1;
    CarrierOption const o = ChooseCarrier(f);
    CHECK(o.choice == CarrierChoice::RUN);
    CHECK(std::string(o.why) == "escorted");
}

TEST(wsg_carrier_does_not_trade_blows_when_low_on_health)
{
    CarrierFacts f = Chased(1, 1);
    f.hpPercent = CARRIER_FIGHT_MIN_HP - 1.0f;
    CarrierOption const o = ChooseCarrier(f);
    CHECK(o.choice == CarrierChoice::RUN);
    CHECK(std::string(o.why) == "low-hp");
    f.hpPercent = CARRIER_FIGHT_MIN_HP;
    CHECK(ChooseCarrier(f).choice == CarrierChoice::FIGHT_ALONE);
}

TEST(wsg_carrier_that_cannot_run_fights_whoever_is_on_it)
{
    CarrierFacts rooted = Chased(3, 4);
    rooted.rooted = true;
    CHECK(ChooseCarrier(rooted).choice == CarrierChoice::FIGHT_ROOTED);

    CarrierFacts slowed = Chased(3, 4);
    slowed.slowed = true;
    CHECK(ChooseCarrier(slowed).choice == CarrierChoice::FIGHT_SLOWED);

    // A root with nobody on the carrier: nothing to fight, the choice stays run (it moves when the root ends).
    CarrierFacts quietRoot = Chased(0, 0);
    quietRoot.rooted = true;
    CHECK(ChooseCarrier(quietRoot).choice == CarrierChoice::RUN);
}

TEST(wsg_carrier_near_its_own_stand_never_stops_unless_rooted)
{
    CarrierFacts f = Chased(1, 1);
    f.nearOwnCapture = true;
    CHECK(ChooseCarrier(f).choice == CarrierChoice::RUN_NEAR_CAPTURE);
    f.slowed = true;
    CHECK(ChooseCarrier(f).choice == CarrierChoice::RUN_NEAR_CAPTURE);
    f.rooted = true;
    CHECK(ChooseCarrier(f).choice == CarrierChoice::FIGHT_ROOTED);
}

TEST(wsg_a_fight_nobody_feeds_ends_and_the_carrier_runs_again)
{
    CarrierFacts f = Chased(1, 1);
    f.hitRecently = false;
    CHECK(ChooseCarrier(f).choice == CarrierChoice::RUN);
    f.slowed = true;
    CHECK(ChooseCarrier(f).choice == CarrierChoice::RUN);
    // A rooted carrier still answers: it cannot leave.
    f.rooted = true;
    CHECK(ChooseCarrier(f).choice == CarrierChoice::FIGHT_ROOTED);
}

TEST(wsg_casual_carrier_runs_at_the_stand_like_the_better_tiers_and_keeps_a_lower_health_floor)
{
    // Owner 2026-11-17: casual carriers play the same circumstances; one attacker among four enemies means run.
    CarrierFacts crowd = Chased(1, 4);
    crowd.tier = Tier::CASUAL;
    CHECK(ChooseCarrier(crowd).choice == CarrierChoice::RUN);
    crowd.escortsNear = 1;
    CHECK(ChooseCarrier(crowd).choice == CarrierChoice::RUN);

    CarrierFacts alone = Chased(1, 1);
    alone.tier = Tier::CASUAL;
    CHECK(ChooseCarrier(alone).choice == CarrierChoice::FIGHT_ALONE);

    // The lower floor: a casual carrier still fights at 30 percent where a good one runs.
    alone.hpPercent = 30.0f;
    CHECK(ChooseCarrier(alone).choice == CarrierChoice::FIGHT_ALONE);
    CarrierFacts good = alone;
    good.tier = Tier::GOOD;
    CHECK(ChooseCarrier(good).choice == CarrierChoice::RUN);

    CHECK(ChooseCarrier([&] { CarrierFacts g = alone; g.attackers = 2; return g; }()).choice == CarrierChoice::RUN);
    CHECK(ChooseCarrier([&] { CarrierFacts g = alone; g.hpPercent = CARRIER_FIGHT_MIN_HP_CASUAL - 1.0f; return g; }()).choice ==
          CarrierChoice::RUN);
}

TEST(wsg_carrier_options_are_sorted_and_the_run_is_always_there)
{
    for (std::uint32_t attackers = 0; attackers <= 3; ++attackers)
        for (std::uint32_t enemies = 0; enemies <= 4; ++enemies)
            for (int flags = 0; flags < 16; ++flags)
                for (Tier tier : { Tier::EXPERT, Tier::GOOD, Tier::CASUAL })
                {
                    CarrierFacts f = Chased(attackers, enemies);
                    f.tier = tier;
                    f.rooted = flags & 1;
                    f.slowed = flags & 2;
                    f.nearOwnCapture = flags & 4;
                    f.hitRecently = !(flags & 8);
                    std::vector<CarrierOption> const list = CarrierOptions(f);
                    bool hasRun = false;
                    for (std::size_t i = 0; i < list.size(); ++i)
                    {
                        hasRun = hasRun || list[i].choice == CarrierChoice::RUN;
                        if (i + 1 < list.size())
                            CHECK(list[i].score >= list[i + 1].score);
                    }
                    CHECK(hasRun);
                    // Never fight with nobody on the carrier.
                    if (attackers == 0)
                        CHECK(!CarrierFights(list.front().choice));
                }
}

TEST(wsg_carrier_uses_a_defensive_under_attack_and_below_its_tier_health)
{
    CHECK(!CarrierWantsDefensive(Tier::GOOD, 40.0f, 0));
    CHECK(CarrierWantsDefensive(Tier::GOOD, CarrierDefensiveHp(Tier::GOOD), 1));
    CHECK(!CarrierWantsDefensive(Tier::GOOD, CarrierDefensiveHp(Tier::GOOD) + 1.0f, 1));
    // Experts start earlier than casual bots.
    CHECK(CarrierDefensiveHp(Tier::EXPERT) > CarrierDefensiveHp(Tier::GOOD));
    CHECK(CarrierDefensiveHp(Tier::GOOD) > CarrierDefensiveHp(Tier::CASUAL));
}

TEST(wsg_carrier_speed_is_for_a_chased_carrier_that_is_not_sped_up_rooted_or_home)
{
    CHECK(CarrierWantsSpeed(1, 1, false, false, false));
    CHECK(CarrierWantsSpeed(0, 2, false, false, false));
    CHECK(!CarrierWantsSpeed(0, 0, false, false, false));
    CHECK(!CarrierWantsSpeed(2, 2, true, false, false));
    CHECK(!CarrierWantsSpeed(2, 2, false, true, false));
    CHECK(!CarrierWantsSpeed(2, 2, false, false, true));
}

TEST(wsg_carrier_freedom_is_for_a_rooted_or_slowed_carrier_under_attack)
{
    CHECK(CarrierWantsFreedom(true, false, 1));
    CHECK(CarrierWantsFreedom(false, true, 2));
    CHECK(!CarrierWantsFreedom(false, false, 3));
    CHECK(!CarrierWantsFreedom(true, true, 0));
}

// --- healers follow the carrier -------------------------------------------------------------------------------------------

TEST(wsg_the_two_nearest_healers_follow_the_carrier)
{
    std::vector<HealerCandidate> const list = { { 10, false, 90.0f }, { 11, false, 30.0f }, { 12, false, 60.0f } };
    std::vector<std::uint32_t> const picked = PickHealerFollowers(list);
    CHECK(picked.size() == 2);
    CHECK(picked[0] == 11);
    CHECK(picked[1] == 12);
}

TEST(wsg_a_lone_healer_always_follows_even_far_away)
{
    std::vector<std::uint32_t> const picked = PickHealerFollowers({ { 7, false, 400.0f } });
    CHECK(picked.size() == 1 && picked[0] == 7);
    CHECK(PickHealerFollowers({}).empty());
}

TEST(wsg_a_home_guard_healer_joins_only_when_the_carrier_is_close)
{
    CHECK(PickHealerFollowers({ { 5, true, 300.0f } }).empty());
    CHECK(PickHealerFollowers({ { 5, true, HOME_HEALER_JOIN_YARDS } }).size() == 1);
    // A far home healer does not take a follower slot from a mid healer.
    std::vector<std::uint32_t> const picked = PickHealerFollowers({ { 5, true, 300.0f }, { 6, false, 200.0f } });
    CHECK(picked.size() == 1 && picked[0] == 6);
}

TEST(wsg_healer_pick_breaks_ties_by_guid_and_ignores_input_order)
{
    std::vector<std::uint32_t> const a = PickHealerFollowers({ { 20, false, 50.0f }, { 21, false, 50.0f }, { 22, false, 50.0f } });
    std::vector<std::uint32_t> const b = PickHealerFollowers({ { 22, false, 50.0f }, { 20, false, 50.0f }, { 21, false, 50.0f } });
    CHECK(a == b);
    CHECK(a.size() == 2 && a[0] == 20 && a[1] == 21);
}

TEST(wsg_following_healer_goes_for_the_carrier_ahead_of_every_phase_duty)
{
    // A MID healer while we carry would hunt the enemy carrier (860) or cover the lane (700); it follows instead.
    for (Role role : { Role::MID, Role::ESCORT, Role::BACKUP, Role::HOME })
        for (Tier tier : { Tier::EXPERT, Tier::GOOD, Tier::CASUAL })
        {
            DecisionContext c = Ctx(role, tier);
            c.theirFlag = FLAG_ON_PLAYER;
            c.healerFollower = true;
            CHECK(Options(c).front().kind == Kind::HEAL_CARRIER);
        }
}

TEST(wsg_following_healer_still_answers_a_dropped_flag_first_and_does_nothing_special_without_a_carrier)
{
    DecisionContext c = Ctx(Role::MID);
    c.healerFollower = true;
    c.theirFlag = FLAG_ON_PLAYER;
    c.ourFlag = FLAG_ON_GROUND;
    c.ownGroundResponder = true;
    CHECK(Options(c).front().kind == Kind::RETURN_OWN_FLAG);

    // We do not carry: the follower flag changes nothing.
    DecisionContext idle = Ctx(Role::MID);
    idle.healerFollower = true;
    for (Option const& option : Options(idle))
        CHECK(option.kind != Kind::HEAL_CARRIER);
}

TEST(wsg_healer_leash_closes_with_hysteresis)
{
    CHECK(!HealerShouldClose(HEALER_LEASH_YARDS, false));
    CHECK(HealerShouldClose(HEALER_LEASH_YARDS + 1.0f, false));
    // Once on its way it keeps closing until it is near, no stop and go at the leash edge.
    CHECK(HealerShouldClose(HEALER_LEASH_YARDS - 5.0f, true));
    CHECK(!HealerShouldClose(HEALER_CLOSE_TO, true));
    CHECK(HealerFollowYards(Tier::GOOD) < HealerFollowYards(Tier::CASUAL));
}

TEST(wsg_healer_stops_to_heal_a_hurt_carrier_in_reach_and_runs_after_a_healthy_or_far_one)
{
    CHECK(HealerStopsToHeal(20.0f, 60.0f));
    CHECK(HealerStopsToHeal(HEALER_STOP_TO_HEAL_YARDS, 89.0f));
    CHECK(!HealerStopsToHeal(HEALER_STOP_TO_HEAL_YARDS + 1.0f, 30.0f));
    CHECK(!HealerStopsToHeal(20.0f, 95.0f));
}

// --- the decision log gate ------------------------------------------------------------------------------------------------

TEST(wsg_decision_log_gate_logs_changes_at_most_every_ten_seconds)
{
    DecisionLogGate gate;
    CHECK(gate.Due(1, 1000));           // the first decision is logged
    CHECK(!gate.Due(1, 20000));         // same decision: never again
    CHECK(!gate.Due(2, 5000));          // changed inside the gap: held back
    CHECK(gate.Due(2, 11000));          // and logged once the gap passed (the change is not lost)
    CHECK(!gate.Due(2, 30000));
    CHECK(gate.Due(3, 21000));
    CHECK(!gate.Due(4, 21000 + DECISION_LOG_GAP_MS - 1));
    CHECK(gate.Due(4, 21000 + DECISION_LOG_GAP_MS));
}
