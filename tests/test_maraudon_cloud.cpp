#include "TestHarness.h"
#include "MaraudonCloudLogic.h"

TEST(maraudon_cloud_uses_the_live_poison_spell_and_lifetime)
{
    CHECK(MaraudonCloud::Relevant(21070, 1000, 5.0f, 0.0f));
    CHECK(!MaraudonCloud::Relevant(21068, 1000, 5.0f, 0.0f));
    CHECK(!MaraudonCloud::Relevant(21070, 0, 5.0f, 0.0f));
    CHECK(!MaraudonCloud::Relevant(21070, -1, 5.0f, 0.0f));
}
TEST(maraudon_cloud_ignores_other_floors_and_invalid_radii)
{
    CHECK(MaraudonCloud::Relevant(21070, 1000, 5.0f, -4.0f));
    CHECK(!MaraudonCloud::Relevant(21070, 1000, 5.0f, 10.0f));
    CHECK(!MaraudonCloud::Relevant(21070, 1000, 5.0f, -10.0f));
    CHECK(!MaraudonCloud::Relevant(21070, 1000, 0.0f, 0.0f));
    CHECK(!MaraudonCloud::Relevant(21070, 1000, 100.0f, 0.0f));
}
