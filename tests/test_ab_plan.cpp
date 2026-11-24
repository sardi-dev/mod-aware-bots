#include "TestHarness.h"
#include "AbPlanLogic.h"
#include "BgCalloutPolicy.h"

// BT-156: Arathi Basin playbook logic (docs/bg-sheets/arathi-basin.md).

using namespace AbLogic;

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
    // Everybody starts at the Alliance base, 540 yd from the Blacksmith.
    c.x = 1313.9f;
    c.y = 1310.7f;
    return c;
}

Candidate Healer(std::uint32_t guid, std::uint8_t level = 20)
{
    Candidate c = Bot(guid, CLS_PRIEST, level);
    c.healer = true;
    c.melee = false;
    return c;
}

std::vector<Candidate> Team15()
{
    return {
        Bot(101, CLS_ROGUE, 23),   Bot(102, CLS_WARRIOR, 24), Bot(103, CLS_MAGE, 22),    Bot(104, CLS_HUNTER, 21),
        Healer(105, 21),           Bot(106, CLS_PALADIN, 20), Bot(107, CLS_WARLOCK, 22), Bot(108, CLS_SHAMAN, 23),
        Bot(109, CLS_DRUID, 21),   Bot(110, CLS_MONK, 20),    Bot(111, CLS_DEATH_KNIGHT, 24), Bot(112, CLS_WARRIOR, 22),
        Healer(113, 22),           Bot(114, CLS_MAGE, 20),    Bot(115, CLS_HUNTER, 23)
    };
}

SlotInput Opening(Plan plan, std::uint32_t size = 15, std::uint8_t team = TEAM_IDX_ALLIANCE)
{
    SlotInput in;
    in.team = team;
    in.plan = plan;
    in.size = size;
    in.playMs = 5000;
    in.nearerContested = NODE_GOLD_MINE;
    return in;
}

std::uint32_t CountSlots(std::vector<Slot> const& slots, Job job, std::uint8_t node = NODE_NONE)
{
    std::uint32_t n = 0;
    for (Slot const& s : slots)
        if (s.job == job && (node == NODE_NONE || s.node == node))
            ++n;
    return n;
}

Slot SlotOfGuid(std::vector<Assignment> const& list, std::uint32_t guid)
{
    for (Assignment const& a : list)
        if (a.guidLow == guid)
            return a.slot;
    return Slot{ Job::MOBILE, 77 };
}

std::uint32_t CountAssigned(std::vector<Assignment> const& list, Job job, std::uint8_t node = NODE_NONE)
{
    std::uint32_t n = 0;
    for (Assignment const& a : list)
        if (a.slot.job == job && (node == NODE_NONE || a.slot.node == node))
            ++n;
    return n;
}
}  // namespace

// --- the core's numbers -------------------------------------------------------------------------------------------------

TEST(ab_core_constants_match_the_server)
{
    // BattlegroundAB.h BG_AB_TickIntervals / BG_AB_TickPoints, BG_AB_MAX_TEAM_SCORE, BG_AB_FLAG_CAPTURING_TIME.
    CHECK(TICK_INTERVAL_MS[1] == 12000 && TICK_INTERVAL_MS[2] == 9000 && TICK_INTERVAL_MS[3] == 6000);
    CHECK(TICK_INTERVAL_MS[4] == 3000 && TICK_INTERVAL_MS[5] == 1000);
    CHECK(TICK_POINTS[1] == 10 && TICK_POINTS[2] == 10 && TICK_POINTS[3] == 10 && TICK_POINTS[4] == 10);
    CHECK(TICK_POINTS[5] == 30);
    CHECK(MAX_SCORE == 1600);
    CHECK(CAPTURE_TIME_MS == 60000);
    CHECK(CAST_TIME_MS == 8000);
    CHECK(NODE_COUNT == 5);
    CHECK(NODE_STABLES == 0 && NODE_BLACKSMITH == 1 && NODE_FARM == 2 && NODE_LUMBER_MILL == 3 && NODE_GOLD_MINE == 4);
}

// --- the click rule ------------------------------------------------------------------------------------------------------

TEST(ab_click_rule_truth_table_mirrors_the_core)
{
    // BattlegroundAB.cpp:413: m_Nodes == 0 || teamIndex == m_Nodes % 2.
    // State 0 neutral, 1 ally contested, 2 horde contested, 3 ally occupied, 4 horde occupied.
    bool const alliance[5] = { true, false, true, false, true };
    bool const horde[5] = { true, true, false, true, false };
    for (std::uint8_t state = 0; state < 5; ++state)
    {
        CHECK(CanClickNode(state, TEAM_IDX_ALLIANCE) == alliance[state]);
        CHECK(CanClickNode(state, TEAM_IDX_HORDE) == horde[state]);
    }
    CHECK(!CanClickNode(5, TEAM_IDX_ALLIANCE));
    CHECK(!CanClickNode(0, 2));
}

TEST(ab_attack_pool_is_what_the_team_may_click_and_leaves_out_its_own_contested_node)
{
    // Regression for the old pool, which took the team's own contested node and left the enemy's out.
    CHECK(InAttackPool(0, TEAM_IDX_ALLIANCE));
    CHECK(InAttackPool(4, TEAM_IDX_ALLIANCE));    // enemy occupied
    CHECK(InAttackPool(2, TEAM_IDX_ALLIANCE));    // enemy contested
    CHECK(!InAttackPool(1, TEAM_IDX_ALLIANCE));   // our own contested node
    CHECK(!InAttackPool(3, TEAM_IDX_ALLIANCE));   // our own occupied node
    CHECK(InAttackPool(3, TEAM_IDX_HORDE));
    CHECK(InAttackPool(1, TEAM_IDX_HORDE));
    CHECK(!InAttackPool(2, TEAM_IDX_HORDE));
    CHECK(!InAttackPool(4, TEAM_IDX_HORDE));
}

TEST(ab_occupied_and_contested_by)
{
    CHECK(IsOccupiedBy(3, TEAM_IDX_ALLIANCE) && !IsOccupiedBy(3, TEAM_IDX_HORDE));
    CHECK(IsOccupiedBy(4, TEAM_IDX_HORDE) && !IsOccupiedBy(4, TEAM_IDX_ALLIANCE));
    CHECK(IsContestedBy(1, TEAM_IDX_ALLIANCE) && !IsContestedBy(1, TEAM_IDX_HORDE));
    CHECK(IsContestedBy(2, TEAM_IDX_HORDE) && !IsContestedBy(2, TEAM_IDX_ALLIANCE));
    CHECK(OtherTeam(TEAM_IDX_ALLIANCE) == TEAM_IDX_HORDE);
    CHECK(HomeNode(TEAM_IDX_ALLIANCE) == NODE_STABLES);
    CHECK(HomeNode(TEAM_IDX_HORDE) == NODE_FARM);
}

TEST(ab_node_positions_find_their_banner)
{
    // BG_AB_NodePositions: a banner object is matched to its node by position.
    CHECK(NodeNear(1166.79f, 1200.13f, CLICK_RANGE) == NODE_STABLES);
    CHECK(NodeNear(977.0f, 1046.6f, CLICK_RANGE) == NODE_BLACKSMITH);
    CHECK(NodeNear(806.2f, 874.3f, CLICK_RANGE) == NODE_FARM);
    CHECK(NodeNear(856.1f, 1148.9f, CLICK_RANGE) == NODE_LUMBER_MILL);
    CHECK(NodeNear(1146.9f, 848.2f, CLICK_RANGE) == NODE_GOLD_MINE);
    CHECK(NodeNear(900.0f, 900.0f, CLICK_RANGE) == NODE_NONE);
    // The Lumber Mill banner and the Blacksmith are 200 yd apart: the radius never mixes two nodes.
    CHECK(NodeNear(860.0f, 1150.0f, 60.0f) == NODE_LUMBER_MILL);
}

TEST(ab_exit_portals_keep_posts_away)
{
    CHECK(NearExitPortal(1394.0f, 1303.0f));
    CHECK(NearExitPortal(640.0f, 700.0f));
    CHECK(!NearExitPortal(1313.9f, 1310.7f));
    CHECK(!NearExitPortal(1166.8f, 1200.1f));
}

// --- rates, eta, swing --------------------------------------------------------------------------------------------------

TEST(ab_rates_and_eta_follow_the_tick_table)
{
    CHECK_NEAR(PointsPerSecond(0), 0.0, 0.0001);
    CHECK_NEAR(PointsPerSecond(1), 0.8333, 0.001);
    CHECK_NEAR(PointsPerSecond(2), 1.1111, 0.001);
    CHECK_NEAR(PointsPerSecond(3), 1.6667, 0.001);
    CHECK_NEAR(PointsPerSecond(4), 3.3333, 0.001);
    CHECK_NEAR(PointsPerSecond(5), 30.0, 0.001);

    // From 0 to 1600: 32:00, 24:00, 16:00, 8:00 and 53 seconds (sheet section 3).
    CHECK(EtaSeconds(0, 1) == 1920);
    CHECK(EtaSeconds(0, 2) == 1440);
    CHECK(EtaSeconds(0, 3) == 960);
    CHECK(EtaSeconds(0, 4) == 480);
    CHECK(EtaSeconds(0, 5) == 54);
    CHECK(EtaSeconds(0, 0) == ETA_NEVER);
    CHECK(EtaSeconds(1600, 3) == 0);
    CHECK(EtaSeconds(1700, 3) == 0);
    CHECK(EtaSeconds(1000, 3) == 360);
}

