#include "TestHarness.h"
#include "PvpTeamPolicy.h"

// BT-628 to BT-631: the shared team state: the kill window, the go, partner timing, control scoring with diminishing returns, the
// executor choice and the pending-cast reservation, and the hold on breakable control.

using namespace PvpTeam;

namespace
{
WindowInputs Healer(std::uint32_t lockedMs, bool trinketDown = false, bool immune = false)
{
    WindowInputs in;
    in.healerPresent = true;
    in.healerLockedMs = lockedMs;
    in.healerTrinketDown = trinketDown;
    in.targetImmune = immune;
    return in;
}

ControlCandidate Sheep(std::uint32_t member, std::uint32_t step = 0)
{
    ControlCandidate c;
    c.member = member;
    c.spellId = 118;
    c.category = DrCategory::INCAPACITATE;
    c.baseMs = 8000;
    c.castMs = 1700;
    c.breakable = true;
    c.drStep = step;
    return c;
}

ControlCandidate Stun(std::uint32_t member, std::uint32_t step = 0)
{
    ControlCandidate c;
    c.member = member;
    c.spellId = 408;
    c.category = DrCategory::STUN;
    c.baseMs = 6000;
    c.castMs = 0;
    c.drStep = step;
    return c;
}
}  // namespace

// --- the window ---------------------------------------------------------------------------------------------------------------

TEST(team_window_opens_when_the_healer_is_locked_four_seconds)
{
    WindowVerdict v = EvaluateWindow(Healer(WINDOW_MIN_LOCK_MS), Tier::GOOD);
    CHECK(v.open && v.reason == WindowReason::LOCKED);
    CHECK(!EvaluateWindow(Healer(WINDOW_MIN_LOCK_MS - 1), Tier::GOOD).open);
    CHECK(EvaluateWindow(Healer(6000), Tier::EXPERT).open);
}

TEST(team_window_opens_for_an_expert_on_a_shorter_lock_with_the_medallion_down)
{
    WindowVerdict v = EvaluateWindow(Healer(2000, true), Tier::EXPERT);
    CHECK(v.open && v.reason == WindowReason::LOCKED_NO_TRINKET);
    // Too short even with no medallion, and a free healer with no medallion is not a window by itself.
    CHECK(!EvaluateWindow(Healer(WINDOW_NO_TRINKET_MIN_LOCK_MS - 1, true), Tier::EXPERT).open);
    CHECK(!EvaluateWindow(Healer(0, true), Tier::EXPERT).open);
    // A good bot does not read the medallion.
    CHECK(!EvaluateWindow(Healer(2000, true), Tier::GOOD).open);
}

TEST(team_window_is_closed_on_an_immune_target_and_without_a_healer)
{
    WindowVerdict immune = EvaluateWindow(Healer(8000, true, true), Tier::EXPERT);
    CHECK(!immune.open && immune.reason == WindowReason::TARGET_IMMUNE);
    WindowInputs none;
    WindowVerdict noHealer = EvaluateWindow(none, Tier::EXPERT);
    CHECK(!noHealer.open && noHealer.reason == WindowReason::NO_HEALER);
}

TEST(team_window_is_never_read_by_a_casual_bot)
{
    CHECK(!EvaluateWindow(Healer(8000, true), Tier::CASUAL).open);
}

// --- the go -------------------------------------------------------------------------------------------------------------------

TEST(team_go_is_shared_and_held_for_twelve_seconds_on_its_target)
{
    Go go;
    CHECK(!GoActive(go, 7, 1000));
    CHECK(OpenGo(go, 7, 1000, WindowReason::LOCKED));
    CHECK(GoActive(go, 7, 1000 + GO_HOLD_MS - 1));
    CHECK(!GoActive(go, 7, 1000 + GO_HOLD_MS));
    CHECK(!GoActive(go, 8, 2000));                    // another target is not this go
    CHECK(!OpenGo(go, 7, 5000, WindowReason::LOCKED));  // a second call while it holds does not reopen it
    CHECK(go.openedMs == 1000);
    CHECK(OpenGo(go, 7, 1000 + GO_HOLD_MS + 5, WindowReason::LOCKED));  // after it ran out, a new go
}

TEST(team_go_survives_the_clock_wrapping)
{
    Go go;
    std::uint32_t const near = 0xFFFFFF00u;
    CHECK(OpenGo(go, 3, near, WindowReason::LOCKED));
    CHECK(GoActive(go, 3, near + 5000));  // wraps past zero
    CHECK(!GoActive(go, 3, near + 20000));
}

