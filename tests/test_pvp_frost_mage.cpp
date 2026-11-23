#include "TestHarness.h"
#include "PvpFrostMagePolicy.h"
#include "PvpTeamPolicy.h"

// BT-650 to BT-655: the Frost Mage package: Alter Time in the window, Frozen Orb held for the window, Pet Freeze before Deep Freeze,
// Deep Freeze kept for the healer, and the Ring of Frost as an armed link of the control chain.

using namespace PvpFrostMage;

namespace
{
AlterInputs LoadedWindow()
{
    AlterInputs in;
    in.knows = true;
    in.windowOpen = true;
    in.icyVeinsUp = true;
    in.fingers = 2;
    return in;
}
}  // namespace

TEST(frost_alter_time_needs_level_87_and_the_spell)
{
    AlterInputs in = LoadedWindow();
    CHECK(AlterTime(in, 87, Tier::EXPERT).cast);
    CHECK(AlterTime(in, 86, Tier::EXPERT).reason == AlterReason::NOT_KNOWN);
    in.knows = false;
    CHECK(AlterTime(in, 90, Tier::EXPERT).reason == AlterReason::NOT_KNOWN);
}

TEST(frost_alter_time_waits_for_the_window)
{
    AlterInputs in = LoadedWindow();
    in.windowOpen = false;
    for (Tier tier : { Tier::EXPERT, Tier::GOOD })
    {
        AlterVerdict v = AlterTime(in, 90, tier);
        CHECK(!v.cast);
        CHECK(v.reason == AlterReason::WINDOW_CLOSED);
    }
}

TEST(frost_alter_time_is_never_cast_with_hypothermia_or_while_running)
{
    AlterInputs in = LoadedWindow();
    in.hypothermia = true;
    CHECK(AlterTime(in, 90, Tier::EXPERT).reason == AlterReason::HYPOTHERMIA);
    in.hypothermia = false;
    in.alterTimeUp = true;
    CHECK(AlterTime(in, 90, Tier::GOOD).reason == AlterReason::ALREADY_UP);
}

TEST(frost_alter_time_expert_wants_icy_veins_and_a_proc_good_one_of_them)
{
    AlterInputs in;
    in.knows = true;
    in.windowOpen = true;
    in.icyVeinsUp = true;
    CHECK(AlterTime(in, 90, Tier::EXPERT).reason == AlterReason::NO_FUEL);   // Icy Veins alone
    CHECK(AlterTime(in, 90, Tier::GOOD).cast);
    in.icyVeinsUp = false;
    in.fingers = 1;
    CHECK(AlterTime(in, 90, Tier::EXPERT).reason == AlterReason::NO_FUEL);   // a charge alone
    CHECK(AlterTime(in, 90, Tier::GOOD).cast);
    in.brainFreeze = true;
    in.icyVeinsUp = true;
    CHECK(AlterTime(in, 90, Tier::EXPERT).cast);
}

TEST(frost_alter_time_casual_bots_do_not_plan_a_rewind)
{
    CHECK(AlterTime(LoadedWindow(), 90, Tier::CASUAL).reason == AlterReason::CASUAL_SKIPS);
}

TEST(frost_alter_time_score_grows_with_fuel_and_is_deterministic)
{
    AlterInputs thin;
    thin.knows = true;
    thin.windowOpen = true;
    thin.icyVeinsUp = true;
    thin.fingers = 1;
    AlterInputs full = LoadedWindow();
    full.brainFreeze = true;
    std::int32_t const a = AlterTime(thin, 90, Tier::EXPERT).score;
    std::int32_t const b = AlterTime(full, 90, Tier::EXPERT).score;
    CHECK(b > a);
    CHECK(a == AlterTime(thin, 90, Tier::EXPERT).score);
    full.fingers = 9;   // a third proc is lost, so the score does not run away
    CHECK(AlterTime(full, 90, Tier::EXPERT).score == b);
}

