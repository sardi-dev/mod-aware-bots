#include "BgLeavePolicy.h"
#include "TestHarness.h"

// The `botscenario <bot|all> bgleave` verb: bots left flagged inside a battleground blocked the next bots-only match
// (live, build 28). `all` takes the bots of a bots-only match and the stale ones, never a match a human plays in.

using namespace BgLeavePolicy;

TEST(bgleave_all_is_read_in_any_case_and_only_exactly)
{
    CHECK(IsAllName("all"));
    CHECK(IsAllName("ALL"));
    CHECK(IsAllName("All"));
    CHECK(!IsAllName("alli"));
    CHECK(!IsAllName("al"));
    CHECK(!IsAllName(""));
    CHECK(!IsAllName("Allan"));
}

TEST(bgleave_all_takes_the_bots_of_a_bots_only_match)
{
    Facts f;
    f.isBot = true;
    f.inMatch = true;
    CHECK(IncludedInAll(f));
}

TEST(bgleave_all_leaves_a_match_with_a_human_alone)
{
    Facts f;
    f.isBot = true;
    f.inMatch = true;
    f.matchHasHuman = true;
    CHECK(!IncludedInAll(f));
}

TEST(bgleave_all_takes_a_bot_flagged_by_a_match_that_is_gone)
{
    Facts f;
    f.isBot = true;
    f.staleBgId = true;
    CHECK(IncludedInAll(f));
    // The stale flag has no match, so no human can be in it.
    f.matchHasHuman = true;
    CHECK(IncludedInAll(f));
}

TEST(bgleave_all_never_takes_a_human_or_a_bot_that_is_free)
{
    Facts human;
    human.inMatch = true;
    human.staleBgId = true;
    CHECK(!IncludedInAll(human));

    Facts free;
    free.isBot = true;
    CHECK(!IncludedInAll(free));
}

TEST(bgleave_landing_goes_to_the_entry_point_unless_that_is_a_match_map_too)
{
    CHECK(LandingFor(true, true, false) == Landing::ENTRY_POINT);
    // A bot put in by a test teleport has an entry point on a battleground map, or none at all.
    CHECK(LandingFor(true, true, true) == Landing::HOMEBIND);
    CHECK(LandingFor(true, false, false) == Landing::HOMEBIND);
    CHECK(LandingFor(true, false, true) == Landing::HOMEBIND);
}

TEST(bgleave_a_bot_already_out_on_a_normal_map_stays)
{
    CHECK(LandingFor(false, true, false) == Landing::STAY);
    CHECK(LandingFor(false, false, false) == Landing::STAY);
}

TEST(bgleave_landing_has_a_name_for_the_log)
{
    CHECK(std::string(LandingName(Landing::ENTRY_POINT)) == "entry");
    CHECK(std::string(LandingName(Landing::HOMEBIND)) == "homebind");
    CHECK(std::string(LandingName(Landing::STAY)) == "stay");
}

TEST(bgleave_a_dash_name_takes_the_target_from_the_next_word)
{
    CHECK(TargetName("-", "all") == "all");
    CHECK(TargetName("-", "Wahjak") == "Wahjak");
    CHECK(TargetName("all", "") == "all");
    CHECK(TargetName("Wahjak", "all") == "Wahjak");
    CHECK(TargetName("-", "") == "-");
}
