#include "BotEventQuestPolicy.h"
#include "TestHarness.h"

TEST(event_quest_policy_requires_active_event_and_cooldown)
{
    using BotEventQuestPolicy::Allowed;
    CHECK(Allowed(false, false, false, false, false, true));
    CHECK(!Allowed(false, true, true, false, false, true));
    CHECK(!Allowed(false, true, true, true, false, true));
    CHECK(!Allowed(true, false, false, false, false, true));
    CHECK(Allowed(true, true, false, false, false, true));
    CHECK(!Allowed(true, true, true, false, false, true));
    CHECK(Allowed(true, true, true, true, false, true));
    CHECK(!Allowed(true, true, true, true, false, false));
    CHECK(Allowed(true, true, true, false, true, true));
    CHECK(!Allowed(true, true, true, false, true, false));
}
