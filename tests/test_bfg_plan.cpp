#include "TestHarness.h"
#include "BfgPlanLogic.h"

// BT-550: Battle for Gilneas playbook logic (docs/bg-sheets/battle-for-gilneas.md), the part the old objective case uses.

using namespace BfgLogic;

namespace
{
Member Bot(std::uint32_t guid, std::uint8_t cls, std::uint8_t level = 85, Tier tier = Tier::GOOD)
{
    Member m;
    m.guidLow = guid;
    m.cls = cls;
    m.level = level;
    m.tier = tier;
    return m;
}

Member Healer(std::uint32_t guid)
{
    Member m = Bot(guid, BgPlan::CLS_PRIEST);
    m.healer = true;
    return m;
}

// States: 0 neutral, 1 and 2 contested by the Alliance and the Horde, 3 and 4 occupied by them.
Input Alliance(std::uint8_t lh, std::uint8_t ww, std::uint8_t mines)
{
    Input in;
    in.team = Ab::TEAM_IDX_ALLIANCE;
    in.state[0] = lh;
    in.state[1] = ww;
    in.state[2] = mines;
    in.others = 9;
    in.x = 908.0f;
    in.y = 1338.0f;
    return in;
}
}  // namespace

TEST(bfg_core_constants_match_the_server)
{
    CHECK(MAX_SCORE == 2000);
    CHECK(TICK_INTERVAL_MS[1] == 12000 && TICK_INTERVAL_MS[2] == 6000 && TICK_INTERVAL_MS[3] == 1000);
    CHECK(TICK_POINTS[1] == 10 && TICK_POINTS[2] == 10 && TICK_POINTS[3] == 30);
    CHECK_NEAR(PointsPerSecond(1), 0.8333, 0.001);
    CHECK_NEAR(PointsPerSecond(2), 1.6667, 0.001);
    CHECK_NEAR(PointsPerSecond(3), 30.0, 0.001);
    CHECK_NEAR(PointsPerSecond(0), 0.0, 0.001);
    // Three nodes take 67 s from zero.
    CHECK(std::lround(MAX_SCORE / PointsPerSecond(3)) == 67);
    CHECK(NearNode(Ab::TEAM_IDX_ALLIANCE) == NODE_LIGHTHOUSE && NearNode(Ab::TEAM_IDX_HORDE) == NODE_MINES);
    CHECK(FarNode(Ab::TEAM_IDX_ALLIANCE) == NODE_MINES && FarNode(Ab::TEAM_IDX_HORDE) == NODE_LIGHTHOUSE);
}

TEST(bfg_swing_values_match_the_sheet_table)
{
    // Take a neutral node: +0.83 at 0 or 1 nodes, +28.3 at 2.
    CHECK_NEAR(SwingPerSecond(0, 0, Ab::SwingKind::TAKE_FREE), 0.833, 0.01);
    CHECK_NEAR(SwingPerSecond(1, 1, Ab::SwingKind::TAKE_FREE), 0.833, 0.01);
    CHECK_NEAR(SwingPerSecond(2, 0, Ab::SwingKind::TAKE_FREE), 28.333, 0.01);
    // Flip an enemy node: 1.67 at 1 v 1, 29.2 at 2 v 1 and at 0 v 3.
    CHECK_NEAR(SwingPerSecond(1, 1, Ab::SwingKind::TAKE_ENEMY), 1.667, 0.01);
    CHECK_NEAR(SwingPerSecond(0, 2, Ab::SwingKind::TAKE_ENEMY), 1.667, 0.01);
    CHECK_NEAR(SwingPerSecond(2, 1, Ab::SwingKind::TAKE_ENEMY), 29.167, 0.01);
    CHECK_NEAR(SwingPerSecond(0, 3, Ab::SwingKind::TAKE_ENEMY), 29.167, 0.01);
    // Deny a 3-0: 28.3 at once.
    CHECK_NEAR(SwingPerSecond(0, 3, Ab::SwingKind::DENY), 28.333, 0.01);
}

TEST(bfg_defender_fit_prefers_rogue_mage_hunter_and_healers_last)
{
    std::vector<Member> team = { Bot(1, BgPlan::CLS_WARRIOR), Healer(2), Bot(3, BgPlan::CLS_MAGE), Bot(4, BgPlan::CLS_ROGUE),
                                 Bot(5, BgPlan::CLS_PALADIN), Bot(6, BgPlan::CLS_HUNTER) };
    std::vector<std::uint32_t> const one = PickDefenders(team, 1);
    CHECK(one.size() == 1);
    CHECK(one[0] == 3 || one[0] == 4 || one[0] == 6);   // one of the three kits
    std::vector<std::uint32_t> const three = PickDefenders(team, 3);
    CHECK(three.size() == 3);
    for (std::uint32_t g : three)
        CHECK(g == 3 || g == 4 || g == 6);
    std::vector<std::uint32_t> const all = PickDefenders(team, 6);
    CHECK(all.back() == 2);   // the healer is last
}

