#include "TestHarness.h"
#include "DpsRotationMath.h"

#include <string>

TEST(arms_heroic_strike_dump_follows_the_colossus_smash_window)
{
    using namespace DpsRotationMath::Warrior;
    // Below 81, and for bots that do not play the expert lines, the levelling dump stays at 60 rage.
    CHECK(HeroicStrikeRage(60, true, false) == 60u);
    CHECK(HeroicStrikeRage(80, true, true) == 60u);
    CHECK(HeroicStrikeRage(90, false, true) == 60u);
    // From 81 an expert pools for the window and dumps inside it, keeping one Slam's rage.
    CHECK(HeroicStrikeRage(81, true, false) == 100u);
    CHECK(HeroicStrikeRage(90, true, true) == 55u);
    CHECK(HeroicStrikeRage(90, true, true) >= SLAM_RAGE + 30);
}

TEST(arms_slam_beats_overpower_only_inside_the_window)
{
    using namespace DpsRotationMath::Warrior;
    CHECK(SlamBeforeOverpower(90, true, true, 25));
    CHECK(!SlamBeforeOverpower(90, true, true, 24));
    CHECK(!SlamBeforeOverpower(90, true, false, 100));
    CHECK(!SlamBeforeOverpower(80, true, true, 100));
    CHECK(!SlamBeforeOverpower(90, false, true, 100));
}

TEST(demonology_expert_saves_molten_core_for_the_form)
{
    using namespace DpsRotationMath::Warlock;
    CHECK(!SoulFireOnMoltenCore(false, 0, true));
    CHECK(SoulFireOnMoltenCore(false, 1, false));
    CHECK(!SoulFireOnMoltenCore(true, 8, false));
    CHECK(SoulFireOnMoltenCore(true, 9, false));
    CHECK(SoulFireOnMoltenCore(true, 1, true));
}

TEST(destruction_ember_tap_only_when_the_other_defensives_are_down)
{
    using namespace DpsRotationMath::Warlock;
    CHECK(EmberTapUseful(true, 44.0f, 10, false));
    CHECK(!EmberTapUseful(true, 45.0f, 10, false));
    CHECK(!EmberTapUseful(true, 30.0f, 9, false));
    CHECK(!EmberTapUseful(true, 30.0f, 40, true));
    CHECK(!EmberTapUseful(false, 30.0f, 40, false));
}

TEST(windwalker_white_tiger_respects_the_other_critical_strike_buffs)
{
    std::string const buffs = DpsRotationMath::Monk::CRITICAL_STRIKE_BUFFS;
    // The value that finds a member without the buff reads the monk's own buff first.
    CHECK(buffs.rfind("legacy of the white tiger,", 0) == 0);
    for (char const* other : { "leader of the pack", "arcane brilliance", "dalaran brilliance", "furious howl",
             "terrifying roar", "fearless roar", "still water" })
        CHECK(buffs.find(other) != std::string::npos);
    CHECK(buffs.find("legacy of the emperor") == std::string::npos);
}

TEST(fire_pyroblast_opens_only_a_solo_fight_on_a_fresh_creature)
{
    using DpsRotationMath::Mage::PyroblastOpener;
    CHECK(PyroblastOpener(false, false, false, true));
    CHECK(!PyroblastOpener(true, false, false, true));   // the tank opens in a group
    CHECK(!PyroblastOpener(false, true, false, true));   // already fighting: the rotation takes over
    CHECK(!PyroblastOpener(false, false, true, true));   // players: the duel package decides
    CHECK(!PyroblastOpener(false, false, false, false)); // below level 10
}

TEST(shadow_expert_refreshes_dots_in_the_last_three_seconds)
{
    using namespace DpsRotationMath::Priest;
    CHECK(RefreshDot(true, 3000));
    CHECK(RefreshDot(true, 1));
    CHECK(!RefreshDot(true, 3001));
    CHECK(!RefreshDot(true, 0));  // gone: the normal DoT trigger recasts it
    CHECK(!RefreshDot(false, 2000));
}

TEST(arms_execute_pools_outside_the_window_from_81)
{
    using namespace DpsRotationMath::Warrior;
    CHECK(ExecuteRage(80, true, false) == EXECUTE_RAGE);
    CHECK(ExecuteRage(90, false, false) == EXECUTE_RAGE);
    CHECK(ExecuteRage(90, true, true) == EXECUTE_RAGE);
    CHECK(ExecuteRage(81, true, false) == EXECUTE_POOL_RAGE);
}

TEST(fire_presence_of_mind_and_alter_time_on_big_fights)
{
    using namespace DpsRotationMath::Mage;
    CHECK(PresenceOfMindNow(true, true, false));
    CHECK(!PresenceOfMindNow(false, true, false));
    CHECK(!PresenceOfMindNow(true, false, false));
    CHECK(!PresenceOfMindNow(true, true, true));
    CHECK(AlterTimeNow(ALTER_TIME_LEVEL, true, true, true, false));
    CHECK(!AlterTimeNow(ALTER_TIME_LEVEL - 1, true, true, true, false));
    CHECK(!AlterTimeNow(90, false, true, true, false));
    CHECK(!AlterTimeNow(90, true, true, false, false));
    CHECK(!AlterTimeNow(90, true, true, true, true));
}

