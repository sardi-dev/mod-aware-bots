#include "TestHarness.h"
#include "BgGenericPlay.h"

// BT-431: the generic battleground play for a map with no tactics of its own.

using namespace BgGenericPlay;

namespace
{
// A bot at its own start, a long way from both other goals.
Goal ChooseAtOwnStart(std::uint32_t role)
{
    return Choose(Options(role, 600.0f, 300.0f, 0.0f));
}

Goal ChooseAtEnemyStart(std::uint32_t role)
{
    return Choose(Options(role, 0.0f, 300.0f, 600.0f));
}
}  // namespace

TEST(generic_play_roles_split_six_three_one)
{
    unsigned push = 0, mid = 0, guard = 0;
    for (std::uint32_t role = 0; role < ROLE_COUNT; ++role)
    {
        Goal const goal = GoalOfRole(role);
        push += goal == Goal::PUSH_ENEMY_START ? 1 : 0;
        mid += goal == Goal::HOLD_MIDFIELD ? 1 : 0;
        guard += goal == Goal::GUARD_OWN_START ? 1 : 0;
    }
    CHECK((push) == (6u));
    CHECK((mid) == (3u));
    CHECK((guard) == (1u));
}

TEST(generic_play_every_role_goes_to_its_own_goal_from_the_spawn)
{
    // the pull toward the nearer goal never outweighs the role: 1 point per 50 yd, at most 10, against 60 between fits
    for (std::uint32_t role = 0; role < ROLE_COUNT; ++role)
    {
        CHECK(ChooseAtOwnStart(role) == GoalOfRole(role));
        CHECK(ChooseAtEnemyStart(role) == GoalOfRole(role));
    }
}

TEST(generic_play_fit_orders_own_next_far)
{
    CHECK((RoleFit(Goal::PUSH_ENEMY_START, 0)) == (FIT_OWN));
    CHECK((RoleFit(Goal::HOLD_MIDFIELD, 0)) == (FIT_NEXT));
    CHECK((RoleFit(Goal::GUARD_OWN_START, 0)) == (FIT_FAR));
    CHECK((RoleFit(Goal::GUARD_OWN_START, 9)) == (FIT_OWN));
    CHECK((RoleFit(Goal::HOLD_MIDFIELD, 9)) == (FIT_NEXT));
    CHECK((RoleFit(Goal::PUSH_ENEMY_START, 9)) == (FIT_FAR));
}

TEST(generic_play_the_nearer_goal_breaks_a_tie)
{
    // a midfield bot would be equally fit for both neighbours' ends; the options of equal fit go to the nearer one
    auto const options = Options(7, 400.0f, 10.0f, 400.0f);
    CHECK(Choose(options) == Goal::HOLD_MIDFIELD);
    CHECK(options[0].score == options[2].score);
}

TEST(generic_play_arrival_range)
{
    CHECK(Arrived(0.0f));
    CHECK(Arrived(ARRIVED_RANGE));
    CHECK(!Arrived(ARRIVED_RANGE + 0.5f));
}

TEST(generic_play_equal_scores_take_the_first_option)
{
    std::array<Option, 3> const tie = { { { Goal::PUSH_ENEMY_START, 5 }, { Goal::HOLD_MIDFIELD, 5 }, { Goal::GUARD_OWN_START, 5 } } };
    CHECK(Choose(tie) == Goal::PUSH_ENEMY_START);
}