TEST(bfg_defender_pick_is_deterministic_and_prefers_tier_and_level)
{
    std::vector<Member> team = { Bot(10, BgPlan::CLS_ROGUE, 85, Tier::CASUAL), Bot(11, BgPlan::CLS_ROGUE, 85, Tier::EXPERT),
                                 Bot(12, BgPlan::CLS_ROGUE, 85, Tier::GOOD) };
    CHECK(PickDefenders(team, 1)[0] == 11);
    std::reverse(team.begin(), team.end());
    CHECK(PickDefenders(team, 1)[0] == 11);

    // A higher level beats a lower one in the same kit and tier.
    std::vector<Member> levels = { Bot(20, BgPlan::CLS_MAGE, 85), Bot(21, BgPlan::CLS_MAGE, 88) };
    CHECK(PickDefenders(levels, 1)[0] == 21);

    // A kit beats three levels.
    std::vector<Member> kit = { Bot(30, BgPlan::CLS_WARRIOR, 89), Bot(31, BgPlan::CLS_HUNTER, 86) };
    CHECK(PickDefenders(kit, 1)[0] == 31);
}

TEST(bfg_defender_count_is_two_only_in_the_defense_plan_from_six_players)
{
    CHECK(DefenderCount(BgPlan::Plan::BALANCED, 10) == 1);
    CHECK(DefenderCount(BgPlan::Plan::OFFENSE, 10) == 1);
    CHECK(DefenderCount(BgPlan::Plan::DEFENSE, 10) == 2);
    CHECK(DefenderCount(BgPlan::Plan::DEFENSE, 5) == 1);
}

TEST(bfg_hold_posts_are_two_two_three_and_three_staged_at_the_far_node)
{
    std::vector<HoldPost> const posts = HoldPosts(Ab::TEAM_IDX_ALLIANCE, 10);
    CHECK(posts.size() == 10);
    unsigned atNear = 0, atHub = 0, atFar = 0, staged = 0;
    for (HoldPost const& p : posts)
    {
        if (p.staged)
        {
            ++staged;
            CHECK(p.node == NODE_MINES);
        }
        else if (p.node == NODE_LIGHTHOUSE)
            ++atNear;
        else if (p.node == NODE_WATERWORKS)
            ++atHub;
        else
            ++atFar;
    }
    CHECK(atNear == 2 && atHub == 2 && atFar == 3 && staged == 3);

    // The Horde's far node is the Lighthouse.
    for (HoldPost const& p : HoldPosts(Ab::TEAM_IDX_HORDE, 10))
        if (p.staged)
            CHECK(p.node == NODE_LIGHTHOUSE);

    // A small team still guards every node once.
    std::vector<HoldPost> const small = HoldPosts(Ab::TEAM_IDX_ALLIANCE, 3);
    CHECK(small.size() == 3);
    CHECK(small[0].node == NODE_MINES && small[1].node == NODE_WATERWORKS && small[2].node == NODE_LIGHTHOUSE);
    CHECK(HoldPosts(Ab::TEAM_IDX_ALLIANCE, 0).empty());
}

TEST(bfg_defender_takes_the_near_node_then_stays_at_it)
{
    Input in = Alliance(0, 0, 0);
    in.defender = true;
    Choice c = Choose(in);
    CHECK(c.what == What::TAKE && c.node == NODE_LIGHTHOUSE);

    in.state[NODE_LIGHTHOUSE] = 1;   // we are capturing it: the contested node cannot be clicked by us
    c = Choose(in);
    CHECK(c.what == What::POST && c.node == NODE_LIGHTHOUSE);

    in.state[NODE_LIGHTHOUSE] = 3;   // ours
    c = Choose(in);
    CHECK(c.what == What::POST && c.node == NODE_LIGHTHOUSE);

    in.state[NODE_LIGHTHOUSE] = 4;   // the enemy took it: click it back (a restore when it was ours)
    c = Choose(in);
    CHECK(c.what == What::TAKE && c.node == NODE_LIGHTHOUSE);

    in.state[NODE_LIGHTHOUSE] = 2;   // the enemy contests it: an instant restore
    c = Choose(in);
    CHECK(c.what == What::TAKE && c.node == NODE_LIGHTHOUSE);

    // The Horde defends the Mines.
    Input horde = Alliance(0, 0, 0);
    horde.team = Ab::TEAM_IDX_HORDE;
    horde.defender = true;
    c = Choose(horde);
    CHECK(c.what == What::TAKE && c.node == NODE_MINES);
}

TEST(bfg_opening_group_goes_to_waterworks_not_the_defenders_node)
{
    Input in = Alliance(0, 0, 0);
    Choice const c = Choose(in);
    CHECK(c.what == What::TAKE);
    CHECK(c.node == NODE_WATERWORKS || c.node == NODE_MINES);
    CHECK(c.node != NODE_LIGHTHOUSE);   // the defender's while nobody has taken it

    // From the Horde side, standing near the Mines start, the nearest of Waterworks and Lighthouse.
    Input horde = Alliance(0, 0, 0);
    horde.team = Ab::TEAM_IDX_HORDE;
    horde.x = 1401.0f;
    horde.y = 977.0f;
    Choice const h = Choose(horde);
    CHECK(h.what == What::TAKE && h.node != NODE_MINES);
}

TEST(bfg_third_node_is_the_first_target_when_the_team_holds_two)
{
    // We hold the Lighthouse and Waterworks (3, 3); the Mines are enemy occupied: flipping the last node ends the match.
    Input in = Alliance(3, 3, 4);
    in.x = 1000.0f;
    in.y = 1000.0f;
    Choice c = Choose(in);
    CHECK(c.what == What::TAKE && c.node == NODE_MINES);

    // The third node neutral: the same.
    in.state[NODE_MINES] = 0;
    c = Choose(in);
    CHECK(c.what == What::TAKE && c.node == NODE_MINES);

    // The enemy contests it (never ours): also a target.
    in.state[NODE_MINES] = 2;
    c = Choose(in);
    CHECK(c.what == What::TAKE && c.node == NODE_MINES);
}

