#include "TestHarness.h"
#include <string>
#include "HealerGuardrails.h"

using namespace HealerGuardrails;

TEST(healer_tank_upkeep_needs_more_than_30_percent_mana)
{
    CHECK(!TankUpkeepAllowed(0));
    CHECK(!TankUpkeepAllowed(30));
    CHECK(TankUpkeepAllowed(31));
    CHECK(TankUpkeepAllowed(100));
}

TEST(healer_member_upkeep_bands)
{
    // Below or at 30 percent: reactive heals only, whatever happened to the member.
    CHECK(!MemberProactiveAllowed(30, true));
    CHECK(!MemberProactiveAllowed(10, false));
    // Between 30 and 60 percent: only a member who took damage a moment ago.
    CHECK(MemberProactiveAllowed(45, true));
    CHECK(!MemberProactiveAllowed(45, false));
    CHECK(!MemberProactiveAllowed(60, false));
    // Above 60 percent: anyone hurt.
    CHECK(MemberProactiveAllowed(61, false));
    CHECK(MemberProactiveAllowed(100, false));
}

TEST(healer_asks_to_drink_at_or_below_30_percent)
{
    CHECK(ShouldAskToDrink(0));
    CHECK(ShouldAskToDrink(30));
    CHECK(!ShouldAskToDrink(31));
}

TEST(healer_pull_imminent_from_mark_or_tank_target)
{
    CHECK(PullImminent({ true, false, true, false }));
    CHECK(PullImminent({ true, false, false, true }));
    CHECK(PullImminent({ true, false, true, true }));
    // No tank in sight, a fight already running, or nothing to pull: nothing to prepare.
    CHECK(!PullImminent({ false, false, true, true }));
    CHECK(!PullImminent({ true, true, true, true }));
    CHECK(!PullImminent({ true, false, false, false }));
}

TEST(healer_pre_pull_window_ends_after_20_seconds_per_target)
{
    PrePullWindow window;
    CHECK(!window.Allows(0, 1000));
    CHECK(window.Allows(77, 1000));
    CHECK(window.Allows(77, 20999));
    CHECK(!window.Allows(77, 21000));
    // A new target opens a new window.
    CHECK(window.Allows(78, 21500));
    // The target went away: the next pull of the same mob starts again.
    CHECK(!window.Allows(0, 22000));
    CHECK(window.Allows(78, 30000));
}

TEST(healer_pre_pull_window_survives_timer_wrap)
{
    PrePullWindow window;
    std::uint32_t const start = 0xFFFFFF00u;
    CHECK(window.Allows(5, start));
    CHECK(window.Allows(5, start + 5000));
    CHECK(!window.Allows(5, start + 25000));
}

TEST(healer_damage_window_sees_a_drop_and_forgets_it)
{
    DamageWindow window;
    CHECK(!window.Observe(1, 100, 1000));   // first sight
    CHECK(!window.Observe(1, 100, 1500));   // no change
    CHECK(window.Observe(1, 90, 2000));     // health fell
    CHECK(window.Observe(1, 95, 3000));     // healed up a little, the drop is still recent
    CHECK(window.Observe(1, 95, 8000));     // exactly at the window edge
    CHECK(!window.Observe(1, 95, 8500));    // forgotten
}

TEST(healer_damage_window_tracks_members_apart_and_recycles)
{
    DamageWindow window;
    CHECK(!window.Observe(1, 100, 1000));
    CHECK(!window.Observe(2, 100, 1000));
    CHECK(window.Observe(2, 80, 1200));
    CHECK(!window.Observe(1, 100, 1300));   // member 1 never dropped
    // More members than slots: the one seen the longest ago is replaced and starts over.
    for (std::uint64_t id = 10; id < 10 + DamageWindow::CAPACITY; ++id)
        window.Observe(id, 100, 2000 + std::uint32_t(id));
    CHECK(!window.Observe(2, 80, 2100));
}

TEST(healer_damage_window_ignores_a_stale_sample)
{
    DamageWindow window;
    CHECK(!window.Observe(1, 100, 1000));
    // Not looked at for a long time: a lower health now is no fresh evidence of damage.
    CHECK(!window.Observe(1, 50, 20000));
    CHECK(window.Observe(1, 40, 20500));
}

TEST(healer_unhealed_member_needs_health_and_time)
{
    CHECK(!HealerGuardrails::MemberStaysHurt(59, 4));
    CHECK(HealerGuardrails::MemberStaysHurt(59, 5));
    CHECK(!HealerGuardrails::MemberStaysHurt(60, 60));
}
TEST(healer_no_heal_reason_order)
{
    CHECK(std::string(HealerGuardrails::NoHealReason(false, false, 0)) == "no line of sight");
    CHECK(std::string(HealerGuardrails::NoHealReason(true, false, 0)) == "out of heal range");
    CHECK(std::string(HealerGuardrails::NoHealReason(true, true, 5)) == "no mana");
    CHECK(std::string(HealerGuardrails::NoHealReason(true, true, 6)).empty());
    CHECK(HealerGuardrails::ReasonNeedsWalk(false, true));
    CHECK(HealerGuardrails::ReasonNeedsWalk(true, false));
    CHECK(!HealerGuardrails::ReasonNeedsWalk(true, true));
}
TEST(healer_melee_fallback_only_when_it_can_start)
{
    // Deadmines runs 4 and 5: the mistweaver's idle auto attack failed 101 and 31 times.
    CHECK(!HealerGuardrails::MeleeFallbackWorthTrying(false, false, true, false));
    CHECK(!HealerGuardrails::MeleeFallbackWorthTrying(true, true, true, true));
    CHECK(!HealerGuardrails::MeleeFallbackWorthTrying(true, false, true, false));
    CHECK(HealerGuardrails::MeleeFallbackWorthTrying(true, false, true, true));
}
TEST(healer_melee_fallback_solo_keeps_its_reach)
{
    CHECK(HealerGuardrails::MeleeFallbackWorthTrying(true, false, false, false));
    CHECK(!HealerGuardrails::MeleeFallbackWorthTrying(false, false, false, false));
}

TEST(healer_dispel_waits_while_a_healable_member_is_dying)
{
    using namespace HealerGuardrails;
    CHECK(DispelWaits(15.0f, true, false));
    CHECK(!DispelWaits(TRIAGE_HEALTH_PCT, true, false));
    // A member the healer cannot heal now (out of range or sight) does not hold the dispel: the walk owns that case.
    CHECK(!DispelWaits(15.0f, false, false));
    // A dispel on the tank never waits.
    CHECK(DispelWaits(15.0f, true, false) && !DispelWaits(15.0f, true, true));
}
