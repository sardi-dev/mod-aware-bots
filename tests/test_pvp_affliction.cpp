#include "TestHarness.h"
#include "PvpAfflictionPolicy.h"
#include "PvpTeamPolicy.h"

// BT-663 to BT-668: the Affliction package: the Shadowfury opener of the healer chain (and the instant pending window that lets Fear
// follow it), the splash rule, Howl of Terror as a peel, Fel Flame in a shadow lockout, Haunt in the window, and the Healthstone
// beside Dark Regeneration.

using namespace PvpAffliction;

namespace
{
PvpTeam::ControlCandidate Fury()
{
    PvpTeam::ControlCandidate c;
    c.member = 1;
    c.spellId = SPELL_SHADOWFURY;
    c.category = PvpSpells::DrCategory::STUN;
    c.baseMs = 3000;
    c.castMs = 0;
    return c;
}
PvpTeam::ControlCandidate FearLink()
{
    PvpTeam::ControlCandidate c;
    c.member = 1;
    c.spellId = 5782;
    c.category = PvpSpells::DrCategory::FEAR;
    c.baseMs = 8000;
    c.castMs = 1500;
    c.breakable = true;
    return c;
}
}  // namespace

TEST(aff_shadowfury_opens_on_a_free_healer_for_good_and_expert_bots)
{
    PvpTeam::ControlTarget free;
    CHECK(PvpTeam::ScoreControl(Fury(), free, Tier::EXPERT) > 0);
    CHECK(PvpTeam::ScoreControl(Fury(), free, Tier::GOOD) > 0);
    CHECK(PvpTeam::ScoreControl(Fury(), free, Tier::CASUAL) == PvpTeam::NOT_DUE);
}

TEST(aff_fear_follows_the_stun_as_it_runs_out_for_an_expert_only)
{
    PvpTeam::ControlTarget stun;
    stun.lockCategory = PvpSpells::DrCategory::STUN;
    stun.lockedMs = 1700;   // inside the lead of a 1.5 s cast
    CHECK(PvpTeam::ScoreControl(FearLink(), stun, Tier::EXPERT) > 0);
    CHECK(PvpTeam::ScoreControl(FearLink(), stun, Tier::GOOD) == PvpTeam::NOT_DUE);
    stun.lockedMs = 2900;   // the stun has just landed: too early
    CHECK(PvpTeam::ScoreControl(FearLink(), stun, Tier::EXPERT) == PvpTeam::NOT_DUE);
}

TEST(aff_an_instant_stun_does_not_hold_the_next_link_back_for_the_whole_cast_window)
{
    PvpTeam::ControlBook book;
    std::uint32_t const healer = 7;
    book.NoteCast(1, healer, PvpSpells::DrCategory::STUN, true, 10000, 0);   // Shadowfury is instant
    CHECK(book.Blocked(2, healer, PvpSpells::DrCategory::FEAR, true, 10300));   // the aura has not shown yet
    CHECK(!book.Blocked(2, healer, PvpSpells::DrCategory::FEAR, true, 10000 + PvpTeam::INSTANT_PENDING_MS));
    // A cast of 1.5 s still holds the others for the old window.
    book.NoteCast(1, healer, PvpSpells::DrCategory::FEAR, true, 20000, 1500);
    CHECK(book.Blocked(2, healer, PvpSpells::DrCategory::STUN, true, 20000 + PvpTeam::INSTANT_PENDING_MS + 200));
    CHECK(!book.Blocked(2, healer, PvpSpells::DrCategory::STUN, true, 20000 + PvpTeam::PENDING_MS));
}

TEST(aff_an_instant_control_still_blocks_a_second_one_of_its_own_category)
{
    PvpTeam::ControlBook book;
    book.NoteCast(1, 7, PvpSpells::DrCategory::STUN, true, 10000, 0);
    CHECK(book.Blocked(2, 7, PvpSpells::DrCategory::STUN, false, 10400));
    CHECK(!book.Blocked(2, 7, PvpSpells::DrCategory::STUN, false, 10000 + PvpTeam::INSTANT_PENDING_MS));
    // The old call without a cast time keeps the whole window.
    PvpTeam::ControlBook old;
    old.NoteCast(1, 7, PvpSpells::DrCategory::STUN, true, 10000);
    CHECK(old.Blocked(2, 7, PvpSpells::DrCategory::STUN, true, 10000 + PvpTeam::PENDING_MS - 1));
}

TEST(aff_splash_is_refused_beside_a_held_enemy)
{
    CHECK(SplashAllowed(0));
    CHECK(!SplashAllowed(1));
    CHECK(!SplashAllowed(3));
}

namespace
{
HowlInputs Melee(std::uint32_t count, float hp = 100.0f)
{
    HowlInputs in;
    in.ready = true;
    in.meleePlayersInRadius = count;
    in.healthPercent = hp;
    return in;
}
}  // namespace

TEST(aff_howl_peels_a_melee_player_for_an_expert_at_any_health)
{
    HowlVerdict v = HowlOfTerror(Melee(1), Tier::EXPERT);
    CHECK(v.cast);
    CHECK(v.reason == HowlReason::PEEL);
    CHECK(HowlOfTerror(Melee(0), Tier::EXPERT).reason == HowlReason::NO_MELEE);
}

TEST(aff_howl_tier_health_lines)
{
    CHECK(HowlOfTerror(Melee(1, 90.0f), Tier::GOOD).reason == HowlReason::HEALTHY_GOOD);
    CHECK(HowlOfTerror(Melee(1, HOWL_GOOD_HP_PERCENT), Tier::GOOD).cast);
    CHECK(HowlOfTerror(Melee(1, 60.0f), Tier::CASUAL).reason == HowlReason::HEALTHY_CASUAL);
    CHECK(HowlOfTerror(Melee(1, HOWL_CASUAL_HP_PERCENT), Tier::CASUAL).cast);
}