// --- partners going in together -----------------------------------------------------------------------------------------------

TEST(team_partners_match_within_one_and_a_half_seconds)
{
    CHECK(Aligned(10000, 10000));
    CHECK(Aligned(10000, 11500));
    CHECK(Aligned(11500, 10000));
    CHECK(!Aligned(10000, 11501));
    CHECK(SkewMs(10000, 10400) == 400);
}

TEST(team_partner_burst_counts_for_ten_seconds)
{
    CHECK(PartnerBurstCounts(0));
    CHECK(PartnerBurstCounts(PARTNER_HOLD_MS - 1));
    CHECK(!PartnerBurstCounts(PARTNER_HOLD_MS));
}

// --- control and diminishing returns ------------------------------------------------------------------------------------------

TEST(team_effective_length_follows_the_step_and_the_pvp_cap)
{
    CHECK(EffectiveMs(8000, 0) == 8000);
    CHECK(EffectiveMs(8000, 1) == 4000);
    CHECK(EffectiveMs(8000, 2) == 2000);
    CHECK(EffectiveMs(8000, 3) == 0);
    CHECK(EffectiveMs(30000, 0) == PVP_CC_CAP_MS);
}

TEST(team_control_is_not_due_into_an_immune_step_or_a_useless_length)
{
    ControlTarget free;
    CHECK(ScoreControl(Sheep(1, 3), free, Tier::EXPERT) == NOT_DUE);
    CHECK(ScoreControl(Sheep(1, 2), free, Tier::EXPERT) == NOT_DUE);   // a quarter of 8 s is 2 s: under the useful length
    CHECK(ScoreControl(Sheep(1, 1), free, Tier::EXPERT) > 0);         // half is 4 s: worth it
}

TEST(team_control_chain_waits_for_the_running_lock_to_end_and_prefers_a_new_category)
{
    ControlTarget locked;
    locked.lockedMs = 5000;
    locked.lockCategory = DrCategory::STUN;
    CHECK(ScoreControl(Sheep(1), locked, Tier::EXPERT) == NOT_DUE);   // not due yet: the stun still runs

    locked.lockedMs = LeadMs(1700, Tier::EXPERT);
    std::int32_t const sheep = ScoreControl(Sheep(1), locked, Tier::EXPERT);   // another category: no diminishing return
    CHECK(sheep > 0);
    std::int32_t const stun = ScoreControl(Stun(2), locked, Tier::EXPERT);     // the same category as the running stun
    CHECK(stun < sheep);
}

TEST(team_control_first_link_on_a_free_target_for_good_but_no_chain)
{
    ControlTarget free;
    CHECK(ScoreControl(Sheep(1), free, Tier::GOOD) > 0);
    ControlTarget locked;
    locked.lockedMs = 100;
    locked.lockCategory = DrCategory::STUN;
    CHECK(ScoreControl(Sheep(1), locked, Tier::GOOD) == NOT_DUE);   // a good bot casts only on a free target
    CHECK(ScoreControl(Sheep(1), locked, Tier::EXPERT) > 0);
    CHECK(ScoreControl(Sheep(1), free, Tier::CASUAL) == NOT_DUE);  // a casual bot has no team control
}

TEST(team_control_instant_beats_a_cast_when_a_kick_is_ready)
{
    ControlTarget kicker;
    kicker.enemyKickReady = true;
    std::int32_t const stun = ScoreControl(Stun(1), kicker, Tier::EXPERT);
    ControlTarget safe;
    CHECK(stun > ScoreControl(Stun(1), safe, Tier::EXPERT));
    CHECK(ScoreControl(Sheep(1), kicker, Tier::EXPERT) < ScoreControl(Sheep(1), safe, Tier::EXPERT));
}

TEST(team_control_score_is_deterministic)
{
    ControlTarget t;
    CHECK(ScoreControl(Sheep(1), t, Tier::EXPERT) == ScoreControl(Sheep(1), t, Tier::EXPERT));
}

// --- offers and reservations --------------------------------------------------------------------------------------------------