TEST(bfg_a_node_we_are_capturing_never_means_no_target_and_never_sends_the_group_away)
{
    // Regression for the old code (BT-541): two nodes ours, the third contested by us. Nothing is clickable (the core refuses
    // our own contested banner), and the old case walked every bot to the enemy start room. Now the bots near the capture stay.
    Input in = Alliance(3, 3, 1);
    in.x = NODE_POS[NODE_MINES].x + 20.0f;
    in.y = NODE_POS[NODE_MINES].y;
    Choice const c = Choose(in);
    CHECK(c.what == What::POST && c.node == NODE_MINES);

    // A bot far from the capture takes a post of the hold state, never the enemy start room.
    in.x = 950.0f;
    in.y = 1000.0f;
    Choice const far = Choose(in);
    CHECK(far.what == What::POST);
    CHECK(!NearEnemyStartRoom(Ab::TEAM_IDX_ALLIANCE, NODE_POS[far.node].x, NODE_POS[far.node].y));
}

TEST(bfg_hold_state_posts_cover_every_node_and_none_is_near_an_enemy_start_room)
{
    unsigned perNode[3] = { 0, 0, 0 };
    unsigned staged = 0;
    for (std::uint8_t team : { Ab::TEAM_IDX_ALLIANCE, Ab::TEAM_IDX_HORDE })
    {
        perNode[0] = perNode[1] = perNode[2] = 0;
        staged = 0;
        for (std::uint32_t rank = 0; rank < 9; ++rank)
        {
            Input in = Alliance(team == Ab::TEAM_IDX_ALLIANCE ? 3 : 4, team == Ab::TEAM_IDX_ALLIANCE ? 3 : 4,
                                team == Ab::TEAM_IDX_ALLIANCE ? 3 : 4);
            in.team = team;
            in.rank = rank;
            in.others = 9;
            in.x = 1100.0f;
            in.y = 1100.0f;
            Choice const c = Choose(in);
            CHECK(c.what == What::POST);
            CHECK(c.node < NODE_COUNT);
            CHECK(!NearEnemyStartRoom(team, NODE_POS[c.node].x, NODE_POS[c.node].y));
            ++perNode[c.node];
            if (c.staged)
                ++staged;
        }
        CHECK(perNode[0] > 0 && perNode[1] > 0 && perNode[2] > 0);
        CHECK(staged == 2 || staged == 3);   // 9 non-defenders: 2, 2, 2 guards and the reserve, the near node's one is the defender
    }
}

TEST(bfg_clicks_only_what_the_core_allows)
{
    // No node is ever chosen to TAKE when the core would ignore the click.
    for (std::uint8_t team : { Ab::TEAM_IDX_ALLIANCE, Ab::TEAM_IDX_HORDE })
        for (std::uint8_t a = 0; a <= 4; ++a)
            for (std::uint8_t b = 0; b <= 4; ++b)
                for (std::uint8_t c = 0; c <= 4; ++c)
                    for (bool defender : { false, true })
                    {
                        Input in = Alliance(a, b, c);
                        in.team = team;
                        in.defender = defender;
                        in.x = 1100.0f;
                        in.y = 1100.0f;
                        Choice const choice = Choose(in);
                        if (choice.what == What::TAKE)
                            CHECK(Ab::CanClickNode(in.state[choice.node], team));
                    }
}

TEST(bfg_enemy_start_room_check_uses_the_enemys_graveyard)
{
    CHECK(NearEnemyStartRoom(Ab::TEAM_IDX_ALLIANCE, 1401.0f, 977.0f));
    CHECK(!NearEnemyStartRoom(Ab::TEAM_IDX_ALLIANCE, 908.0f, 1338.0f));
    CHECK(NearEnemyStartRoom(Ab::TEAM_IDX_HORDE, 908.0f, 1338.0f));
    CHECK(!NearEnemyStartRoom(Ab::TEAM_IDX_ALLIANCE, NODE_POS[NODE_MINES].x, NODE_POS[NODE_MINES].y));
}

// ---------------------------------------------------------------------------------------------------------------------
// BT-552: the node plan's rules for Battle for Gilneas (AbPlanLogic.h with the Gilneas row of BgNodeMap.h).
// ---------------------------------------------------------------------------------------------------------------------
namespace
{
using BgNodeMap::GILNEAS;

Ab::SlotInput GilneasSlots(BgPlan::Plan plan, std::uint32_t size, std::uint8_t team = Ab::TEAM_IDX_ALLIANCE)
{
    Ab::SlotInput in;
    in.team = team;
    in.plan = plan;
    in.size = size;
    in.playMs = 5000;
    in.map = &GILNEAS;
    return in;
}

std::uint32_t CountJob(std::vector<Ab::Slot> const& slots, Ab::Job job, std::uint8_t node = Ab::NODE_NONE)
{
    std::uint32_t n = 0;
    for (Ab::Slot const& s : slots)
        if (s.job == job && (node == Ab::NODE_NONE || s.node == node))
            ++n;
    return n;
}

Ab::Candidate Cand(std::uint32_t guid, std::uint8_t cls, std::uint8_t level = 85, Tier tier = Tier::GOOD)
{
    Ab::Candidate c;
    c.guidLow = guid;
    c.cls = cls;
    c.level = level;
    c.tier = tier;
    c.melee = cls == BgPlan::CLS_WARRIOR || cls == BgPlan::CLS_ROGUE;
    c.x = 908.0f;
    c.y = 1338.0f;
    return c;
}
}  // namespace

