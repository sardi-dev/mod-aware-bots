#include "TestHarness.h"
#include "PvpHolyPaladinPolicy.h"

// BT-683 to BT-687: the Holy Paladin package: Denounce in the downtime, Hand of Sacrifice before a control, Hand of Protection on a
// stunned or burst caster, Devotion Aura in a silence, and the Beacon that follows the focus.

using namespace PvpHolyPaladin;

namespace
{
DenounceInputs Quiet()
{
    DenounceInputs in;
    in.known = true;
    in.targetIsPlayer = true;
    in.targetDistance = 25.0f;
    return in;
}
}  // namespace

TEST(holy_denounce_in_the_downtime_by_tier_line)
{
    DenounceInputs in = Quiet();
    CHECK(Denounce(in, Tier::EXPERT) == DenounceReason::DOWNTIME);
    in.lowestAllyPercent = 85.0f;
    CHECK(Denounce(in, Tier::EXPERT) == DenounceReason::DOWNTIME);
    CHECK(Denounce(in, Tier::GOOD) == DenounceReason::ALLY_NEEDS_HEAL);   // the good line is 90
    in.lowestAllyPercent = 79.0f;
    CHECK(Denounce(in, Tier::EXPERT) == DenounceReason::ALLY_NEEDS_HEAL);
}

TEST(holy_denounce_never_on_the_run_into_a_sheep_or_into_an_unspent_kick_for_an_expert)
{
    DenounceInputs in = Quiet();
    in.moving = true;
    CHECK(Denounce(in, Tier::EXPERT) == DenounceReason::MOVING);
    in = Quiet();
    in.targetBreakableCc = true;
    CHECK(Denounce(in, Tier::GOOD) == DenounceReason::BREAKABLE_CC);
    in = Quiet();
    in.enemyKickNotSpent = true;
    CHECK(Denounce(in, Tier::EXPERT) == DenounceReason::KICK_RISK);
    CHECK(Denounce(in, Tier::GOOD) == DenounceReason::DOWNTIME);   // a good bot does not read the kick
}

TEST(holy_denounce_needs_a_player_in_reach_and_a_known_spell_and_skips_casual_bots)
{
    DenounceInputs in = Quiet();
    in.targetIsPlayer = false;
    CHECK(Denounce(in, Tier::EXPERT) == DenounceReason::NOT_A_PLAYER);
    in = Quiet();
    in.targetDistance = 31.0f;
    CHECK(Denounce(in, Tier::EXPERT) == DenounceReason::OUT_OF_REACH);
    in = Quiet();
    in.known = false;
    CHECK(Denounce(in, Tier::EXPERT) == DenounceReason::NOT_KNOWN);
    CHECK(Denounce(Quiet(), Tier::CASUAL) == DenounceReason::CASUAL_SKIPS);
}

namespace
{
SacrificeInputs Ally()
{
    SacrificeInputs in;
    in.ready = true;
    return in;
}
}  // namespace

TEST(holy_sacrifice_goes_on_before_the_control_lands_for_an_expert)
{
    SacrificeInputs in = Ally();
    in.controlLeftMs = 900;
    SacrificeVerdict v = HandOfSacrifice(in, Tier::EXPERT);
    CHECK(v.cast);
    CHECK(v.reason == SacrificeReason::BEFORE_CONTROL);
    in.controlLeftMs = SACRIFICE_MIN_LEFT_MS - 1;
    CHECK(!HandOfSacrifice(in, Tier::EXPERT).cast);
    in.controlLeftMs = 900;
    CHECK(!HandOfSacrifice(in, Tier::GOOD).cast);   // a good bot does not plan for the cast
}

TEST(holy_sacrifice_emergency_needs_a_player_hitting_and_a_healthy_paladin)
{
    SacrificeInputs in = Ally();
    in.playerAttackers = 1;
    in.allyHealthPercent = 40.0f;
    CHECK(HandOfSacrifice(in, Tier::EXPERT).reason == SacrificeReason::EMERGENCY);
    CHECK(HandOfSacrifice(in, Tier::GOOD).reason == SacrificeReason::NO_THREAT);   // good line is 35
    in.allyHealthPercent = 35.0f;
    CHECK(HandOfSacrifice(in, Tier::GOOD).cast);
    in.paladinHealthPercent = 50.0f;
    CHECK(!HandOfSacrifice(in, Tier::EXPERT).cast);
    in = Ally();
    in.allyHealthPercent = 20.0f;
    CHECK(HandOfSacrifice(in, Tier::EXPERT).reason == SacrificeReason::NO_THREAT);
}

