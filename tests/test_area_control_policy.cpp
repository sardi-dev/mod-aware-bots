#include "TestHarness.h"
#include "AreaControlPolicy.h"

TEST(area_control_rejects_breakable_control_in_radius)
{
    AreaControlPolicy::Count count;
    count.Add(true, 1.0f, 8.0f, false);
    count.Add(true, 2.0f, 8.0f, false);
    count.Add(true, 8.0f, 8.0f, false);
    CHECK(count.CanCast(3));
    count.Add(true, 8.01f, 8.0f, true);
    count.Add(false, 1.0f, 8.0f, true);
    CHECK(count.CanCast(3));
    count.Add(true, 8.0f, 8.0f, true);
    CHECK(!count.CanCast(3));
    CHECK(count.targets == 3);
}
