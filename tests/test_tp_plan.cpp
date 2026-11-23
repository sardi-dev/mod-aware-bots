#include "TestHarness.h"
#include "TpPlanLogic.h"
#include "WsgPlanLogic.h"

// Twin Peaks playbook logic (docs/bg-sheets/twin-peaks.md, owner approved 2026-11-18): BT-471 the map data, and in later
// steps the lanes, the respawn model and the wipe states.

using namespace TpLogic;

namespace
{
float Dist(BgFlagMap::Pos const& a, BgFlagMap::Pos const& b)
{
    float const dx = a.x - b.x;
    float const dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}
}  // namespace

TEST(tp_data_stands_graveyards_and_middle_are_the_cores)
{
    BgFlagMap::MapData const& d = Data();
    CHECK(d.mapId == 726);
    // Flag stands (BattlegroundTP.cpp:708-709), middle graveyards (WorldSafeLocs 1749 / 1750), the river centre.
    CHECK_NEAR(d.sides[0].standWalk.x, 2118.210, 0.001);
    CHECK_NEAR(d.sides[0].standWalk.z, 44.052, 0.001);
    CHECK_NEAR(d.sides[1].standWalk.x, 1578.380, 0.001);
    CHECK_NEAR(d.sides[1].standWalk.z, 2.419, 0.001);
    CHECK_NEAR(d.sides[0].mainGraveyard.x, 1877.150, 0.001);
    CHECK_NEAR(d.sides[0].mainGraveyard.y, 439.111, 0.001);
    CHECK_NEAR(d.sides[1].mainGraveyard.x, 1816.870, 0.001);
    CHECK_NEAR(d.sides[1].mainGraveyard.y, 160.083, 0.001);
    CHECK_NEAR(d.mid.x, 1840.0, 0.001);
    CHECK_NEAR(d.mid.z, -24.945, 0.001);
    CHECK(d.roofAboveStand == 0.0f);
}

TEST(tp_data_the_stands_are_561_yards_apart_and_the_graveyards_285)
{
    BgFlagMap::MapData const& d = Data();
    // Sheet section 2: stand to stand 561 yd, middle graveyard to middle graveyard 285 yd.
    CHECK_NEAR(Dist(d.sides[0].standWalk, d.sides[1].standWalk), 561.0, 1.5);
    CHECK_NEAR(Dist(d.sides[0].mainGraveyard, d.sides[1].mainGraveyard), 285.0, 1.5);
}

TEST(tp_data_posts_and_hide_spots_lie_inside_the_own_base)
{
    BgFlagMap::MapData const& d = Data();
    for (int t = 0; t < 2; ++t)
    {
        BgFlagMap::Side const& side = d.sides[t];
        for (BgFlagMap::Pos const& post : side.post)
            CHECK(Dist(post, side.standWalk) < 50.0f);
        for (BgFlagMap::Pos const& hide : side.hide)
            CHECK(Dist(hide, side.standWalk) < 50.0f);
        for (bool usable : side.hideUsable)
            CHECK(usable);
    }
}

TEST(tp_data_each_strike_group_gathers_65_to_90_yards_outside_the_enemy_base)
{
    BgFlagMap::MapData const& d = Data();
    for (int t = 0; t < 2; ++t)
    {
        // The gather point of a base is where the ENEMY group stops: outside the entrance posts, not at the stand.
        BgFlagMap::Side const& base = d.sides[t];
        float nearestPost = 1.0e9f;
        for (BgFlagMap::Pos const& post : base.post)
            nearestPost = std::min(nearestPost, Dist(post, base.gather));
        CHECK(nearestPost > 60.0f && nearestPost < 90.0f);
        CHECK(Dist(base.gather, base.standWalk) > 80.0f);
    }
}

TEST(tp_data_the_strike_groups_ride_opposite_lanes)
{
    BgFlagMap::MapData const& d = Data();
    // The Alliance strike group arrives at the Horde base from the lane of the Horde graveyard (south, y under the
    // river centre); the Horde group arrives at the Alliance base from the north lane (sheet section 2, the loop).
    CHECK(d.sides[1].gather.y < d.mid.y);
    // At the Alliance plateau the lane H approach runs along y 234.7 (vPath_TP_AllianceFlagRoom_to_HordeFlagRoom) and the
    // lane A approach comes in above it (vPath_TP_HordeFlagRoom_to_AllianceFlagRoom).
    CHECK(d.sides[0].gather.y > 234.7f);
    // The Horde graveyard stands on the south lane, the Alliance one on the north lane.
    CHECK(d.sides[1].mainGraveyard.y < d.mid.y);
    CHECK(d.sides[0].mainGraveyard.y > d.mid.y);
}

TEST(tp_data_the_graveyard_hop_lands_within_the_island_hop_radius)
{
    BgFlagMap::MapData const& d = Data();
    for (int t = 0; t < 2; ++t)
    {
        BgFlagMap::Side const& side = d.sides[t];
        CHECK(Dist(side.tunnelExit, side.mainGraveyard) < WsgLogic::GY_EXIT_HOP_RADIUS);
        // A bot just respawned at its graveyard with its objective far away hops first; one already at the exit does not.
        CHECK(WsgLogic::NeedsGraveyardExitHop(2.0f, 300.0f, 300.0f, false));
        CHECK(!WsgLogic::NeedsGraveyardExitHop(2.0f, 300.0f, 5.0f, false));
        CHECK(!WsgLogic::NeedsGraveyardExitHop(2.0f, 300.0f, 300.0f, true));
    }
}