TEST(holy_sacrifice_not_on_self_twice_unready_or_by_a_casual_bot)
{
    SacrificeInputs in = Ally();
    in.controlLeftMs = 900;
    in.allyIsSelf = true;
    CHECK(HandOfSacrifice(in, Tier::EXPERT).reason == SacrificeReason::ON_SELF);
    in = Ally();
    in.controlLeftMs = 900;
    in.allyHasSacrifice = true;
    CHECK(HandOfSacrifice(in, Tier::EXPERT).reason == SacrificeReason::ALREADY_SACRIFICED);
    in = Ally();
    in.controlLeftMs = 900;
    in.ready = false;
    CHECK(!HandOfSacrifice(in, Tier::EXPERT).cast);
    in.ready = true;
    CHECK(HandOfSacrifice(in, Tier::CASUAL).reason == SacrificeReason::CASUAL_SKIPS);
}

TEST(holy_sacrifice_score_prefers_the_more_hurt_ally_and_the_control)
{
    SacrificeInputs a = Ally();
    a.controlLeftMs = 900;
    a.allyHealthPercent = 90.0f;
    SacrificeInputs b = a;
    b.allyHealthPercent = 60.0f;
    CHECK(HandOfSacrifice(b, Tier::EXPERT).score > HandOfSacrifice(a, Tier::EXPERT).score);
    SacrificeInputs e = Ally();
    e.playerAttackers = 1;
    e.allyHealthPercent = 40.0f;
    SacrificeInputs c = Ally();
    c.controlLeftMs = 900;
    c.allyHealthPercent = 40.0f;
    CHECK(HandOfSacrifice(c, Tier::EXPERT).score > HandOfSacrifice(e, Tier::EXPERT).score);
}

namespace
{
ProtectionInputs Mauled(float pct)
{
    ProtectionInputs in;
    in.ready = true;
    in.meleeAttackers = 1;
    in.allyHealthPercent = pct;
    return in;
}
}  // namespace

TEST(holy_protection_on_a_stunned_caster_under_melee_for_an_expert)
{
    ProtectionInputs in = Mauled(70.0f);
    in.allyStunned = true;
    ProtectionVerdict v = HandOfProtection(in, Tier::EXPERT);
    CHECK(v.cast);
    CHECK(v.reason == ProtectionReason::STUNNED);
    in.allyHealthPercent = 71.0f;
    CHECK(!HandOfProtection(in, Tier::EXPERT).cast);
    in.allyHealthPercent = 70.0f;
    CHECK(!HandOfProtection(in, Tier::GOOD).cast);
}

TEST(holy_protection_on_a_burst_and_in_an_emergency)
{
    ProtectionInputs in = Mauled(60.0f);
    in.enemyBurstSeen = true;
    CHECK(HandOfProtection(in, Tier::EXPERT).reason == ProtectionReason::PHYSICAL_BURST);
    CHECK(!HandOfProtection(in, Tier::GOOD).cast);   // a good bot does not read a burst
    CHECK(HandOfProtection(Mauled(35.0f), Tier::GOOD).reason == ProtectionReason::EMERGENCY);
    CHECK(HandOfProtection(Mauled(35.0f), Tier::EXPERT).reason == ProtectionReason::EMERGENCY);
    CHECK(!HandOfProtection(Mauled(50.0f), Tier::EXPERT).cast);
}

TEST(holy_protection_leaves_a_melee_damage_dealer_hitting_unless_he_is_stunned)
{
    ProtectionInputs in = Mauled(30.0f);
    in.allyIsMeleeDamage = true;
    CHECK(HandOfProtection(in, Tier::EXPERT).reason == ProtectionReason::ALLY_HITS_HARD);
    in.allyStunned = true;
    CHECK(HandOfProtection(in, Tier::EXPERT).cast);
}

