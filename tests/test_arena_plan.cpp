/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "ArenaPlanMath.h"
#include "TestHarness.h"

using namespace ArenaPlanMath;

namespace
{
FocusInput Enemy(std::uint32_t id, std::int32_t hp)
{
    FocusInput in;
    in.id = id;
    in.hpPercent = hp;
    in.inRangeOfBoth = true;
    in.losSharePercent = 100;
    return in;
}
}  // namespace

TEST(arena_plan_tier_profiles_follow_the_design_table)
{
    TierProfile const casual = ProfileOf(Tier::CASUAL);
    TierProfile const good = ProfileOf(Tier::GOOD);
    TierProfile const expert = ProfileOf(Tier::EXPERT);
    CHECK(casual.planIntervalMs == 2000 && good.planIntervalMs == 1000 && expert.planIntervalMs == 500);
    CHECK(casual.minFocusHoldMs == 8000 && good.minFocusHoldMs == 4000 && expert.minFocusHoldMs == 2000);
    CHECK(casual.swapMargin == 25 && good.swapMargin == 15 && expert.swapMargin == 8);
    CHECK(casual.defensiveThreshold == 25 && good.defensiveThreshold == 40 && expert.defensiveThreshold == 55);
    CHECK(!casual.losRetreat && good.losRetreat && expert.losRetreat);
    CHECK(!good.losPreemptive && expert.losPreemptive);
}

TEST(arena_plan_focus_prefers_the_weaker_target)
{
    std::vector<FocusInput> enemies = { Enemy(1, 90), Enemy(2, 40) };
    FocusDecision const decision = ChooseFocus(enemies, 0, 0, ProfileOf(Tier::GOOD));
    CHECK(decision.hasTarget && decision.id == 2 && decision.changed);
}

TEST(arena_plan_good_and_expert_go_for_the_healer_casual_does_not)
{
    FocusInput damage = Enemy(1, 70);
    damage.meleeDamage = true;
    FocusInput healer = Enemy(2, 85);
    healer.healer = true;
    std::vector<FocusInput> enemies = { damage, healer };

    // 85 hp + 35 for the healer (good) beats 70 hp + 15 for melee: 15 + 35 = 50 against 30 + 15 = 45
    CHECK(ChooseFocus(enemies, 0, 0, ProfileOf(Tier::GOOD)).id == 2);
    CHECK(ChooseFocus(enemies, 0, 0, ProfileOf(Tier::EXPERT)).id == 2);
    // casual only looks at hp: the lower hp melee target
    CHECK(ChooseFocus(enemies, 0, 0, ProfileOf(Tier::CASUAL)).id == 1);
}

TEST(arena_plan_focus_holds_until_the_margin_and_the_time_are_met)
{
    std::vector<FocusInput> enemies = { Enemy(1, 60), Enemy(2, 30) };
    TierProfile const good = ProfileOf(Tier::GOOD);

    // current focus 1; target 2 leads by 30 - 20 stickiness = 10 points, under the margin of 15
    FocusDecision d = ChooseFocus(enemies, 1, 10000, good);
    CHECK(d.id == 1 && !d.changed);

    // target 2 at 5 hp leads by 55 - 20 = 35 points: swap, but only after the hold time
    enemies[1].hpPercent = 5;
    d = ChooseFocus(enemies, 1, 3999, good);
    CHECK(d.id == 1 && !d.changed);
    d = ChooseFocus(enemies, 1, 4000, good);
    CHECK(d.id == 2 && d.changed);
}