TEST(ab_swing_values_match_the_sheet_table)
{
    // Taking one of their nodes: 1/4 -> 1.94, 2/3 -> 1.11, 3/2 -> 1.94, 4/1 -> 27.5, 0/5 -> 27.5.
    CHECK_NEAR(SwingValue(1, 4, SwingKind::TAKE_ENEMY), 1.94, 0.01);
    CHECK_NEAR(SwingValue(2, 3, SwingKind::TAKE_ENEMY), 1.11, 0.01);
    CHECK_NEAR(SwingValue(3, 2, SwingKind::TAKE_ENEMY), 1.94, 0.01);
    CHECK_NEAR(SwingValue(4, 1, SwingKind::TAKE_ENEMY), 27.5, 0.05);
    CHECK_NEAR(SwingValue(0, 5, SwingKind::TAKE_ENEMY), 27.5, 0.05);
    // Denying one of their nodes: 1.67, 0.56, 0.28, 0.83, 26.7.
    CHECK_NEAR(SwingValue(1, 4, SwingKind::DENY), 1.67, 0.01);
    CHECK_NEAR(SwingValue(2, 3, SwingKind::DENY), 0.56, 0.01);
    CHECK_NEAR(SwingValue(3, 2, SwingKind::DENY), 0.28, 0.01);
    CHECK_NEAR(SwingValue(4, 1, SwingKind::DENY), 0.83, 0.01);
    CHECK_NEAR(SwingValue(0, 5, SwingKind::DENY), 26.7, 0.05);
    // A free node has no loss on the other side.
    CHECK_NEAR(SwingValue(3, 2, SwingKind::TAKE_FREE), 1.67, 0.01);
    CHECK_NEAR(SwingValue(0, 0, SwingKind::DENY), 0.0, 0.0001);
}

TEST(ab_travel_time_uses_the_mount_from_level_20)
{
    std::uint32_t const foot = TravelMs(300.0f, 17);
    std::uint32_t const mounted = TravelMs(300.0f, 20);
    CHECK(mounted < foot);
    CHECK_NEAR(static_cast<double>(mounted) / foot, 7.0 / 11.2, 0.01);
}

TEST(ab_nearer_contested_node_uses_the_recorded_paths_and_the_parity_on_a_tie)
{
    // Alliance: 462 to Gold Mine against 482 to Lumber Mill is within 25 yd, a tie: parity decides.
    CHECK(NearerContested(TEAM_IDX_ALLIANCE, 0) == NODE_GOLD_MINE);
    CHECK(NearerContested(TEAM_IDX_ALLIANCE, 1) == NODE_LUMBER_MILL);
    // Horde: 470 against 518: the Gold Mine, whatever the parity.
    CHECK(NearerContested(TEAM_IDX_HORDE, 0) == NODE_GOLD_MINE);
    CHECK(NearerContested(TEAM_IDX_HORDE, 1) == NODE_GOLD_MINE);
}

// --- node situations ------------------------------------------------------------------------------------------------------

TEST(ab_node_situations_for_each_team)
{
    // Alliance view.
    CHECK(Classify(0, 0, TEAM_IDX_ALLIANCE) == NodeSituation::NEUTRAL);
    CHECK(Classify(3, 1, TEAM_IDX_ALLIANCE) == NodeSituation::OURS);
    CHECK(Classify(1, 0, TEAM_IDX_ALLIANCE) == NodeSituation::CAPPING);
    CHECK(Classify(4, 2, TEAM_IDX_ALLIANCE) == NodeSituation::THEIRS);
    // Contested by the Horde after it was Alliance occupied: under assault, restorable at once.
    CHECK(Classify(2, 3, TEAM_IDX_ALLIANCE) == NodeSituation::UNDER_ASSAULT);
    // Contested by the Horde from neutral, or flipped back and forth: no instant restore.
    CHECK(Classify(2, 0, TEAM_IDX_ALLIANCE) == NodeSituation::FLIPPED_BACK);
    CHECK(Classify(2, 1, TEAM_IDX_ALLIANCE) == NodeSituation::FLIPPED_BACK);
    // Horde view of the same states.
    CHECK(Classify(2, 3, TEAM_IDX_HORDE) == NodeSituation::CAPPING);
    CHECK(Classify(1, 4, TEAM_IDX_HORDE) == NodeSituation::UNDER_ASSAULT);
    CHECK(Classify(3, 1, TEAM_IDX_HORDE) == NodeSituation::THEIRS);
    CHECK(Classify(1, 0, TEAM_IDX_HORDE) == NodeSituation::FLIPPED_BACK);
    // A node that is held or can be taken.
    CHECK(IsHeld(NodeSituation::OURS) && IsHeld(NodeSituation::CAPPING) && IsHeld(NodeSituation::UNDER_ASSAULT));
    CHECK(!IsHeld(NodeSituation::THEIRS) && !IsHeld(NodeSituation::NEUTRAL));
    CHECK(IsTargetable(NodeSituation::THEIRS) && IsTargetable(NodeSituation::NEUTRAL) &&
          IsTargetable(NodeSituation::FLIPPED_BACK));
    CHECK(!IsTargetable(NodeSituation::OURS) && !IsTargetable(NodeSituation::CAPPING));
}

TEST(ab_contest_remaining_time_is_inferred_from_the_observed_change)
{
    CHECK(ContestRemainingMs(0) == 60000);
    CHECK(ContestRemainingMs(45000) == 15000);
    CHECK(ContestRemainingMs(60000) == 0);
    CHECK(ContestRemainingMs(90000) == 0);
}

// --- regimes ------------------------------------------------------------------------------------------------------------

TEST(ab_regime_even_at_the_start_and_when_close)
{
    RaceInput in;
    CHECK(ChooseRegime(in, Regime::EVEN) == Regime::EVEN);   // nothing held yet
    in.ourNodes = 1;
    in.theirNodes = 1;
    CHECK(ChooseRegime(in, Regime::EVEN) == Regime::EVEN);   // the normal opening: one node each
    in.ourNodes = 3;
    in.theirNodes = 3;
    in.ourScore = 500;
    in.theirScore = 450;
    CHECK(ChooseRegime(in, Regime::EVEN) == Regime::EVEN);   // ahead on the clock by 30 s: less than the 90 s margin
}

TEST(ab_regime_hold_needs_three_nodes_and_a_margin_with_hysteresis)
{
    RaceInput in;
    in.ourNodes = 3;
    in.theirNodes = 2;
    in.ourScore = 800;
    in.theirScore = 400;
    // eta us 800 / 1.667 = 480 s, them 1200 / 1.111 = 1080 s: far ahead.
    CHECK(ChooseRegime(in, Regime::EVEN) == Regime::HOLD);

    // 2 nodes against 3 and ahead on the clock: no hold, a hold needs three nodes.
    RaceInput two;
    two.ourNodes = 2;
    two.theirNodes = 1;
    two.ourScore = 900;
    two.theirScore = 100;
    CHECK(ChooseRegime(two, Regime::EVEN) != Regime::HOLD);

    // Hysteresis: a margin of 80 s does not enter a hold but keeps it (enter at 90, leave below 60).
    RaceInput edge;
    edge.ourNodes = 3;
    edge.theirNodes = 3;
    edge.ourScore = 700;     // eta 540
    edge.theirScore = 640;   // eta 576: margin 36
    CHECK(ChooseRegime(edge, Regime::EVEN) == Regime::EVEN);
    edge.theirScore = 570;   // eta 618: margin 78
    CHECK(ChooseRegime(edge, Regime::EVEN) == Regime::EVEN);
    CHECK(ChooseRegime(edge, Regime::HOLD) == Regime::HOLD);
    edge.theirScore = 640;   // margin 36: leaves the hold
    CHECK(ChooseRegime(edge, Regime::HOLD) == Regime::EVEN);
}

TEST(ab_regime_push_when_behind_by_more_than_the_margin_with_hysteresis)
{
    RaceInput in;
    in.ourNodes = 2;
    in.theirNodes = 3;
    in.ourScore = 400;      // eta 1080
    in.theirScore = 600;    // eta 600: behind by 480 s
    CHECK(ChooseRegime(in, Regime::EVEN) == Regime::PUSH);

    RaceInput edge;
    edge.ourNodes = 3;
    edge.theirNodes = 3;
    edge.ourScore = 600;     // eta 600
    edge.theirScore = 700;   // eta 540: behind by 60 s
    CHECK(ChooseRegime(edge, Regime::EVEN) == Regime::EVEN);
    edge.theirScore = 740;   // eta 516: behind by 84 s
    CHECK(ChooseRegime(edge, Regime::EVEN) == Regime::EVEN);
    CHECK(ChooseRegime(edge, Regime::PUSH) == Regime::PUSH);   // stays: it was a push
    edge.theirScore = 700;   // behind by 60 s: below the stay margin (60) it ends
    CHECK(ChooseRegime(edge, Regime::PUSH) == Regime::PUSH);
    edge.theirScore = 650;   // behind by 30 s
    CHECK(ChooseRegime(edge, Regime::PUSH) == Regime::EVEN);
}