TEST(shadow_mind_flay_clip_after_the_second_tick)
{
    using namespace DpsRotationMath::Priest;
    CHECK(!ClipMindFlay(true, true, 2500, 3000));
    CHECK(!ClipMindFlay(true, true, 1500, 3000));
    CHECK(ClipMindFlay(true, true, 1000, 3000));
    CHECK(ClipMindFlay(true, true, 700, 2100));
    CHECK(!ClipMindFlay(true, false, 900, 3000));
    CHECK(!ClipMindFlay(false, true, 900, 3000));
    CHECK(!ClipMindFlay(true, true, 0, 3000));
}

TEST(shadow_mind_sear_waits_for_bystanders)
{
    using namespace DpsRotationMath::Priest;
    CHECK(MindSearSafe(0, 0));
    CHECK(!MindSearSafe(1, 0));
    CHECK(!MindSearSafe(0, 1));
}

TEST(hunter_trap_launcher_and_pack_trap)
{
    using namespace DpsRotationMath::Hunter;
    CHECK(KeepTrapLauncher(TRAP_LAUNCHER_LEVEL, true, false));
    CHECK(!KeepTrapLauncher(TRAP_LAUNCHER_LEVEL - 1, true, false));
    CHECK(!KeepTrapLauncher(90, false, false));
    CHECK(!KeepTrapLauncher(90, true, true));
    CHECK(ExplosiveTrapAtPack(true, true, TRAP_PACK, 30.0f));
    CHECK(!ExplosiveTrapAtPack(false, true, TRAP_PACK, 30.0f));
    CHECK(!ExplosiveTrapAtPack(true, false, TRAP_PACK, 30.0f));
    CHECK(!ExplosiveTrapAtPack(true, true, TRAP_PACK - 1, 30.0f));
    CHECK(!ExplosiveTrapAtPack(true, true, TRAP_PACK, TRAP_LAUNCH_RANGE_YD + 1.0f));
}

TEST(rogue_level_80_burst_windows)
{
    using namespace DpsRotationMath::Rogue;
    CHECK(KillingSpreeNow(false, 100, false, false));
    CHECK(!KillingSpreeNow(true, 100, false, false));
    CHECK(KillingSpreeNow(true, KILLING_SPREE_ENERGY - 1, false, false));
    CHECK(!KillingSpreeNow(true, 10, true, false));
    CHECK(!KillingSpreeNow(false, 10, false, true));
    CHECK(ShadowDanceNow(80, true, SHADOW_DANCE_ENERGY, false, false));
    CHECK(!ShadowDanceNow(79, true, 100, false, false));
    CHECK(!ShadowDanceNow(90, false, 100, false, false));
    CHECK(!ShadowDanceNow(90, true, SHADOW_DANCE_ENERGY - 1, false, false));
    CHECK(!ShadowDanceNow(90, true, 100, true, false));
    CHECK(!ShadowDanceNow(90, true, 100, false, true));
    CHECK(SubtletyShadowBladesNow(false, true));
    CHECK(SubtletyShadowBladesNow(true, false));
    CHECK(!SubtletyShadowBladesNow(true, true));
}

TEST(death_knight_army_only_before_a_ready_boss_pull)
{
    using namespace DpsRotationMath::DeathKnight;
    CHECK(PrePullArmy(ARMY_LEVEL, true, true, true, true, 30.0f));
    CHECK(!PrePullArmy(ARMY_LEVEL - 1, true, true, true, true, 30.0f));
    CHECK(!PrePullArmy(90, false, true, true, true, 30.0f));
    CHECK(!PrePullArmy(90, true, false, true, true, 30.0f));  // a blood tank keeps its own rules
    CHECK(!PrePullArmy(90, true, true, false, true, 30.0f));
    CHECK(!PrePullArmy(90, true, true, true, false, 30.0f));
    CHECK(!PrePullArmy(90, true, true, true, true, ARMY_PREPULL_RANGE_YD + 1.0f));
}

TEST(warlock_dots_refresh_in_a_window_only_for_experts)
{
    using namespace DpsRotationMath::Warlock;
    CHECK(DotRefreshMs(true) == DOT_REFRESH_MS);
    CHECK(DotRefreshMs(false) == 0);
    // About one tick and a cast: more than the 1.5 s Unstable Affliction cast, far less than the 40 percent of an
    // 18 s Corruption that Method names, which would throw a third of every DoT away on 5.4.8.
    CHECK(DOT_REFRESH_MS > 1500);
    CHECK(DOT_REFRESH_MS < 18000 * 4 / 10);
}