TEST(bfg_guard_counts_follow_the_plan_and_the_hold_state)
{
    Ab::GuardInput g;
    auto const count = [&](BgPlan::Plan plan, std::uint8_t node, Ab::Regime regime)
    {
        g.regime = regime;
        return Ab::GuardCount(plan, Ab::TEAM_IDX_ALLIANCE, node, g, GILNEAS);
    };
    // Alliance: near = Lighthouse (0), hub = Waterworks (1), far = Mines (2).
    CHECK(count(BgPlan::Plan::BALANCED, 0, Ab::Regime::EVEN) == 1);
    CHECK(count(BgPlan::Plan::BALANCED, 1, Ab::Regime::EVEN) == 2);
    CHECK(count(BgPlan::Plan::BALANCED, 2, Ab::Regime::EVEN) == 1);
    CHECK(count(BgPlan::Plan::OFFENSE, 1, Ab::Regime::EVEN) == 1);
    CHECK(count(BgPlan::Plan::DEFENSE, 0, Ab::Regime::EVEN) == 2);
    CHECK(count(BgPlan::Plan::DEFENSE, 1, Ab::Regime::EVEN) == 3);
    // The hold state is 2 / 2 / 3 whatever the plan.
    for (BgPlan::Plan plan : { BgPlan::Plan::BALANCED, BgPlan::Plan::OFFENSE, BgPlan::Plan::DEFENSE })
    {
        CHECK(count(plan, 0, Ab::Regime::HOLD) == 2);
        CHECK(count(plan, 1, Ab::Regime::HOLD) == 2);
        CHECK(count(plan, 2, Ab::Regime::HOLD) == 3);
    }
    // The third node: one guard per held node, everybody else goes.
    CHECK(count(BgPlan::Plan::DEFENSE, 1, Ab::Regime::CLOSE_OUT) == 1);
    CHECK(count(BgPlan::Plan::BALANCED, 0, Ab::Regime::LAST_NODE) == 4);
    // The Horde's far node is the Lighthouse.
    g.regime = Ab::Regime::HOLD;
    CHECK(Ab::GuardCount(BgPlan::Plan::BALANCED, Ab::TEAM_IDX_HORDE, 0, g, GILNEAS) == 3);
    CHECK(Ab::GuardCount(BgPlan::Plan::BALANCED, Ab::TEAM_IDX_HORDE, 2, g, GILNEAS) == 2);
}

TEST(bfg_opening_counts_match_the_sheet_table_for_five_to_ten_players)
{
    struct Row { std::uint32_t size; std::uint32_t nearB, hubB, nearO, hubO, raidO, nearD, hubD; };
    Row const rows[] = {
        { 10, 1, 9, 1, 7, 2, 2, 8 }, { 9, 1, 8, 1, 6, 2, 2, 7 }, { 8, 1, 7, 1, 5, 2, 2, 6 },
        { 7, 1, 6, 1, 5, 1, 2, 5 },  { 6, 1, 5, 1, 4, 1, 2, 4 }, { 5, 1, 4, 1, 4, 0, 1, 4 }
    };
    for (Row const& r : rows)
    {
        Ab::OpeningCounts const b = Ab::GilneasOpening(BgPlan::Plan::BALANCED, 0, r.size, 4, GILNEAS);
        CHECK(b.byNode[0] == r.nearB && b.byNode[1] == r.hubB && b.raiders == 0);
        Ab::OpeningCounts const o = Ab::GilneasOpening(BgPlan::Plan::OFFENSE, 0, r.size, 4, GILNEAS);
        CHECK(o.byNode[0] == r.nearO && o.byNode[1] == r.hubO && o.raiders == r.raidO);
        CHECK(o.Total() == r.size);
        Ab::OpeningCounts const d = Ab::GilneasOpening(BgPlan::Plan::DEFENSE, 0, r.size, 4, GILNEAS);
        CHECK(d.byNode[0] == r.nearD && d.byNode[1] == r.hubD);
        CHECK(d.Total() == r.size);
    }
    // The Horde's near node is the Mines.
    Ab::OpeningCounts const h = Ab::GilneasOpening(BgPlan::Plan::BALANCED, 1, 10, 0, GILNEAS);
    CHECK(h.byNode[2] == 1 && h.byNode[1] == 9);
    // Raiders need two stealth bots: with one or none the slots go to the Waterworks group.
    Ab::OpeningCounts const one = Ab::GilneasOpening(BgPlan::Plan::OFFENSE, 0, 10, 1, GILNEAS);
    CHECK(one.raiders == 0 && one.byNode[1] == 9);
}

TEST(bfg_slots_at_the_gates_are_one_defender_and_the_waterworks_group)
{
    std::vector<Ab::Slot> const slots = Ab::BuildSlots(GilneasSlots(BgPlan::Plan::BALANCED, 10));
    CHECK(slots.size() == 10);
    CHECK(CountJob(slots, Ab::Job::ASSAULT, 0) == 1);   // the Lighthouse: the defender
    CHECK(CountJob(slots, Ab::Job::ASSAULT, 1) == 9);   // Waterworks
    CHECK(slots[0].job == Ab::Job::ASSAULT && slots[0].node == 0);   // the near node is filled first

    std::vector<Ab::Slot> const horde = Ab::BuildSlots(GilneasSlots(BgPlan::Plan::BALANCED, 10, Ab::TEAM_IDX_HORDE));
    CHECK(CountJob(horde, Ab::Job::ASSAULT, 2) == 1 && CountJob(horde, Ab::Job::ASSAULT, 1) == 9);

    // A human takes a slot and is never commanded: nine bots, nine slots.
    Ab::SlotInput withHuman = GilneasSlots(BgPlan::Plan::BALANCED, 10);
    withHuman.humans = 1;
    CHECK(Ab::BuildSlots(withHuman).size() == 9);
}

