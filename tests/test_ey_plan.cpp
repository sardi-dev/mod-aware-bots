#include "TestHarness.h"
#include "EyPlanLogic.h"

// BT-580: Eye of the Storm playbook logic (docs/bg-sheets/eye-of-the-storm.md), the part the old objective case uses.

using namespace EyLogic;

namespace
{
Member Bot(std::uint32_t guid, std::uint8_t cls = BgPlan::CLS_WARRIOR, std::uint8_t level = 60, Tier tier = Tier::GOOD)
{
    Member m;
    m.guidLow = guid;
    m.cls = cls;
    m.level = level;
    m.tier = tier;
    return m;
}

std::vector<Member> Team(std::uint32_t count)
{
    std::vector<Member> bots;
    for (std::uint32_t i = 0; i < count; ++i)
        bots.push_back(Bot(1000 + i, i == 0 ? BgPlan::CLS_ROGUE : i == 1 ? BgPlan::CLS_MAGE : BgPlan::CLS_WARRIOR));
    return bots;
}

World Match(std::uint8_t team)
{
    World w;
    w.team = team;
    return w;
}

Me MeOf(std::uint32_t guid, float x = 2200.0f, float y = 1569.0f)
{
    Me me;
    me.guidLow = guid;
    me.x = x;
    me.y = y;
    me.level = 60;
    return me;
}

unsigned Count(std::vector<Assigned> const& all, Role role, std::uint8_t base = BASE_NONE)
{
    unsigned n = 0;
    for (Assigned const& a : all)
        if (a.slot.role == role && (base == BASE_NONE || a.slot.base == base))
            ++n;
    return n;
}
}  // namespace

TEST(ey_map_data_is_the_cores)
{
    CHECK(MAP_ID == 566 && BASE_COUNT == 4 && MAX_SCORE == 1600);
    CHECK(TICK_POINTS[1] == 1 && TICK_POINTS[2] == 2 && TICK_POINTS[3] == 5 && TICK_POINTS[4] == 10);
    CHECK(FLAG_POINTS[1] == 75 && FLAG_POINTS[2] == 85 && FLAG_POINTS[3] == 100 && FLAG_POINTS[4] == 500);
    CHECK(BAR_TICK_MS == 2000 && CIRCLE_RADIUS == 70.0f && TRIGGER_RADIUS == 3.0f);
    CHECK_NEAR(PointsPerSecond(1), 0.5, 0.001);
    CHECK_NEAR(PointsPerSecond(2), 1.0, 0.001);
    CHECK_NEAR(PointsPerSecond(3), 2.5, 0.001);
    CHECK_NEAR(PointsPerSecond(4), 5.0, 0.001);
    CHECK(FlagPoints(0) == 0 && FlagPoints(9) == 500);
    CHECK(TRIGGER_ID[FEL_REAVER] == 4514 && TRIGGER_ID[BLOOD_ELF] == 4476 && TRIGGER_ID[DRAENEI_RUINS] == 4518 &&
          TRIGGER_ID[MAGE_TOWER] == 4516);
    CHECK(std::string(BaseWord(MAGE_TOWER)) == "mt" && std::string(BaseWord(9)) == "?");
}

TEST(ey_near_bases_are_the_two_nearest_the_start_room)
{
    std::uint8_t nb[2];
    NearBases(TEAM_IDX_ALLIANCE, nb);
    CHECK(nb[0] == MAGE_TOWER && nb[1] == DRAENEI_RUINS);
    NearBases(TEAM_IDX_HORDE, nb);
    CHECK(nb[0] == FEL_REAVER && nb[1] == BLOOD_ELF);
    // The data agrees: each near base is nearer its start room than the far bases (275 to 307 yd against 498 to 518).
    for (std::uint8_t team : { TEAM_IDX_ALLIANCE, TEAM_IDX_HORDE })
        for (std::uint8_t b = 0; b < BASE_COUNT; ++b)
        {
            float const d = Dist2d(START_ROOM[team].x, START_ROOM[team].y, TRIGGER_POS[b].x, TRIGGER_POS[b].y);
            CHECK(IsNearBase(team, b) ? d < 320.0f : d > 480.0f);
        }
}

TEST(ey_circle_is_70_yards_round_the_cap_object)
{
    CHECK(InCircle(MAGE_TOWER, 2284.0f, 1731.0f, 1190.0f));      // at the flag, 29 yd from the cap object
    CHECK(InCircle(MAGE_TOWER, 2282.0f, 1825.0f, 1190.0f));     // 65 yd from the cap object: still inside
    CHECK(!InCircle(MAGE_TOWER, 2282.0f, 1835.0f, 1190.0f));
    CHECK(!InCircle(9, 0.0f, 0.0f, 0.0f));
}