TEST(ab_regime_close_out_at_four_nodes_and_last_node_against_three)
{
    RaceInput in;
    in.ourNodes = 4;
    in.theirNodes = 1;
    in.ourScore = 600;
    in.theirScore = 300;
    CHECK(ChooseRegime(in, Regime::EVEN) == Regime::CLOSE_OUT);

    RaceInput last;
    last.ourNodes = 1;
    last.theirNodes = 4;
    last.ourScore = 300;
    last.theirScore = 300;
    CHECK(ChooseRegime(last, Regime::EVEN) == Regime::LAST_NODE);

    // One node each is the normal opening and is not the last node.
    last.theirNodes = 1;
    CHECK(ChooseRegime(last, Regime::EVEN) == Regime::EVEN);
    last.theirNodes = 2;
    CHECK(ChooseRegime(last, Regime::EVEN) == Regime::PUSH);   // one against two is behind on the clock, not the last node
}

TEST(ab_regime_panic_when_the_enemy_is_about_to_win_the_race)
{
    RaceInput in;
    in.ourNodes = 2;
    in.theirNodes = 4;
    in.ourScore = 700;
    in.theirScore = 1400;   // 200 points at 3.33 per second: eta 60 s
    CHECK(ChooseRegime(in, Regime::EVEN) == Regime::PANIC);

    // Ahead on the race: no panic even if the enemy is near the end.
    RaceInput ahead;
    ahead.ourNodes = 4;
    ahead.theirNodes = 3;
    ahead.ourScore = 1590;
    ahead.theirScore = 1500;
    CHECK(ChooseRegime(ahead, Regime::EVEN) != Regime::PANIC);

    // Hysteresis: eta(them) of 200 s ends a new panic but keeps a running one.
    RaceInput edge;
    edge.ourNodes = 2;
    edge.theirNodes = 3;
    edge.ourScore = 1000;     // eta 540
    edge.theirScore = 1300;   // eta 180: panic
    CHECK(ChooseRegime(edge, Regime::EVEN) == Regime::PANIC);
    edge.theirScore = 1270;   // eta 198
    CHECK(ChooseRegime(edge, Regime::EVEN) != Regime::PANIC);
    CHECK(ChooseRegime(edge, Regime::PANIC) == Regime::PANIC);
}

// --- guards and the opening ------------------------------------------------------------------------------------------

TEST(ab_guard_counts_per_plan)
{
    GuardInput g;
    // Alliance, home = Stables.
    CHECK(GuardCount(Plan::BALANCED, TEAM_IDX_ALLIANCE, NODE_STABLES, g) == 1);
    CHECK(GuardCount(Plan::BALANCED, TEAM_IDX_ALLIANCE, NODE_BLACKSMITH, g) == 2);
    CHECK(GuardCount(Plan::BALANCED, TEAM_IDX_ALLIANCE, NODE_GOLD_MINE, g) == 2);
    CHECK(GuardCount(Plan::BALANCED, TEAM_IDX_ALLIANCE, NODE_LUMBER_MILL, g) == 2);
    CHECK(GuardCount(Plan::OFFENSE, TEAM_IDX_ALLIANCE, NODE_STABLES, g) == 1);
    CHECK(GuardCount(Plan::OFFENSE, TEAM_IDX_ALLIANCE, NODE_BLACKSMITH, g) == 2);
    CHECK(GuardCount(Plan::OFFENSE, TEAM_IDX_ALLIANCE, NODE_GOLD_MINE, g) == 1);
    CHECK(GuardCount(Plan::DEFENSE, TEAM_IDX_ALLIANCE, NODE_STABLES, g) == 2);
    CHECK(GuardCount(Plan::DEFENSE, TEAM_IDX_ALLIANCE, NODE_BLACKSMITH, g) == 3);
    CHECK(GuardCount(Plan::DEFENSE, TEAM_IDX_ALLIANCE, NODE_GOLD_MINE, g) == 3);
    // Horde: home = Farm; the enemy's home node (Stables) gets one guard.
    CHECK(GuardCount(Plan::DEFENSE, TEAM_IDX_HORDE, NODE_FARM, g) == 2);
    CHECK(GuardCount(Plan::DEFENSE, TEAM_IDX_HORDE, NODE_STABLES, g) == 1);
    CHECK(GuardCount(Plan::BALANCED, TEAM_IDX_HORDE, NODE_FARM, g) == 1);
}

TEST(ab_guard_counts_change_with_bracket_healers_and_regime)
{
    GuardInput low;
    low.lowBracket = true;
    CHECK(GuardCount(Plan::OFFENSE, TEAM_IDX_ALLIANCE, NODE_GOLD_MINE, low) == 2);   // 2 each at the far nodes, not 1
    CHECK(GuardCount(Plan::OFFENSE, TEAM_IDX_ALLIANCE, NODE_BLACKSMITH, low) == 2);

    GuardInput noHealer;
    noHealer.hasHealer = false;
    CHECK(GuardCount(Plan::BALANCED, TEAM_IDX_ALLIANCE, NODE_BLACKSMITH, noHealer) == 3);
    CHECK(GuardCount(Plan::BALANCED, TEAM_IDX_ALLIANCE, NODE_GOLD_MINE, noHealer) == 2);

    GuardInput last;
    last.regime = Regime::LAST_NODE;
    CHECK(GuardCount(Plan::BALANCED, TEAM_IDX_ALLIANCE, NODE_BLACKSMITH, last) == 4);

    GuardInput panic;
    panic.regime = Regime::PANIC;
    CHECK(GuardCount(Plan::DEFENSE, TEAM_IDX_ALLIANCE, NODE_BLACKSMITH, panic) == 1);

    GuardInput push;
    push.regime = Regime::PUSH;
    CHECK(GuardCount(Plan::DEFENSE, TEAM_IDX_ALLIANCE, NODE_BLACKSMITH, push) == 1);   // quiet node
    push.enemyNear50 = 4;
    CHECK(GuardCount(Plan::DEFENSE, TEAM_IDX_ALLIANCE, NODE_BLACKSMITH, push) == 2);   // enemies near: 2
}

TEST(ab_opening_counts_of_the_three_plans_at_fifteen)
{
    OpeningCounts const bal = OpeningFor(Plan::BALANCED, TEAM_IDX_ALLIANCE, 15, NODE_GOLD_MINE);
    CHECK(bal.byNode[NODE_STABLES] == 2 && bal.byNode[NODE_BLACKSMITH] == 4 && bal.byNode[NODE_GOLD_MINE] == 4);
    CHECK(bal.byNode[NODE_LUMBER_MILL] == 4 && bal.raiders == 1 && bal.reserve == 0 && bal.byNode[NODE_FARM] == 0);

    OpeningCounts const off = OpeningFor(Plan::OFFENSE, TEAM_IDX_ALLIANCE, 15, NODE_GOLD_MINE);
    CHECK(off.byNode[NODE_STABLES] == 1 && off.byNode[NODE_BLACKSMITH] == 4 && off.byNode[NODE_GOLD_MINE] == 4);
    CHECK(off.byNode[NODE_LUMBER_MILL] == 4 && off.raiders == 2 && off.reserve == 0);

    OpeningCounts const def = OpeningFor(Plan::DEFENSE, TEAM_IDX_ALLIANCE, 15, NODE_GOLD_MINE);
    CHECK(def.byNode[NODE_STABLES] == 2 && def.byNode[NODE_BLACKSMITH] == 5 && def.byNode[NODE_GOLD_MINE] == 5);
    CHECK(def.byNode[NODE_LUMBER_MILL] == 0 && def.raiders == 0 && def.reserve == 3);

    // The Horde's home is the Farm, and the nearer contested node can be the Lumber Mill.
    OpeningCounts const horde = OpeningFor(Plan::DEFENSE, TEAM_IDX_HORDE, 15, NODE_LUMBER_MILL);
    CHECK(horde.byNode[NODE_FARM] == 2 && horde.byNode[NODE_LUMBER_MILL] == 5 && horde.byNode[NODE_GOLD_MINE] == 0);
    CHECK(horde.byNode[NODE_STABLES] == 0);
}