TEST(bfg_hold_state_slots_are_two_two_three_and_the_reserve)
{
    Ab::SlotInput in = GilneasSlots(BgPlan::Plan::BALANCED, 10);
    in.playMs = 600000;
    in.regime = Ab::Regime::HOLD;
    for (int n = 0; n < 3; ++n)
        in.sit[n] = Ab::NodeSituation::OURS;
    std::vector<Ab::Slot> const slots = Ab::BuildSlots(in);
    CHECK(slots.size() == 10);
    CHECK(CountJob(slots, Ab::Job::GUARD, 0) == 2);
    CHECK(CountJob(slots, Ab::Job::GUARD, 1) == 2);
    CHECK(CountJob(slots, Ab::Job::GUARD, 2) == 3);
    CHECK(CountJob(slots, Ab::Job::MOBILE) == 3);
    CHECK(CountJob(slots, Ab::Job::ASSAULT) == 0);
    CHECK(CountJob(slots, Ab::Job::RAIDER) == 0);
}

TEST(bfg_third_node_sends_everybody_but_one_guard_per_node)
{
    Ab::SlotInput in = GilneasSlots(BgPlan::Plan::BALANCED, 10);
    in.playMs = 600000;
    in.regime = Ab::Regime::CLOSE_OUT;
    in.sit[0] = Ab::NodeSituation::OURS;
    in.sit[1] = Ab::NodeSituation::OURS;
    in.sit[2] = Ab::NodeSituation::THEIRS;
    std::vector<Ab::Slot> const slots = Ab::BuildSlots(in);
    CHECK(CountJob(slots, Ab::Job::GUARD) == 2);
    CHECK(CountJob(slots, Ab::Job::ASSAULT) == 8);
}

TEST(bfg_the_near_node_slot_goes_to_the_best_kit_not_the_highest_level)
{
    std::vector<Ab::Candidate> team = {
        Cand(1, BgPlan::CLS_WARRIOR, 89), Cand(2, BgPlan::CLS_ROGUE, 85), Cand(3, BgPlan::CLS_PALADIN, 88),
        Cand(4, BgPlan::CLS_HUNTER, 86),  Cand(5, BgPlan::CLS_WARLOCK, 87)
    };
    team.push_back(Cand(6, BgPlan::CLS_PRIEST, 90));
    team.back().healer = true;
    std::vector<Ab::Slot> const slots = Ab::BuildSlots(GilneasSlots(BgPlan::Plan::BALANCED, 6));
    std::vector<Ab::Assignment> const result = Ab::Assign(team, slots, Ab::TEAM_IDX_ALLIANCE, GILNEAS);
    Ab::Assignment const* defender = nullptr;
    for (Ab::Assignment const& a : result)
        if (a.slot.node == 0)
            defender = &a;
    CHECK(defender != nullptr);
    if (defender)
        CHECK(defender->guidLow == 2 || defender->guidLow == 4);   // rogue or hunter, not the level 89 warrior or the healer
    // The healer goes with the Waterworks group.
    for (Ab::Assignment const& a : result)
        if (a.guidLow == 6)
            CHECK(a.slot.job == Ab::Job::ASSAULT && a.slot.node == 1);
}

TEST(bfg_the_defender_keeps_the_node_after_the_capture)
{
    std::vector<Ab::Candidate> team = { Cand(1, BgPlan::CLS_ROGUE), Cand(2, BgPlan::CLS_MAGE), Cand(3, BgPlan::CLS_WARRIOR),
                                        Cand(4, BgPlan::CLS_PALADIN), Cand(5, BgPlan::CLS_SHAMAN) };
    std::vector<Ab::Slot> const gates = Ab::BuildSlots(GilneasSlots(BgPlan::Plan::BALANCED, 5));
    std::vector<Ab::Assignment> const first = Ab::Assign(team, gates, Ab::TEAM_IDX_ALLIANCE, GILNEAS);
    std::uint32_t defender = 0;
    for (Ab::Assignment const& a : first)
        if (a.slot.node == 0)
            defender = a.guidLow;
    CHECK(defender != 0);
    for (Ab::Candidate& c : team)
    {
        for (Ab::Assignment const& a : first)
            if (a.guidLow == c.guidLow)
            {
                c.hasPrevious = true;
                c.previous = a.slot;
            }
    }
    // The node is being captured now: its slot is a guard slot.
    Ab::SlotInput in = GilneasSlots(BgPlan::Plan::BALANCED, 5);
    in.sit[0] = Ab::NodeSituation::CAPPING;
    std::vector<Ab::Assignment> const second = Ab::Assign(team, Ab::BuildSlots(in), Ab::TEAM_IDX_ALLIANCE, GILNEAS);
    for (Ab::Assignment const& a : second)
        if (a.guidLow == defender)
            CHECK(a.slot.job == Ab::Job::GUARD && a.slot.node == 0);
}

