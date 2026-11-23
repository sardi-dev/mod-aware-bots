#include "TestHarness.h"
#include "TrainingDummyPolicy.h"

TEST(training_dummy_scripts_are_recognised)
{
    CHECK(TrainingDummyPolicy::IsDummyScript("npc_training_dummy"));
    CHECK(TrainingDummyPolicy::IsDummyScript("npc_tournament_training_dummy"));
    CHECK(!TrainingDummyPolicy::IsDummyScript("npc_ingvar_throw_dummy"));
    CHECK(!TrainingDummyPolicy::IsDummyScript(""));
    CHECK(!TrainingDummyPolicy::IsDummyScript(nullptr));
}

TEST(training_dummy_only_when_the_leader_picks_it)
{
    CHECK(TrainingDummyPolicy::Allowed(false, false));
    CHECK(!TrainingDummyPolicy::Allowed(true, false));
    CHECK(TrainingDummyPolicy::Allowed(true, true));
}
