#include "TestHarness.h"
#include "PullSightPolicy.h"

#include <cmath>

TEST(pull_sight_spots_lie_on_their_rings_all_round_the_mark)
{
    using namespace PullSightPolicy;
    for (unsigned i = 0; i < SPOT_COUNT; ++i)
    {
        float dx = 0.0f, dy = 0.0f;
        Offset(i, dx, dy);
        CHECK(std::fabs(std::sqrt(dx * dx + dy * dy) - RING_YARDS[i / SPOTS_PER_RING]) < 0.01f);
    }
    float dx = 0.0f, dy = 0.0f;
    Offset(0, dx, dy);
    CHECK(dx > 7.9f && std::fabs(dy) < 0.01f);
    Offset(SPOTS_PER_RING / 2, dx, dy);
    CHECK(dx < -7.9f);
}

TEST(pull_sight_spot_must_stand_on_the_marks_floor_and_have_a_short_path)
{
    using namespace PullSightPolicy;
    CHECK(SameFloor(52.9f, 54.0f));
    CHECK(!SameFloor(52.9f, 57.0f));
    CHECK(PathIsShort(20.0f, 18.0f));
    CHECK(!PathIsShort(80.0f, 18.0f));
}

TEST(pull_sight_search_is_rate_limited)
{
    using namespace PullSightPolicy;
    CHECK(MaySearch(0, 1000));
    CHECK(!MaySearch(1000, 1000 + RETRY_SECONDS - 1));
    CHECK(MaySearch(1000, 1000 + RETRY_SECONDS));
}