TEST(bfg_third_node_options_good_bots_go_casual_take_the_nearest)
{
    Ab::DecisionContext c;
    c.map = &GILNEAS;
    c.job = Ab::Job::ASSAULT;
    c.regime = Ab::Regime::CLOSE_OUT;
    c.sit[0] = Ab::NodeSituation::OURS;
    c.sit[1] = Ab::NodeSituation::OURS;
    c.sit[2] = Ab::NodeSituation::THEIRS;
    c.closeOutNode = 2;
    c.assaultNode = 2;
    c.nearestNode = 2;
    c.stageNode = 1;

    c.tier = Tier::GOOD;
    CHECK(Ab::Options(c).front().kind == Ab::Kind::CLOSE_OUT);   // a good rule at Gilneas (Arathi: expert)
    c.tier = Tier::CASUAL;
    CHECK(Ab::Options(c).front().kind == Ab::Kind::TAKE);

    // At Arathi Basin the close-out stays an expert line.
    Ab::DecisionContext ab;
    ab.job = Ab::Job::ASSAULT;
    ab.regime = Ab::Regime::CLOSE_OUT;
    ab.closeOutNode = Ab::NODE_FARM;
    ab.assaultNode = Ab::NODE_FARM;
    ab.nearestNode = Ab::NODE_FARM;
    ab.sit[Ab::NODE_FARM] = Ab::NodeSituation::THEIRS;
    ab.tier = Tier::GOOD;
    for (Ab::Option const& o : Ab::Options(ab))
        CHECK(o.kind != Ab::Kind::CLOSE_OUT);
}

TEST(bfg_the_capping_group_stays_through_the_whole_contest)
{
    Ab::DecisionContext c;
    c.map = &GILNEAS;
    c.job = Ab::Job::ASSAULT;
    c.slotNode = 1;
    c.assaultNode = 1;
    c.tier = Tier::GOOD;
    c.sit[1] = Ab::NodeSituation::CAPPING;
    c.captureAgeMs[1] = 40000;   // Arathi Basin lets go after 15 s
    CHECK(Ab::Options(c).front().kind == Ab::Kind::HOLD_CAPTURE);
    Ab::DecisionContext ab = c;
    ab.map = &BgNodeMap::ARATHI;
    ab.slotNode = Ab::NODE_BLACKSMITH;
    ab.assaultNode = Ab::NODE_BLACKSMITH;
    ab.sit[Ab::NODE_BLACKSMITH] = Ab::NodeSituation::CAPPING;
    ab.captureAgeMs[Ab::NODE_BLACKSMITH] = 40000;
    CHECK(Ab::Options(ab).front().kind != Ab::Kind::HOLD_CAPTURE);
    c.captureAgeMs[1] = 61000;   // flipped
}

TEST(bfg_retreat_rules_odds_healers_and_the_rally_node)
{
    CHECK(Ab::FightOdds(5, 8) == -3);
    CHECK(Ab::Losing(-3, true));
    CHECK(!Ab::Losing(-2, true));
    CHECK(Ab::Losing(-2, false));    // no healer left: the threshold is one lower
    CHECK(!Ab::Losing(-1, false));
    CHECK(!Ab::Losing(2, false));

    CHECK(!Ab::RetreatOver(2, 10000));
    CHECK(Ab::RetreatOver(4, 1000));
    CHECK(Ab::RetreatOver(1, 25000));

    // A capping group close to its flip stays; a fresh one may retreat.
    CHECK(Ab::CappingNearFlip(Ab::NodeSituation::CAPPING, 50000));
    CHECK(!Ab::CappingNearFlip(Ab::NodeSituation::CAPPING, 20000));
    CHECK(!Ab::CappingNearFlip(Ab::NodeSituation::OURS, 50000));

    // The rally node is the nearest node we hold, not the one we fight at.
    Ab::NodeSituation sit[Ab::NODE_COUNT] = { Ab::NodeSituation::OURS, Ab::NodeSituation::THEIRS, Ab::NodeSituation::OURS,
                                              Ab::NodeSituation::NEUTRAL, Ab::NodeSituation::NEUTRAL };
    CHECK(Ab::RallyNode(sit, 1, 980.0f, 948.0f, GILNEAS) == 2);   // fighting at Waterworks, the Mines are nearer than the Lighthouse
    CHECK(Ab::RallyNode(sit, 1, 1050.0f, 1250.0f, GILNEAS) == 0);
    sit[0] = sit[2] = Ab::NodeSituation::THEIRS;
    CHECK(Ab::RallyNode(sit, 1, 980.0f, 948.0f, GILNEAS) == Ab::NODE_NONE);   // nothing held: fight on
    sit[1] = Ab::NodeSituation::CAPPING;
    CHECK(Ab::RallyNode(sit, 0, 1050.0f, 1250.0f, GILNEAS) == 1);   // a node we are capturing counts as held
}

