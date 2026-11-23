#include "TestHarness.h"
#include "ToCLogic.h"

#include <vector>

using namespace ToCLogic;

TEST(toc_distance_to_segment_clamps_to_the_ends)
{
    CHECK(std::fabs(DistanceToSegment({5.0f, 3.0f}, {0.0f, 0.0f}, {10.0f, 0.0f}) - 3.0f) < 0.001f);
    CHECK(std::fabs(DistanceToSegment({-4.0f, 3.0f}, {0.0f, 0.0f}, {10.0f, 0.0f}) - 5.0f) < 0.001f);
    CHECK(std::fabs(DistanceToSegment({14.0f, 3.0f}, {0.0f, 0.0f}, {10.0f, 0.0f}) - 5.0f) < 0.001f);
    CHECK(std::fabs(DistanceToSegment({3.0f, 4.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}) - 5.0f) < 0.001f);
}

TEST(toc_charge_runs_through_the_boss_from_the_mirrored_hop_point)
{
    Point const boss = {563.0f, 140.0f};
    Point const target = {563.0f, 110.0f};
    Point const start = ChargeStart(boss, target);
    CHECK(std::fabs(start.x - 563.0f) < 0.001f);
    CHECK(std::fabs(start.y - 170.0f) < 0.001f);
    // Behind the boss, on the line, is in the way as well
    CHECK(InChargeZone({563.0f, 160.0f}, boss, target));
    CHECK(InChargeZone({563.0f, 125.0f}, boss, target));
}

TEST(toc_charge_zone_has_the_trample_circle_at_the_end)
{
    Point const boss = {563.0f, 140.0f};
    Point const target = {563.0f, 110.0f};
    // 10 yards beside the target: inside the 12 yard Trample circle
    CHECK(InChargeZone({573.0f, 110.0f}, boss, target));
    // 20 yards beside the middle of the line: clear
    CHECK(!InChargeZone({583.0f, 125.0f}, boss, target));
}

TEST(toc_escape_charge_steps_to_the_nearest_clear_spot)
{
    Point const boss = {563.0f, 140.0f};
    Point const target = {563.0f, 100.0f};
    Point out;
    CHECK(!EscapeCharge({590.0f, 140.0f}, boss, target, out));
    Point const inLine = {565.0f, 125.0f};
    CHECK(EscapeCharge(inLine, boss, target, out));
    CHECK(!InChargeZone(out, boss, target));
    // A side step, not a run to the far end of the room
    CHECK(Distance(inLine, out) < 15.0f);
}

TEST(toc_escape_charge_result_stays_inside_the_coliseum)
{
    Point const boss = {563.0f, 140.0f};
    Point const target = {563.0f, 100.0f};
    Point out;
    CHECK(EscapeCharge({563.0f, 120.0f}, boss, target, out));
    CHECK(Distance(out, ARENA_CENTER) <= ARENA_RUN_RADIUS + 0.01f);
}

TEST(toc_trample_target_runs_away_from_the_boss_inside_the_room)
{
    Point const boss = ARENA_CENTER;
    Point const self = {ARENA_CENTER.x, ARENA_CENTER.y - 10.0f};
    Point const spot = TrampleTargetSpot(self, boss);
    CHECK(Distance(spot, boss) > Distance(self, boss));
    CHECK(Distance(spot, ARENA_CENTER) <= ARENA_RUN_RADIUS + 0.01f);
    // Already at the wall: stays at the wall
    Point const wall = TrampleTargetSpot({ARENA_CENTER.x, ARENA_CENTER.y - ARENA_RUN_RADIUS}, boss);
    CHECK(Distance(wall, ARENA_CENTER) <= ARENA_RUN_RADIUS + 0.01f);
}

TEST(toc_worm_skull_moves_only_past_the_margin)
{
    CHECK(!SwitchWorm(60.0f, 62.0f));
    CHECK(!SwitchWorm(60.0f, 65.0f));
    CHECK(SwitchWorm(60.0f, 66.0f));
    CHECK(!SwitchWorm(60.0f, 40.0f));
}

TEST(toc_hazard_radius_by_entry)
{
    CHECK(HazardRadius(NPC_FIRE_BOMB) == FIRE_BOMB_AVOID);
    CHECK(HazardRadius(NPC_SLIME_POOL) == SLIME_POOL_AVOID);
    CHECK(HazardRadius(NPC_LEGION_FLAME) == LEGION_FLAME_AVOID);
    CHECK(HazardRadius(34796) == 0.0f);
    CHECK(SLIME_POOL_AVOID > FIRE_BOMB_AVOID);
}

TEST(toc_legion_flame_carrier_runs_from_the_raid)
{
    std::vector<Point> const raid = {{0.0f, 0.0f}, {2.0f, 0.0f}, {0.0f, 2.0f}, {2.0f, 2.0f}};
    Point out;
    Point const self = {5.0f, 1.0f};
    CHECK(LegionFlameSpot(self, raid, out));
    CHECK(Distance(out, Centroid(raid)) > Distance(self, Centroid(raid)));
    // Far enough already, or alone: no move
    CHECK(!LegionFlameSpot({40.0f, 0.0f}, raid, out));
    CHECK(!LegionFlameSpot(self, {}, out));
}

TEST(toc_champion_tiers_put_healers_first)
{
    CHECK(ChampionTier(34469) == CHAMPION_TIER_HEALER);  // alliance restoration druid
    CHECK(ChampionTier(34444) == CHAMPION_TIER_HEALER);  // horde restoration shaman
    CHECK(ChampionTier(34449) == CHAMPION_TIER_CASTER);  // horde mage
    CHECK(ChampionTier(34453) == CHAMPION_TIER_OTHER);   // horde warrior
    CHECK(ChampionTier(99999) == CHAMPION_TIER_NONE);
}