TEST(tp_clock_counts_27_minutes_and_reads_a_30_minute_core_live)
{
    BgFlagMap::Clock const& clock = Data().clock;
    CHECK(clock.limitMs == 27u * 60000u);
    CHECK(clock.legacyLimitMs == 30u * 60000u);
    // Before and after the core change (BT-461): the same answer up to 27 minutes, then the live core decides.
    CHECK(BgFlagMap::LeftMs(10u * 60000u, clock) == 17 * 60000);
    CHECK(BgFlagMap::LeftMs(28u * 60000u, clock) == 2 * 60000);
}

// ---------------------------------------------------------------------------------------------------------------------
// BT-473: the respawn model (sheet gap G5, owner answer 1a).
// ---------------------------------------------------------------------------------------------------------------------

namespace
{
BgFlagMap::Pos Near(BgFlagMap::Pos const& p, float dx = 3.0f, float dy = -2.0f)
{
    return { p.x + dx, p.y + dy, p.z };
}
}  // namespace

TEST(tp_respawn_classifies_a_bot_by_the_graveyard_it_stands_at)
{
    for (std::uint32_t t = 0; t < 2; ++t)
    {
        RespawnPoints const points = RespawnPointsOf(t);
        CHECK(ClassifyRespawn(Near(points.middle), points) == RespawnSide::MIDDLE);
        CHECK(ClassifyRespawn(Near(points.start), points) == RespawnSide::START);
        // Mid-lane or in the flag room: neither graveyard.
        CHECK(ClassifyRespawn(Data().mid, points) == RespawnSide::UNKNOWN);
        CHECK(ClassifyRespawn(Data().sides[t].standWalk, points) == RespawnSide::UNKNOWN);
    }
}

TEST(tp_respawn_points_are_the_core_graveyards)
{
    RespawnPoints const alliance = RespawnPointsOf(0);
    RespawnPoints const horde = RespawnPointsOf(1);
    CHECK_NEAR(alliance.middle.x, 1877.15, 0.01);   // WorldSafeLocs 1749
    CHECK_NEAR(horde.middle.x, 1816.87, 0.01);      // 1750
    CHECK_NEAR(alliance.start.x, 2168.10, 0.01);    // 1729
    CHECK_NEAR(horde.start.x, 1561.16, 0.01);       // 1728
}

TEST(tp_respawn_run_back_matches_the_sheets_distances)
{
    // Sheet section 2: own middle graveyard to own stand 345 / 301 yd, own start graveyard to own stand 149 / 132 yd.
    for (std::uint32_t t = 0; t < 2; ++t)
    {
        RespawnPoints const points = RespawnPointsOf(t);
        BgFlagMap::Pos const& stand = Data().sides[t].standWalk;
        CHECK_NEAR(RunBackYards(points, RespawnSide::MIDDLE, stand), t == 0 ? 345.0 : 301.0, 2.0);
        CHECK_NEAR(RunBackYards(points, RespawnSide::START, stand), t == 0 ? 149.0 : 132.0, 2.0);
    }
    // 7 yd/s on foot, twice that mounted: a dead Alliance defender is about 49 s from the room, 25 s mounted.
    CHECK_NEAR(RunBackSeconds(345.0f, false), 49.3, 0.1);
    CHECK_NEAR(RunBackSeconds(345.0f, true), 24.6, 0.1);
}

TEST(tp_respawn_rule_assumes_the_core_until_it_has_seen_a_respawn)
{
    RespawnRule rule;
    CHECK(rule.Current() == RespawnSide::MIDDLE);
    // A read that matched no graveyard teaches nothing.
    rule.Record(RespawnSide::UNKNOWN);
    CHECK(rule.Current() == RespawnSide::MIDDLE);
}

TEST(tp_respawn_rule_follows_the_majority_and_a_tie_goes_to_the_latest)
{
    RespawnRule rule;
    rule.Record(RespawnSide::START);
    CHECK(rule.Current() == RespawnSide::START);
    rule.Record(RespawnSide::MIDDLE);
    CHECK(rule.Current() == RespawnSide::MIDDLE);   // 1:1, the latest was the middle
    rule.Record(RespawnSide::START);
    CHECK(rule.Current() == RespawnSide::START);    // 2:1
    rule.Record(RespawnSide::MIDDLE);
    rule.Record(RespawnSide::MIDDLE);
    CHECK(rule.Current() == RespawnSide::MIDDLE);   // 3:2
}

TEST(tp_respawn_rule_forgets_old_respawns_after_a_rule_change)
{
    // The fork may move to the 4.3.4 rule between matches, never inside one, but a mixed start must settle fast.
    RespawnRule rule;
    for (int i = 0; i < 6; ++i)
        rule.Record(RespawnSide::MIDDLE);
    CHECK(rule.Current() == RespawnSide::MIDDLE);
    for (int i = 0; i < 4; ++i)
        rule.Record(RespawnSide::START);
    CHECK(rule.Current() == RespawnSide::START);   // the last 6 are 2 middle and 4 start
}