TEST(frost_orb_is_a_window_cooldown_against_a_player)
{
    OrbInputs in;
    in.targetIsPlayer = true;
    CHECK(FrozenOrb(in, Tier::EXPERT) == OrbReason::HOLD_FOR_WINDOW);
    CHECK(FrozenOrb(in, Tier::GOOD) == OrbReason::HOLD_FOR_WINDOW);
    CHECK(!OrbAllowed(FrozenOrb(in, Tier::EXPERT)));
    in.windowOpen = true;
    CHECK(OrbAllowed(FrozenOrb(in, Tier::EXPERT)));
    CHECK(FrozenOrb(in, Tier::GOOD) == OrbReason::WINDOW);
}

TEST(frost_orb_does_not_break_a_sheep_and_casual_keeps_its_filler)
{
    OrbInputs in;
    in.targetIsPlayer = true;
    in.windowOpen = true;
    in.targetBreakableCc = true;
    CHECK(FrozenOrb(in, Tier::EXPERT) == OrbReason::HOLD_FOR_SHEEP);
    CHECK(FrozenOrb(in, Tier::GOOD) == OrbReason::HOLD_FOR_SHEEP);
    in.windowOpen = false;
    CHECK(FrozenOrb(in, Tier::CASUAL) == OrbReason::CASUAL_FILLER);
    CHECK(OrbAllowed(FrozenOrb(in, Tier::CASUAL)));
}

TEST(frost_orb_pve_rotation_is_untouched)
{
    OrbInputs in;   // not a player
    for (Tier tier : { Tier::EXPERT, Tier::GOOD, Tier::CASUAL })
        CHECK(OrbAllowed(FrozenOrb(in, tier)));
    CHECK(FrozenOrb(in, Tier::EXPERT) == OrbReason::NOT_A_PLAYER);
}

namespace
{
FreezeInputs Ready()
{
    FreezeInputs in;
    in.petPresent = true;
    in.petReady = true;
    in.targetIsPlayer = true;
    in.targetDistance = 25.0f;
    return in;
}
}  // namespace

TEST(frost_pet_freeze_sets_up_deep_freeze)
{
    FreezeInputs in = Ready();
    in.deepFreezeReady = true;
    FreezeVerdict v = PetFreeze(in, Tier::EXPERT);
    CHECK(v.cast);
    CHECK(v.reason == FreezeReason::DEEP_FREEZE_SETUP);
    CHECK(v.score == SCORE_FREEZE_SETUP);
    CHECK(PetFreeze(in, Tier::GOOD).cast);
}

TEST(frost_pet_freeze_banks_a_charge_only_with_room_and_a_reason)
{
    FreezeInputs in = Ready();
    in.fingers = 1;
    CHECK(PetFreeze(in, Tier::GOOD).reason == FreezeReason::FINGERS_BANK);
    CHECK(PetFreeze(in, Tier::EXPERT).reason == FreezeReason::NOT_NEEDED);   // an expert banks it for the window
    in.windowOpen = true;
    CHECK(PetFreeze(in, Tier::EXPERT).reason == FreezeReason::FINGERS_BANK);
    in.fingers = FINGERS_CAP;
    CHECK(!PetFreeze(in, Tier::EXPERT).cast);
    CHECK(!PetFreeze(in, Tier::GOOD).cast);
}

TEST(frost_pet_freeze_refuses_a_held_target_a_far_one_and_a_missing_pet)
{
    FreezeInputs in = Ready();
    in.deepFreezeReady = true;
    in.targetHeld = true;
    CHECK(PetFreeze(in, Tier::EXPERT).reason == FreezeReason::TARGET_HELD);
    in.targetHeld = false;
    in.targetDistance = 55.0f;
    CHECK(PetFreeze(in, Tier::EXPERT).reason == FreezeReason::OUT_OF_RANGE);
    in.targetDistance = 20.0f;
    in.targetIsPlayer = false;
    CHECK(PetFreeze(in, Tier::EXPERT).reason == FreezeReason::NOT_A_PLAYER);
    in = Ready();
    in.petReady = false;
    CHECK(PetFreeze(in, Tier::EXPERT).reason == FreezeReason::NOT_READY);
    in.petPresent = false;
    CHECK(PetFreeze(in, Tier::EXPERT).reason == FreezeReason::NO_PET);
    CHECK(PetFreeze(Ready(), Tier::CASUAL).reason == FreezeReason::CASUAL_SKIPS);
}

