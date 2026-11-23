#include "TestHarness.h"
#include "AbPlanLogic.h"
#include "BgNodeMap.h"
#include "EyPlanLogic.h"

// BT-582: Eye of the Storm as the third row of the node-map table (docs/bg-sheets/eye-of-the-storm.md): the presence capture
// model, the two near bases, the hold state and the fourth base. The Arathi Basin and Battle for Gilneas tests are the guard of
// the shared rules (test_ab_plan.cpp, test_bfg_plan.cpp, test_bg_node_map.cpp).

using namespace AbLogic;

namespace
{
Map const& E = BgNodeMap::EYE;

Candidate Cand(std::uint32_t guid, std::uint8_t cls, std::uint8_t level = 60, bool healer = false)
{
    Candidate c;
    c.guidLow = guid;
    c.cls = cls;
    c.level = level;
    c.healer = healer;
    c.tier = Tier::GOOD;
    c.x = 2523.0f;
    c.y = 1596.0f;
    return c;
}

unsigned CountSlots(std::vector<Slot> const& slots, Job job, std::uint8_t node = NODE_NONE)
{
    unsigned n = 0;
    for (Slot const& s : slots)
        if (s.job == job && (node == NODE_NONE || s.node == node))
            ++n;
    return n;
}

SlotInput Input(Plan plan, std::uint32_t size)
{
    SlotInput in;
    in.map = &E;
    in.team = TEAM_IDX_ALLIANCE;
    in.plan = plan;
    in.size = size;
    for (std::uint8_t n = 0; n < 5; ++n)
        in.sit[n] = NodeSituation::NEUTRAL;
    return in;
}
}  // namespace

TEST(ey_row_is_what_the_core_has)
{
    CHECK(E.mapId == 566 && E.nodeCount == 4 && E.maxScore == 1600 && E.kind == BgNodeMap::Kind::EYE);
    CHECK(E.captureModel == BgNodeMap::CaptureModel::PRESENCE && IsPresence(E) && !IsPresence(BgNodeMap::ARATHI));
    for (int i = 1; i <= 4; ++i)
    {
        CHECK(E.tickIntervalMs[i] == 2000);
        CHECK(E.tickPoints[i] == EyLogic::TICK_POINTS[i]);
        CHECK(E.flagPoints[i] == EyLogic::FLAG_POINTS[i]);
    }
    CHECK(E.circleRadius == EyLogic::CIRCLE_RADIUS && E.maxScore == EyLogic::MAX_SCORE);
    for (int b = 0; b < 4; ++b)
    {
        CHECK_NEAR(E.pos[b].x, EyLogic::TRIGGER_POS[b].x, 0.001);
        CHECK_NEAR(E.pos[b].y, EyLogic::TRIGGER_POS[b].y, 0.001);
        CHECK_NEAR(E.cap[b].x, EyLogic::CAP_POS[b].x, 0.001);
        CHECK_NEAR(E.cap[b].z, EyLogic::CAP_POS[b].z, 0.001);
    }
    CHECK_NEAR(E.centre.x, EyLogic::CENTRE_FLAG.x, 0.001);
    CHECK_NEAR(E.start[0].x, EyLogic::START_ROOM[0].x, 0.001);
    CHECK(BgNodeMap::ByMapId(566) == &BgNodeMap::EYE);
    CHECK(std::string(NodeWord(2, E)) == "dr" && std::string(NodeWord(3, E)) == "mt" && std::string(NodeWord(4, E)) == "?");
    // The older rows are untouched by the new fields.
    CHECK(!IsPresence(BgNodeMap::GILNEAS) && BgNodeMap::ARATHI.circleRadius == 0.0f && BgNodeMap::GILNEAS.holdAtNodes == 0);
}

TEST(ey_near_nodes_are_two_per_team_and_the_home_is_the_first)
{
    CHECK(IsNearNode(TEAM_IDX_ALLIANCE, 3, E) && IsNearNode(TEAM_IDX_ALLIANCE, 2, E) && !IsNearNode(TEAM_IDX_ALLIANCE, 0, E));
    CHECK(IsNearNode(TEAM_IDX_HORDE, 0, E) && IsNearNode(TEAM_IDX_HORDE, 1, E) && !IsNearNode(TEAM_IDX_HORDE, 3, E));
    CHECK(HomeNode(TEAM_IDX_ALLIANCE, E) == 3 && HomeNode(TEAM_IDX_HORDE, E) == 0);
    // The click maps repeat their one near node.
    CHECK(IsNearNode(TEAM_IDX_ALLIANCE, 0, BgNodeMap::GILNEAS) && !IsNearNode(TEAM_IDX_ALLIANCE, 1, BgNodeMap::GILNEAS));
}