TEST(ab_opening_counts_add_up_for_8_to_15_players_and_keep_the_minimums)
{
    for (Plan plan : { Plan::BALANCED, Plan::OFFENSE, Plan::DEFENSE })
    {
        for (std::uint32_t size = 1; size <= 15; ++size)
        {
            OpeningCounts const o = OpeningFor(plan, TEAM_IDX_ALLIANCE, size, NODE_GOLD_MINE);
            CHECK(o.Total() == size);
            if (size < 5)
                continue;
            CHECK(o.byNode[NODE_STABLES] >= 1);   // at least one at home
            CHECK(o.byNode[NODE_BLACKSMITH] >= 2);
            CHECK(o.byNode[NODE_GOLD_MINE] >= 2);
            if (size < FULL_TEAM)
            {
                // Below 10: home, Blacksmith and the nearer contested node only.
                CHECK(o.byNode[NODE_LUMBER_MILL] == 0);
                CHECK(o.byNode[NODE_FARM] == 0);
            }
            if (size < RAIDER_MIN_TEAM)
                CHECK(o.raiders == 0);
        }
    }
    // Full teams open both contested nodes with at least two bots each.
    OpeningCounts const ten = OpeningFor(Plan::BALANCED, TEAM_IDX_ALLIANCE, 10, NODE_GOLD_MINE);
    CHECK(ten.byNode[NODE_LUMBER_MILL] >= 2 && ten.byNode[NODE_GOLD_MINE] >= 2);
}

// --- slots ------------------------------------------------------------------------------------------------------------

TEST(ab_opening_slots_balanced_cover_the_team_and_put_the_groups_on_the_nodes)
{
    std::vector<Slot> const slots = BuildSlots(Opening(Plan::BALANCED));
    CHECK(slots.size() == 15);
    CHECK(CountSlots(slots, Job::GUARD) == 0);
    CHECK(CountSlots(slots, Job::RAIDER) == 1);
    CHECK(CountSlots(slots, Job::ASSAULT, NODE_STABLES) == 2);
    CHECK(CountSlots(slots, Job::ASSAULT, NODE_BLACKSMITH) == 4);
    CHECK(CountSlots(slots, Job::ASSAULT, NODE_GOLD_MINE) == 4);
    CHECK(CountSlots(slots, Job::ASSAULT, NODE_LUMBER_MILL) == 4);
    CHECK(CountSlots(slots, Job::MOBILE) == 0);
}

TEST(ab_opening_slots_defense_hold_three_with_a_reserve)
{
    std::vector<Slot> const slots = BuildSlots(Opening(Plan::DEFENSE));
    CHECK(slots.size() == 15);
    CHECK(CountSlots(slots, Job::RAIDER) == 0);
    CHECK(CountSlots(slots, Job::ASSAULT, NODE_BLACKSMITH) == 5);
    CHECK(CountSlots(slots, Job::ASSAULT, NODE_GOLD_MINE) == 5);
    CHECK(CountSlots(slots, Job::ASSAULT, NODE_LUMBER_MILL) == 0);
    CHECK(CountSlots(slots, Job::MOBILE) == 3);
}

TEST(ab_slots_steady_state_four_nodes_held_balanced_matches_the_sheet)
{
    SlotInput in = Opening(Plan::BALANCED);
    in.playMs = 600000;
    in.sit[NODE_STABLES] = NodeSituation::OURS;
    in.sit[NODE_BLACKSMITH] = NodeSituation::OURS;
    in.sit[NODE_GOLD_MINE] = NodeSituation::OURS;
    in.sit[NODE_LUMBER_MILL] = NodeSituation::OURS;
    in.sit[NODE_FARM] = NodeSituation::THEIRS;
    in.regime = Regime::EVEN;
    std::vector<Slot> const slots = BuildSlots(in);
    // 7 guards (1 + 2 + 2 + 2), 1 raider, 4 assault, 3 mobile: the sheet's steady split.
    CHECK(slots.size() == 15);
    CHECK(CountSlots(slots, Job::GUARD) == 7);
    CHECK(CountSlots(slots, Job::RAIDER) == 1);
    CHECK(CountSlots(slots, Job::ASSAULT) == 4);
    CHECK(CountSlots(slots, Job::MOBILE) == 3);
    CHECK(CountSlots(slots, Job::GUARD, NODE_STABLES) == 1);
    CHECK(CountSlots(slots, Job::GUARD, NODE_BLACKSMITH) == 2);
}

TEST(ab_slots_offense_and_defense_steady_state)
{
    SlotInput in = Opening(Plan::OFFENSE);
    in.playMs = 600000;
    in.sit[NODE_STABLES] = NodeSituation::OURS;
    in.sit[NODE_BLACKSMITH] = NodeSituation::OURS;
    in.sit[NODE_GOLD_MINE] = NodeSituation::OURS;
    in.sit[NODE_LUMBER_MILL] = NodeSituation::OURS;
    std::vector<Slot> const off = BuildSlots(in);
    // 5 guards (1 everywhere, 2 at the Blacksmith), 2 raiders, 5 assault, 3 mobile.
    CHECK(CountSlots(off, Job::GUARD) == 5);
    CHECK(CountSlots(off, Job::RAIDER) == 2);
    CHECK(CountSlots(off, Job::ASSAULT) == 5);
    CHECK(CountSlots(off, Job::MOBILE) == 3);

    SlotInput def = Opening(Plan::DEFENSE);
    def.playMs = 600000;
    def.sit[NODE_STABLES] = NodeSituation::OURS;
    def.sit[NODE_BLACKSMITH] = NodeSituation::OURS;
    def.sit[NODE_GOLD_MINE] = NodeSituation::OURS;
    std::vector<Slot> const d = BuildSlots(def);
    // 8 guards (2 + 3 + 3), no raider, 2 assault, 5 mobile.
    CHECK(CountSlots(d, Job::GUARD) == 8);
    CHECK(CountSlots(d, Job::RAIDER) == 0);
    CHECK(CountSlots(d, Job::ASSAULT) == 2);
    CHECK(CountSlots(d, Job::MOBILE) == 5);
}

TEST(ab_slots_regimes_change_the_assault_size)
{
    SlotInput in = Opening(Plan::BALANCED);
    in.playMs = 600000;
    in.sit[NODE_STABLES] = NodeSituation::OURS;
    in.sit[NODE_BLACKSMITH] = NodeSituation::OURS;
    in.sit[NODE_GOLD_MINE] = NodeSituation::OURS;
    in.sit[NODE_FARM] = NodeSituation::THEIRS;
    in.sit[NODE_LUMBER_MILL] = NodeSituation::THEIRS;

    in.regime = Regime::HOLD;
    CHECK(CountSlots(BuildSlots(in), Job::ASSAULT) == ASSAULT_HOLD);
    in.regime = Regime::PUSH;
    // PUSH: one guard at quiet nodes (3), 1 raider, 11 free: assault = max(4 + 2, 11 - 1) = 10 capped by the free bots.
    std::vector<Slot> const push = BuildSlots(in);
    CHECK(CountSlots(push, Job::GUARD) == 3);
    CHECK(CountSlots(push, Job::ASSAULT) > CountSlots(BuildSlots(Opening(Plan::BALANCED)), Job::MOBILE));
    in.regime = Regime::CLOSE_OUT;
    std::vector<Slot> const close = BuildSlots(in);
    CHECK(CountSlots(close, Job::MOBILE) == 0);   // everybody who is not a guard or the raider goes
    in.regime = Regime::EVEN;
    CHECK(CountSlots(BuildSlots(in), Job::ASSAULT) <= ASSAULT_GROUP_MAX);
}

TEST(ab_slots_a_node_under_assault_keeps_its_guards_and_humans_take_the_last_slots)
{
    SlotInput in = Opening(Plan::BALANCED);
    in.playMs = 600000;
    in.sit[NODE_STABLES] = NodeSituation::OURS;
    in.sit[NODE_BLACKSMITH] = NodeSituation::UNDER_ASSAULT;
    std::vector<Slot> const slots = BuildSlots(in);
    CHECK(CountSlots(slots, Job::GUARD, NODE_BLACKSMITH) == 2);

    in.humans = 3;
    std::vector<Slot> const withHumans = BuildSlots(in);
    CHECK(withHumans.size() == 12);
    CHECK(CountSlots(withHumans, Job::GUARD) == CountSlots(slots, Job::GUARD));   // the reserve gives way first
    in.humans = 15;
    CHECK(BuildSlots(in).empty());
}

TEST(ab_slots_guard_order_puts_the_blacksmith_first_and_home_last)
{
    SlotInput in = Opening(Plan::BALANCED);
    in.playMs = 600000;
    for (std::uint8_t n = 0; n < NODE_COUNT; ++n)
        in.sit[n] = NodeSituation::OURS;
    std::vector<Slot> const slots = BuildSlots(in);
    CHECK(slots.front().job == Job::GUARD && slots.front().node == NODE_BLACKSMITH);
    std::vector<std::uint8_t> const order = GuardOrder(TEAM_IDX_ALLIANCE);
    CHECK(order.front() == NODE_BLACKSMITH);
    CHECK(order.back() == NODE_STABLES);
    CHECK(GuardOrder(TEAM_IDX_HORDE).back() == NODE_FARM);
}

