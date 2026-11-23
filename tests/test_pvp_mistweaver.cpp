#include "TestHarness.h"
#include "PvpMistweaverPolicy.h"
#include "PvpSpellCatalog.h"

// BT-657 to BT-662: the Mistweaver package: Healing Sphere spot heals, Mana Tea in a lockout, Life Cocoon as a tempo tool, Thunder
// Focus Tea into an instant Surging Mist, Tiger's Lust in a PvP fight, and the Nimble Brew catalog row.

using namespace PvpMistweaver;

namespace
{
SphereInputs Hurt(float pct)
{
    SphereInputs in;
    in.known = true;
    in.allyHealthPercent = pct;
    in.allyDistance = 25.0f;
    return in;
}
}  // namespace

TEST(mw_sphere_tops_off_a_hurt_ally_inside_the_line)
{
    CHECK(HealingSphere(Hurt(70.0f), Tier::EXPERT).cast);
    CHECK(HealingSphere(Hurt(70.0f), Tier::GOOD).cast);
    CHECK(HealingSphere(Hurt(75.0f), Tier::EXPERT).reason == SphereReason::SPOT_HEAL);
    CHECK(HealingSphere(Hurt(75.0f), Tier::GOOD).reason == SphereReason::TOO_HEALTHY);   // a good bot waits for 70
    CHECK(HealingSphere(Hurt(95.0f), Tier::EXPERT).reason == SphereReason::TOO_HEALTHY);
}

TEST(mw_sphere_leaves_the_critical_ally_to_the_direct_heals)
{
    CHECK(HealingSphere(Hurt(SPHERE_CRITICAL_PCT), Tier::EXPERT).reason == SphereReason::TOO_HURT);
    CHECK(HealingSphere(Hurt(31.0f), Tier::EXPERT).cast);
}

TEST(mw_sphere_range_known_and_casual_gates)
{
    SphereInputs in = Hurt(60.0f);
    in.allyDistance = 45.0f;
    CHECK(HealingSphere(in, Tier::EXPERT).reason == SphereReason::OUT_OF_RANGE);
    in = Hurt(60.0f);
    in.known = false;
    CHECK(HealingSphere(in, Tier::EXPERT).reason == SphereReason::NOT_KNOWN);
    CHECK(HealingSphere(Hurt(60.0f), Tier::CASUAL).reason == SphereReason::CASUAL_SKIPS);
}

TEST(mw_sphere_expert_keeps_a_slot_free_and_a_good_bot_does_not)
{
    SphereInputs in = Hurt(60.0f);
    in.spheresOut = 2;
    CHECK(HealingSphere(in, Tier::EXPERT).reason == SphereReason::TOO_MANY_SPHERES);
    CHECK(HealingSphere(in, Tier::GOOD).cast);
    in.spheresOut = 3;
    CHECK(HealingSphere(in, Tier::GOOD).reason == SphereReason::TOO_MANY_SPHERES);
}

TEST(mw_sphere_score_prefers_the_more_hurt_and_the_attacked_ally)
{
    std::int32_t const a = HealingSphere(Hurt(70.0f), Tier::EXPERT).score;
    std::int32_t const b = HealingSphere(Hurt(50.0f), Tier::EXPERT).score;
    CHECK(b > a);
    SphereInputs attacked = Hurt(70.0f);
    attacked.allyAttackedByPlayer = true;
    CHECK(HealingSphere(attacked, Tier::EXPERT).score == a + SCORE_SPHERE_ATTACKED);
}

namespace
{
TeaInputs Tea(std::uint32_t stacks, float mana)
{
    TeaInputs in;
    in.stacks = stacks;
    in.manaPercent = mana;
    return in;
}
}  // namespace

TEST(mw_mana_tea_needs_two_stacks)
{
    CHECK(ManaTea(Tea(1, 20.0f), Tier::EXPERT) == TeaReason::NO_STACKS);
    CHECK(TeaDue(ManaTea(Tea(2, 70.0f), Tier::EXPERT)));
}