TEST(ey_capture_bar_is_clamped_on_both_sides)
{
    CHECK(NetPerTick(3, 0) == 3 && NetPerTick(0, 3) == -3);
    CHECK(NetPerTick(9, 1) == 5 && NetPerTick(1, 9) == -5);   // the fork's clamp (6f7bac7dfb): the Horde no longer moves it 8
    CHECK(NetPerTick(4, 4) == 0);
}

TEST(ey_take_times_follow_the_net_count)
{
    CHECK(TakeSeconds(1) == 40 && TakeSeconds(2) == 20 && TakeSeconds(3) == 14);
    CHECK(TakeSeconds(4) == 10 && TakeSeconds(5) == 8);
    CHECK(TakeSeconds(-3) == 14);       // either side
    CHECK(TakeSeconds(9) == 8);         // never below 8 s
    CHECK(TakeSeconds(0) == 40);
}

TEST(ey_margin_is_how_far_the_bar_is_from_the_owners_edge)
{
    CHECK(Margin(70, TEAM_IDX_ALLIANCE) == 1 && Margin(100, TEAM_IDX_ALLIANCE) == MAX_MARGIN);
    CHECK(Margin(30, TEAM_IDX_HORDE) == 1 && Margin(0, TEAM_IDX_HORDE) == MAX_MARGIN);
    CHECK(Margin(50, TEAM_IDX_ALLIANCE) == 0 && Margin(50, TEAM_IDX_HORDE) == 0);   // neutral: nothing to defend
    CHECK(Margin(70, TEAM_NONE) == 0);
    CHECK(!Hardened(74, TEAM_IDX_ALLIANCE) && Hardened(84, TEAM_IDX_ALLIANCE) && !Hardened(84, TEAM_IDX_HORDE));
    CHECK(NeutraliseSeconds(1, 1) == 2 && NeutraliseSeconds(31, 5) == 14 && NeutraliseSeconds(31, 3) == 22);
}

TEST(ey_swing_values_are_the_sheet_table)
{
    CHECK_NEAR(TakeValue(0), 0.5, 0.001);
    CHECK_NEAR(TakeValue(2), 1.5, 0.001);        // the third base
    CHECK_NEAR(TakeValue(3), 2.5, 0.001);        // the fourth base
    CHECK_NEAR(TakeValue(4), 0.0, 0.001);
    CHECK_NEAR(NeutraliseValue(4), 2.5, 0.001);
    CHECK_NEAR(NeutraliseValue(3), 1.5, 0.001);
    CHECK_NEAR(NeutraliseValue(1), 0.5, 0.001);
    CHECK_NEAR(NeutraliseValue(0), 0.0, 0.001);
}

TEST(ey_squads_are_three_with_a_pinned_defender_per_near_base)
{
    std::vector<Member> const bots = Team(15);
    std::vector<Assigned> const a = AssignOpening(BgPlan::Plan::BALANCED, TEAM_IDX_ALLIANCE, bots);
    CHECK(a.size() == 15);
    CHECK(Count(a, Role::PINNED, MAGE_TOWER) == 1 && Count(a, Role::PINNED, DRAENEI_RUINS) == 1);
    CHECK(Count(a, Role::SQUAD, MAGE_TOWER) == 2 && Count(a, Role::SQUAD, DRAENEI_RUINS) == 2);   // 3 at each base with the pinned
    CHECK(Count(a, Role::MIDDLE) == 9);
    // The best kits hold the posts: the rogue and the mage.
    CHECK(SlotOf(a, 1000).role == Role::PINNED && SlotOf(a, 1001).role == Role::PINNED);
    CHECK(SlotOf(a, 1000).base != SlotOf(a, 1001).base);
}

TEST(ey_defense_plan_has_four_squads_and_two_pinned)
{
    std::vector<Assigned> const a = AssignOpening(BgPlan::Plan::DEFENSE, TEAM_IDX_HORDE, Team(15));
    CHECK(Count(a, Role::PINNED, FEL_REAVER) == 2 && Count(a, Role::PINNED, BLOOD_ELF) == 2);
    CHECK(Count(a, Role::SQUAD, FEL_REAVER) == 2 && Count(a, Role::SQUAD, BLOOD_ELF) == 2);
    CHECK(Count(a, Role::MIDDLE) == 7);
    // A small team keeps squads of 3.
    std::vector<Assigned> const small = AssignOpening(BgPlan::Plan::DEFENSE, TEAM_IDX_HORDE, Team(10));
    CHECK(Count(small, Role::PINNED) == 4 && Count(small, Role::SQUAD) == 2 && Count(small, Role::MIDDLE) == 4);
}