TEST(team_executor_is_the_best_fresh_offer_ties_to_the_lower_member)
{
    ControlBook book;
    book.Publish({ 5, 100, 118, DrCategory::INCAPACITATE, 70, 1000 }, 1000);
    book.Publish({ 3, 100, 408, DrCategory::STUN, 90, 1000 }, 1000);
    CHECK(book.IsExecutor(3, 100, 1100));
    CHECK(!book.IsExecutor(5, 100, 1100));

    book.Publish({ 5, 100, 118, DrCategory::INCAPACITATE, 90, 1100 }, 1100);
    CHECK(book.IsExecutor(3, 100, 1200));   // a tie: the lower member id
    CHECK(!book.IsExecutor(5, 100, 1200));
}

TEST(team_executor_ignores_stale_offers_and_other_targets)
{
    ControlBook book;
    book.Publish({ 3, 100, 408, DrCategory::STUN, 90, 1000 }, 1000);
    book.Publish({ 5, 100, 118, DrCategory::INCAPACITATE, 50, 1000 }, 1000);
    CHECK(book.IsExecutor(3, 100, 1000 + OFFER_TTL_MS - 1));
    // The best offer went stale (the bot stopped publishing: its cast is no longer possible), so the next one is the executor
    // once it is refreshed and the first is not.
    book.Publish({ 5, 100, 118, DrCategory::INCAPACITATE, 50, 1900 }, 1900);
    CHECK(!book.IsExecutor(3, 100, 1900));
    CHECK(book.IsExecutor(5, 100, 1900));
    CHECK(!book.IsExecutor(5, 200, 1900));  // nobody offered for that target
}

TEST(team_pending_cast_blocks_the_same_category_and_a_second_hard_control_for_every_member)
{
    ControlBook book;
    book.NoteCast(3, 100, DrCategory::STUN, true, 1000);
    CHECK(book.Blocked(5, 100, DrCategory::STUN, true, 1500));          // the same category on the same target
    CHECK(book.Blocked(5, 100, DrCategory::INCAPACITATE, true, 1500));  // a second hard control would overlap the first
    CHECK(!book.Blocked(5, 100, DrCategory::SILENCE, false, 1500));     // a silence layers on top of it
    CHECK(!book.Blocked(5, 200, DrCategory::STUN, true, 1500));         // another target
    CHECK(book.Blocked(3, 100, DrCategory::STUN, true, 1500));          // the caster itself waits for its own cast to land
    CHECK(!book.Blocked(5, 100, DrCategory::STUN, true, 1000 + PENDING_MS));  // the pending time ran out
}

TEST(team_book_is_bounded_and_forgets_what_expired)
{
    ControlBook book;
    for (std::uint32_t i = 0; i < 100; ++i)
    {
        book.Publish({ i, 1, 118, DrCategory::INCAPACITATE, 10, 1000 }, 1000);
        book.NoteCast(i, 1, DrCategory::STUN, true, 1000);
    }
    CHECK(book.OfferCount() <= MAX_OFFERS);
    CHECK(book.PendingCount() <= MAX_PENDING);
    book.Expire(1000 + 100000);
    CHECK(book.OfferCount() == 0 && book.PendingCount() == 0);
}

TEST(team_cast_withdraws_the_casters_offer)
{
    ControlBook book;
    book.Publish({ 3, 100, 408, DrCategory::STUN, 90, 1000 }, 1000);
    book.NoteCast(3, 100, DrCategory::STUN, true, 1010);
    CHECK(!book.IsExecutor(3, 100, 1020));
}

// --- not breaking control -----------------------------------------------------------------------------------------------------

TEST(team_damage_is_held_on_a_sheep_for_good_and_expert_bots_only)
{
    CHECK(HoldDamage(true, 5000, true, Tier::EXPERT));
    CHECK(HoldDamage(true, 5000, true, Tier::GOOD));
    CHECK(!HoldDamage(true, 5000, true, Tier::CASUAL));
    CHECK(!HoldDamage(true, 5000, false, Tier::EXPERT));              // a heal or a buff is fine
    CHECK(!HoldDamage(false, 5000, true, Tier::EXPERT));              // no control, no hold
    CHECK(!HoldDamage(true, HOLD_DAMAGE_MIN_LEFT_MS - 1, true, Tier::EXPERT));  // about to end anyway
}

TEST(team_healer_held_from_two_and_a_half_seconds)
{
    CHECK(HealerHeld(HEALER_HELD_MIN_MS));
    CHECK(!HealerHeld(HEALER_HELD_MIN_MS - 1));
}