TEST(ey_circle_is_the_cap_objects_70_yards)
{
    CHECK(InCircle(3, 2284.0f, 1731.0f, 1190.0f, E));
    CHECK(!InCircle(3, 2284.0f, 1835.0f, 1190.0f, E));
    CHECK(!InCircle(3, 2284.0f, 1731.0f, 1190.0f, BgNodeMap::ARATHI));   // no circle on a click map
}

TEST(ey_rates_etas_and_swings_follow_the_table)
{
    CHECK_NEAR(PointsPerSecond(1, E), 0.5, 0.001);
    CHECK_NEAR(PointsPerSecond(2, E), 1.0, 0.001);
    CHECK_NEAR(PointsPerSecond(3, E), 2.5, 0.001);
    CHECK_NEAR(PointsPerSecond(4, E), 5.0, 0.001);
    CHECK(EtaSeconds(0, 4, E) == 320 && EtaSeconds(0, 3, E) == 640 && EtaSeconds(0, 0, E) == ETA_NEVER);
    CHECK_NEAR(SwingValue(2, 1, SwingKind::TAKE_FREE, E), 1.5, 0.001);     // the third base
    CHECK_NEAR(SwingValue(3, 1, SwingKind::TAKE_ENEMY, E), 3.0, 0.001);    // the fourth, flipped from their last
    CHECK_NEAR(SwingValue(3, 0, SwingKind::TAKE_FREE, E), 2.5, 0.001);
    CHECK_NEAR(SwingValue(1, 3, SwingKind::DENY, E), 1.5, 0.001);
}

TEST(ey_presence_situations_have_no_contest_state)
{
    CHECK(ClassifyPresence(0, 0, TEAM_IDX_ALLIANCE) == NodeSituation::NEUTRAL);
    CHECK(ClassifyPresence(3, 0, TEAM_IDX_ALLIANCE) == NodeSituation::OURS);
    CHECK(ClassifyPresence(4, 0, TEAM_IDX_ALLIANCE) == NodeSituation::THEIRS);
    CHECK(ClassifyPresence(3, 0, TEAM_IDX_HORDE) == NodeSituation::THEIRS);
    // A base that was ours and is neutral now was neutralised: retake it.
    CHECK(ClassifyPresence(0, 3, TEAM_IDX_ALLIANCE) == NodeSituation::UNDER_ASSAULT);
    CHECK(ClassifyPresence(0, 3, TEAM_IDX_HORDE) == NodeSituation::NEUTRAL);
    // The dispatcher picks the model by the map.
    CHECK(ClassifyFor(E, 0, 3, TEAM_IDX_ALLIANCE) == NodeSituation::UNDER_ASSAULT);
    CHECK(ClassifyFor(BgNodeMap::ARATHI, 2, 0, TEAM_IDX_ALLIANCE) == Classify(2, 0, TEAM_IDX_ALLIANCE));
}

TEST(ey_opening_counts_are_squads_of_three_at_the_near_bases)
{
    OpeningCounts const o = EyeOpening(Plan::BALANCED, TEAM_IDX_ALLIANCE, 15, 3, E);
    CHECK(o.byNode[3] == 3 && o.byNode[2] == 3 && o.byNode[0] == 0 && o.byNode[1] == 0 && o.raiders == 0);
    OpeningCounts const d = EyeOpening(Plan::DEFENSE, TEAM_IDX_HORDE, 15, 0, E);
    CHECK(d.byNode[0] == 4 && d.byNode[1] == 4);
    OpeningCounts const smallDefense = EyeOpening(Plan::DEFENSE, TEAM_IDX_HORDE, 10, 0, E);
    CHECK(smallDefense.byNode[0] == 3 && smallDefense.byNode[1] == 3);
    OpeningCounts const off = EyeOpening(Plan::OFFENSE, TEAM_IDX_ALLIANCE, 15, 2, E);
    CHECK(off.raiders == 2 && off.byNode[3] == 3);
    CHECK(EyeOpening(Plan::OFFENSE, TEAM_IDX_ALLIANCE, 10, 4, E).raiders == 0);      // raiders from 12 players
    CHECK(EyeOpening(Plan::OFFENSE, TEAM_IDX_ALLIANCE, 15, 1, E).raiders == 0);      // the pair needs two stealth bots
    // Five bots take one base, two take none: a squad is 3 or more.
    OpeningCounts const five = EyeOpening(Plan::BALANCED, TEAM_IDX_ALLIANCE, 5, 0, E);
    CHECK(five.byNode[3] == 3 && five.byNode[2] == 0);
    CHECK(EyeOpening(Plan::BALANCED, TEAM_IDX_ALLIANCE, 2, 0, E).Total() == 0);
}