TEST(ab_opening_groups_over_when_a_node_is_held_and_after_the_opening)
{
    SlotInput in = Opening(Plan::BALANCED);
    in.sit[NODE_BLACKSMITH] = NodeSituation::CAPPING;
    std::vector<Slot> const slots = BuildSlots(in);
    // The Blacksmith group became guards (2) and the surplus is not stuck at the node: it joins the free assault group
    // or the reserve.
    CHECK(CountSlots(slots, Job::GUARD, NODE_BLACKSMITH) == 2);
    CHECK(CountSlots(slots, Job::ASSAULT, NODE_BLACKSMITH) == 0);
    CHECK(CountSlots(slots, Job::ASSAULT, NODE_GOLD_MINE) == 4);
    CHECK(slots.size() == 15);

    in.sit[NODE_BLACKSMITH] = NodeSituation::NEUTRAL;
    in.playMs = OPENING_SLOT_MS;
    std::vector<Slot> const late = BuildSlots(in);
    CHECK(CountSlots(late, Job::ASSAULT, NODE_GOLD_MINE) == 0);   // no opening group after the opening
}

// --- assignment ---------------------------------------------------------------------------------------------------------

TEST(ab_assignment_fills_every_slot_and_every_bot_gets_a_job)
{
    std::vector<Candidate> const team = Team15();
    std::vector<Slot> const slots = BuildSlots(Opening(Plan::BALANCED));
    std::vector<Assignment> const result = Assign(team, slots, TEAM_IDX_ALLIANCE);
    CHECK(result.size() == team.size());
    CHECK(CountAssigned(result, Job::ASSAULT, NODE_BLACKSMITH) == 4);
    CHECK(CountAssigned(result, Job::ASSAULT, NODE_GOLD_MINE) == 4);
    CHECK(CountAssigned(result, Job::ASSAULT, NODE_LUMBER_MILL) == 4);
    CHECK(CountAssigned(result, Job::ASSAULT, NODE_STABLES) == 2);
    CHECK(CountAssigned(result, Job::RAIDER) == 1);
}

TEST(ab_assignment_is_deterministic_and_independent_of_the_candidate_order)
{
    std::vector<Candidate> const team = Team15();
    std::vector<Candidate> reversed(team.rbegin(), team.rend());
    std::vector<Slot> const slots = BuildSlots(Opening(Plan::BALANCED));
    std::vector<Assignment> const a = Assign(team, slots, TEAM_IDX_ALLIANCE);
    std::vector<Assignment> const b = Assign(reversed, slots, TEAM_IDX_ALLIANCE);
    for (Candidate const& c : team)
        CHECK(SlotOfGuid(a, c.guidLow) == SlotOfGuid(b, c.guidLow));
}

TEST(ab_stealth_class_takes_the_raider_slot_and_without_one_the_slot_stays_empty)
{
    std::vector<Candidate> team = Team15();
    std::vector<Slot> const slots = BuildSlots(Opening(Plan::BALANCED));
    std::vector<Assignment> const result = Assign(team, slots, TEAM_IDX_ALLIANCE);
    CHECK(SlotOfGuid(result, 101).job == Job::RAIDER);   // the rogue

    // Two stealth classes, one slot: the higher level one raids; a feral druid counts.
    team.push_back(Bot(120, CLS_ROGUE, 19));
    std::vector<Assignment> const two = Assign(team, BuildSlots(Opening(Plan::BALANCED, 16)), TEAM_IDX_ALLIANCE);
    CHECK(CountAssigned(two, Job::RAIDER) == 1);

    // No stealth class: the raider slot is not filled and its bot is in the reserve.
    std::vector<Candidate> noStealth;
    for (Candidate const& c : Team15())
        if (c.cls != CLS_ROGUE)
            noStealth.push_back(c);
    std::vector<Assignment> const none = Assign(noStealth, BuildSlots(Opening(Plan::BALANCED, 14)), TEAM_IDX_ALLIANCE);
    CHECK(CountAssigned(none, Job::RAIDER) == 0);
    CHECK(none.size() == noStealth.size());

    std::vector<Candidate> cat = { Bot(1, CLS_DRUID, 20), Bot(2, CLS_WARRIOR, 20) };
    cat[0].feral = true;
    std::vector<Slot> const raiderOnly = { Slot{ Job::RAIDER, NODE_NONE } };
    CHECK(SlotOfGuid(Assign(cat, raiderOnly, TEAM_IDX_ALLIANCE), 1).job == Job::RAIDER);
    cat[0].feral = false;
    CHECK(CountAssigned(Assign(cat, raiderOnly, TEAM_IDX_ALLIANCE), Job::RAIDER) == 0);
}

TEST(ab_healers_spread_one_at_the_blacksmith_one_with_the_assault_group)
{
    SlotInput in = Opening(Plan::BALANCED);
    in.playMs = 600000;
    in.sit[NODE_STABLES] = NodeSituation::OURS;
    in.sit[NODE_BLACKSMITH] = NodeSituation::OURS;
    in.sit[NODE_GOLD_MINE] = NodeSituation::OURS;
    in.sit[NODE_LUMBER_MILL] = NodeSituation::OURS;
    in.sit[NODE_FARM] = NodeSituation::THEIRS;
    std::vector<Candidate> const team = Team15();   // two healers: 105 and 113
    std::vector<Assignment> const result = Assign(team, BuildSlots(in), TEAM_IDX_ALLIANCE);

    Slot const h1 = SlotOfGuid(result, 105);
    Slot const h2 = SlotOfGuid(result, 113);
    bool const oneAtBlacksmith = (h1.job == Job::GUARD && h1.node == NODE_BLACKSMITH) ||
                                 (h2.job == Job::GUARD && h2.node == NODE_BLACKSMITH);
    bool const oneAssault = h1.job == Job::ASSAULT || h2.job == Job::ASSAULT;
    CHECK(oneAtBlacksmith);
    CHECK(oneAssault);
    CHECK(h1 != h2);
}

TEST(ab_assignment_is_stable_when_nothing_changed_and_positions_drift)
{
    std::vector<Candidate> team = Team15();
    SlotInput in = Opening(Plan::BALANCED);
    in.playMs = 600000;
    in.sit[NODE_STABLES] = NodeSituation::OURS;
    in.sit[NODE_BLACKSMITH] = NodeSituation::OURS;
    in.sit[NODE_GOLD_MINE] = NodeSituation::OURS;
    in.sit[NODE_LUMBER_MILL] = NodeSituation::OURS;
    in.sit[NODE_FARM] = NodeSituation::THEIRS;
    std::vector<Slot> const slots = BuildSlots(in);
    std::vector<Assignment> const first = Assign(team, slots, TEAM_IDX_ALLIANCE);

    // The next refresh: everybody remembers the slot and the bots have moved (to the nodes, in a different order).
    float shift = 0.0f;
    for (Candidate& c : team)
    {
        Slot const s = SlotOfGuid(first, c.guidLow);
        c.hasPrevious = true;
        c.previous = s;
        c.x = (s.node < NODE_COUNT ? NODE_POS[s.node].x : 1000.0f) + shift;
        c.y = (s.node < NODE_COUNT ? NODE_POS[s.node].y : 1000.0f) - shift;
        shift += 7.0f;
    }
    std::vector<Assignment> const second = Assign(team, slots, TEAM_IDX_ALLIANCE);
    for (Candidate const& c : team)
        CHECK(SlotOfGuid(first, c.guidLow) == SlotOfGuid(second, c.guidLow));
}

TEST(ab_a_death_changes_one_slot_not_the_team)
{
    std::vector<Candidate> team = Team15();
    SlotInput in = Opening(Plan::BALANCED);
    in.playMs = 600000;
    in.sit[NODE_STABLES] = NodeSituation::OURS;
    in.sit[NODE_BLACKSMITH] = NodeSituation::OURS;
    in.sit[NODE_GOLD_MINE] = NodeSituation::OURS;
    in.sit[NODE_LUMBER_MILL] = NodeSituation::OURS;
    in.sit[NODE_FARM] = NodeSituation::THEIRS;
    std::vector<Slot> const slots = BuildSlots(in);
    std::vector<Assignment> const before = Assign(team, slots, TEAM_IDX_ALLIANCE);
    for (Candidate& c : team)
    {
        c.hasPrevious = true;
        c.previous = SlotOfGuid(before, c.guidLow);
    }

    // The first Blacksmith guard dies (leaves the candidates).
    std::uint32_t deadGuid = 0;
    for (Assignment const& a : before)
        if (a.slot.job == Job::GUARD && a.slot.node == NODE_BLACKSMITH)
        {
            deadGuid = a.guidLow;
            break;
        }
    std::vector<Candidate> alive;
    for (Candidate const& c : team)
        if (c.guidLow != deadGuid)
            alive.push_back(c);
    std::vector<Assignment> const after = Assign(alive, slots, TEAM_IDX_ALLIANCE);

    // The Blacksmith still has its two guards; a spare filled the gap, and most bots kept their slot.
    CHECK(CountAssigned(after, Job::GUARD, NODE_BLACKSMITH) == 2);
    std::uint32_t moved = 0;
    for (Candidate const& c : alive)
        if (!(SlotOfGuid(before, c.guidLow) == SlotOfGuid(after, c.guidLow)))
            ++moved;
    CHECK(moved <= 3);
}