TEST(tp_respawn_point_follows_the_side)
{
    RespawnPoints const points = RespawnPointsOf(1);
    CHECK_NEAR(RespawnPoint(points, RespawnSide::MIDDLE).x, 1816.87, 0.01);
    CHECK_NEAR(RespawnPoint(points, RespawnSide::START).x, 1561.16, 0.01);
    CHECK_NEAR(RespawnPoint(points, RespawnSide::UNKNOWN).x, 1816.87, 0.01);   // unknown is read as the middle
}

// ---------------------------------------------------------------------------------------------------------------------
// BT-474: lanes, posts, the kill window, the wipe states and the decision lists (sheet gap G2).
// ---------------------------------------------------------------------------------------------------------------------

using WsgLogic::Kind;
using WsgLogic::Place;
using WsgLogic::Role;
using WsgLogic::Tier;

TEST(tp_lane_posts_are_the_buffs_of_the_core_script)
{
    // BattlegroundTP.cpp:713-716: the Alliance cuts at the lane H berserk and covers at the lane A restoration, the Horde the
    // other way round.
    CHECK_NEAR(LanePostsOf(0).cut.x, 1934.369, 0.001);
    CHECK_NEAR(LanePostsOf(0).cover.x, 1952.121, 0.001);
    CHECK_NEAR(LanePostsOf(1).cut.x, 1725.240, 0.001);
    CHECK_NEAR(LanePostsOf(1).cover.x, 1753.957, 0.001);
}

TEST(tp_cut_posts_lie_on_the_enemy_carriers_lane_180_to_190_yards_from_our_stand)
{
    // Sheet section 4: the enemy carrier passes the cut post about 190 yd (Alliance) or 180 yd (Horde) from our stand, and
    // the cover post is 250 / 200 yd from it.
    BgFlagMap::MapData const& d = Data();
    CHECK_NEAR(Dist(LanePostsOf(0).cut, d.sides[0].standWalk), 190.0, 8.0);
    CHECK_NEAR(Dist(LanePostsOf(1).cut, d.sides[1].standWalk), 180.0, 8.0);
    CHECK_NEAR(Dist(LanePostsOf(0).cover, d.sides[0].standWalk), 250.0, 8.0);
    CHECK_NEAR(Dist(LanePostsOf(1).cover, d.sides[1].standWalk), 200.0, 8.0);
}

TEST(tp_mid_posts_two_cut_and_one_cover_of_three)
{
    // No floater: the last of the list covers.
    CHECK(MidPostOf(false, 0, 3, false) == MidPost::CUT);
    CHECK(MidPostOf(false, 1, 3, false) == MidPost::CUT);
    CHECK(MidPostOf(false, 2, 3, false) == MidPost::COVER);
    // Two bots: one each. One bot: the cut post.
    CHECK(MidPostOf(false, 0, 2, false) == MidPost::CUT);
    CHECK(MidPostOf(false, 1, 2, false) == MidPost::COVER);
    CHECK(MidPostOf(false, 0, 1, false) == MidPost::CUT);
    CHECK(MidPostOf(false, 0, 0, false) == MidPost::CUT);
}

TEST(tp_mid_floater_always_covers_and_no_second_cover_is_added)
{
    CHECK(MidPostOf(true, 0, 3, true) == MidPost::COVER);
    CHECK(MidPostOf(true, 2, 3, true) == MidPost::COVER);
    // With a floater among the MID bots the last of the list stays at the cut post: still one cover bot.
    CHECK(MidPostOf(false, 2, 3, true) == MidPost::CUT);
}

TEST(tp_floater_is_the_mid_healer_that_does_not_follow_the_carrier)
{
    std::vector<FloaterCandidate> list = {
        { 101, true, true },     // a healer on the carrier: not the floater
        { 102, false, false },   // not a healer
        { 103, true, false },
        { 104, true, false },
    };
    std::uint32_t const pick = PickFloater(list);
    CHECK(pick == 103 || pick == 104);
    // Stable: the same answer in any order.
    std::reverse(list.begin(), list.end());
    CHECK(PickFloater(list) == pick);
    // Nobody to float.
    CHECK(PickFloater({ { 105, true, true }, { 106, false, false } }) == 0);
    CHECK(PickFloater({}) == 0);
}

TEST(tp_kill_window_phases_of_the_horde_carrier_on_lane_h)
{
    // The Horde carrier runs from the Alliance stand through lane H to its stand, past its own middle graveyard.
    BgFlagMap::MapData const& d = Data();
    Pos const stand = d.sides[1].standWalk;
    Pos const respawn = d.sides[1].mainGraveyard;
    // At the Alliance ramp, far from its graveyard: the window.
    CHECK(EfcPhaseOf({ 2069.0f, 234.7f, 44.0f }, stand, respawn) == EfcPhase::WINDOW);
    // On the berserk of lane H.
    CHECK(EfcPhaseOf({ 1934.0f, 226.0f, -17.0f }, stand, respawn) == EfcPhase::WINDOW);
    // Beside its own graveyard: its team joins every 30 s.
    CHECK(EfcPhaseOf({ 1829.0f, 171.0f, -4.0f }, stand, respawn) == EfcPhase::AT_GRAVEYARD);
    // Beyond it, toward its stand: the hunters stop.
    CHECK(EfcPhaseOf({ 1728.0f, 234.7f, -13.9f }, stand, respawn) == EfcPhase::PAST);
    CHECK(EfcPhaseOf({ 1632.0f, 271.5f, -5.0f }, stand, respawn) == EfcPhase::PAST);
}

