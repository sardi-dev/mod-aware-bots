#include "TestHarness.h"
#include "TpMidLockPolicy.h"
#include "WsgPlanLogic.h"

// BT-705: the lane detour outranks the fight a taker stands in (live run, build 30: takers on the detour stayed in the brawl).
// BT-706: in a Twin Peaks stalemate (both flags held) the guards and the first escort hunt as well after 4 minutes (live run,
// build 30: one hunter per team against a carrier with healer and guards, 13 minutes, no carrier died).

namespace
{
using namespace WsgLogic;

DecisionContext Stalemate(Role role, std::uint32_t index, bool allIn)
{
    DecisionContext c;
    c.role = role;
    c.roleIndex = index;
    c.tier = Tier::GOOD;
    c.ourFlag = FLAG_ON_PLAYER;     // the enemy carries our flag
    c.theirFlag = FLAG_ON_PLAYER;   // we carry theirs: both held
    c.efcKnown = true;
    c.bothHeldMs = 6u * 60u * 1000u;
    c.ownDebuff = 10;
    c.enemyDebuff = 10;
    c.efcToOurStand = 400.0f;
    c.stalemateAllIn = allIn;
    return c;
}

bool Offers(std::vector<Option> const& options, Kind kind)
{
    for (Option const& o : options)
        if (o.kind == kind)
            return true;
    return false;
}
}  // namespace

TEST(detour_it_outranks_combat_for_a_taker_with_health_left)
{
    CHECK(TpMidLock::DetourOverridesCombat(true, true, 100.0f));
    CHECK(TpMidLock::DetourOverridesCombat(true, true, TpMidLock::DETOUR_MIN_HEALTH_PCT));
}

TEST(detour_a_bot_that_is_low_dead_or_not_on_the_detour_fights_by_the_usual_rules)
{
    CHECK(!TpMidLock::DetourOverridesCombat(true, true, TpMidLock::DETOUR_MIN_HEALTH_PCT - 1.0f));
    CHECK(!TpMidLock::DetourOverridesCombat(true, false, 100.0f));
    CHECK(!TpMidLock::DetourOverridesCombat(false, true, 100.0f));
}

TEST(allin_the_clock_is_four_minutes_of_both_flags_held)
{
    CHECK(!StalemateAllIn(0));
    CHECK(!StalemateAllIn(STALEMATE_ALL_IN_MS - 1));
    CHECK(StalemateAllIn(STALEMATE_ALL_IN_MS));
}

TEST(allin_before_it_one_bot_hunts_the_guards_and_the_first_escort_stay)
{
    // The live shape: home guard index 0 and escort index 0 held their posts, the backup hunted alone.
    CHECK(!Offers(Options(Stalemate(Role::HOME, 0, false)), Kind::HUNT_EFC));
    CHECK(!Offers(Options(Stalemate(Role::ESCORT, 0, false)), Kind::HUNT_EFC));
    CHECK(Offers(Options(Stalemate(Role::BACKUP, 0, false)), Kind::HUNT_EFC));
}

TEST(allin_after_it_the_guards_and_the_first_escort_hunt_too)
{
    std::vector<Option> const home = Options(Stalemate(Role::HOME, 0, true));
    CHECK(home.front().kind == Kind::HUNT_EFC);
    CHECK(Offers(home, Kind::HOLD_HOME_POST));   // the post stays the fallback behind it

    CHECK(Offers(Options(Stalemate(Role::ESCORT, 0, true)), Kind::HUNT_EFC));
    CHECK(Offers(Options(Stalemate(Role::HOME, 1, true)), Kind::HUNT_EFC));
}

TEST(allin_a_casual_guard_keeps_its_post_and_nobody_hunts_without_both_flags_held)
{
    DecisionContext casual = Stalemate(Role::HOME, 0, true);
    casual.tier = Tier::CASUAL;
    CHECK(!Offers(Options(casual), Kind::HUNT_EFC));

    // Our flag is home: no stalemate, nobody is sent by this rule.
    DecisionContext ours = Stalemate(Role::HOME, 0, true);
    ours.ourFlag = FLAG_ON_BASE;
    CHECK(!Offers(Options(ours), Kind::HUNT_EFC));

    // Their flag is home: the same.
    DecisionContext theirs = Stalemate(Role::HOME, 0, true);
    theirs.theirFlag = FLAG_ON_BASE;
    CHECK(!Offers(Options(theirs), Kind::HUNT_EFC));
}

TEST(allin_the_carrier_and_the_healers_are_not_sent)
{
    DecisionContext carrier = Stalemate(Role::TAKER, 0, true);
    carrier.hasFlag = true;
    CHECK(!Offers(Options(carrier), Kind::HUNT_EFC));

    DecisionContext healer = Stalemate(Role::ESCORT, 0, true);
    healer.healerFollower = true;
    CHECK(Options(healer).front().kind == Kind::HEAL_CARRIER);
}