TEST(ey_small_teams_take_fewer_bases_and_never_a_lone_one)
{
    std::vector<Assigned> const eight = AssignOpening(BgPlan::Plan::BALANCED, TEAM_IDX_ALLIANCE, Team(8));
    CHECK(Count(eight, Role::PINNED) == 2 && Count(eight, Role::SQUAD) == 4 && Count(eight, Role::MIDDLE) == 2);
    std::vector<Assigned> const five = AssignOpening(BgPlan::Plan::BALANCED, TEAM_IDX_ALLIANCE, Team(5));
    CHECK(Count(five, Role::PINNED) == 1 && Count(five, Role::SQUAD) == 2 && Count(five, Role::MIDDLE) == 2);
    CHECK(Count(five, Role::PINNED, MAGE_TOWER) == 1 && Count(five, Role::SQUAD, MAGE_TOWER) == 2);
    // Fewer than 3 bots: nobody leaves for a base alone.
    std::vector<Assigned> const two = AssignOpening(BgPlan::Plan::BALANCED, TEAM_IDX_ALLIANCE, Team(2));
    CHECK(Count(two, Role::MIDDLE) == 2 && Count(two, Role::PINNED) == 0 && Count(two, Role::SQUAD) == 0);
    CHECK(AssignOpening(BgPlan::Plan::BALANCED, TEAM_IDX_ALLIANCE, {}).empty());
}

TEST(ey_assignment_is_deterministic_and_stable_under_a_death)
{
    std::vector<Member> bots = Team(15);
    std::vector<Assigned> const a = AssignOpening(BgPlan::Plan::BALANCED, TEAM_IDX_ALLIANCE, bots);
    std::vector<Assigned> const b = AssignOpening(BgPlan::Plan::BALANCED, TEAM_IDX_ALLIANCE, bots);
    for (std::size_t i = 0; i < a.size(); ++i)
        CHECK(a[i].slot.role == b[i].slot.role && a[i].slot.base == b[i].slot.base);
    // The same input in another order gives the same slots.
    std::reverse(bots.begin(), bots.end());
    std::vector<Assigned> const c = AssignOpening(BgPlan::Plan::BALANCED, TEAM_IDX_ALLIANCE, bots);
    for (Assigned const& x : a)
    {
        Slot const s = SlotOf(c, x.guidLow);
        CHECK(s.role == x.slot.role && s.base == x.slot.base);
    }
}

TEST(ey_a_carrier_runs_to_the_nearest_base_its_team_holds)
{
    World w = Match(TEAM_IDX_ALLIANCE);
    w.owner[DRAENEI_RUINS] = TEAM_IDX_ALLIANCE;
    w.owner[MAGE_TOWER] = TEAM_IDX_ALLIANCE;
    Me me = MeOf(1005, 2174.0f, 1569.0f);
    me.carrying = true;
    Choice c = Choose(w, BgPlan::Plan::BALANCED, Team(15), me);
    CHECK(c.what == What::CARRY && c.base == MAGE_TOWER);   // 196 yd against 201
    // A base that falls is not chosen the next time.
    w.owner[MAGE_TOWER] = TEAM_IDX_HORDE;
    c = Choose(w, BgPlan::Plan::BALANCED, Team(15), me);
    CHECK(c.what == What::CARRY && c.base == DRAENEI_RUINS);
    // No base: it keeps clear on its own side and never scores.
    w.owner[DRAENEI_RUINS] = TEAM_NONE;
    c = Choose(w, BgPlan::Plan::BALANCED, Team(15), me);
    CHECK(c.what == What::RETREAT && c.base == BASE_NONE);
}

TEST(ey_pinned_defender_stays_at_its_base_whatever_happens)
{
    World w = Match(TEAM_IDX_ALLIANCE);
    std::vector<Member> const bots = Team(15);
    std::vector<Assigned> const a = AssignOpening(BgPlan::Plan::BALANCED, TEAM_IDX_ALLIANCE, bots);
    Slot const s = SlotOf(a, 1000);
    CHECK(s.role == Role::PINNED);
    Choice c = Choose(w, BgPlan::Plan::BALANCED, bots, MeOf(1000));
    CHECK(c.what == What::POST && c.base == s.base);
    w.owner[s.base] = TEAM_IDX_HORDE;    // the base fell: the defender still stands in the circle
    c = Choose(w, BgPlan::Plan::BALANCED, bots, MeOf(1000));
    CHECK(c.what == What::POST && c.base == s.base);
}