TEST(frost_deep_freeze_is_kept_for_a_free_healer_in_team_play)
{
    DeepFreezeInputs in;
    in.teamContext = true;
    in.healerPresent = true;
    in.healerLockedMs = 0;
    CHECK(DeepFreezeUse(in, Tier::EXPERT) == DeepFreezeReason::KEEP_FOR_HEALER);
    CHECK(!DeepFreezeAllowed(DeepFreezeUse(in, Tier::GOOD)));
    in.targetIsHealer = true;
    CHECK(DeepFreezeAllowed(DeepFreezeUse(in, Tier::EXPERT)));
}

TEST(frost_deep_freeze_goes_to_the_kill_target_once_the_healer_is_held)
{
    DeepFreezeInputs in;
    in.teamContext = true;
    in.healerPresent = true;
    in.healerLockedMs = PvpTeam::HEALER_HELD_MIN_MS;
    CHECK(DeepFreezeUse(in, Tier::EXPERT) == DeepFreezeReason::HEALER_HELD);
    in.healerLockedMs = PvpTeam::HEALER_HELD_MIN_MS - 1;
    CHECK(!DeepFreezeAllowed(DeepFreezeUse(in, Tier::EXPERT)));
}

TEST(frost_deep_freeze_has_no_reserve_without_a_healer_a_team_or_for_casual_bots)
{
    DeepFreezeInputs in;
    in.healerPresent = true;
    CHECK(DeepFreezeUse(in, Tier::EXPERT) == DeepFreezeReason::FREE);   // a duel or the open world
    in.teamContext = true;
    CHECK(DeepFreezeUse(in, Tier::CASUAL) == DeepFreezeReason::CASUAL_FREE);
    in.healerPresent = false;
    CHECK(DeepFreezeUse(in, Tier::EXPERT) == DeepFreezeReason::NO_HEALER);
    CHECK(DeepFreezeAllowed(DeepFreezeUse(in, Tier::GOOD)));
}

namespace
{
PvpTeam::ControlCandidate Ring(std::uint32_t drStep = 0)
{
    PvpTeam::ControlCandidate c;
    c.member = 1;
    c.spellId = SPELL_RING_OF_FROST;
    c.category = PvpSpells::DrCategory::INCAPACITATE;
    c.baseMs = 10000;
    c.castMs = 0;
    c.armMs = 2000;
    c.drStep = drStep;
    return c;
}
PvpTeam::ControlTarget HeldByStun(std::uint32_t leftMs)
{
    PvpTeam::ControlTarget t;
    t.lockedMs = leftMs;
    t.lockCategory = PvpSpells::DrCategory::STUN;
    return t;
}
}  // namespace

TEST(frost_ring_is_never_an_opener_on_a_free_healer)
{
    PvpTeam::ControlTarget free;
    for (Tier tier : { Tier::EXPERT, Tier::GOOD, Tier::CASUAL })
        CHECK(PvpTeam::ScoreControl(Ring(), free, tier) == PvpTeam::NOT_DUE);
}

TEST(frost_ring_is_an_expert_link_while_the_stun_still_holds_for_the_arming_time)
{
    CHECK(PvpTeam::ScoreControl(Ring(), HeldByStun(2400), Tier::EXPERT) > 0);
    CHECK(PvpTeam::ScoreControl(Ring(), HeldByStun(2000), Tier::EXPERT) > 0);
    CHECK(PvpTeam::ScoreControl(Ring(), HeldByStun(1900), Tier::EXPERT) == PvpTeam::NOT_DUE);   // it would arm after the stun ended
    CHECK(PvpTeam::ScoreControl(Ring(), HeldByStun(4000), Tier::EXPERT) == PvpTeam::NOT_DUE);   // too early: the stun is not spent
    CHECK(PvpTeam::ScoreControl(Ring(), HeldByStun(2400), Tier::GOOD) == PvpTeam::NOT_DUE);     // a good bot has no chain
}

