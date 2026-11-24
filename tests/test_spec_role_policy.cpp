#include "TestHarness.h"
#include "SpecRolePolicy.h"

using namespace SpecRolePolicy;

TEST(spec_role_prefers_missing_roles_in_finder_band)
{
    uint32_t shares[] = {25, 20, 55};
    bool all[] = {true, true, true};
    uint32_t counts[] = {0, 3, 20};
    CHECK(Choose(counts, shares, all, 35) == Tank);
    counts[0] = 4;
    CHECK(Choose(counts, shares, all, 35) == Healer);
}

TEST(spec_role_uses_shares_and_class_availability)
{
    uint32_t shares[] = {25, 20, 55};
    uint32_t counts[] = {5, 4, 2};
    bool all[] = {true, true, true};
    bool damageOnly[] = {false, false, true};
    CHECK(Choose(counts, shares, all, 60) == Damage);
    CHECK(Choose(counts, shares, damageOnly, 30) == Damage);
    bool healDamage[] = {false, true, true};
    uint32_t shortage[] = {0, 0, 10};
    CHECK(Choose(shortage, shares, healDamage, 30) == Healer);
}

TEST(spec_role_floor_and_zero_shares_have_boundaries)
{
    uint32_t shares[] = {25, 20, 55};
    uint32_t counts[] = {3, 4, 0};
    bool all[] = {true, true, true};
    CHECK(Choose(counts, shares, all, 24) == Damage);
    CHECK(Choose(counts, shares, all, 25) == Tank);
    CHECK(Choose(counts, shares, all, 54) == Tank);
    CHECK(Choose(counts, shares, all, 55) == Damage);
    uint32_t noTank[] = {0, 20, 55};
    CHECK(Choose(counts, noTank, all, 30) == Damage);
    uint32_t none[] = {0, 0, 0};
    CHECK(Choose(counts, none, all, 30) == -1);
}