TEST(bfg_retreat_and_ghost_options_by_tier)
{
    Ab::DecisionContext c;
    c.map = &GILNEAS;
    c.job = Ab::Job::ASSAULT;
    c.slotNode = 1;
    c.assaultNode = 1;
    c.sit[0] = Ab::NodeSituation::OURS;
    c.sit[1] = Ab::NodeSituation::THEIRS;
    c.sit[2] = Ab::NodeSituation::THEIRS;
    c.stageNode = 0;
    c.retreatNode = 0;

    c.tier = Tier::GOOD;
    CHECK(Ab::Options(c).front().kind == Ab::Kind::RETREAT);
    CHECK(Ab::Options(c).front().node == 0);
    c.tier = Tier::CASUAL;   // casual bots never retreat
    for (Ab::Option const& o : Ab::Options(c))
        CHECK(o.kind != Ab::Kind::RETREAT);

    // The ghost is an expert line, above the retreat's neighbours, for the pair that was named.
    c.tier = Tier::GOOD;
    c.ghostNode = 2;
    for (Ab::Option const& o : Ab::Options(c))
        CHECK(o.kind != Ab::Kind::GHOST);
    c.tier = Tier::EXPERT;
    bool ghost = false;
    for (Ab::Option const& o : Ab::Options(c))
        if (o.kind == Ab::Kind::GHOST && o.node == 2)
            ghost = true;
    CHECK(ghost);

    // A guard never retreats: it restores or dies at its post.
    Ab::DecisionContext guard = c;
    guard.job = Ab::Job::GUARD;
    guard.slotNode = 0;
    guard.tier = Tier::EXPERT;
    for (Ab::Option const& o : Ab::Options(guard))
        CHECK(o.kind != Ab::Kind::RETREAT && o.kind != Ab::Kind::GHOST);

    CHECK(Ab::GhostWanted(-3, true, 2, 1));
    CHECK(!Ab::GhostWanted(-3, true, 1, 1));    // one stealth bot joins the defender instead
    CHECK(!Ab::GhostWanted(-3, true, 2, 2));    // two defenders in sight
    CHECK(!Ab::GhostWanted(0, true, 2, 0));     // the fight is not lost
}

// ---------------------------------------------------------------------------------------------------------------------
// BT-553: the call-out words, the decision points D12 to D14, the seen-only intel.
// ---------------------------------------------------------------------------------------------------------------------
#include "BgCalloutPolicy.h"
#include "BgLeaderAdapters.h"
#include "AiPvpDecisionPolicy.h"

TEST(bfg_callout_reader_knows_the_three_nodes_and_their_names)
{
    using BgCallout::MapKind;
    using BgCallout::Parse;
    CHECK(BgCallout::MapKindOf(761) == MapKind::BFG);
    CHECK(Parse("inc ww 4", MapKind::BFG).valid);
    CHECK(Parse("inc ww 4", MapKind::BFG).place == 1 && Parse("inc ww 4", MapKind::BFG).count == 4);
    CHECK(Parse("inc lh", MapKind::BFG).place == 0);
    CHECK(Parse("inc lighthouse 2", MapKind::BFG).place == 0);
    CHECK(Parse("inc mines", MapKind::BFG).place == 2);
    CHECK(Parse("help mine", MapKind::BFG).place == 2);
    CHECK(Parse("inc water works", MapKind::BFG).place == 1);   // two words, one place
    CHECK(Parse("take ww", MapKind::BFG).valid);
    CHECK(Parse("ww lost", MapKind::BFG).valid);
    CHECK(Parse("mines clear", MapKind::BFG).valid);
    // The Arathi words are not Gilneas words and the other way round.
    CHECK(!Parse("inc bs", MapKind::BFG).valid);
    CHECK(!Parse("inc ww", MapKind::AB).valid);
    CHECK(!Parse("inc lm", MapKind::BFG).valid);
    // Chat is still chat.
    CHECK(!Parse("ww is so annoying", MapKind::BFG).valid);
    CHECK(!Parse("inc ww?", MapKind::BFG).valid);
}

TEST(bfg_fight_offer_rule_answer_is_first_and_follows_the_odds)
{
    using namespace BgLeader;
    FightFacts f;
    f.odds = -4;
    f.healerAlive = true;
    f.rallyAvailable = true;
    Offer o = FightOffer(f);
    CHECK(o.point == DecisionPoint::D13_BFG_FIGHT);
    CHECK(o.Fallback() == static_cast<std::uint8_t>(BfgFight::RETREAT));
    CHECK(o.options.size() == 2);   // retreat and fight on

    f.ghostWanted = true;
    CHECK(FightOffer(f).Fallback() == static_cast<std::uint8_t>(BfgFight::GHOST));

    f = FightFacts();
    f.odds = -1;
    f.rallyAvailable = true;
    f.reinforceAvailable = true;
    o = FightOffer(f);
    CHECK(o.Fallback() == static_cast<std::uint8_t>(BfgFight::FIGHT_ON));
    CHECK(o.options.size() == 3 && o.options[1].id == static_cast<std::uint8_t>(BfgFight::REINFORCE));

    // No node to fall back to: never offer the retreat.
    f = FightFacts();
    f.odds = -5;
    f.rallyAvailable = false;
    o = FightOffer(f);
    CHECK(o.Fallback() == static_cast<std::uint8_t>(BfgFight::FIGHT_ON));
    for (Option const& opt : o.options)
        CHECK(opt.id != static_cast<std::uint8_t>(BfgFight::RETREAT));

    // No healer: -2 is already lost.
    f = FightFacts();
    f.odds = -2;
    f.healerAlive = false;
    f.rallyAvailable = true;
    CHECK(FightOffer(f).Fallback() == static_cast<std::uint8_t>(BfgFight::RETREAT));
    // The same input, the same list.
    CHECK(FightOffer(f).options.size() == FightOffer(f).options.size());
}