TEST(tp_kill_window_phases_of_the_alliance_carrier_on_lane_a)
{
    BgFlagMap::MapData const& d = Data();
    Pos const stand = d.sides[0].standWalk;
    Pos const respawn = d.sides[0].mainGraveyard;
    CHECK(EfcPhaseOf({ 1628.0f, 426.7f, -7.0f }, stand, respawn) == EfcPhase::WINDOW);    // just out of the Horde base
    CHECK(EfcPhaseOf({ 1856.0f, 405.3f, -20.5f }, stand, respawn) == EfcPhase::AT_GRAVEYARD);
    CHECK(EfcPhaseOf({ 1954.0f, 384.8f, -9.9f }, stand, respawn) == EfcPhase::PAST);
    CHECK(EfcPhaseOf({ 2086.0f, 221.0f, 43.7f }, stand, respawn) == EfcPhase::PAST);
}

TEST(tp_kill_window_follows_the_start_graveyard_when_that_is_the_rule)
{
    // Under the 4.3.4 rule the carrier's team respawns 132 to 149 yd from its stand: the window is the whole run until the
    // carrier is near its own base.
    BgFlagMap::MapData const& d = Data();
    RespawnPoints const points = RespawnPointsOf(1);
    Pos const stand = d.sides[1].standWalk;
    Pos const respawn = RespawnPoint(points, RespawnSide::START);
    CHECK(EfcPhaseOf({ 1934.0f, 226.0f, -17.0f }, stand, respawn) == EfcPhase::WINDOW);
    CHECK(EfcPhaseOf({ 1800.0f, 190.0f, -17.0f }, stand, respawn) == EfcPhase::WINDOW);
    CHECK(EfcPhaseOf({ 1570.0f, 300.0f, 0.5f }, stand, respawn) == EfcPhase::PAST);
}

TEST(tp_wipe_counts_deaths_inside_15_seconds_and_inside_the_zones)
{
    std::vector<DeathMark> deaths = {
        { 100000, { 1900.0f, 230.0f, 0.0f } },
        { 105000, { 1905.0f, 228.0f, 0.0f } },
        { 112000, { 2110.0f, 190.0f, 44.0f } },     // at the Alliance stand
        { 80000, { 1900.0f, 230.0f, 0.0f } },       // too old at 115 s
        { 0, { 0.0f, 0.0f, 0.0f } },                // no death
    };
    CHECK(DeathsInWindow(deaths, 115000, {}) == 3);
    CHECK(AttackWiped(DeathsInWindow(deaths, 115000, {})));
    // Later the oldest fall out of the 15 s window.
    CHECK(DeathsInWindow(deaths, 119000, {}) == 2);
    CHECK(!AttackWiped(DeathsInWindow(deaths, 119000, {})));
    // Zones: the Alliance cut post (1934, 226) and the Alliance stand (2118, 191): all three are within 60 yd of a zone
    // centre ((1900, 230) is 52 yd from the cut post).
    std::vector<Pos> const zones = { LanePostsOf(0).cut, Data().sides[0].standWalk };
    CHECK(DeathsInWindow(deaths, 115000, zones) == 3);
    // Zones elsewhere (the Horde cover post): none.
    CHECK(DeathsInWindow(deaths, 115000, { LanePostsOf(1).cover }) == 0);
}

TEST(tp_wipe_needs_three_and_the_base_threat_needs_a_carrier_or_two_enemies)
{
    CHECK(!AttackWiped(0));
    CHECK(!AttackWiped(2));
    CHECK(AttackWiped(3));
    CHECK(AttackWiped(7));
    CHECK(!BaseThreatened(false, 0));
    CHECK(!BaseThreatened(false, 1));
    CHECK(BaseThreatened(false, 2));
    CHECK(BaseThreatened(true, 0));
}

TEST(tp_wipe_reply_defends_first_when_the_base_is_threatened_else_regroups)
{
    auto const quiet = WipeReplies({ false, false });
    CHECK(quiet.front().id == WipeReply::REGROUP_LANE);
    auto const threatened = WipeReplies({ true, false });
    CHECK(threatened.front().id == WipeReply::DEFEND_FIRST);
    // Their offense is dead as well and our base is quiet: push on.
    auto const both = WipeReplies({ false, true });
    CHECK(both.front().id == WipeReply::PUSH_NOW);
    // A threatened base beats a push.
    CHECK(WipeReplies({ true, true }).front().id == WipeReply::DEFEND_FIRST);
    // Same input, same list; every option is there.
    CHECK(WipeReplies({ true, true }).size() == 3);
}

TEST(tp_floater_jobs_follow_the_sheets_scores)
{
    CHECK(FloaterJobs({ false, false, 3 }).front().id == FloaterJob::STAY_COVER);
    CHECK(FloaterJobs({ true, false, 3 }).front().id == FloaterJob::TO_STRIKE);
    CHECK(FloaterJobs({ false, true, 3 }).front().id == FloaterJob::TO_HOME);
    CHECK(FloaterJobs({ false, false, 1 }).front().id == FloaterJob::TO_HOME);   // home has fewer than 2 bots
    // Both wipes: our home comes first.
    CHECK(FloaterJobs({ true, true, 3 }).front().id == FloaterJob::TO_HOME);
}

