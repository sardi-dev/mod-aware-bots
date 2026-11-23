#include "TestHarness.h"
#include "BuffRecastPolicy.h"

TEST(buff_recast_same_target_waits)
{
    CHECK(!BuffRecastPolicy::RecastAllowed(true, 4));
    CHECK(!BuffRecastPolicy::RecastAllowed(true, 29));
    CHECK(BuffRecastPolicy::RecastAllowed(true, 30));
}
TEST(buff_recast_other_target_is_free)
{
    CHECK(BuffRecastPolicy::RecastAllowed(false, 0));
}
