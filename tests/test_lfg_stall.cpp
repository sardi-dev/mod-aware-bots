#include "TestHarness.h"
#include "LfgStallPolicy.h"

using namespace LfgStallPolicy;

TEST(lfg_stall_needs_every_boss_skipped)
{
    // Gnomeregan: five bosses, all skipped within 45 s, nothing pulled.
    CHECK(RunStalled(5, 5, false));
    CHECK(!RunStalled(5, 4, false));
    CHECK(!RunStalled(5, 0, false));
}

TEST(lfg_stall_one_boss_left_and_skipped_ends_the_run)
{
    // The Wicked Grotto: the run's last boss was skipped over and over.
    CHECK(RunStalled(1, 1, false));
}

TEST(lfg_stall_no_boss_left_is_no_stall)
{
    CHECK(!RunStalled(0, 0, false));
}

TEST(lfg_stall_a_pull_after_the_first_skip_keeps_the_run_going)
{
    CHECK(!RunStalled(3, 3, true));
    CHECK(PulledSince(2000, 1500));
    CHECK(PulledSince(1500, 1500));
    CHECK(!PulledSince(1499, 1500));
    CHECK(!PulledSince(0, 1500));
}

TEST(lfg_stall_pull_times_survive_the_clock_wrap)
{
    uint32_t const nearWrap = 0xFFFFFF00u;
    CHECK(PulledSince(0x00000100u, nearWrap));
    CHECK(!PulledSince(0xFFFFFE00u, nearWrap));
}

TEST(lfg_stall_cooldown_runs_for_the_hours_and_zero_disables_it)
{
    int64_t const now = 1000000;
    int64_t const until = CooldownEnd(now, 4);
    CHECK(until == now + 4 * 3600);
    CHECK(CoolingDown(now, until));
    CHECK(CoolingDown(until - 1, until));
    CHECK(!CoolingDown(until, until));
    CHECK(CooldownEnd(now, 0) == 0);
    CHECK(!CoolingDown(now, CooldownEnd(now, 0)));
}