TEST(arena_plan_expert_swaps_sooner_than_casual)
{
    std::vector<FocusInput> enemies = { Enemy(1, 60), Enemy(2, 40) };
    // lead of 20 - 20 stickiness is 0, so use hp 35: lead 25 - 20 = 5 ... use a clear case
    enemies[1].hpPercent = 20;  // lead 40 - 20 = 20 points
    CHECK(ChooseFocus(enemies, 1, 99999, ProfileOf(Tier::EXPERT)).id == 2);  // margin 8
    CHECK(ChooseFocus(enemies, 1, 99999, ProfileOf(Tier::GOOD)).id == 2);    // margin 15
    CHECK(ChooseFocus(enemies, 1, 99999, ProfileOf(Tier::CASUAL)).id == 1);  // margin 25
    CHECK(ChooseFocus(enemies, 1, 2000, ProfileOf(Tier::EXPERT)).id == 2);   // hold 2000
    CHECK(ChooseFocus(enemies, 1, 1999, ProfileOf(Tier::EXPERT)).id == 1);
}

TEST(arena_plan_a_gone_or_immune_focus_is_dropped_at_once)
{
    TierProfile const good = ProfileOf(Tier::GOOD);
    std::vector<FocusInput> enemies = { Enemy(2, 80) };
    // the current focus (1) is no longer in the list: it died or left reach
    FocusDecision d = ChooseFocus(enemies, 1, 0, good);
    CHECK(d.id == 2 && d.changed);

    // the current focus is immune: swap even inside the hold time
    FocusInput immune = Enemy(1, 20);
    immune.immune = true;
    enemies = { immune, Enemy(2, 80) };
    d = ChooseFocus(enemies, 1, 0, good);
    CHECK(d.id == 2);

    // casual does not see immunity (the good/expert terms)
    enemies = { immune, Enemy(2, 80) };
    d = ChooseFocus(enemies, 1, 0, ProfileOf(Tier::CASUAL));
    CHECK(d.id == 1);
}

TEST(arena_plan_an_immune_target_is_skipped_while_another_exists)
{
    FocusInput immune = Enemy(1, 10);  // almost dead, but behind an Ice Block
    immune.immune = true;
    std::vector<FocusInput> enemies = { immune, Enemy(2, 90) };
    CHECK(ChooseFocus(enemies, 0, 0, ProfileOf(Tier::GOOD)).id == 2);
    CHECK(ChooseFocus(enemies, 0, 0, ProfileOf(Tier::EXPERT)).id == 2);
    // casual does not see it
    CHECK(ChooseFocus(enemies, 0, 0, ProfileOf(Tier::CASUAL)).id == 1);
    // everybody immune: the weaker one is taken anyway
    enemies[1].immune = true;
    CHECK(ChooseFocus(enemies, 0, 0, ProfileOf(Tier::GOOD)).id == 1);
}

TEST(arena_plan_cc_that_breaks_on_damage_is_avoided)
{
    FocusInput sheep = Enemy(1, 30);
    sheep.ccBreaksOnDamage = true;
    std::vector<FocusInput> enemies = { sheep, Enemy(2, 75) };
    // 70 - 50 = 20 against 25
    CHECK(ChooseFocus(enemies, 0, 0, ProfileOf(Tier::GOOD)).id == 2);
}

TEST(arena_plan_expert_sees_big_defensives_and_the_kill_window)
{
    FocusInput shielded = Enemy(1, 30);
    shielded.heavyDefensive = true;
    FocusInput open = Enemy(2, 50);
    std::vector<FocusInput> enemies = { shielded, open };
    CHECK(ChooseFocus(enemies, 0, 0, ProfileOf(Tier::EXPERT)).id == 2);  // 70 - 40 = 30 against 50
    CHECK(ChooseFocus(enemies, 0, 0, ProfileOf(Tier::GOOD)).id == 1);    // the good tier does not look at it

    FocusInput low = Enemy(3, 55);
    low.burstWindow = true;
    CHECK(FocusScore(low, ProfileOf(Tier::EXPERT)) == FocusScore(Enemy(3, 55), ProfileOf(Tier::EXPERT)) + FOCUS_BURST_WINDOW);
    low.hpPercent = 80;
    CHECK(FocusScore(low, ProfileOf(Tier::EXPERT)) == FocusScore(Enemy(3, 80), ProfileOf(Tier::EXPERT)));  // only under 60 percent
}

