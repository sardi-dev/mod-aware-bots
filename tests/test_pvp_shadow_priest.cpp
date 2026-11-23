#include "TestHarness.h"
#include "PvpShadowPriestPolicy.h"

// BT-688 to BT-690: the Shadow Priest package: Vampiric Embrace with the burst, Leap of Faith on a held ally, Mass Dispel on a magic
// immunity.

using namespace PvpShadowPriest;

TEST(shadow_embrace_goes_with_two_hurt_allies_or_the_burst_with_one)
{
    EmbraceInputs in;
    in.pvpFight = true;
    in.alliesHurt = 2;
    CHECK(VampiricEmbrace(in, Tier::EXPERT) == EmbraceReason::TEAM_HURT);
    CHECK(EmbraceAllowed(VampiricEmbrace(in, Tier::GOOD)));
    in.alliesHurt = 1;
    CHECK(VampiricEmbrace(in, Tier::EXPERT) == EmbraceReason::HOLD);
    in.windowOpen = true;
    CHECK(VampiricEmbrace(in, Tier::EXPERT) == EmbraceReason::BURST_WITH_HURT);
}

TEST(shadow_embrace_is_held_when_the_team_is_healthy_and_free_for_pve_casual_and_a_hurt_priest)
{
    EmbraceInputs in;
    in.pvpFight = true;
    in.windowOpen = true;
    CHECK(!EmbraceAllowed(VampiricEmbrace(in, Tier::EXPERT)));   // nobody hurt: the window alone does not heal anyone
    in.selfHealthPercent = EMBRACE_SELF_PCT;
    CHECK(VampiricEmbrace(in, Tier::EXPERT) == EmbraceReason::SELF_LOW);
    in = EmbraceInputs();
    CHECK(VampiricEmbrace(in, Tier::EXPERT) == EmbraceReason::NOT_A_PVP_FIGHT);
    CHECK(EmbraceAllowed(VampiricEmbrace(in, Tier::EXPERT)));
    in.pvpFight = true;
    CHECK(VampiricEmbrace(in, Tier::CASUAL) == EmbraceReason::CASUAL_FREE);
}

namespace
{
LeapInputs Trapped(float pct)
{
    LeapInputs in;
    in.ready = true;
    in.allyHeld = true;
    in.playerAttackers = 1;
    in.allyHealthPercent = pct;
    in.allyDistance = 25.0f;
    return in;
}
}  // namespace

TEST(shadow_leap_pulls_a_held_ally_under_attack_by_tier_line)
{
    CHECK(LeapOfFaith(Trapped(65.0f), Tier::EXPERT).reason == LeapReason::PULL);
    CHECK(LeapOfFaith(Trapped(66.0f), Tier::EXPERT).reason == LeapReason::TOO_HEALTHY);
    CHECK(LeapOfFaith(Trapped(50.0f), Tier::GOOD).reason == LeapReason::TOO_HEALTHY);
    CHECK(LeapOfFaith(Trapped(40.0f), Tier::GOOD).cast);
}

TEST(shadow_leap_needs_a_held_ally_a_threat_distance_and_a_ready_spell)
{
    LeapInputs in = Trapped(30.0f);
    in.allyHeld = false;
    CHECK(LeapOfFaith(in, Tier::EXPERT).reason == LeapReason::NOT_HELD);
    in = Trapped(30.0f);
    in.playerAttackers = 0;
    CHECK(LeapOfFaith(in, Tier::EXPERT).reason == LeapReason::NO_THREAT);
    in = Trapped(30.0f);
    in.allyDistance = 5.0f;
    CHECK(LeapOfFaith(in, Tier::EXPERT).reason == LeapReason::TOO_CLOSE);
    in.allyDistance = 41.0f;
    CHECK(LeapOfFaith(in, Tier::EXPERT).reason == LeapReason::TOO_CLOSE);
    in = Trapped(30.0f);
    in.ready = false;
    CHECK(!LeapOfFaith(in, Tier::EXPERT).cast);
    in = Trapped(30.0f);
    in.allyIsSelf = true;
    CHECK(LeapOfFaith(in, Tier::EXPERT).reason == LeapReason::ON_SELF);
    CHECK(LeapOfFaith(Trapped(30.0f), Tier::CASUAL).reason == LeapReason::CASUAL_SKIPS);
}

TEST(shadow_leap_score_prefers_the_more_hurt_and_the_more_attacked_ally)
{
    std::int32_t const a = LeapOfFaith(Trapped(60.0f), Tier::EXPERT).score;
    CHECK(LeapOfFaith(Trapped(30.0f), Tier::EXPERT).score > a);
    LeapInputs two = Trapped(60.0f);
    two.playerAttackers = 2;
    CHECK(LeapOfFaith(two, Tier::EXPERT).score == a + SCORE_LEAP_PER_ATTACKER);
    two.playerAttackers = 9;
    LeapInputs three = Trapped(60.0f);
    three.playerAttackers = LEAP_ATTACKERS_COUNTED;
    CHECK(LeapOfFaith(two, Tier::EXPERT).score == LeapOfFaith(three, Tier::EXPERT).score);
}

namespace
{
DispelInputs Bubble()
{
    DispelInputs in;
    in.ready = true;
    in.glyphed = true;
    in.targetImmune = true;
    in.targetDistance = 25.0f;
    return in;
}
}  // namespace

TEST(shadow_mass_dispel_breaks_a_magic_immunity_with_the_glyph)
{
    CHECK(MassDispel(Bubble(), Tier::EXPERT) == DispelReason::BREAK_IMMUNITY);
    CHECK(MassDispel(Bubble(), Tier::GOOD) == DispelReason::BREAK_IMMUNITY);
    DispelInputs in = Bubble();
    in.glyphed = false;
    CHECK(MassDispel(in, Tier::EXPERT) == DispelReason::NO_GLYPH);
}

TEST(shadow_mass_dispel_needs_an_immunity_a_voice_and_range_and_skips_casual_bots)
{
    DispelInputs in = Bubble();
    in.targetImmune = false;
    CHECK(MassDispel(in, Tier::EXPERT) == DispelReason::NOT_IMMUNE);
    in = Bubble();
    in.silenced = true;
    CHECK(MassDispel(in, Tier::EXPERT) == DispelReason::SILENCED);
    in = Bubble();
    in.targetDistance = 31.0f;
    CHECK(MassDispel(in, Tier::EXPERT) == DispelReason::OUT_OF_RANGE);
    in = Bubble();
    in.ready = false;
    CHECK(MassDispel(in, Tier::EXPERT) == DispelReason::NOT_READY);
    CHECK(MassDispel(Bubble(), Tier::CASUAL) == DispelReason::CASUAL_SKIPS);
}
