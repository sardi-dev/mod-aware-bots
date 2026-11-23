#include "TestHarness.h"
#include "AbPlanLogic.h"
#include "BgNodeMap.h"
#include "BfgPlanLogic.h"

// BT-551: the node maps as data (docs/bg-sheets/arathi-basin.md, battle-for-gilneas.md): the Arathi Basin row reproduces the
// numbers the Arathi code always had, the Battle for Gilneas row the core's, and the generic functions read them.

using namespace AbLogic;

TEST(node_map_arathi_row_is_what_the_arathi_code_always_used)
{
    BgNodeMap::MapData const& m = BgNodeMap::ARATHI;
    CHECK(m.mapId == 529 && m.nodeCount == 5 && m.maxScore == 1600 && MAX_SCORE == 1600);
    CHECK(m.tickIntervalMs[1] == 12000 && m.tickIntervalMs[2] == 9000 && m.tickIntervalMs[3] == 6000);
    CHECK(m.tickIntervalMs[4] == 3000 && m.tickIntervalMs[5] == 1000);
    CHECK(m.tickPoints[4] == 10 && m.tickPoints[5] == 30);
    CHECK(HomeNode(TEAM_IDX_ALLIANCE) == NODE_STABLES && HomeNode(TEAM_IDX_HORDE) == NODE_FARM);
    CHECK(m.hub == NODE_BLACKSMITH);
    CHECK(std::string(NodeWord(NODE_LUMBER_MILL)) == "lm" && std::string(NodeWord(NODE_GOLD_MINE)) == "gm");
    CHECK(std::string(NodeWord(7)) == "?");
    CHECK_NEAR(NODE_POS[NODE_BLACKSMITH].x, 977.0156, 0.001);
    CHECK_NEAR(PointsPerSecond(5), 30.0, 0.001);
    CHECK_NEAR(PointsPerSecond(4), 3.333, 0.001);
    CHECK(NearExitPortal(1394.0f, 1303.0f));
    CHECK(!NearExitPortal(1000.0f, 1000.0f));
}

TEST(node_map_gilneas_row_is_what_the_core_has)
{
    BgNodeMap::MapData const& m = BgNodeMap::GILNEAS;
    CHECK(m.mapId == 761 && m.nodeCount == 3 && m.maxScore == 2000);
    CHECK(m.tickIntervalMs[1] == 12000 && m.tickIntervalMs[2] == 6000 && m.tickIntervalMs[3] == 1000);
    CHECK(m.tickPoints[1] == 10 && m.tickPoints[2] == 10 && m.tickPoints[3] == 30);
    CHECK(HomeNode(TEAM_IDX_ALLIANCE, m) == 0 && HomeNode(TEAM_IDX_HORDE, m) == 2);
    CHECK(m.hub == 1);
    CHECK(std::string(NodeWord(0, m)) == "lh" && std::string(NodeWord(1, m)) == "ww" && std::string(NodeWord(2, m)) == "mines");
    CHECK(std::string(NodeWord(3, m)) == "?");   // beyond the map's nodes
    CHECK_NEAR(m.pos[1].x, 980.0446, 0.001);
    CHECK_NEAR(m.pos[2].y, 958.2685, 0.001);
    // The map is found by its id.
    CHECK(BgNodeMap::ByMapId(761) == &BgNodeMap::GILNEAS && BgNodeMap::ByMapId(529) == &BgNodeMap::ARATHI);
    CHECK(BgNodeMap::ByMapId(489) == nullptr);
    CHECK(!NearExitPortal(1394.0f, 1303.0f, m));   // no exit portals in this script
}

TEST(node_map_rates_etas_and_swings_follow_the_map)
{
    BgNodeMap::MapData const& g = BgNodeMap::GILNEAS;
    CHECK_NEAR(PointsPerSecond(1, g), 0.8333, 0.001);
    CHECK_NEAR(PointsPerSecond(2, g), 1.6667, 0.001);
    CHECK_NEAR(PointsPerSecond(3, g), 30.0, 0.001);
    CHECK_NEAR(PointsPerSecond(9, g), 30.0, 0.001);   // never above the map's nodes
    CHECK(EtaSeconds(0, 3, g) == 67);
    CHECK(EtaSeconds(0, 2, g) == 1200);
    CHECK(EtaSeconds(0, 0, g) == ETA_NEVER);
    CHECK(EtaSeconds(1990, 3, g) == 1);
    // Arathi Basin keeps its own table.
    CHECK(EtaSeconds(0, 5) == 54);
    CHECK_NEAR(SwingValue(2, 1, SwingKind::TAKE_ENEMY, g), 29.167, 0.01);
    CHECK_NEAR(SwingValue(3, 2, SwingKind::TAKE_ENEMY), 1.944, 0.01);   // the Arathi sheet's 3 / 2 flip
}