TEST(holy_protection_needs_a_melee_attacker_and_no_bubble_already)
{
    ProtectionInputs in = Mauled(20.0f);
    in.meleeAttackers = 0;
    CHECK(HandOfProtection(in, Tier::EXPERT).reason == ProtectionReason::NO_PHYSICAL);
    in = Mauled(20.0f);
    in.allyHasProtection = true;
    CHECK(HandOfProtection(in, Tier::EXPERT).reason == ProtectionReason::ALREADY_PROTECTED);
    CHECK(HandOfProtection(Mauled(20.0f), Tier::CASUAL).reason == ProtectionReason::CASUAL_SKIPS);
}

TEST(holy_devotion_aura_in_a_silence_when_the_team_needs_heals)
{
    AuraInputs in;
    in.ready = true;
    in.silenced = true;
    in.lowestAllyPercent = 75.0f;
    CHECK(DevotionAuraNow(in, Tier::EXPERT) == AuraReason::PRESS);
    CHECK(DevotionAuraNow(in, Tier::GOOD) == AuraReason::TEAM_SAFE);   // the good line is 50
    in.lowestAllyPercent = 50.0f;
    CHECK(DevotionAuraNow(in, Tier::GOOD) == AuraReason::PRESS);
    in.lowestAllyPercent = 80.0f;
    CHECK(DevotionAuraNow(in, Tier::EXPERT) == AuraReason::TEAM_SAFE);
}

TEST(holy_devotion_aura_needs_a_silence_a_ready_spell_and_a_non_casual_bot)
{
    AuraInputs in;
    in.ready = true;
    in.lowestAllyPercent = 10.0f;
    CHECK(DevotionAuraNow(in, Tier::EXPERT) == AuraReason::NOT_SILENCED);
    in.silenced = true;
    CHECK(DevotionAuraNow(in, Tier::CASUAL) == AuraReason::TIER_SKIPS);
    in.ready = false;
    CHECK(DevotionAuraNow(in, Tier::EXPERT) == AuraReason::NOT_READY);
}

namespace
{
BeaconInputs Focus()
{
    BeaconInputs in;
    in.known = true;
    in.candidateAttackers = 1;
    in.candidateHealthPercent = 70.0f;
    return in;
}
}  // namespace

TEST(holy_beacon_is_placed_on_the_focused_ally_when_none_is_up)
{
    BeaconInputs in = Focus();
    CHECK(Beacon(in, Tier::EXPERT) == BeaconReason::PLACE);
    CHECK(BeaconDue(Beacon(in, Tier::GOOD)));
    CHECK(Beacon(in, Tier::CASUAL) == BeaconReason::CASUAL_SKIPS);
}

TEST(holy_beacon_follows_the_focus_only_for_an_expert_and_only_off_an_ally_nobody_hits)
{
    BeaconInputs in = Focus();
    in.beaconUp = true;
    in.beaconHolderAttackers = 0;
    CHECK(Beacon(in, Tier::EXPERT) == BeaconReason::MOVE);
    CHECK(Beacon(in, Tier::GOOD) == BeaconReason::NO);
    in.beaconHolderAttackers = 1;
    CHECK(Beacon(in, Tier::EXPERT) == BeaconReason::NO);
    in.candidateIsHolder = true;
    CHECK(Beacon(in, Tier::EXPERT) == BeaconReason::ALREADY_ON_FOCUS);
}

TEST(holy_beacon_needs_a_focus_and_a_known_spell)
{
    BeaconInputs in = Focus();
    in.candidateAttackers = 0;
    CHECK(Beacon(in, Tier::EXPERT) == BeaconReason::NO_FOCUS);
    in = Focus();
    in.candidateHealthPercent = 95.0f;
    CHECK(Beacon(in, Tier::EXPERT) == BeaconReason::NO_FOCUS);
    in = Focus();
    in.known = false;
    CHECK(Beacon(in, Tier::EXPERT) == BeaconReason::NOT_KNOWN);
}