TEST(tp_carrier_lane_is_the_own_lane_unless_an_expert_meets_three_enemies)
{
    CHECK(CarrierLanes(true, 0).front().id == CarrierLane::OWN_LANE);
    CHECK(CarrierLanes(true, 2).front().id == CarrierLane::OWN_LANE);
    CHECK(CarrierLanes(true, 3).front().id == CarrierLane::OTHER_LANE);
    CHECK(CarrierLanes(false, 5).front().id == CarrierLane::OWN_LANE);   // only the expert tier changes lanes
    CHECK(CarrierLanes(true, 3).size() == 2);
}

TEST(tp_place_words_name_lane_buff_door_and_graveyard)
{
    std::vector<WsgLogic::PlaceAnchor> const anchors = PlaceAnchors();
    auto const where = [&anchors](float x, float y) { return WsgLogic::ClassifyPlace(anchors, x, y); };
    BgFlagMap::MapData const& d = Data();

    CHECK(where(d.sides[0].standWalk.x, d.sides[0].standWalk.y) == Place::FLAG_ROOM);
    CHECK(where(d.sides[1].mainGraveyard.x + 5, d.sides[1].mainGraveyard.y) == Place::GRAVEYARD);
    CHECK(where(LanePostsOf(0).cut.x + 10, LanePostsOf(0).cut.y) == Place::BERS_H);
    CHECK(where(LanePostsOf(1).cut.x, LanePostsOf(1).cut.y + 10) == Place::BERS_A);
    CHECK(where(LanePostsOf(0).cover.x, LanePostsOf(0).cover.y - 10) == Place::LEAF_A);
    CHECK(where(LanePostsOf(1).cover.x + 8, LanePostsOf(1).cover.y) == Place::LEAF_H);
    CHECK(where(d.mid.x, d.mid.y) == Place::MID);
    CHECK(where(d.sides[0].post[0].x, d.sides[0].post[0].y) == Place::DOOR);
    // Lane H between the buffs and the graveyard, lane A likewise.
    CHECK(where(1880.0f, 190.0f) == Place::LANE_H);
    CHECK(where(1870.0f, 360.0f) == Place::LANE_A);
    // The words of the call-outs.
    CHECK(std::string(WsgLogic::PlaceWord(Place::LANE_A)) == "lane a");
    CHECK(std::string(WsgLogic::PlaceWord(Place::BERS_H)) == "bers h");
    CHECK(std::string(WsgLogic::PlaceWord(Place::LEAF_A)) == "leaf a");
    CHECK(std::string(WsgLogic::PlaceWord(Place::DOOR)) == "door");
    // Far outside everything: no word.
    CHECK(where(1500.0f, 100.0f) == Place::NONE);
}

namespace
{
WsgLogic::DecisionContext Striker(Role role)
{
    WsgLogic::DecisionContext c;
    c.tier = Tier::GOOD;
    c.role = role;
    c.roleIndex = 1;
    return c;
}
}  // namespace

TEST(tp_state_g_sends_the_wiped_bot_home_when_threatened_else_to_the_lane_post)
{
    WsgLogic::DecisionContext c = Striker(Role::ESCORT);
    c.wipeDefend = true;
    CHECK(WsgLogic::Options(c).front().kind == Kind::HOLD_HOME_POST);

    c = Striker(Role::MID);
    c.wipeRegroup = true;
    CHECK(WsgLogic::Options(c).front().kind == Kind::REGROUP_LANE);

    // Never the pickup alone: the regroup beats the take-flag duty of every strike role.
    for (Role role : { Role::TAKER, Role::BACKUP, Role::ESCORT })
    {
        c = Striker(role);
        c.wipeRegroup = true;
        CHECK(WsgLogic::Options(c).front().kind == Kind::REGROUP_LANE);
    }
}

TEST(tp_state_h_sends_the_floater_and_a_spare_home_guard_to_the_strike_group)
{
    WsgLogic::DecisionContext c = Striker(Role::MID);
    c.wipeJoin = true;
    CHECK(WsgLogic::Options(c).front().kind == Kind::GATHER_STRIKE);
    c.strikeGathered = true;
    CHECK(WsgLogic::Options(c).front().kind == Kind::TAKE_ENEMY_FLAG);

    c = Striker(Role::HOME);
    c.wipeJoin = true;
    CHECK(WsgLogic::Options(c).front().kind == Kind::GATHER_STRIKE);

    // Not while our own carrier is out: the escort duty comes first.
    c = Striker(Role::MID);
    c.wipeJoin = true;
    c.theirFlag = WsgLogic::FLAG_ON_PLAYER;
    CHECK(WsgLogic::Options(c).front().kind != Kind::GATHER_STRIKE);
}

TEST(tp_hunters_stop_when_the_enemy_carrier_is_past_its_graveyard_but_not_in_a_stalemate)
{
    WsgLogic::DecisionContext c = Striker(Role::MID);
    c.ourFlag = WsgLogic::FLAG_ON_PLAYER;
    c.efcKnown = true;
    c.efcHunter = true;
    CHECK(WsgLogic::Options(c).front().kind == Kind::HUNT_EFC);

    c.efcPastGy = true;
    for (WsgLogic::Option const& option : WsgLogic::Options(c))
        CHECK(option.kind != Kind::HUNT_EFC && option.kind != Kind::CUT_EFC_LANE);

    // The strike group gives up the chase and goes for the flag.
    WsgLogic::DecisionContext e = Striker(Role::ESCORT);
    e.ourFlag = WsgLogic::FLAG_ON_PLAYER;
    e.efcKnown = true;
    e.efcPastGy = true;
    CHECK(WsgLogic::Options(e).front().kind != Kind::HUNT_EFC);

    // Both flags out: the carrier that hides at its base is hunted there.
    WsgLogic::DecisionContext d = Striker(Role::MID);
    d.ourFlag = WsgLogic::FLAG_ON_PLAYER;
    d.theirFlag = WsgLogic::FLAG_ON_PLAYER;
    d.efcKnown = true;
    d.efcHunter = true;
    d.efcPastGy = true;
    CHECK(WsgLogic::Options(d).front().kind == Kind::HUNT_EFC);
}