TEST(ab_a_bot_that_is_casting_the_capture_keeps_its_slot)
{
    std::vector<Candidate> team = Team15();
    SlotInput in = Opening(Plan::BALANCED);
    std::vector<Slot> const slots = BuildSlots(in);
    team[3].hasPrevious = true;
    team[3].previous = Slot{ Job::ASSAULT, NODE_LUMBER_MILL };
    team[3].pinned = true;
    std::vector<Assignment> const result = Assign(team, slots, TEAM_IDX_ALLIANCE);
    CHECK(SlotOfGuid(result, team[3].guidLow) == (Slot{ Job::ASSAULT, NODE_LUMBER_MILL }));
}

TEST(ab_tier_fit_puts_expert_bots_on_the_moving_jobs_and_casual_bots_on_posts)
{
    // Two identical warriors, one expert and one casual, for one assault slot and one guard slot.
    std::vector<Candidate> pair = { Bot(1, CLS_WARRIOR, 20, Tier::EXPERT), Bot(2, CLS_WARRIOR, 20, Tier::CASUAL) };
    std::vector<Slot> const slots = { Slot{ Job::ASSAULT, NODE_BLACKSMITH }, Slot{ Job::GUARD, NODE_BLACKSMITH } };
    std::vector<Assignment> const a = Assign(pair, slots, TEAM_IDX_ALLIANCE);
    CHECK(SlotOfGuid(a, 1).job == Job::ASSAULT);
    CHECK(SlotOfGuid(a, 2).job == Job::GUARD);
}

TEST(ab_level_beats_class_fit_and_the_nearest_bot_holds_the_node)
{
    // The higher level takes the Blacksmith guard slot even with a worse class fit.
    std::vector<Candidate> pair = { Bot(1, CLS_PALADIN, 20), Bot(2, CLS_MAGE, 24) };
    std::vector<Assignment> const a = Assign(pair, { Slot{ Job::GUARD, NODE_BLACKSMITH } }, TEAM_IDX_ALLIANCE);
    CHECK(SlotOfGuid(a, 2).job == Job::GUARD);

    // Same class and level: the one closer to the banner holds it.
    std::vector<Candidate> near = { Bot(3, CLS_WARRIOR, 20), Bot(4, CLS_WARRIOR, 20) };
    near[0].x = NODE_POS[NODE_GOLD_MINE].x + 200.0f;
    near[0].y = NODE_POS[NODE_GOLD_MINE].y;
    near[1].x = NODE_POS[NODE_GOLD_MINE].x + 5.0f;
    near[1].y = NODE_POS[NODE_GOLD_MINE].y;
    std::vector<Assignment> const b = Assign(near, { Slot{ Job::GUARD, NODE_GOLD_MINE } }, TEAM_IDX_ALLIANCE);
    CHECK(SlotOfGuid(b, 4).job == Job::GUARD);
    CHECK(SlotOfGuid(b, 3).job == Job::MOBILE);
}

// --- options ------------------------------------------------------------------------------------------------------------

namespace
{
DecisionContext Guard(Tier tier = Tier::GOOD, std::uint8_t node = NODE_BLACKSMITH)
{
    DecisionContext c;
    c.job = Job::GUARD;
    c.slotNode = node;
    c.tier = tier;
    c.sit[node] = NodeSituation::OURS;
    return c;
}
}  // namespace

TEST(ab_guard_holds_its_post_and_restores_first)
{
    DecisionContext c = Guard();
    std::vector<Option> const quiet = Options(c);
    CHECK(quiet.front().kind == Kind::HOLD_POST);
    CHECK(quiet.front().node == NODE_BLACKSMITH);

    // The node is assaulted and this guard is the restorer: the click comes before everything.
    c.sit[NODE_BLACKSMITH] = NodeSituation::UNDER_ASSAULT;
    c.restoreResponder = true;
    c.restoreNode = NODE_BLACKSMITH;
    std::vector<Option> const assault = Options(c);
    CHECK(assault.front().kind == Kind::RESTORE);
    CHECK(assault.front().node == NODE_BLACKSMITH);
    CHECK(assault.back().kind == Kind::HOLD_QUIET);
}

TEST(ab_guard_stays_at_a_node_it_is_capping_until_it_flips_but_casual_guards_drift)
{
    DecisionContext c = Guard();
    c.sit[NODE_BLACKSMITH] = NodeSituation::CAPPING;
    CHECK(Options(c).front().kind == Kind::HOLD_CAPTURE);

    DecisionContext casual = Guard(Tier::CASUAL);
    casual.sit[NODE_BLACKSMITH] = NodeSituation::CAPPING;
    std::vector<Option> const opts = Options(casual);
    CHECK(opts.front().kind == Kind::HOLD_LOOSE);     // the loose post, no hold-capture rule
    for (Option const& o : opts)
        CHECK(o.kind != Kind::HOLD_CAPTURE);
}

TEST(ab_assault_group_gathers_then_takes_its_node_and_stays_after_the_cast)
{
    DecisionContext c;
    c.job = Job::ASSAULT;
    c.slotNode = NODE_GOLD_MINE;
    c.tier = Tier::GOOD;
    c.groupGathered = false;
    CHECK(Options(c).front().kind == Kind::GATHER);

    c.groupGathered = true;
    CHECK(Options(c).front().kind == Kind::TAKE);
    CHECK(Options(c).front().node == NODE_GOLD_MINE);

    // The cast is done: the node is being captured, the group stays 15 s.
    c.sit[NODE_GOLD_MINE] = NodeSituation::CAPPING;
    c.captureAgeMs[NODE_GOLD_MINE] = 3000;
    CHECK(Options(c).front().kind == Kind::HOLD_CAPTURE);
    // After 15 s it stops holding and has nothing to take (the record gives it a new target next).
    c.captureAgeMs[NODE_GOLD_MINE] = STAY_AFTER_CAST_MS;
    CHECK(Options(c).front().kind != Kind::HOLD_CAPTURE);
    c.assaultNode = NODE_LUMBER_MILL;
    c.slotNode = NODE_NONE;
    CHECK(Options(c).front().kind == Kind::TAKE);
    CHECK(Options(c).front().node == NODE_LUMBER_MILL);
}

TEST(ab_casual_assault_does_not_wait_for_the_group_and_takes_the_nearest_node)
{
    DecisionContext c;
    c.job = Job::ASSAULT;
    c.slotNode = NODE_GOLD_MINE;
    c.tier = Tier::CASUAL;
    c.groupGathered = false;
    CHECK(Options(c).front().kind == Kind::TAKE);   // no gather for casual bots

    DecisionContext free;
    free.job = Job::ASSAULT;
    free.tier = Tier::CASUAL;
    free.assaultNode = NODE_LUMBER_MILL;    // the swing choice
    free.nearestNode = NODE_STABLES;        // the casual choice
    CHECK(Options(free).front().node == NODE_STABLES);
    free.tier = Tier::GOOD;
    CHECK(Options(free).front().node == NODE_LUMBER_MILL);
}

TEST(ab_reserve_answers_inc_and_restores_but_a_casual_reserve_stays_staged)
{
    DecisionContext c;
    c.job = Job::MOBILE;
    c.tier = Tier::GOOD;
    CHECK(Options(c).front().kind == Kind::STAGE);

    c.incResponder = true;
    c.incNode = NODE_GOLD_MINE;
    CHECK(Options(c).front().kind == Kind::REINFORCE);
    CHECK(Options(c).front().node == NODE_GOLD_MINE);

    c.restoreResponder = true;
    c.restoreNode = NODE_LUMBER_MILL;
    CHECK(Options(c).front().kind == Kind::RESTORE);   // a restore outranks an inc

    DecisionContext casual = c;
    casual.tier = Tier::CASUAL;
    CHECK(Options(casual).front().kind == Kind::STAGE);   // answering is a good rule
}

TEST(ab_reserve_joins_a_human_who_captures_and_in_a_push)
{
    DecisionContext c;
    c.job = Job::MOBILE;
    c.tier = Tier::GOOD;
    c.supportHuman = true;
    c.supportNode = NODE_FARM;
    CHECK(Options(c).front().kind == Kind::SUPPORT_HUMAN);
    CHECK(Options(c).front().node == NODE_FARM);

    DecisionContext push;
    push.job = Job::MOBILE;
    push.tier = Tier::GOOD;
    push.regime = Regime::PUSH;
    push.assaultNode = NODE_GOLD_MINE;
    CHECK(Options(push).front().kind == Kind::TAKE);
    push.regime = Regime::EVEN;
    CHECK(Options(push).front().kind == Kind::STAGE);
}

TEST(ab_close_out_sends_expert_bots_to_the_fifth_node)
{
    DecisionContext c;
    c.job = Job::ASSAULT;
    c.tier = Tier::EXPERT;
    c.regime = Regime::CLOSE_OUT;
    c.closeOutNode = NODE_FARM;
    c.assaultNode = NODE_FARM;
    std::vector<Option> const opts = Options(c);
    CHECK(opts.front().kind == Kind::CLOSE_OUT);
    CHECK(opts.front().node == NODE_FARM);

    // A good bot reaches the same node by the ordinary take.
    c.tier = Tier::GOOD;
    CHECK(Options(c).front().kind == Kind::TAKE);
    CHECK(Options(c).front().node == NODE_FARM);
}

