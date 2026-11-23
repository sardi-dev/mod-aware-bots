#include "TestHarness.h"
#include "DoorPathMath.h"

namespace {
struct Point { float x, y, z; Point(float a, float b, float c) : x(a), y(b), z(c) {} };
}
TEST(door_stop_on_hit_segment)
{
    std::vector<Point> path = { {0, 0, 0}, {10, 0, 0} };
    DoorPathMath::StopShort(path, 0, Point(7, 0, 0), 2);
    CHECK(path.size() == 2);
    CHECK_NEAR(path.back().x, 5, 0.001);
}
TEST(door_stop_retreats_around_corner)
{
    std::vector<Point> path = { {0, 0, 0}, {5, 0, 0}, {5, 5, 0} };
    DoorPathMath::StopShort(path, 1, Point(5, 1, 0), 2);
    CHECK(path.size() == 2);
    CHECK_NEAR(path.back().x, 4, 0.001);
    CHECK_NEAR(path.back().y, 0, 0.001);
}
TEST(door_stop_cannot_retreat_before_start)
{
    std::vector<Point> path = { {0, 0, 0}, {1, 0, 0} };
    DoorPathMath::StopShort(path, 0, Point(1, 0, 0), 2);
    CHECK(path.size() == 1);
}
TEST(door_stop_interpolates_height)
{
    std::vector<Point> path = { {0, 0, 0}, {0, 0, 10} };
    DoorPathMath::StopShort(path, 0, Point(0, 0, 8), 2);
    CHECK_NEAR(path.back().z, 6, 0.001);
}