TEST(tp_warsong_options_ignore_the_twin_peaks_fields)
{
    // The defaults are false: a Warsong decision is the same with or without them (the existing tests pin the rest).
    WsgLogic::DecisionContext c = Striker(Role::MID);
    c.ourFlag = WsgLogic::FLAG_ON_PLAYER;
    c.efcKnown = true;
    c.efcHunter = true;
    auto const before = WsgLogic::Options(c);
    c.efcPastGy = false;
    c.wipeDefend = false;
    c.wipeRegroup = false;
    c.wipeJoin = false;
    auto const after = WsgLogic::Options(c);
    CHECK(before.size() == after.size());
    for (std::size_t i = 0; i < before.size(); ++i)
        CHECK(before[i].kind == after[i].kind && before[i].score == after[i].score);
}

// ---------------------------------------------------------------------------------------------------------------------
// The wipe and floater jobs of one team from a roster (EvaluateTeam).
// ---------------------------------------------------------------------------------------------------------------------

namespace
{
MemberFacts Fact(std::uint32_t guid, Role role, std::uint32_t index, bool healer = false)
{
    MemberFacts m;
    m.guid = guid;
    m.bot = true;
    m.alive = true;
    m.role = role;
    m.roleIndex = index;
    m.healer = healer;
    return m;
}

// A full team of 10 in the balanced table: taker, backup, 2 escorts, 3 mid, 3 home.
TeamFacts FullTeam(std::uint32_t nowMs)
{
    TeamFacts f;
    f.nowMs = nowMs;
    f.inProgress = true;
    f.ourCutPost = LanePostsOf(0).cut;
    f.enemyStand = Data().sides[0].standWalk;
    f.members = {
        Fact(1, Role::TAKER, 0),  Fact(2, Role::BACKUP, 0), Fact(3, Role::ESCORT, 0), Fact(4, Role::ESCORT, 1),
        Fact(5, Role::MID, 0),    Fact(6, Role::MID, 1, true), Fact(7, Role::MID, 2), Fact(8, Role::HOME, 0),
        Fact(9, Role::HOME, 1),   Fact(10, Role::HOME, 2),
    };
    return f;
}

MemberJobs JobOf(TeamJobs const& jobs, std::uint32_t guid)
{
    for (MemberJobs const& j : jobs.members)
        if (j.guid == guid)
            return j;
    return MemberJobs();
}

void Kill(TeamFacts& f, std::uint32_t guid, std::uint32_t atMs)
{
    for (MemberFacts& m : f.members)
        if (m.guid == guid)
        {
            m.death.diedMs = atMs;
            m.death.at = { 1900.0f, 230.0f, 0.0f };
        }
}
}  // namespace

TEST(tp_team_a_quiet_team_has_no_wipe_state_and_one_floater)
{
    TeamFacts f = FullTeam(100000);
    TeamJobs const jobs = EvaluateTeam(f);
    CHECK(jobs.wipeGUntilMs == 0 && jobs.wipeHUntilMs == 0);
    CHECK(jobs.midAlive == 3);
    CHECK(jobs.floaterAmongMid);
    std::uint32_t floaters = 0;
    for (MemberJobs const& j : jobs.members)
    {
        CHECK(!j.wipeDefend && !j.wipeRegroup && !j.wipeJoin);
        if (j.floater)
            ++floaters;
    }
    CHECK(floaters == 1);
    CHECK(JobOf(jobs, 6).floater);   // the only MID healer
}

TEST(tp_team_the_preparation_reads_only_the_floater)
{
    TeamFacts f = FullTeam(5000);
    f.inProgress = false;
    for (std::uint32_t guid : { 1u, 2u, 3u })
        Kill(f, guid, 4000);
    TeamJobs const jobs = EvaluateTeam(f);
    CHECK(jobs.wipeGUntilMs == 0);
    CHECK(JobOf(jobs, 6).floater);
}

TEST(tp_team_state_g_regroups_the_wiped_bots_at_the_lane_post_when_the_base_is_quiet)
{
    TeamFacts f = FullTeam(120000);
    // Three of the strike group and MID died 5 s ago and are back.
    for (std::uint32_t guid : { 2u, 3u, 5u })
        Kill(f, guid, 115000);
    TeamJobs const jobs = EvaluateTeam(f);
    CHECK(jobs.wipeGUntilMs == 120000 + WIPE_HOLD_MS);
    CHECK(JobOf(jobs, 2).wipeRegroup && !JobOf(jobs, 2).wipeDefend);
    CHECK(JobOf(jobs, 3).wipeRegroup);
    CHECK(JobOf(jobs, 5).wipeRegroup);
    // The ones that did not die go on with their job, the home guards stay.
    CHECK(!JobOf(jobs, 1).wipeRegroup && !JobOf(jobs, 4).wipeRegroup);
    CHECK(!JobOf(jobs, 8).wipeRegroup && !JobOf(jobs, 8).wipeDefend);
}