TEST(arena_plan_focus_is_deterministic_in_ties)
{
    std::vector<FocusInput> a = { Enemy(9, 50), Enemy(4, 50) };
    std::vector<FocusInput> b = { Enemy(4, 50), Enemy(9, 50) };
    CHECK(ChooseFocus(a, 0, 0, ProfileOf(Tier::GOOD)).id == 4);
    CHECK(ChooseFocus(b, 0, 0, ProfileOf(Tier::GOOD)).id == 4);
    CHECK(!ChooseFocus({}, 0, 0, ProfileOf(Tier::GOOD)).hasTarget);
}

TEST(arena_plan_defensive_score_follows_the_threshold)
{
    // casual threshold 25: at 12 hp (about half way) the base score is 52
    CHECK(DefensiveScore(12, 25, false, false, false) == 52);
    CHECK(DefensiveScore(25, 25, false, false, false) == 0);
    CHECK(DefensiveScore(80, 25, false, false, false) == 0);
    CHECK(DefensiveScore(0, 25, false, false, false) == 100);
    CHECK(DefensiveScore(20, 40, true, false, false) == 50 + DEFENSIVE_PRESSURE);
    CHECK(DefensiveScore(20, 40, true, true, true) == 50 + DEFENSIVE_PRESSURE + DEFENSIVE_NO_HEALER_REACH + DEFENSIVE_RECENTLY_USED);
    CHECK(DefensiveScore(50, 0, true, false, false) == DEFENSIVE_PRESSURE);  // no threshold: only the extras
}

TEST(arena_plan_defensive_fires_at_fifty_and_experts_also_on_predicted_lethal)
{
    TierProfile const good = ProfileOf(Tier::GOOD);
    TierProfile const expert = ProfileOf(Tier::EXPERT);
    CHECK(ShouldUseDefensive(50, false, good));
    CHECK(!ShouldUseDefensive(49, false, good));
    CHECK(!ShouldUseDefensive(10, true, good));    // the good tier has no prediction
    CHECK(ShouldUseDefensive(10, true, expert));
    CHECK(PredictedLethal(1000, 800));
    CHECK(!PredictedLethal(1000, 799));
    CHECK(!PredictedLethal(0, 500));
}

TEST(arena_plan_defensive_thresholds_by_tier_fire_at_different_health)
{
    // pressure on, same hp: the higher the tier's threshold, the earlier it fires
    auto fires = [](Tier tier, std::int32_t hp)
    {
        TierProfile const profile = ProfileOf(tier);
        return ShouldUseDefensive(DefensiveScore(hp, profile.defensiveThreshold, true, false, false), false, profile);
    };
    CHECK(fires(Tier::EXPERT, 40));
    CHECK(!fires(Tier::GOOD, 40));
    CHECK(fires(Tier::GOOD, 30));
    CHECK(!fires(Tier::CASUAL, 30));
    CHECK(fires(Tier::CASUAL, 15));
}

TEST(arena_plan_option_order_is_total_and_deterministic)
{
    std::vector<ArenaOption> options = {
        { OptionKind::LOS, 5, 0, 0, 70 },
        { OptionKind::FOCUS, 7, 3, 0, 70 },
        { OptionKind::DEFENSIVE, 2, 0, 45438, 70 },
        { OptionKind::FOCUS, 6, 9, 0, 40 },
    };
    ArenaOption const* best = PickBest(options);
    CHECK(best && best->kind == OptionKind::FOCUS && best->actor == 7);  // kind order breaks the tie

    std::reverse(options.begin(), options.end());
    best = PickBest(options);
    CHECK(best && best->kind == OptionKind::FOCUS && best->actor == 7);

    CHECK(PickBest({}) == nullptr);

    std::vector<ArenaOption> sameKind = { { OptionKind::FOCUS, 8, 1, 0, 30 }, { OptionKind::FOCUS, 3, 2, 0, 30 }, { OptionKind::FOCUS, 3, 1, 0, 30 } };
    best = PickBest(sameKind);
    CHECK(best && best->actor == 3 && best->target == 1);
}

