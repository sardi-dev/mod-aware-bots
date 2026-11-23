#include "TestHarness.h"
#include "ResurrectPolicy.h"

using namespace ResurrectPolicy;

namespace
{
Candidate Make(bool human, bool inSight, int role, float distance)
{
    return Candidate{ human, false, false, inSight, role, distance };
}
}  // namespace

TEST(resurrect_a_pending_request_or_a_cast_in_progress_is_not_eligible)
{
    Candidate pending = Make(true, true, ROLE_OTHER, 10.0f);
    pending.pendingRequest = true;
    CHECK(!Eligible(pending));
    Candidate cast = Make(true, true, ROLE_OTHER, 10.0f);
    cast.beingResurrected = true;
    CHECK(!Eligible(cast));
    CHECK(Eligible(Make(true, true, ROLE_OTHER, 10.0f)));
}

TEST(resurrect_too_far_is_not_eligible)
{
    CHECK(Eligible(Make(true, false, ROLE_OTHER, MAX_WALK_DISTANCE)));
    CHECK(!Eligible(Make(true, false, ROLE_OTHER, MAX_WALK_DISTANCE + 1.0f)));
}

TEST(resurrect_human_comes_before_bot_whatever_the_role_and_sight)
{
    Candidate human = Make(true, false, ROLE_OTHER, 90.0f);
    Candidate healerBot = Make(false, true, ROLE_HEALER, 5.0f);
    CHECK(Prefer(human, healerBot));
    CHECK(!Prefer(healerBot, human));
}

TEST(resurrect_in_sight_then_role_then_distance)
{
    CHECK(Prefer(Make(true, true, ROLE_OTHER, 40.0f), Make(true, false, ROLE_HEALER, 5.0f)));
    CHECK(Prefer(Make(false, true, ROLE_HEALER, 40.0f), Make(false, true, ROLE_TANK, 5.0f)));
    CHECK(Prefer(Make(false, true, ROLE_TANK, 5.0f), Make(false, true, ROLE_TANK, 6.0f)));
    CHECK(!Prefer(Make(false, true, ROLE_TANK, 5.0f), Make(false, true, ROLE_TANK, 5.0f)));
}

TEST(resurrect_corpse_is_the_target_only_for_a_ghost_away_from_its_body)
{
    CHECK(!UseCorpseTarget(false, 500.0f));     // not released: the player himself is the body
    CHECK(!UseCorpseTarget(true, 0.0f));        // ghost at its corpse
    CHECK(!UseCorpseTarget(true, GHOST_NEAR_CORPSE_DISTANCE));
    CHECK(UseCorpseTarget(true, GHOST_NEAR_CORPSE_DISTANCE + 0.5f));
    CHECK(UseCorpseTarget(true, -1.0f));        // ghost on another map
}

TEST(resurrect_corpse_cast_needs_sight_and_range_with_a_margin)
{
    CHECK(CorpseInCastRange(30.0f, true, 40.0f));
    CHECK(CorpseInCastRange(37.0f, true, 40.0f));
    CHECK(!CorpseInCastRange(37.5f, true, 40.0f));
    CHECK(!CorpseInCastRange(5.0f, false, 40.0f));
    // No range in the spell data: 20 yards.
    CHECK(CorpseInCastRange(20.0f, true, 0.0f));
    CHECK(!CorpseInCastRange(21.0f, true, 0.0f));
}
