#include "TestHarness.h"
#include "DpsCooldownPolicy.h"
TEST(dps_cooldowns_require_boss_or_elite_pack)
{
    using namespace DpsCooldownPolicy;
    CHECK(!NeedsGate(179999, false));
    CHECK(NeedsGate(180000, false));
    CHECK(NeedsGate(0, true));
    CHECK(!Allowed(false, 0));
    CHECK(!Allowed(false, 1));
    CHECK(Allowed(false, 2));
    CHECK(Allowed(true, 0));
}

TEST(dps_cooldown_gate_leaves_group_utility_alone)
{
    CHECK(DpsCooldownPolicy::ServesTheGroup("innervate"));
    CHECK(!DpsCooldownPolicy::ServesTheGroup("bloodlust"));
    CHECK(!DpsCooldownPolicy::ServesTheGroup("recklessness"));
    // The damage cooldowns stay behind the gate.
    CHECK(DpsCooldownPolicy::NeedsGate(180000, false));
    CHECK(DpsCooldownPolicy::NeedsGate(0, true));
}