TEST(ey_a_squad_stays_until_its_base_is_ours_and_hardened)
{
    World w = Match(TEAM_IDX_ALLIANCE);
    std::vector<Member> const bots = Team(15);
    std::vector<Assigned> const a = AssignOpening(BgPlan::Plan::BALANCED, TEAM_IDX_ALLIANCE, bots);
    std::uint32_t squadBot = 0;
    for (Assigned const& x : a)
        if (x.slot.role == Role::SQUAD)
        {
            squadBot = x.guidLow;
            break;
        }
    Slot const s = SlotOf(a, squadBot);
    Choice c = Choose(w, BgPlan::Plan::BALANCED, bots, MeOf(squadBot));
    CHECK(c.what == What::TAKE && c.base == s.base);          // neutral: walk into the circle and stay
    w.owner[s.base] = TEAM_IDX_ALLIANCE;
    w.bar[s.base] = 72;                                         // just taken: margin 3, one enemy neutralises it in 2 s
    c = Choose(w, BgPlan::Plan::BALANCED, bots, MeOf(squadBot));
    CHECK(c.what == What::POST && c.base == s.base);
    w.bar[s.base] = 90;                                         // hardened: the squad is released
    c = Choose(w, BgPlan::Plan::BALANCED, bots, MeOf(squadBot));
    CHECK(c.base != s.base || c.what != What::POST);
}

TEST(ey_take_group_is_three_or_more_never_fewer)
{
    World w = Match(TEAM_IDX_ALLIANCE);
    // Two free bots (a team of 8 after the squads): nobody starts a take.
    std::vector<Member> const eight = Team(8);
    std::vector<Assigned> const a = AssignOpening(BgPlan::Plan::BALANCED, TEAM_IDX_ALLIANCE, eight);
    for (Assigned const& x : a)
        if (x.slot.role == Role::MIDDLE)
        {
            Choice const c = Choose(w, BgPlan::Plan::BALANCED, eight, MeOf(x.guidLow));
            CHECK(c.what != What::TAKE);
        }
    // Nine free bots: a group of 4 takes a base, the rest do not.
    std::vector<Member> const fifteen = Team(15);
    std::vector<Assigned> const b = AssignOpening(BgPlan::Plan::BALANCED, TEAM_IDX_ALLIANCE, fifteen);
    unsigned takers = 0;
    unsigned free = 0;
    for (Assigned const& x : b)
        if (x.slot.role == Role::MIDDLE)
        {
            ++free;
            if (Choose(w, BgPlan::Plan::BALANCED, fifteen, MeOf(x.guidLow)).what == What::TAKE)
                ++takers;
        }
    CHECK(free == 9 && takers == 4);
}

TEST(ey_hold_state_with_three_bases_guards_two_per_base_and_never_camps)
{
    World w = Match(TEAM_IDX_ALLIANCE);
    w.owner[MAGE_TOWER] = w.owner[DRAENEI_RUINS] = w.owner[BLOOD_ELF] = TEAM_IDX_ALLIANCE;
    for (int b = 0; b < 4; ++b)
        w.bar[b] = 100;
    std::vector<Member> const bots = Team(15);
    std::vector<Assigned> const a = AssignOpening(BgPlan::Plan::BALANCED, TEAM_IDX_ALLIANCE, bots);
    unsigned posts[BASE_COUNT] = { 0, 0, 0, 0 };
    unsigned stage = 0;
    unsigned take = 0;
    for (Assigned const& x : a)
    {
        Choice const c = Choose(w, BgPlan::Plan::BALANCED, bots, MeOf(x.guidLow));
        CHECK(c.what != What::CARRY && c.what != What::RETREAT);
        if (c.what == What::POST)
            ++posts[c.base];
        else if (c.what == What::STAGE || c.what == What::FLAG)
            ++stage;
        else if (c.what == What::TAKE)
            ++take;
    }
    CHECK(posts[MAGE_TOWER] == 2 && posts[DRAENEI_RUINS] == 2 && posts[BLOOD_ELF] == 2 && posts[FEL_REAVER] == 0);
    // 15 bots: 2 pinned + 6 guards in the circles of three bases, 9 free minus 4 guard slots go on to the fourth base (6 or more are free).
    CHECK(take + stage + posts[0] + posts[1] + posts[2] + posts[3] == 15);
}