TEST(frost_ring_follows_the_diminishing_return_of_the_incapacitate_category)
{
    std::int32_t const full = PvpTeam::ScoreControl(Ring(0), HeldByStun(2400), Tier::EXPERT);
    std::int32_t const half = PvpTeam::ScoreControl(Ring(1), HeldByStun(2400), Tier::EXPERT);
    CHECK(full > half);
    CHECK(half > 0);
    CHECK(PvpTeam::ScoreControl(Ring(PvpAware::DR_IMMUNE_STEP), HeldByStun(2400), Tier::EXPERT) == PvpTeam::NOT_DUE);
}

TEST(frost_ring_into_the_same_category_scores_below_a_new_one)
{
    PvpTeam::ControlTarget sheep = HeldByStun(2400);
    sheep.lockCategory = PvpSpells::DrCategory::INCAPACITATE;
    CHECK(PvpTeam::ScoreControl(Ring(), sheep, Tier::EXPERT) < PvpTeam::ScoreControl(Ring(), HeldByStun(2400), Tier::EXPERT));
}

TEST(frost_unarmed_controls_keep_their_old_chain_rule)
{
    PvpTeam::ControlCandidate poly;
    poly.member = 1;
    poly.spellId = 118;
    poly.category = PvpSpells::DrCategory::INCAPACITATE;
    poly.baseMs = 8000;
    poly.castMs = 1700;
    PvpTeam::ControlTarget stun = HeldByStun(1800);
    CHECK(PvpTeam::ScoreControl(poly, stun, Tier::EXPERT) > 0);
    stun.lockedMs = 5000;
    CHECK(PvpTeam::ScoreControl(poly, stun, Tier::EXPERT) == PvpTeam::NOT_DUE);
}

namespace
{
WardInputs WardFight()
{
    WardInputs in;
    in.knows = true;
    in.ready = true;
    in.targetIsPlayer = true;
    return in;
}
}  // namespace

TEST(frost_ward_goes_up_before_the_go_for_good_and_expert_bots)
{
    WardInputs in = WardFight();
    in.windowOpen = true;
    CHECK(IncantersWard(in, Tier::EXPERT) == WardReason::BEFORE_THE_GO);
    CHECK(WardDue(IncantersWard(in, Tier::GOOD)));
    CHECK(!WardDue(IncantersWard(in, Tier::CASUAL)));   // a casual bot reads no window
}

TEST(frost_ward_under_attack_needs_a_hurt_mage_except_for_casual_bots)
{
    WardInputs in = WardFight();
    in.attackedByPlayer = true;
    in.healthPercent = 100.0f;
    CHECK(!WardDue(IncantersWard(in, Tier::EXPERT)));
    CHECK(WardDue(IncantersWard(in, Tier::CASUAL)));
    in.healthPercent = WARD_ATTACKED_HP_PERCENT;
    CHECK(IncantersWard(in, Tier::GOOD) == WardReason::UNDER_ATTACK);
}

TEST(frost_ward_is_not_cast_unready_up_or_against_a_monster)
{
    WardInputs in = WardFight();
    in.windowOpen = true;
    in.up = true;
    CHECK(IncantersWard(in, Tier::EXPERT) == WardReason::NOT_READY);
    in.up = false;
    in.ready = false;
    CHECK(!WardDue(IncantersWard(in, Tier::EXPERT)));
    in.ready = true;
    in.targetIsPlayer = false;
    CHECK(IncantersWard(in, Tier::EXPERT) == WardReason::NOT_A_FIGHT);
    in = WardFight();
    in.knows = false;
    in.windowOpen = true;
    CHECK(!WardDue(IncantersWard(in, Tier::EXPERT)));
}
