#include "InterruptPolicy.h"
#include "TestHarness.h"

TEST(interrupt_order_is_role_then_guid)
{
    using namespace InterruptPolicy;
    CHECK(Before(Role::TANK, 99, Role::MELEE, 1));
    CHECK(Before(Role::MELEE, 99, Role::RANGED, 1));
    CHECK(Before(Role::MELEE, 1, Role::MELEE, 2));
    CHECK(!Before(Role::MELEE, 2, Role::MELEE, 1));
}

TEST(interrupt_stagger_and_skill_delay)
{
    using namespace InterruptPolicy;
    CHECK(DelayMs(0, BotSkillMath::Tier::EXPERT) == 0);
    CHECK(DelayMs(1, BotSkillMath::Tier::EXPERT) == 500);
    CHECK(DelayMs(1, BotSkillMath::Tier::GOOD) == 650);
}

TEST(interrupt_casual_miss_is_stable_and_near_one_third)
{
    using namespace InterruptPolicy;
    unsigned misses = 0;
    for (std::uint32_t cast = 1; cast <= 12000; ++cast)
    {
        bool const casual = TakesTurn(BotSkillMath::Tier::CASUAL, 17, cast);
        CHECK(casual == TakesTurn(BotSkillMath::Tier::CASUAL, 17, cast));
        CHECK(TakesTurn(BotSkillMath::Tier::EXPERT, 17, cast));
        CHECK(TakesTurn(BotSkillMath::Tier::GOOD, 17, cast));
        misses += !casual;
    }
    CHECK(misses > 3700 && misses < 4300);
}
