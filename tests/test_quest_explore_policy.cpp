#include "TestHarness.h"
#include "QuestExplorePolicy.h"

#include <vector>

using namespace QuestExplorePolicy;

TEST(explore_sphere_trigger)
{
    Shape s;
    s.x = 100.0f;
    s.y = 200.0f;
    s.z = 10.0f;
    s.radius = 20.0f;
    CHECK(Inside(s, 100.0f, 200.0f, 10.0f));
    CHECK(Inside(s, 115.0f, 210.0f, 12.0f));
    CHECK(!Inside(s, 121.0f, 200.0f, 10.0f));
    // Below the sphere (the trigger of a cave entrance seen from the cliff above it).
    CHECK(!Inside(s, 100.0f, 200.0f, 35.0f));
}

TEST(explore_box_trigger_turned)
{
    // A box 40 long and 10 wide, turned a quarter: long along y in the world.
    Shape s;
    s.x = 0.0f;
    s.y = 0.0f;
    s.z = 0.0f;
    s.boxX = 40.0f;
    s.boxY = 10.0f;
    s.boxZ = 20.0f;
    s.boxOrientation = float(M_PI / 2);
    CHECK(Inside(s, 0.0f, 18.0f, 0.0f));
    CHECK(!Inside(s, 18.0f, 0.0f, 0.0f));
    CHECK(Inside(s, 4.0f, -19.0f, 9.0f));
    CHECK(!Inside(s, 0.0f, 0.0f, 11.0f));
}

TEST(explore_box_trigger_unturned)
{
    Shape s;
    s.boxX = 40.0f;
    s.boxY = 10.0f;
    s.boxZ = 20.0f;
    CHECK(Inside(s, 18.0f, 0.0f, 0.0f));
    CHECK(!Inside(s, 0.0f, 18.0f, 0.0f));
}

TEST(explore_nearest_trigger_in_reach)
{
    std::vector<Shape> shapes(3);
    shapes[0].x = 2000.0f;
    shapes[1].x = 300.0f;
    shapes[2].x = -200.0f;
    CHECK(Nearest(shapes, 0.0f, 0.0f) == 2);
    CHECK(Nearest(shapes, 260.0f, 0.0f) == 1);
    std::vector<Shape> far(1);
    far[0].x = MAX_TRIP_DIST + 1.0f;
    CHECK(Nearest(far, 0.0f, 0.0f) == -1);
    CHECK(Nearest(std::vector<Shape>(), 0.0f, 0.0f) == -1);
}