TEST(tp_team_state_g_runs_home_when_the_enemy_carries_our_flag_or_stands_at_the_base)
{
    TeamFacts f = FullTeam(120000);
    for (std::uint32_t guid : { 2u, 3u, 5u })
        Kill(f, guid, 115000);
    f.enemyCarriesOurFlag = true;
    TeamJobs jobs = EvaluateTeam(f);
    CHECK(JobOf(jobs, 2).wipeDefend && !JobOf(jobs, 2).wipeRegroup);
    CHECK(JobOf(jobs, 5).wipeDefend);

    f.enemyCarriesOurFlag = false;
    f.enemiesNearBase = 2;
    jobs = EvaluateTeam(f);
    CHECK(JobOf(jobs, 3).wipeDefend);
}

TEST(tp_team_two_deaths_are_no_wipe_and_the_state_holds_then_ends)
{
    TeamFacts f = FullTeam(120000);
    Kill(f, 2, 115000);
    Kill(f, 3, 116000);
    CHECK(EvaluateTeam(f).wipeGUntilMs == 0);

    // A real wipe at 120 s: the state holds at 150 s (the third death has left the window) and ends at 165 s.
    Kill(f, 5, 117000);
    TeamJobs jobs = EvaluateTeam(f);
    CHECK(jobs.wipeGUntilMs == 165000);
    f.nowMs = 150000;
    f.wipeGUntilMs = jobs.wipeGUntilMs;
    jobs = EvaluateTeam(f);
    CHECK(jobs.wipeGUntilMs == 165000);
    CHECK(JobOf(jobs, 2).wipeRegroup);   // back from the wipe 35 s ago, still inside the hold
    f.nowMs = 166000;
    jobs = EvaluateTeam(f);
    CHECK(jobs.wipeGUntilMs == 0);
    CHECK(!JobOf(jobs, 2).wipeRegroup && !JobOf(jobs, 2).wipeDefend);
}

TEST(tp_team_state_g_sends_the_floater_to_the_base)
{
    TeamFacts f = FullTeam(120000);
    for (std::uint32_t guid : { 2u, 3u, 5u })
        Kill(f, guid, 115000);
    TeamJobs const jobs = EvaluateTeam(f);
    // Guid 6 is the floater (a MID healer): it did not die, and it goes to the base in state G.
    CHECK(JobOf(jobs, 6).floater);
    CHECK(JobOf(jobs, 6).wipeDefend);
}

TEST(tp_team_state_h_sends_the_floater_and_one_spare_home_guard)
{
    TeamFacts f = FullTeam(120000);
    for (std::uint32_t i = 0; i < 3; ++i)
        f.enemyDeaths.push_back({ 112000u + i * 1000u, { 1930.0f, 230.0f, -17.0f } });   // at our cut post
    TeamJobs const jobs = EvaluateTeam(f);
    CHECK(jobs.wipeHUntilMs == 120000 + WIPE_HOLD_MS);
    CHECK(JobOf(jobs, 6).wipeJoin);    // the floater
    CHECK(JobOf(jobs, 10).wipeJoin);   // the last home guard of the list
    CHECK(!JobOf(jobs, 8).wipeJoin && !JobOf(jobs, 9).wipeJoin);
    // Nobody else changes job.
    for (std::uint32_t guid : { 1u, 2u, 3u, 4u, 5u, 7u })
        CHECK(!JobOf(jobs, guid).wipeJoin && !JobOf(jobs, guid).wipeDefend && !JobOf(jobs, guid).wipeRegroup);
}

TEST(tp_team_state_h_counts_only_deaths_near_the_cut_post_or_the_enemy_stand)
{
    TeamFacts f = FullTeam(120000);
    for (std::uint32_t i = 0; i < 3; ++i)
        f.enemyDeaths.push_back({ 112000u + i * 1000u, { 1850.0f, 320.0f, -25.0f } });   // mid river: nobody's post
    CHECK(EvaluateTeam(f).wipeHUntilMs == 0);

    f.enemyDeaths.clear();
    f.enemyDeaths.push_back({ 112000, { 2110.0f, 190.0f, 44.0f } });     // at the enemy stand (the Alliance's here)
    f.enemyDeaths.push_back({ 113000, { 2120.0f, 195.0f, 44.0f } });
    f.enemyDeaths.push_back({ 114000, { 2100.0f, 185.0f, 44.0f } });
    CHECK(EvaluateTeam(f).wipeHUntilMs == 120000 + WIPE_HOLD_MS);
}

TEST(tp_team_a_team_without_a_healer_sends_its_last_mid_bot_in_state_h)
{
    TeamFacts f = FullTeam(120000);
    f.members[5].healer = false;   // guid 6 is no healer: nobody floats
    for (std::uint32_t i = 0; i < 3; ++i)
        f.enemyDeaths.push_back({ 112000u + i * 1000u, { 1930.0f, 230.0f, -17.0f } });
    TeamJobs const jobs = EvaluateTeam(f);
    CHECK(!jobs.floaterAmongMid);
    for (MemberJobs const& j : jobs.members)
        CHECK(!j.floater);
    CHECK(JobOf(jobs, 7).wipeJoin);   // the last MID bot (index 2) stands in
    CHECK(!JobOf(jobs, 5).wipeJoin && !JobOf(jobs, 6).wipeJoin);
}