TEST(bfg_stage_offer_has_the_rule_node_first_and_only_usable_alternatives)
{
    using namespace BgLeader;
    StageFacts f;
    f.hubNode = 1;
    f.farNode = 2;
    f.ownNearNode = 0;
    f.ruleNode = 1;
    f.hubUsable = true;
    f.farUsable = true;
    f.ownNearUsable = false;
    Offer o = StageOffer(f);
    CHECK(o.point == DecisionPoint::D14_BFG_STAGE);
    CHECK(o.Fallback() == static_cast<std::uint8_t>(BfgStage::STAY_HUB));
    CHECK(o.options.size() == 2);
    CHECK(StageNodeOf(BfgStage::TO_FAR_NODE, f) == 2);

    f.ruleNode = 2;   // the hold state: the far node
    o = StageOffer(f);
    CHECK(o.Fallback() == static_cast<std::uint8_t>(BfgStage::TO_FAR_NODE));

    // The rule's node is offered even when it is not "usable".
    f = StageFacts();
    f.ruleNode = 0;
    f.hubUsable = f.farUsable = false;
    o = StageOffer(f);
    CHECK(o.options.size() == 1 && o.Fallback() == static_cast<std::uint8_t>(BfgStage::TO_OWN_NEAR));
}

TEST(bfg_route_offer_splits_from_six_and_the_points_are_named)
{
    using namespace BgLeader;
    CHECK(RouteOffer(9).Fallback() == static_cast<std::uint8_t>(BfgRoute::SPLIT));
    CHECK(RouteOffer(5).Fallback() == static_cast<std::uint8_t>(BfgRoute::SHORT_ROUTE));
    CHECK(RouteOffer(5).options.size() == 2);
    CHECK(std::string(CandidateName(DecisionPoint::D13_BFG_FIGHT, static_cast<std::uint8_t>(BfgFight::GHOST))) == "ghost");
    CHECK(std::string(CandidateName(DecisionPoint::D14_BFG_STAGE, static_cast<std::uint8_t>(BfgStage::TO_FAR_NODE))) == "to_far_node");
    CHECK(std::string(CandidateName(DecisionPoint::D12_BFG_ROUTE, static_cast<std::uint8_t>(BfgRoute::SPLIT))) == "split");
    // Node names of the second map in the same point as Arathi Basin's.
    CHECK(std::string(CandidateName(DecisionPoint::D9_NODE_TARGET, BFG_NODE_ID_BASE + 1)) == "waterworks");
    CHECK(std::string(CandidateName(DecisionPoint::D9_NODE_TARGET, BFG_NODE_ID_BASE + 2)) == "mines");
    CHECK(std::string(CandidateName(DecisionPoint::D9_NODE_TARGET, AbLogic::NODE_FARM)) == "farm");
    CHECK(std::string(CandidateName(DecisionPoint::D9_NODE_TARGET, BFG_NODE_ID_BASE + 3)) == "unknown");
    CHECK(std::string(NodeName(1, BgNodeMap::GILNEAS)) == "waterworks");
}

TEST(bfg_node_target_offer_carries_the_map_offset)
{
    std::vector<Ab::TargetScore> sorted = { { 2, 400 }, { 0, 200 }, { 1, 100 } };
    BgLeader::Offer const ab = BgLeader::NodeTargetOffer(sorted);
    CHECK(ab.options[0].id == 2);
    BgLeader::Offer const bfg = BgLeader::NodeTargetOffer(sorted, BgLeader::BFG_NODE_ID_BASE);
    CHECK(bfg.options[0].id == BgLeader::BFG_NODE_ID_BASE + 2 && bfg.options[0].score == 100);
    CHECK(bfg.options.size() == 3);
}

TEST(bfg_decision_points_are_wired_and_fit_the_book)
{
    using BgLeader::DecisionPoint;
    CHECK(AiPvp::IsWired(DecisionPoint::D13_BFG_FIGHT));
    CHECK(AiPvp::IsWired(DecisionPoint::D14_BFG_STAGE));
    CHECK(!AiPvp::IsWired(DecisionPoint::D12_BFG_ROUTE));   // until the Horde's routes are measured
    CHECK(AiPvp::IsRepeatable(DecisionPoint::D13_BFG_FIGHT));
    CHECK(!AiPvp::IsRepeatable(DecisionPoint::D14_BFG_STAGE));
    CHECK(std::string(AiPvp::PointName(DecisionPoint::D13_BFG_FIGHT)) == "bfg_fight");
    CHECK(static_cast<std::size_t>(DecisionPoint::D14_BFG_STAGE) < AiPvp::BOOK_SIZE);

    // The points keep their own place in the book: no point shares a slot with another.
    AiPvp::Book book;
    BgLeader::Offer a;
    a.point = DecisionPoint::D13_BFG_FIGHT;
    a.options.push_back({ 1, 100 });
    AiPvp::Publish(book, a, {}, true, 1000);
    CHECK(book.At(DecisionPoint::D13_BFG_FIGHT).published);
    CHECK(!book.At(DecisionPoint::D1_STANCE).published);
    CHECK(!book.At(DecisionPoint::D14_BFG_STAGE).published);
}

TEST(bfg_seen_only_intel_counts_what_a_friend_stands_near)
{
    CHECK(Ab::SeenFrom(0.0f, 0.0f, 50.0f, 40.0f));    // 64 yd
    CHECK(!Ab::SeenFrom(0.0f, 0.0f, 60.0f, 60.0f));   // 85 yd
    CHECK(Ab::SeenFrom(0.0f, 0.0f, 70.0f, 0.0f));    // the radius itself
    // What the call-outs report is merged, never below what is seen.
    CHECK(PvpObs::ReconciledEnemies(2, 5) == 5);
    CHECK(PvpObs::ReconciledEnemies(4, 1) == 4);
}