TEST(mw_mana_tea_works_in_a_lockout_even_with_allies_hurt)
{
    TeaInputs in = Tea(3, 85.0f);
    in.silenced = true;
    in.lowestAllyPercent = 30.0f;
    CHECK(ManaTea(in, Tier::EXPERT) == TeaReason::LOCKOUT);
    CHECK(ManaTea(in, Tier::GOOD) == TeaReason::LOCKOUT);
    in.manaPercent = 95.0f;   // nothing worth drinking
    CHECK(!TeaDue(ManaTea(in, Tier::EXPERT)));
}

TEST(mw_mana_tea_in_a_quiet_moment_by_tier_line)
{
    TeaInputs in = Tea(2, 75.0f);
    CHECK(ManaTea(in, Tier::EXPERT) == TeaReason::QUIET);
    CHECK(!TeaDue(ManaTea(in, Tier::GOOD)));   // the good line is 65
    in.manaPercent = 60.0f;
    CHECK(TeaDue(ManaTea(in, Tier::GOOD)));
    in.lowestAllyPercent = 60.0f;   // an ally needs a heal: no tea
    CHECK(!TeaDue(ManaTea(in, Tier::EXPERT)));
    in.lowestAllyPercent = 100.0f;
    in.attackedByPlayer = true;
    CHECK(!TeaDue(ManaTea(in, Tier::EXPERT)));
}

TEST(mw_mana_tea_casual_keeps_the_old_mana_edge)
{
    TeaInputs in = Tea(4, 30.0f);
    in.silenced = true;
    CHECK(ManaTea(in, Tier::CASUAL) == TeaReason::CASUAL_OLD_RULE);
    CHECK(!TeaDue(ManaTea(in, Tier::CASUAL)));
}

namespace
{
CocoonInputs Focused(float pct, std::int32_t attackers = 1)
{
    CocoonInputs in;
    in.ready = true;
    in.allyHealthPercent = pct;
    in.playerAttackers = attackers;
    return in;
}
}  // namespace

TEST(mw_cocoon_expert_cocoons_at_the_tempo_line_when_a_burst_is_seen)
{
    CocoonInputs in = Focused(55.0f);
    CHECK(LifeCocoon(in, Tier::EXPERT).reason == CocoonReason::TOO_HEALTHY);   // no burst: the lower line
    in.enemyBurstSeen = true;
    CocoonVerdict v = LifeCocoon(in, Tier::EXPERT);
    CHECK(v.cast);
    CHECK(v.reason == CocoonReason::BURST_SEEN);
    in.allyHealthPercent = 56.0f;
    CHECK(!LifeCocoon(in, Tier::EXPERT).cast);
}

TEST(mw_cocoon_without_a_burst_waits_for_the_lower_line_and_a_good_bot_for_forty)
{
    CHECK(LifeCocoon(Focused(45.0f), Tier::EXPERT).reason == CocoonReason::UNDER_FIRE);
    CHECK(!LifeCocoon(Focused(46.0f), Tier::EXPERT).cast);
    CHECK(!LifeCocoon(Focused(45.0f), Tier::GOOD).cast);
    CHECK(LifeCocoon(Focused(40.0f), Tier::GOOD).cast);
    // A good bot does not read a burst aura.
    CocoonInputs in = Focused(52.0f);
    in.enemyBurstSeen = true;
    CHECK(!LifeCocoon(in, Tier::GOOD).cast);
}

TEST(mw_cocoon_needs_a_player_on_the_ally_and_no_cocoon_already)
{
    CHECK(LifeCocoon(Focused(30.0f, 0), Tier::EXPERT).reason == CocoonReason::NOT_FOCUSED);
    CocoonInputs in = Focused(30.0f);
    in.allyHasCocoon = true;
    CHECK(LifeCocoon(in, Tier::EXPERT).reason == CocoonReason::NOT_READY);
    in = Focused(30.0f);
    in.ready = false;
    CHECK(!LifeCocoon(in, Tier::EXPERT).cast);
    CHECK(LifeCocoon(Focused(30.0f), Tier::CASUAL).reason == CocoonReason::CASUAL_SKIPS);
}