TEST(tp_team_a_small_team_keeps_its_home_guards)
{
    // Two home guards only: no spare leaves in state H, and the floater is needed at home in state G.
    TeamFacts f = FullTeam(120000);
    f.members.pop_back();
    for (std::uint32_t i = 0; i < 3; ++i)
        f.enemyDeaths.push_back({ 112000u + i * 1000u, { 1930.0f, 230.0f, -17.0f } });
    TeamJobs const jobs = EvaluateTeam(f);
    CHECK(!JobOf(jobs, 8).wipeJoin && !JobOf(jobs, 9).wipeJoin);
    // With fewer than 2 home bots alive the floater goes home instead of to the strike group.
    f.members.pop_back();
    TeamJobs const lone = EvaluateTeam(f);
    CHECK(JobOf(lone, 6).wipeDefend && !JobOf(lone, 6).wipeJoin);
}

TEST(tp_team_humans_are_never_given_a_job)
{
    TeamFacts f = FullTeam(120000);
    f.members[4].bot = false;   // guid 5 is a human MID
    for (std::uint32_t guid : { 2u, 3u, 5u })
        Kill(f, guid, 115000);
    f.enemyCarriesOurFlag = true;
    TeamJobs const jobs = EvaluateTeam(f);
    CHECK(!JobOf(jobs, 5).wipeDefend && !JobOf(jobs, 5).wipeRegroup && !JobOf(jobs, 5).wipeJoin && !JobOf(jobs, 5).floater);
}

// ---------------------------------------------------------------------------------------------------------------------
// BT-502: the answers of a decision layer replace the first candidate of the wipe reply (D7) and the floater job (D6), and the
// lists the plan publishes to it are the ones the rule used.
// ---------------------------------------------------------------------------------------------------------------------

TEST(tp_team_the_lists_the_rule_used_are_exposed_for_the_decision_layer)
{
    TeamFacts f = FullTeam(120000);
    for (std::uint32_t guid : { 2u, 3u, 5u })
        Kill(f, guid, 115000);
    TeamJobs const jobs = EvaluateTeam(f);
    CHECK(jobs.listsValid);
    CHECK(jobs.wipeG && !jobs.wipeH && !jobs.baseThreatened);
    CHECK(jobs.homeAlive == 3);
    // A quiet base after the wipe: regroup is the rule's answer, the push and the defense are the alternatives.
    CHECK(jobs.wipeList.size() == 3);
    CHECK(jobs.wipeList.front().id == WipeReply::REGROUP_LANE);
    CHECK(jobs.floaterList.size() == 3);
    CHECK(jobs.floaterList.front().id == FloaterJob::TO_HOME);

    // The preparation reads no lists.
    TeamFacts prep = FullTeam(5000);
    prep.inProgress = false;
    CHECK(!EvaluateTeam(prep).listsValid);
}

TEST(tp_team_a_layer_answer_for_the_wipe_reply_replaces_the_rule_when_the_list_holds_it)
{
    TeamFacts f = FullTeam(120000);
    for (std::uint32_t guid : { 2u, 3u, 5u })
        Kill(f, guid, 115000);

    // The rule regroups (quiet base); the layer says defend first: the wiped bots go home instead.
    f.wipeOverride = WipeReply::DEFEND_FIRST;
    TeamJobs jobs = EvaluateTeam(f);
    CHECK(JobOf(jobs, 2).wipeDefend && !JobOf(jobs, 2).wipeRegroup);
    CHECK(JobOf(jobs, 5).wipeDefend);

    // Push now: neither a defense nor a regroup, the wiped bots go on with their jobs.
    f.wipeOverride = WipeReply::PUSH_NOW;
    jobs = EvaluateTeam(f);
    CHECK(!JobOf(jobs, 2).wipeDefend && !JobOf(jobs, 2).wipeRegroup);

    // No answer, the rule: regroup.
    f.wipeOverride.reset();
    jobs = EvaluateTeam(f);
    CHECK(JobOf(jobs, 2).wipeRegroup);

    // An answer of an id outside the list is ignored (the lists always hold the three).
    f.wipeOverride = static_cast<WipeReply>(9);
    jobs = EvaluateTeam(f);
    CHECK(JobOf(jobs, 2).wipeRegroup);
}

TEST(tp_team_a_layer_answer_for_the_floater_replaces_the_rule)
{
    TeamFacts f = FullTeam(120000);
    for (std::uint32_t guid : { 2u, 3u, 5u })
        Kill(f, guid, 115000);

    // State G: the rule sends the floater home. The layer keeps it with the cover post.
    f.floaterOverride = FloaterJob::STAY_COVER;
    TeamJobs jobs = EvaluateTeam(f);
    CHECK(JobOf(jobs, 6).floater);
    CHECK(!JobOf(jobs, 6).wipeDefend && !JobOf(jobs, 6).wipeJoin);

    // A layer that says "to strike" without a wiped enemy attack does nothing: the job needs state H.
    f.floaterOverride = FloaterJob::TO_STRIKE;
    jobs = EvaluateTeam(f);
    CHECK(!JobOf(jobs, 6).wipeJoin);
}