TEST(ab_raider_raids_leaves_after_the_cast_unless_nobody_is_near_and_has_a_fallback)
{
    DecisionContext c;
    c.job = Job::RAIDER;
    c.tier = Tier::EXPERT;
    c.raidNode = NODE_FARM;
    CHECK(Options(c).front().kind == Kind::RAID);

    c.sit[NODE_FARM] = NodeSituation::CAPPING;   // the cast is done
    c.raidStay = false;
    CHECK(Options(c).front().kind == Kind::STAGE);   // leaves: hit and run
    c.raidStay = true;
    CHECK(Options(c).front().kind == Kind::HOLD_CAPTURE);   // nobody within 60 yd: stays for the 60 s

    c.raidNode = NODE_NONE;
    c.sit[NODE_FARM] = NodeSituation::THEIRS;
    CHECK(Options(c).front().kind == Kind::STAGE);
}

TEST(ab_every_decision_has_at_least_one_option_and_a_deterministic_order)
{
    for (Job job : { Job::GUARD, Job::RAIDER, Job::ASSAULT, Job::MOBILE })
    {
        for (Tier tier : { Tier::EXPERT, Tier::GOOD, Tier::CASUAL })
        {
            DecisionContext c;
            c.job = job;
            c.tier = tier;
            std::vector<Option> const a = Options(c);
            std::vector<Option> const b = Options(c);
            CHECK(!a.empty());
            CHECK(a.size() == b.size());
            for (std::size_t i = 0; i < a.size(); ++i)
            {
                CHECK(a[i].kind == b[i].kind);
                CHECK(a[i].score == b[i].score);
                if (i)
                    CHECK(a[i - 1].score >= a[i].score);
            }
        }
    }
}

// --- targets, raiders, responders -------------------------------------------------------------------------------------

TEST(ab_targets_prefer_the_bigger_swing_per_second_and_punish_unwinnable_fights)
{
    std::vector<TargetInput> const inputs = {
        { NODE_FARM, NodeSituation::THEIRS, 0, 0, 60.0f, false },
        { NODE_GOLD_MINE, NodeSituation::THEIRS, 0, 0, 60.0f, false },
        { NODE_LUMBER_MILL, NodeSituation::NEUTRAL, 0, 0, 60.0f, false },
        { NODE_BLACKSMITH, NodeSituation::OURS, 0, 0, 10.0f, false }
    };
    std::vector<TargetScore> const t = ScoreTargets(inputs, 2, 3, 4, Tier::GOOD);
    CHECK(t.size() == 3);                       // an owned node is not a target
    CHECK(t[0].node == NODE_FARM);              // flipping a node gains and denies: ties by the lower node
    CHECK(t[1].node == NODE_GOLD_MINE);
    CHECK(t[2].node == NODE_LUMBER_MILL);       // a free node only gains
    CHECK(t[0].score > t[2].score);

    // Six enemies at one banner against a group of four: that node falls behind.
    std::vector<TargetInput> hard = inputs;
    hard[0].enemyNear40 = 6;
    std::vector<TargetScore> const t2 = ScoreTargets(hard, 2, 3, 4, Tier::GOOD);
    CHECK(t2[0].node == NODE_GOLD_MINE);

    // Nearer wins when the swing is the same.
    std::vector<TargetInput> eta = inputs;
    eta[0].etaS = 200.0f;
    CHECK(ScoreTargets(eta, 2, 3, 4, Tier::GOOD)[0].node == NODE_GOLD_MINE);

    // The fifth node is worth far more than any other.
    std::vector<TargetScore> const close = ScoreTargets({ { NODE_FARM, NodeSituation::THEIRS, 0, 0, 60.0f, false } }, 4, 1, 4, Tier::GOOD);
    CHECK(close[0].score > t[0].score * 5);
}

TEST(ab_flip_limited_nodes_are_left_out_for_expert_bots_only)
{
    std::vector<TargetInput> inputs = {
        { NODE_FARM, NodeSituation::THEIRS, 0, 0, 60.0f, true },
        { NODE_GOLD_MINE, NodeSituation::THEIRS, 0, 0, 60.0f, false }
    };
    CHECK(ScoreTargets(inputs, 2, 3, 4, Tier::EXPERT).size() == 1);
    CHECK(ScoreTargets(inputs, 2, 3, 4, Tier::GOOD).size() == 2);
    CHECK(ScoreTargets(inputs, 2, 3, 4, Tier::CASUAL).size() == 2);
}

TEST(ab_nearest_target_for_casual_groups_and_the_close_out_node)
{
    std::vector<TargetInput> const inputs = {
        { NODE_FARM, NodeSituation::THEIRS, 0, 0, 200.0f, false },
        { NODE_GOLD_MINE, NodeSituation::NEUTRAL, 0, 0, 90.0f, false },
        { NODE_STABLES, NodeSituation::OURS, 0, 0, 5.0f, false }
    };
    CHECK(NearestTarget(inputs) == NODE_GOLD_MINE);
    CHECK(NearestTarget({}) == NODE_NONE);

    NodeSituation sit[NODE_COUNT] = { NodeSituation::OURS, NodeSituation::OURS, NodeSituation::THEIRS,
                                      NodeSituation::OURS, NodeSituation::OURS };
    CHECK(CloseOutNode(sit) == NODE_FARM);
    sit[NODE_GOLD_MINE] = NodeSituation::NEUTRAL;
    CHECK(CloseOutNode(sit) == NODE_NONE);   // two nodes are not ours: no close-out
    NodeSituation all[NODE_COUNT] = { NodeSituation::OURS, NodeSituation::OURS, NodeSituation::OURS,
                                      NodeSituation::OURS, NodeSituation::OURS };
    CHECK(CloseOutNode(all) == NODE_NONE);
}

TEST(ab_raiders_pick_a_lightly_defended_enemy_node_and_prefer_the_enemy_home)
{
    std::vector<RaidInput> inputs = {
        { NODE_FARM, NodeSituation::THEIRS, 0 },
        { NODE_GOLD_MINE, NodeSituation::THEIRS, 0 },
        { NODE_BLACKSMITH, NodeSituation::OURS, 0 },
        { NODE_LUMBER_MILL, NodeSituation::THEIRS, 3 }
    };
    // Alliance raider: the enemy home is the Farm.
    CHECK(PickRaidNode(inputs, TEAM_IDX_ALLIANCE, Tier::EXPERT) == NODE_FARM);
    // Horde raider: the enemy home is the Stables; none of the listed nodes is it, so the lower node wins the tie.
    CHECK(PickRaidNode(inputs, TEAM_IDX_HORDE, Tier::EXPERT) == NODE_FARM);

    // One defender is an expert's job (Sap), not a good bot's; three defenders is nobody's.
    inputs[0].enemyNear40 = 1;
    CHECK(PickRaidNode(inputs, TEAM_IDX_ALLIANCE, Tier::EXPERT) == NODE_GOLD_MINE);   // fewest defenders first
    inputs[1].enemyNear40 = 1;
    CHECK(PickRaidNode(inputs, TEAM_IDX_ALLIANCE, Tier::EXPERT) == NODE_FARM);        // tie: the enemy home
    CHECK(PickRaidNode(inputs, TEAM_IDX_ALLIANCE, Tier::GOOD) == NODE_NONE);
    inputs[0].enemyNear40 = 2;
    inputs[1].enemyNear40 = 2;
    CHECK(PickRaidNode(inputs, TEAM_IDX_ALLIANCE, Tier::EXPERT) == NODE_NONE);
}

TEST(ab_responders_are_the_two_nearest_reserve_bots_never_a_guard_of_another_node)
{
    std::vector<ResponderCandidate> const all = {
        { 1, Job::MOBILE, NODE_NONE, 30000 },
        { 2, Job::MOBILE, NODE_NONE, 20000 },
        { 3, Job::MOBILE, NODE_NONE, 45000 },
        { 4, Job::GUARD, NODE_BLACKSMITH, 5000 },
        { 5, Job::GUARD, NODE_GOLD_MINE, 2000 },
        { 6, Job::RAIDER, NODE_NONE, 1000 },
        { 7, Job::ASSAULT, NODE_NONE, 10000 },
        { 8, Job::ASSAULT, NODE_NONE, 12000 }
    };
    std::vector<std::uint32_t> const r = ChooseResponders(all, NODE_BLACKSMITH, 2, RESPONDER_MAX_ETA_MS, 4);
    // Two reserve bots by eta (2 then 1), plus the node's own guard (4). No raider, no other guard, no assault bot
    // while the reserve fills the count.
    CHECK(r.size() == 3);
    CHECK(r[0] == 2 && r[1] == 1 && r[2] == 4);

    // No reserve: assault bots answer, never more than half of the group.
    std::vector<ResponderCandidate> assaultOnly = { all[3], all[4], all[5], all[6], all[7] };
    std::vector<std::uint32_t> const a = ChooseResponders(assaultOnly, NODE_BLACKSMITH, 2, RESPONDER_MAX_ETA_MS, 2);
    CHECK(a.size() == 2);     // one assault bot (half of 2) and the guard
    CHECK(a[0] == 7 && a[1] == 4);

    // Too far to arrive: out. A restore uses what is left of the contest.
    std::vector<std::uint32_t> const far = ChooseResponders(all, NODE_BLACKSMITH, 2, 25000, 4);
    CHECK(far[0] == 2);
    CHECK(far.size() == 3 || far.size() == 2);
    for (std::uint32_t g : far)
        CHECK(g != 3 && g != 1);

    CHECK(ResponderCount(3) == 2);
    CHECK(ResponderCount(6) == 3);
    CHECK(IncThreshold(false) == 3);
    CHECK(IncThreshold(true) == 2);
}