TEST(mw_cocoon_score_picks_the_most_pressed_ally_deterministically)
{
    std::int32_t const lone = LifeCocoon(Focused(40.0f, 1), Tier::EXPERT).score;
    std::int32_t const two = LifeCocoon(Focused(40.0f, 2), Tier::EXPERT).score;
    std::int32_t const lower = LifeCocoon(Focused(30.0f, 1), Tier::EXPERT).score;
    CHECK(two == lone + SCORE_COCOON_PER_ATTACKER);
    CHECK(lower > lone);
    CHECK(LifeCocoon(Focused(40.0f, 9), Tier::EXPERT).score == LifeCocoon(Focused(40.0f, COCOON_ATTACKERS_COUNTED), Tier::EXPERT).score);
}

namespace
{
TftInputs Tft(float lowest)
{
    TftInputs in;
    in.known = true;
    in.ready = true;
    in.lowestAllyPercent = lowest;
    in.allyAttackedByPlayer = true;
    in.enemyKickNotSpent = true;
    return in;
}
}  // namespace

TEST(mw_tft_is_pressed_before_a_surging_mist_when_a_kick_could_stop_the_heal)
{
    CHECK(ThunderFocusTea(Tft(55.0f), Tier::EXPERT) == TftReason::CAST_TEA);
    TftInputs in = Tft(55.0f);
    in.enemyKickNotSpent = false;
    CHECK(ThunderFocusTea(in, Tier::EXPERT) == TftReason::NO_KICK_RISK);
    CHECK(ThunderFocusTea(in, Tier::GOOD) == TftReason::NOT_THREATENED);   // good line is 50
    in.lowestAllyPercent = 45.0f;
    CHECK(ThunderFocusTea(in, Tier::GOOD) == TftReason::CAST_TEA);   // a good bot does not read the kick
}

TEST(mw_tft_up_sends_the_instant_surging_mist_at_the_weak_ally)
{
    TftInputs in = Tft(70.0f);
    in.teaUp = true;
    CHECK(ThunderFocusTea(in, Tier::EXPERT) == TftReason::SURGE_NOW);
    CHECK(ThunderFocusTea(in, Tier::CASUAL) == TftReason::SURGE_NOW);   // the instant heal is free for any tier
    in.lowestAllyPercent = 90.0f;
    CHECK(ThunderFocusTea(in, Tier::EXPERT) == TftReason::NOT_READY);   // the empowered cast waits for a hurt ally
}

TEST(mw_tft_needs_threat_readiness_and_a_non_casual_bot)
{
    TftInputs in = Tft(55.0f);
    in.allyAttackedByPlayer = false;
    CHECK(ThunderFocusTea(in, Tier::EXPERT) == TftReason::NOT_THREATENED);
    in = Tft(55.0f);
    in.ready = false;
    CHECK(ThunderFocusTea(in, Tier::EXPERT) == TftReason::NOT_READY);
    CHECK(ThunderFocusTea(Tft(40.0f), Tier::CASUAL) == TftReason::CASUAL_SKIPS);
}

TEST(mw_tigers_lust_frees_self_before_an_ally_only_in_a_pvp_fight)
{
    CHECK(TigersLust(true, true, true, Tier::EXPERT) == LustReason::FREE_SELF);
    CHECK(TigersLust(true, false, true, Tier::GOOD) == LustReason::FREE_ALLY);
    CHECK(TigersLust(true, false, false, Tier::EXPERT) == LustReason::NO);
    CHECK(TigersLust(false, true, true, Tier::EXPERT) == LustReason::NOT_PVP);
    CHECK(TigersLust(true, true, true, Tier::CASUAL) == LustReason::CASUAL_SKIPS);
    CHECK(!LustDue(LustReason::NOT_PVP));
}

TEST(mw_nimble_brew_is_a_cheap_class_break_for_every_monk)
{
    using namespace PvpSpells;
    bool found = false;
    for (Entry const& e : RowsFor(CLS_MONK, Kind::BREAKER))
        if (e.spellId == 137562)
        {
            found = true;
            CHECK(e.cost == BREAK_COST_CLASS);
            CHECK((e.breaks & CC_STUN) != 0 && (e.breaks & CC_FEAR) != 0 && (e.breaks & CC_ROOT) != 0);
            CHECK((e.breaks & CC_SILENCE) == 0);
            CHECK(e.cost < Find(TRINKET_SPELL)->cost);   // before the medallion
        }
    CHECK(found);
    CHECK(RowsFor(CLS_MAGE, Kind::BREAKER).size() == 3);   // other classes are untouched
}