TEST(toc_champion_order_is_tier_then_health)
{
    CHECK(ChampionBetter(CHAMPION_TIER_HEALER, 100.0f, CHAMPION_TIER_CASTER, 10.0f));
    CHECK(!ChampionBetter(CHAMPION_TIER_OTHER, 10.0f, CHAMPION_TIER_CASTER, 100.0f));
    CHECK(ChampionBetter(CHAMPION_TIER_CASTER, 30.0f, CHAMPION_TIER_CASTER, 80.0f));
    CHECK(!ChampionBetter(CHAMPION_TIER_CASTER, 80.0f, CHAMPION_TIER_CASTER, 30.0f));
}

TEST(toc_every_champion_entry_has_a_tier)
{
    // The 28 entries of trial_of_the_crusader.h: 14 per faction, 4 healers, 4 casters and 6 others each
    uint32_t const entries[] = {34461, 34460, 34469, 34467, 34468, 34465, 34471, 34466, 34473, 34472, 34463, 34470,
                                34474, 34475, 34458, 34451, 34459, 34448, 34449, 34445, 34456, 34447, 34441, 34454,
                                34455, 34444, 34450, 34453};
    int counts[4] = {0, 0, 0, 0};
    for (uint32_t entry : entries)
        ++counts[ChampionTier(entry)];
    CHECK(counts[CHAMPION_TIER_HEALER] == 8);
    CHECK(counts[CHAMPION_TIER_CASTER] == 8);
    CHECK(counts[CHAMPION_TIER_OTHER] == 12);
    CHECK(counts[CHAMPION_TIER_NONE] == 0);
}

TEST(toc_essence_is_the_opposite_colour_of_the_twin_attacked)
{
    CHECK(Opposite(Twin::Light) == Twin::Dark);
    CHECK(Opposite(Twin::Dark) == Twin::Light);
    CHECK(Opposite(Opposite(Twin::Light)) == Twin::Light);
}

TEST(toc_twin_assignment_tanks_split_and_the_rest_by_guid)
{
    CHECK(AssignedTwin(true, false, 7) == Twin::Light);
    CHECK(AssignedTwin(false, true, 8) == Twin::Dark);
    CHECK(AssignedTwin(false, false, 8) == Twin::Light);
    CHECK(AssignedTwin(false, false, 9) == Twin::Dark);
}

TEST(toc_orbs_of_the_other_colour_are_dangerous)
{
    CHECK(!OrbDangerous(Twin::Light, true, Twin::Light));
    CHECK(OrbDangerous(Twin::Dark, true, Twin::Light));
    // No essence at all: every orb hurts
    CHECK(OrbDangerous(Twin::Light, false, Twin::Light));
}

TEST(toc_step_out_of_orbs_only_when_one_is_close)
{
    Point out;
    std::vector<Point> const near = {{10.0f, 0.0f}, {60.0f, 0.0f}};
    CHECK(StepOutOfOrbs({13.0f, 0.0f}, near, out));
    CHECK(Distance(out, near[0]) > Distance({13.0f, 0.0f}, near[0]));
    CHECK(!StepOutOfOrbs({30.0f, 0.0f}, near, out));
    CHECK(!StepOutOfOrbs({0.0f, 0.0f}, {}, out));
}

TEST(toc_spike_runs_behind_a_sphere_beside_the_way)
{
    Point const self = {0.0f, 0.0f};
    Point const spike = {-10.0f, 0.0f};
    // A sphere beside the way: the far side of it puts the sphere between the bot and the spike
    std::vector<Point> const spheres = {{-6.0f, 14.0f}};
    CHECK(SpikeSphere(self, spike, spheres) == 0);
    Point const dest = BehindSphere(spheres[0], spike);
    CHECK(Distance(spike, dest) > Distance(spike, spheres[0]));
}

TEST(toc_spike_ignores_spheres_that_need_a_run_past_the_spike_or_that_already_stopped_it)
{
    Point const self = {0.0f, 0.0f};
    Point const spike = {10.0f, 0.0f};
    // The sphere lies behind the spike: the way to its far side passes the spike
    CHECK(SpikeSphere(self, spike, {{25.0f, 0.0f}}) == -1);
    // A sphere the spike is at already
    CHECK(SpikeSphere(self, spike, {{14.0f, 3.0f}}) == -1);
    // Straight away from the spike, past a sphere: the spike follows through it
    CHECK(SpikeSphere(self, spike, {{-30.0f, 0.0f}}) == 0);
    CHECK(SpikeSphere(self, spike, {}) == -1);
}

TEST(toc_spike_picks_the_shortest_run)
{
    Point const self = {0.0f, 0.0f};
    Point const spike = {-12.0f, 0.0f};
    std::vector<Point> const spheres = {{-6.0f, 30.0f}, {-6.0f, 10.0f}};
    CHECK(SpikeSphere(self, spike, spheres) == 1);
}

TEST(toc_clamp_to_arena_keeps_inside_points)
{
    Point const inside = {ARENA_CENTER.x + 10.0f, ARENA_CENTER.y};
    CHECK(ClampToArena(inside).x == inside.x);
    Point const far = ClampToArena({ARENA_CENTER.x + 100.0f, ARENA_CENTER.y});
    CHECK(std::fabs(Distance(far, ARENA_CENTER) - ARENA_RUN_RADIUS) < 0.01f);
}