TEST(ey_slots_at_the_opening_are_two_squads_the_middle_and_the_reserve)
{
    SlotInput in = Input(Plan::BALANCED, 15);
    std::vector<Slot> const slots = BuildSlots(in);
    CHECK(slots.size() == 15);
    CHECK(CountSlots(slots, Job::ASSAULT, 3) == 3 && CountSlots(slots, Job::ASSAULT, 2) == 3);
    CHECK(CountSlots(slots, Job::GUARD) == 0 && CountSlots(slots, Job::RAIDER) == 0);
    CHECK(CountSlots(slots, Job::ASSAULT, NODE_NONE) >= 6);
}

TEST(ey_a_fresh_base_keeps_a_squad_of_three_until_it_is_hardened)
{
    SlotInput in = Input(Plan::BALANCED, 15);
    in.sit[3] = NodeSituation::OURS;
    in.hardened[3] = false;
    CHECK(CountSlots(BuildSlots(in), Job::GUARD, 3) == 3);
    in.hardened[3] = true;
    CHECK(CountSlots(BuildSlots(in), Job::GUARD, 3) == 1);       // the pinned defender
    in.plan = Plan::DEFENSE;
    CHECK(CountSlots(BuildSlots(in), Job::GUARD, 3) == 2);       // two pinned in the defense plan
}

TEST(ey_hold_state_guards_two_per_held_base_and_three_where_the_enemy_is)
{
    SlotInput in = Input(Plan::BALANCED, 15);
    in.regime = Regime::HOLD;
    in.sit[3] = in.sit[2] = in.sit[1] = NodeSituation::OURS;
    std::vector<Slot> const slots = BuildSlots(in);
    CHECK(CountSlots(slots, Job::GUARD, 3) == 2 && CountSlots(slots, Job::GUARD, 2) == 2 && CountSlots(slots, Job::GUARD, 1) == 2);
    in.enemyNear50[1] = 4;
    CHECK(CountSlots(BuildSlots(in), Job::GUARD, 1) == 3);
    // With everything held the free bots are the flag group (all but the reserve), never a take.
    CHECK(AssaultCount(Plan::BALANCED, Regime::HOLD, 9, E) == 7 && AssaultCount(Plan::DEFENSE, Regime::HOLD, 9, E) == 6);
    CHECK(AssaultCount(Plan::BALANCED, Regime::HOLD, 1, E) == 0);
}

TEST(ey_guard_order_starts_at_the_base_nearest_the_enemy_start_room)
{
    std::vector<std::uint8_t> const alliance = GuardOrder(TEAM_IDX_ALLIANCE, E);
    CHECK(alliance.size() == 4 && alliance[0] == 1 && alliance[1] == 0);   // Blood Elf (286 yd from the Horde) then Fel Reaver (307)
    std::vector<std::uint8_t> const horde = GuardOrder(TEAM_IDX_HORDE, E);
    CHECK(horde[0] == 3 && horde[1] == 2);                                 // Mage Tower (275 yd from the Alliance) then Draenei Ruins (306)
}