TEST(aff_howl_is_not_cast_into_a_team_sheep_or_off_cooldown_except_by_a_casual_bot)
{
    HowlInputs in = Melee(1);
    in.breakableHeldInRadius = 1;
    CHECK(HowlOfTerror(in, Tier::EXPERT).reason == HowlReason::TEAM_CONTROL);
    CHECK(HowlOfTerror(in, Tier::GOOD).reason == HowlReason::TEAM_CONTROL);
    in.healthPercent = 40.0f;
    CHECK(HowlOfTerror(in, Tier::CASUAL).cast);   // a casual bot does not look
    in = Melee(1);
    in.ready = false;
    CHECK(HowlOfTerror(in, Tier::EXPERT).reason == HowlReason::NOT_READY);
}

TEST(aff_howl_score_grows_with_the_melee_and_the_damage_taken)
{
    std::int32_t const one = HowlOfTerror(Melee(1, 100.0f), Tier::EXPERT).score;
    CHECK(HowlOfTerror(Melee(2, 100.0f), Tier::EXPERT).score == one + SCORE_HOWL_PER_MELEE);
    CHECK(HowlOfTerror(Melee(1, 60.0f), Tier::EXPERT).score > one);
    CHECK(HowlOfTerror(Melee(9, 100.0f), Tier::EXPERT).score == HowlOfTerror(Melee(MELEE_COUNTED, 100.0f), Tier::EXPERT).score);
}

TEST(aff_fel_flame_only_while_the_shadow_school_is_locked_in_a_fight_against_a_player)
{
    FelFlameInputs in;
    in.known = true;
    in.targetIsPlayer = true;
    CHECK(FelFlame(in, Tier::EXPERT) == FelFlameReason::SHADOW_OPEN);
    in.shadowLocked = true;
    CHECK(FelFlameDue(FelFlame(in, Tier::EXPERT)));
    CHECK(FelFlameDue(FelFlame(in, Tier::GOOD)));
    CHECK(FelFlame(in, Tier::CASUAL) == FelFlameReason::CASUAL_SKIPS);
    in.targetIsPlayer = false;
    CHECK(FelFlame(in, Tier::EXPERT) == FelFlameReason::NOT_A_FIGHT);
    in.targetIsPlayer = true;
    in.known = false;
    CHECK(FelFlame(in, Tier::EXPERT) == FelFlameReason::NOT_READY);
}

namespace
{
HauntInputs Hunt(bool window = false, float hp = 100.0f)
{
    HauntInputs in;
    in.targetIsPlayer = true;
    in.windowOpen = window;
    in.targetHealthPercent = hp;
    return in;
}
}  // namespace

TEST(aff_haunt_waits_for_the_window_a_hurt_target_or_a_held_one)
{
    CHECK(Haunt(Hunt(), Tier::EXPERT) == HauntReason::HOLD);
    CHECK(!HauntAllowed(Haunt(Hunt(), Tier::GOOD)));
    CHECK(Haunt(Hunt(true), Tier::EXPERT) == HauntReason::WINDOW);
    CHECK(Haunt(Hunt(false, HAUNT_LOW_HP_PERCENT), Tier::GOOD) == HauntReason::TARGET_LOW);
    HauntInputs held = Hunt();
    held.targetHeld = true;
    CHECK(Haunt(held, Tier::EXPERT) == HauntReason::TARGET_HELD);
    CHECK(Haunt(held, Tier::GOOD) == HauntReason::HOLD);   // only an expert reads it
}

TEST(aff_haunt_never_frees_a_sheep_and_pve_and_casual_keep_the_old_rule)
{
    HauntInputs in = Hunt(true, 10.0f);
    in.targetBreakableCc = true;
    CHECK(Haunt(in, Tier::EXPERT) == HauntReason::BREAKABLE_CC);
    CHECK(!HauntAllowed(Haunt(in, Tier::GOOD)));
    CHECK(HauntAllowed(Haunt(Hunt(), Tier::CASUAL)));
    CHECK(Haunt(in, Tier::CASUAL) == HauntReason::CASUAL_KEEPS);
    HauntInputs pve;
    CHECK(HauntAllowed(Haunt(pve, Tier::EXPERT)));
    CHECK(Haunt(pve, Tier::EXPERT) == HauntReason::NOT_A_PLAYER);
}

TEST(aff_regeneration_and_healthstone_go_together_at_half_health)
{
    StoneInputs in;
    in.regenerationRunning = true;
    in.stoneUsable = true;
    in.healthPercent = 45.0f;
    CHECK(StoneDue(RegenerationStone(in, Tier::EXPERT)));
    CHECK(StoneDue(RegenerationStone(in, Tier::GOOD)));
    in.healthPercent = 51.0f;
    CHECK(RegenerationStone(in, Tier::EXPERT) == StoneReason::HEALTHY);
    in.healthPercent = STONE_PAIR_HP_PERCENT;
    CHECK(StoneDue(RegenerationStone(in, Tier::EXPERT)));
}

TEST(aff_healthstone_pair_needs_the_regeneration_the_stone_and_a_non_casual_bot)
{
    StoneInputs in;
    in.stoneUsable = true;
    in.healthPercent = 30.0f;
    CHECK(RegenerationStone(in, Tier::EXPERT) == StoneReason::NO_REGENERATION);
    in.regenerationRunning = true;
    in.stoneUsable = false;
    CHECK(RegenerationStone(in, Tier::EXPERT) == StoneReason::NO_STONE);
    in.stoneUsable = true;
    CHECK(RegenerationStone(in, Tier::CASUAL) == StoneReason::CASUAL_SKIPS);
}