TEST(ab_flip_limiter_blocks_after_two_flips_in_three_minutes_unless_the_odds_improved)
{
    FlipLimiter limiter;
    CHECK(!limiter.Blocked(NODE_GOLD_MINE, 100000, 0));
    limiter.Record(NODE_GOLD_MINE, 100000);
    CHECK(!limiter.Blocked(NODE_GOLD_MINE, 110000, 0));
    limiter.Record(NODE_GOLD_MINE, 130000);
    CHECK(limiter.Blocked(NODE_GOLD_MINE, 140000, 0));
    CHECK(limiter.Blocked(NODE_GOLD_MINE, 140000, 1));
    CHECK(!limiter.Blocked(NODE_GOLD_MINE, 140000, ODDS_IMPROVED_MARGIN));   // the local odds improved
    CHECK(!limiter.Blocked(NODE_FARM, 140000, 0));                           // per node
    // The flips age out of the window.
    CHECK(!limiter.Blocked(NODE_GOLD_MINE, 100000 + FLIP_WINDOW_MS + 31000, 0));
    CHECK(!limiter.Blocked(NODE_NONE, 140000, 0));
}

TEST(ab_opening_group_is_gathered_with_three_members_or_after_the_wait)
{
    CHECK(GroupGathered(4, 3, 0));
    CHECK(!GroupGathered(4, 2, 5000));
    CHECK(GroupGathered(4, 2, GATHER_WAIT_MS));
    CHECK(GroupGathered(2, 2, 0));    // a pair needs both
    CHECK(!GroupGathered(2, 1, 0));
    CHECK(GroupGathered(0, 0, 0));
}

// --- calls --------------------------------------------------------------------------------------------------------------

TEST(ab_assaulted_call_waits_ten_seconds_after_an_inc)
{
    CallLimiter limiter;
    limiter.Commit(1, Call::INCOMING, 100000);
    CHECK(!limiter.Allow(2, Call::ASSAULTED, 106000));      // the team gap passed, the inc gap did not
    CHECK(limiter.Allow(2, Call::ASSAULTED, 110000));
    CHECK(limiter.Allow(2, Call::NODE_BACK, 106000));       // other kinds are not held back
    CHECK(!limiter.Allow(2, Call::NODE_BACK, 105999));      // the team gap of 6 s
}

TEST(ab_human_calls_with_node_words_are_read)
{
    using PvpObs::Kind;
    BgCallout::Report c = BgCallout::Parse("inc bs 3", BgCallout::MapKind::AB);
    CHECK(c.valid && c.kind == Kind::INCOMING && c.place == NODE_BLACKSMITH && c.count == 3);

    c = BgCallout::Parse("3 inc bs", BgCallout::MapKind::AB);
    CHECK(c.valid && c.kind == Kind::INCOMING && c.place == NODE_BLACKSMITH && c.count == 3);

    c = BgCallout::Parse("HELP farm", BgCallout::MapKind::AB);
    CHECK(c.valid && c.kind == Kind::HELP && c.place == NODE_FARM);

    c = BgCallout::Parse("take lm", BgCallout::MapKind::AB);
    CHECK(c.valid && c.kind == Kind::CLAIM && c.place == NODE_LUMBER_MILL);

    // "clear" is a report that nobody is there, not an order to go (it was read as "take" before).
    c = BgCallout::Parse("gm clear", BgCallout::MapKind::AB);
    CHECK(c.valid && c.kind == Kind::CLEAR && c.place == NODE_GOLD_MINE);

    c = BgCallout::Parse("st lost!", BgCallout::MapKind::AB);
    CHECK(c.valid && c.kind == Kind::LOST && c.place == NODE_STABLES);

    c = BgCallout::Parse("incoming at Blacksmith", BgCallout::MapKind::AB);
    CHECK(c.valid && c.kind == Kind::INCOMING && c.place == NODE_BLACKSMITH);

    c = BgCallout::Parse("inc gold mine", BgCallout::MapKind::AB);
    CHECK(c.valid && c.place == NODE_GOLD_MINE);
}

TEST(ab_other_chat_is_not_a_call)
{
    CHECK(!BgCallout::Parse("", BgCallout::MapKind::AB).valid);
    CHECK(!BgCallout::Parse("good luck everyone", BgCallout::MapKind::AB).valid);
    CHECK(!BgCallout::Parse("inc", BgCallout::MapKind::AB).valid);                    // no node to go to
    CHECK(!BgCallout::Parse("bs", BgCallout::MapKind::AB).valid);                     // no verb
    CHECK(!BgCallout::Parse("inc bs and everything else you can think of about this match", BgCallout::MapKind::AB).valid);   // too long
}

TEST(ab_node_words_for_chat_lines)
{
    CHECK(std::string(NodeWord(NODE_STABLES)) == "st");
    CHECK(std::string(NodeWord(NODE_BLACKSMITH)) == "bs");
    CHECK(std::string(NodeWord(NODE_FARM)) == "farm");
    CHECK(std::string(NodeWord(NODE_LUMBER_MILL)) == "lm");
    CHECK(std::string(NodeWord(NODE_GOLD_MINE)) == "gm");
    // Every word a bot says is a word a human's call is read from.
    for (std::uint8_t n = 0; n < NODE_COUNT; ++n)
    {
        BgCallout::Report const c = BgCallout::Parse(std::string("inc ") + NodeWord(n), BgCallout::MapKind::AB);
        CHECK(c.valid && c.place == n);
    }
}

TEST(ab_target_is_kept_while_nearly_as_good_and_changes_when_clearly_worse)
{
    std::vector<TargetScore> sorted = { { NODE_FARM, 240 }, { NODE_GOLD_MINE, 225 }, { NODE_LUMBER_MILL, 100 } };
    CHECK(StickyTarget(sorted, NODE_NONE) == NODE_FARM);
    CHECK(StickyTarget(sorted, NODE_FARM) == NODE_FARM);
    CHECK(StickyTarget(sorted, NODE_GOLD_MINE) == NODE_GOLD_MINE);    // 15 points behind: within the margin
    CHECK(StickyTarget(sorted, NODE_LUMBER_MILL) == NODE_FARM);       // far behind: changes
    CHECK(StickyTarget(sorted, NODE_BLACKSMITH) == NODE_FARM);        // no longer a target
    CHECK(StickyTarget({}, NODE_FARM) == NODE_NONE);

    // A big score gives a bigger margin (20 percent of the best).
    std::vector<TargetScore> big = { { NODE_FARM, 3400 }, { NODE_GOLD_MINE, 2800 } };
    CHECK(StickyTarget(big, NODE_GOLD_MINE) == NODE_GOLD_MINE);
}

TEST(ab_responders_keep_their_answer_with_the_sticky_bonus)
{
    // Two reserve bots, one slightly nearer: without the bonus the nearer one wins the single place; with the bonus the
    // bot that answered before keeps it.
    std::vector<ResponderCandidate> all = { { 1, Job::MOBILE, NODE_NONE, 20000 }, { 2, Job::MOBILE, NODE_NONE, 24000 } };
    CHECK(ChooseResponders(all, NODE_BLACKSMITH, 1, RESPONDER_MAX_ETA_MS, 0)[0] == 1);
    all[1].etaMs -= RESPONDER_STICKY_MS;
    CHECK(ChooseResponders(all, NODE_BLACKSMITH, 1, RESPONDER_MAX_ETA_MS, 0)[0] == 2);
}

TEST(ab_team_intel_requires_a_living_observer_who_can_detect_the_enemy)
{
    struct Observer { bool alive; float x; bool visible; };
    std::vector<Observer> observers;
    auto seen = [&](bool enemyAlive) {
        return SeenByTeam(enemyAlive, observers,
            [](Observer const& o) { return o.alive; },
            [](Observer const& o) { return SeenFrom(0.0f, 0.0f, o.x, 0.0f); },
            [](Observer const& o) { return o.visible; });
    };
    CHECK(!seen(true));
    observers = {{false, 1.0f, true}, {true, 71.0f, true}, {true, 10.0f, false}};
    CHECK(!seen(true));
    observers.push_back({true, 70.0f, true});
    CHECK(seen(true));
    CHECK(!seen(false));
    CHECK(WithinSight(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 70.0f));
    CHECK(!WithinSight(0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 70.0f));
}