TEST(ey_regimes_close_out_at_three_bases_only_when_the_odds_allow_else_hold)
{
    RaceInput race;
    race.map = &E;
    race.ourScore = 400;
    race.theirScore = 400;
    race.ourNodes = 3;
    race.theirNodes = 1;
    CHECK(ChooseRegime(race, Regime::EVEN) == Regime::CLOSE_OUT);
    race.closeOutOk = false;
    CHECK(ChooseRegime(race, Regime::EVEN) == Regime::HOLD);      // hold from 3 bases whatever the clock says
    race.ourNodes = 4;
    race.theirNodes = 0;
    CHECK(ChooseRegime(race, Regime::EVEN) == Regime::HOLD);
    race.ourNodes = 2;
    race.theirNodes = 2;
    race.closeOutOk = true;
    CHECK(ChooseRegime(race, Regime::EVEN) == Regime::EVEN);
    race.ourNodes = 1;
    race.theirNodes = 3;
    CHECK(ChooseRegime(race, Regime::EVEN) == Regime::LAST_NODE);
    // The click maps do not know the new field.
    RaceInput gil;
    gil.map = &BgNodeMap::GILNEAS;
    gil.ourNodes = 2;
    gil.closeOutOk = false;
    CHECK(ChooseRegime(gil, Regime::EVEN) == Regime::EVEN);
}

TEST(ey_near_base_slots_go_to_the_best_kit_and_a_sticky_bot_keeps_its_base)
{
    // A rogue, a mage and warriors: the squad slots at the near bases take the kits first (the pinned defenders).
    std::vector<Candidate> cands = { Cand(1, CLS_WARRIOR), Cand(2, CLS_ROGUE), Cand(3, CLS_WARRIOR), Cand(4, CLS_MAGE),
                                     Cand(5, CLS_WARRIOR), Cand(6, CLS_WARRIOR) };
    SlotInput in = Input(Plan::BALANCED, 6);
    std::vector<Assignment> const a = Assign(cands, BuildSlots(in), TEAM_IDX_ALLIANCE, E);
    std::uint8_t rogueNode = NODE_NONE, mageNode = NODE_NONE;
    for (Assignment const& x : a)
    {
        if (x.guidLow == 2)
            rogueNode = x.slot.node;
        if (x.guidLow == 4)
            mageNode = x.slot.node;
    }
    CHECK(rogueNode != NODE_NONE && mageNode != NODE_NONE && rogueNode != mageNode);

    // After the capture the squad's bot stays as the guard even when a rogue stands in the middle.
    for (Candidate& c : cands)
    {
        c.hasPrevious = true;
        c.previous = { Job::ASSAULT, 3 };
        c.x = 2284.0f;
        c.y = 1731.0f;
    }
    cands[1].previous = { Job::ASSAULT, NODE_NONE };   // the rogue is with the middle group, far away
    cands[1].x = 2174.0f;
    cands[1].y = 1569.0f;
    SlotInput held = Input(Plan::BALANCED, 6);
    held.sit[3] = NodeSituation::OURS;
    held.hardened[3] = true;
    std::vector<Assignment> const b = Assign(cands, BuildSlots(held), TEAM_IDX_ALLIANCE, E);
    for (Assignment const& x : b)
        if (x.slot.job == Job::GUARD && x.slot.node == 3)
            CHECK(x.guidLow != 2);
}

TEST(ey_raiders_go_only_to_a_base_with_no_defender_seen)
{
    std::vector<RaidInput> inputs = { { 0, NodeSituation::THEIRS, 1 }, { 1, NodeSituation::THEIRS, 0 } };
    CHECK(PickRaidNode(inputs, TEAM_IDX_ALLIANCE, Tier::EXPERT, E) == 1);
    inputs = { { 0, NodeSituation::THEIRS, 1 } };
    CHECK(PickRaidNode(inputs, TEAM_IDX_ALLIANCE, Tier::EXPERT, E) == NODE_NONE);
    // The click maps keep the one-defender expert raid.
    std::vector<RaidInput> one = { { 0, NodeSituation::THEIRS, 1 } };
    CHECK(PickRaidNode(one, TEAM_IDX_ALLIANCE, Tier::EXPERT, BgNodeMap::GILNEAS) == 0);
}

TEST(ey_intel_is_seen_only_and_the_squad_waits_outside_the_circle)
{
    CHECK(SeenOnlyIntel(E) && SeenOnlyIntel(BgNodeMap::GILNEAS) && !SeenOnlyIntel(BgNodeMap::ARATHI));
    CHECK_NEAR(GatherDistance(E), 100.0, 0.001);
    CHECK_NEAR(GatherDistance(BgNodeMap::ARATHI), GATHER_DISTANCE, 0.001);
    CHECK(StayAfterCastMs(E) > 60000 && StayAfterCastMs(BgNodeMap::GILNEAS) == CAPTURE_TIME_MS);
}