TEST(node_map_regimes_follow_the_map_thresholds)
{
    BgNodeMap::MapData const& g = BgNodeMap::GILNEAS;
    RaceInput in;
    in.map = &g;

    // Two nodes held: the third node ends the match, the close-out regime (the sheet's THIRD).
    in.ourNodes = 2;
    in.theirNodes = 1;
    CHECK(ChooseRegime(in, Regime::EVEN) == Regime::CLOSE_OUT);

    // All three held: hold, whatever the clock says (the rate is 30 a second).
    in.ourNodes = 3;
    in.theirNodes = 0;
    in.ourScore = 0;
    in.theirScore = 1500;
    CHECK(ChooseRegime(in, Regime::EVEN) == Regime::HOLD);

    // One node against two: the last node.
    in.ourNodes = 1;
    in.theirNodes = 2;
    in.ourScore = 300;
    in.theirScore = 300;
    CHECK(ChooseRegime(in, Regime::EVEN) == Regime::LAST_NODE);
    // Nothing against two as well (the whole team goes for a node).
    in.ourNodes = 0;
    CHECK(ChooseRegime(in, Regime::EVEN) == Regime::LAST_NODE);
    // The opening: one node each is normal.
    in.ourNodes = 1;
    in.theirNodes = 1;
    in.ourScore = in.theirScore = 0;
    CHECK(ChooseRegime(in, Regime::EVEN) == Regime::EVEN);

    // The enemy at three nodes with the score near the end: panic.
    in.ourNodes = 0;
    in.theirNodes = 3;
    in.ourScore = 0;
    in.theirScore = 1500;
    CHECK(ChooseRegime(in, Regime::EVEN) == Regime::PANIC);

    // Arathi Basin is unchanged: four of five close it out, three of five hold only ahead on the clock.
    RaceInput ab;
    ab.ourNodes = 4;
    CHECK(ChooseRegime(ab, Regime::EVEN) == Regime::CLOSE_OUT);
    ab.ourNodes = 3;
    ab.theirNodes = 2;
    ab.ourScore = 800;
    ab.theirScore = 800;
    CHECK(ChooseRegime(ab, Regime::EVEN) == Regime::HOLD);   // 120 s ahead on the clock
    ab.theirScore = 1300;
    CHECK(ChooseRegime(ab, Regime::EVEN) == Regime::PUSH);   // behind on the clock
    ab.ourScore = ab.theirScore = 0;
    ab.ourNodes = 1;
    ab.theirNodes = 3;
    CHECK(ChooseRegime(ab, Regime::EVEN) == Regime::LAST_NODE);
    ab.ourNodes = 0;
    ab.theirNodes = 3;
    CHECK(ChooseRegime(ab, Regime::EVEN) != Regime::LAST_NODE);
}

TEST(node_map_targets_stay_inside_the_maps_nodes)
{
    BgNodeMap::MapData const& g = BgNodeMap::GILNEAS;
    std::vector<TargetInput> inputs;
    for (std::uint8_t n = 0; n < 5; ++n)
    {
        TargetInput t;
        t.node = n;
        t.sit = NodeSituation::NEUTRAL;
        t.etaS = 30.0f;
        inputs.push_back(t);
    }
    // A caller that listed more nodes than the map has: the extra ones are not targets.
    std::vector<TargetScore> const scored = ScoreTargets(inputs, 2, 0, 5, Tier::GOOD, g);
    CHECK(scored.size() == 3);
    for (TargetScore const& t : scored)
        CHECK(t.node < 3);
    CHECK(NearestTarget(inputs, g) < 3);

    NodeSituation sit[NODE_COUNT] = { NodeSituation::OURS, NodeSituation::OURS, NodeSituation::NEUTRAL, NodeSituation::NEUTRAL,
                                      NodeSituation::NEUTRAL };
    CHECK(CloseOutNode(sit, g) == 2);   // two of three ours: the third; the nodes beyond the map do not count
    sit[2] = NodeSituation::OURS;
    CHECK(CloseOutNode(sit, g) == NODE_NONE);
}

TEST(node_map_node_near_and_distance_use_the_maps_positions)
{
    BgNodeMap::MapData const& g = BgNodeMap::GILNEAS;
    CHECK(NodeNear(980.0f, 948.0f, 10.0f, g) == 1);
    CHECK(NodeNear(980.0f, 948.0f, 10.0f) == NODE_NONE);   // Arathi Basin has no node there
    CHECK(DistToNode(3, 0.0f, 0.0f, g) > 99999.0f);
    CHECK(BfgLogic::NODE_POS[1].x == g.pos[1].x && BfgLogic::NODE_COUNT == 3);
}