TEST(ey_hold_state_never_sends_a_bot_to_the_enemy_start_room)
{
    World w = Match(TEAM_IDX_ALLIANCE);
    for (int b = 0; b < 4; ++b)
    {
        w.owner[b] = TEAM_IDX_ALLIANCE;
        w.bar[b] = 100;
    }
    std::vector<Member> const bots = Team(15);
    for (Member const& m : bots)
    {
        Choice const c = Choose(w, BgPlan::Plan::BALANCED, bots, MeOf(m.guidLow));
        // Every answer is a post at a base we hold, the flag or a stage point: never a base to take (all four are ours).
        CHECK(c.what == What::POST || c.what == What::STAGE || c.what == What::FLAG);
        CHECK(c.what != What::TAKE && c.what != What::NOTHING);
    }
}

TEST(ey_the_hold_order_starts_at_the_base_nearest_the_enemy_start)
{
    World w = Match(TEAM_IDX_ALLIANCE);
    w.owner[MAGE_TOWER] = w.owner[FEL_REAVER] = w.owner[BLOOD_ELF] = TEAM_IDX_ALLIANCE;
    std::vector<std::uint8_t> const order = HoldOrder(w);
    CHECK(order.size() == 3);
    CHECK(order[0] == BLOOD_ELF);   // 286 yd from the Horde start room: the nearest
    CHECK(order[2] == MAGE_TOWER);
}

TEST(ey_best_base_to_take_follows_the_swing)
{
    World w = Match(TEAM_IDX_ALLIANCE);
    w.owner[MAGE_TOWER] = w.owner[DRAENEI_RUINS] = TEAM_IDX_ALLIANCE;
    w.owner[FEL_REAVER] = TEAM_IDX_HORDE;
    // The third base is a neutral one (+1.5) or the enemy's (+1.5 and 0.5): the enemy's base wins at equal distance.
    std::uint8_t const best = BestTakeBase(w, 2174.0f, 1569.0f, 60);
    CHECK(best == FEL_REAVER || best == BLOOD_ELF);
    w.owner[BLOOD_ELF] = TEAM_IDX_HORDE;
    CHECK(BestTakeBase(w, 2174.0f, 1569.0f, 60) == FEL_REAVER || BestTakeBase(w, 2174.0f, 1569.0f, 60) == BLOOD_ELF);
    for (int b = 0; b < 4; ++b)
        w.owner[b] = TEAM_IDX_ALLIANCE;
    CHECK(BestTakeBase(w, 2174.0f, 1569.0f, 60) == BASE_NONE);
}

TEST(ey_flag_goes_to_one_taker_when_it_is_free_and_a_base_is_held)
{
    World w = Match(TEAM_IDX_ALLIANCE);
    w.owner[MAGE_TOWER] = TEAM_IDX_ALLIANCE;
    w.bar[MAGE_TOWER] = 100;
    std::vector<Member> const bots = Team(15);
    unsigned flag = 0;
    for (Member const& m : bots)
        if (Choose(w, BgPlan::Plan::BALANCED, bots, MeOf(m.guidLow)).what == What::FLAG)
            ++flag;
    CHECK(flag == 1);
    // A team with no base does not take the flag (it cannot score it): nobody casts.
    World none = Match(TEAM_IDX_ALLIANCE);
    flag = 0;
    for (Member const& m : bots)
        if (Choose(none, BgPlan::Plan::BALANCED, bots, MeOf(m.guidLow)).what == What::FLAG)
            ++flag;
    CHECK(flag == 0);
    // Carried flag: nobody goes to the stand.
    w.teamCarries = true;
    w.flagState = 2;
    flag = 0;
    for (Member const& m : bots)
        if (Choose(w, BgPlan::Plan::BALANCED, bots, MeOf(m.guidLow)).what == What::FLAG)
            ++flag;
    CHECK(flag == 0);
}

TEST(ey_a_bot_the_roster_missed_still_gets_an_answer)
{
    World w = Match(TEAM_IDX_HORDE);
    std::vector<Member> const bots = Team(15);
    Choice const c = Choose(w, BgPlan::Plan::BALANCED, bots, MeOf(77));   // not in the roster
    CHECK(c.what != What::NOTHING);
}
