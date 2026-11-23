#include "TestHarness.h"
#include "PvpElementalPolicy.h"

// BT-677 to BT-681: the Elemental Shaman package: Ancestral Swiftness for the Hex, Grounding on an incoming control and the reset
// of the totems, the burst held for the window, and Spiritwalker's Grace.

using namespace PvpElemental;

TEST(ele_swiftness_is_pressed_before_the_hex_when_a_kick_could_stop_it)
{
    SwiftnessInputs in;
    in.ready = true;
    in.hexDue = true;
    in.enemyKickNotSpent = true;
    CHECK(SwiftnessForHex(in, Tier::EXPERT) == SwiftnessReason::PRESS);
    in.enemyKickNotSpent = false;
    CHECK(SwiftnessForHex(in, Tier::EXPERT) == SwiftnessReason::NO_KICK_TO_FEAR);
}

TEST(ele_swiftness_needs_the_hex_to_be_due_a_ready_spell_and_an_expert)
{
    SwiftnessInputs in;
    in.ready = true;
    in.enemyKickNotSpent = true;
    CHECK(SwiftnessForHex(in, Tier::EXPERT) == SwiftnessReason::HEX_NOT_DUE);
    in.hexDue = true;
    CHECK(SwiftnessForHex(in, Tier::GOOD) == SwiftnessReason::TIER_SKIPS);
    CHECK(SwiftnessForHex(in, Tier::CASUAL) == SwiftnessReason::TIER_SKIPS);
    in.ready = false;
    CHECK(SwiftnessForHex(in, Tier::EXPERT) == SwiftnessReason::NOT_READY);
}

TEST(ele_swiftness_is_kept_for_the_hex_in_a_team_fight)
{
    CHECK(SwiftnessHeld(true, true, true, Tier::EXPERT));
    CHECK(SwiftnessHeld(true, true, true, Tier::GOOD));
    CHECK(!SwiftnessHeld(true, true, true, Tier::CASUAL));
    CHECK(!SwiftnessHeld(false, true, true, Tier::EXPERT));   // a duel or the open world: cast it as before
    CHECK(!SwiftnessHeld(true, false, true, Tier::EXPERT));   // no Hex to keep it for
    CHECK(!SwiftnessHeld(true, true, false, Tier::EXPERT));   // the Hex is on cooldown: the buff has no use to wait for
}

namespace
{
GroundInputs Cast(bool atMe, std::uint32_t left)
{
    GroundInputs in;
    in.ready = true;
    in.incomingControl = true;
    in.atShaman = atMe;
    in.leftMs = left;
    return in;
}
}  // namespace

TEST(ele_grounding_expert_covers_an_ally_and_good_bot_only_itself)
{
    CHECK(Grounding(Cast(true, 1000), Tier::EXPERT) == GroundReason::GROUND);
    CHECK(Grounding(Cast(false, 1000), Tier::EXPERT) == GroundReason::GROUND);
    CHECK(Grounding(Cast(true, 1000), Tier::GOOD) == GroundReason::GROUND);
    CHECK(Grounding(Cast(false, 1000), Tier::GOOD) == GroundReason::NOT_FOR_ME);
    CHECK(Grounding(Cast(true, 1000), Tier::CASUAL) == GroundReason::TIER_SKIPS);
}

TEST(ele_grounding_needs_time_left_by_tier)
{
    CHECK(Grounding(Cast(true, GROUND_MIN_LEFT_EXPERT_MS), Tier::EXPERT) == GroundReason::GROUND);
    CHECK(Grounding(Cast(true, GROUND_MIN_LEFT_EXPERT_MS - 1), Tier::EXPERT) == GroundReason::TOO_LATE);
    CHECK(Grounding(Cast(true, GROUND_MIN_LEFT_EXPERT_MS), Tier::GOOD) == GroundReason::TOO_LATE);
    CHECK(Grounding(Cast(true, GROUND_MIN_LEFT_GOOD_MS), Tier::GOOD) == GroundReason::GROUND);
}

TEST(ele_grounding_needs_the_totem_and_a_control_cast)
{
    GroundInputs in = Cast(true, 1000);
    in.ready = false;
    CHECK(Grounding(in, Tier::EXPERT) == GroundReason::NOT_READY);
    in = Cast(true, 1000);
    in.incomingControl = false;
    CHECK(Grounding(in, Tier::EXPERT) == GroundReason::NOTHING_INCOMING);
}

TEST(ele_call_of_the_elements_brings_the_grounding_back_for_the_next_control)
{
    CHECK(CallOfTheElementsNow(true, true, true, 1700, Tier::EXPERT));
    CHECK(!CallOfTheElementsNow(false, true, true, 1700, Tier::EXPERT));   // the Grounding is still there
    CHECK(!CallOfTheElementsNow(true, false, true, 1700, Tier::EXPERT));
    CHECK(!CallOfTheElementsNow(true, true, false, 1700, Tier::EXPERT));
    CHECK(!CallOfTheElementsNow(true, true, true, 500, Tier::EXPERT));     // too little of the cast left for two totems
    CHECK(!CallOfTheElementsNow(true, true, true, 1700, Tier::GOOD));
}

TEST(ele_burst_is_held_for_the_window_against_a_player)
{
    CHECK(Burst(true, false, Tier::EXPERT) == BurstReason::HOLD);
    CHECK(!BurstAllowed(Burst(true, false, Tier::GOOD)));
    CHECK(Burst(true, true, Tier::EXPERT) == BurstReason::WINDOW);
    CHECK(BurstAllowed(Burst(true, true, Tier::GOOD)));
}

TEST(ele_burst_pve_and_casual_bots_keep_the_old_rotation)
{
    CHECK(Burst(false, false, Tier::EXPERT) == BurstReason::NOT_A_PLAYER);
    CHECK(BurstAllowed(Burst(false, false, Tier::EXPERT)));
    CHECK(Burst(true, false, Tier::CASUAL) == BurstReason::CASUAL_FREE);
    CHECK(BurstAllowed(Burst(true, false, Tier::CASUAL)));
}

TEST(ele_grace_is_pressed_when_running_with_pressure_in_reach_by_tier)
{
    GraceInputs in;
    in.ready = true;
    in.moving = true;
    in.hasTargetInReach = true;
    in.nearestEnemyPlayerYd = 25.0f;
    CHECK(SpiritwalkersGrace(in, Tier::EXPERT) == GraceReason::PRESS);
    CHECK(SpiritwalkersGrace(in, Tier::GOOD) == GraceReason::NO_PRESSURE);   // the good line is 15 yd
    in.nearestEnemyPlayerYd = 12.0f;
    CHECK(SpiritwalkersGrace(in, Tier::GOOD) == GraceReason::PRESS);
}

TEST(ele_grace_needs_movement_a_target_a_ready_spell_and_a_non_casual_bot)
{
    GraceInputs in;
    in.ready = true;
    in.moving = false;
    in.hasTargetInReach = true;
    in.nearestEnemyPlayerYd = 5.0f;
    CHECK(SpiritwalkersGrace(in, Tier::EXPERT) == GraceReason::NOT_MOVING);
    in.moving = true;
    in.hasTargetInReach = false;
    CHECK(SpiritwalkersGrace(in, Tier::EXPERT) == GraceReason::NO_PRESSURE);
    in.hasTargetInReach = true;
    in.ready = false;
    CHECK(SpiritwalkersGrace(in, Tier::EXPERT) == GraceReason::NOT_READY);
    in.ready = true;
    CHECK(SpiritwalkersGrace(in, Tier::CASUAL) == GraceReason::TIER_SKIPS);
}