TEST(arena_plan_los_ring_points_lie_on_the_rings)
{
    for (std::uint32_t ring = 0; ring < LOS_RING_COUNT; ++ring)
        for (std::uint32_t index = 0; index < LOS_RING_POINTS; ++index)
        {
            float x, y;
            RingPoint(100.0f, -50.0f, ring, index, x, y);
            double const dist = std::sqrt(double(x - 100.0f) * (x - 100.0f) + double(y + 50.0f) * (y + 50.0f));
            CHECK_NEAR(dist, LOS_RING_RADIUS[ring], 0.01);
        }

    // 24 distinct candidates
    int distinct = 0;
    std::vector<std::pair<float, float>> seen;
    for (std::uint32_t ring = 0; ring < LOS_RING_COUNT; ++ring)
        for (std::uint32_t index = 0; index < LOS_RING_POINTS; ++index)
        {
            float x, y;
            RingPoint(0.0f, 0.0f, ring, index, x, y);
            bool dup = false;
            for (auto const& s : seen)
                dup = dup || (std::fabs(s.first - x) < 0.01f && std::fabs(s.second - y) < 0.01f);
            if (!dup)
                ++distinct;
            seen.emplace_back(x, y);
        }
    CHECK(distinct == 24);
}

TEST(arena_plan_los_score_and_cover_choice)
{
    LosCandidate hidden;
    hidden.breaksEnemySight = true;
    hidden.keepsPartnerSight = true;
    hidden.outOfMeleeReach = true;
    hidden.pathLength = 10.0f;
    CHECK(LosScore(hidden) == 60 + 30 + 20 - 20);

    LosCandidate nearHidden = hidden;
    nearHidden.pathLength = 6.0f;

    LosCandidate exposed;
    exposed.keepsPartnerSight = true;
    exposed.outOfMeleeReach = true;
    exposed.pathLength = 2.0f;

    LosCandidate hazard = hidden;
    hazard.hazard = true;
    LosCandidate dead = hidden;
    dead.deadEnd = true;
    CHECK(LosScore(hazard) == LosScore(hidden) + LOS_HAZARD);
    CHECK(LosScore(dead) == LosScore(hidden) + LOS_DEAD_END);

    std::vector<LosCandidate> candidates = { exposed, hidden, nearHidden, hazard };
    CHECK(BestCover(candidates) == 2);  // the nearer hidden point
    CHECK(BestCover({ exposed }) == -1);  // a point that does not break sight is never taken
    CHECK(BestCover({}) == -1);

    LosCandidate far = hidden;
    far.pathLength = 80.0f;  // -160: the walk costs more than the cover is worth
    CHECK(BestCover({ far }) == -1);
}

TEST(arena_plan_los_path_may_be_at_most_sixteen_tenths_of_the_straight_line)
{
    CHECK(PathAcceptable(16.0f, 10.0f));
    CHECK(!PathAcceptable(16.5f, 10.0f));
    CHECK(!PathAcceptable(0.0f, 10.0f));
}

TEST(arena_plan_cover_is_wanted_by_tier)
{
    TierProfile const casual = ProfileOf(Tier::CASUAL);
    TierProfile const good = ProfileOf(Tier::GOOD);
    TierProfile const expert = ProfileOf(Tier::EXPERT);
    CHECK(!WantsCover(casual, 10, true, false));
    CHECK(WantsCover(good, 39, true, false));
    CHECK(!WantsCover(good, 41, true, false));
    CHECK(!WantsCover(good, 10, false, false));  // no caster on it
    CHECK(!WantsCover(good, 60, true, true));    // good does not hide the healer early
    CHECK(WantsCover(expert, 60, true, true));   // the expert healer does
    CHECK(!WantsCover(expert, 60, true, false));
    CHECK(!WantsCover(expert, 75, true, true));
}
