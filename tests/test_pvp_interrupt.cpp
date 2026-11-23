#include "TestHarness.h"
#include "PvpInterruptPolicy.h"

// BT-521: what a bot kicks in a PvP fight (rule R1) and when: crowd control on our side, heals inside our kill window, large
// damage on a hurt friend; filler held; late in the cast by tier; a backup kicker waits; a healer's wrong school is held.

using namespace PvpInterrupt;
using namespace PvpSpells;

namespace
{
CastShape Polymorph() { CastShape s; s.spellId = 118; s.mechanicMask = MechBit(MECH_POLYMORPH); s.castMs = 1700; return s; }
CastShape Heal(std::uint32_t castMs = 2000) { CastShape s; s.spellId = 2061; s.positive = true; s.heal = true; s.castMs = castMs; return s; }
CastShape Nuke(std::uint32_t castMs = 2250) { CastShape s; s.spellId = 133; s.damage = true; s.castMs = castMs; return s; }
CastShape Filler() { CastShape s; s.spellId = 585; s.damage = true; s.castMs = 1000; return s; }

CastFacts Base(Tier tier = Tier::EXPERT)
{
    CastFacts f;
    f.tier = tier;
    f.botGuid = 4242;
    f.castKey = 1;
    f.ownTarget = true;
    return f;
}

// A castKey for which the filler roll holds the kick (no unwise kick) at this tier.
std::uint32_t HoldingKey(Tier tier, std::uint32_t guid)
{
    for (std::uint32_t k = 1; k < 5000; ++k)
        if (PvpAware::Roll(guid, k, 0x1D3u) >= FillerKickPercent(tier) && InterruptPolicy::TakesTurn(tier, guid, k))
            return k;
    return 0;
}
}  // namespace

TEST(pvp_int_classifies_the_cast_by_what_it_does)
{
    CHECK(Classify(Polymorph()) == CastClass::CROWD_CONTROL);
    CastShape fear; fear.spellId = 5782; fear.castMs = 1500;
    CHECK(Classify(fear) == CastClass::CROWD_CONTROL);
    CastShape cyclone; cyclone.spellId = 33786; cyclone.castMs = 1700;
    CHECK(Classify(cyclone) == CastClass::CROWD_CONTROL);
    // A spell the catalog does not know falls back on its mechanics.
    CastShape unknownStun; unknownStun.spellId = 999999; unknownStun.mechanicMask = MechBit(MECH_STUN);
    CHECK(Classify(unknownStun) == CastClass::CROWD_CONTROL);
    CHECK(Classify(Heal()) == CastClass::HEAL);
    CHECK(Classify(Nuke()) == CastClass::BIG_DAMAGE);
    CHECK(Classify(Filler()) == CastClass::FILLER);
    // A channeled damage spell is large whatever its cast time says.
    CastShape channel; channel.spellId = 5; channel.damage = true; channel.channeled = true;
    CHECK(Classify(channel) == CastClass::BIG_DAMAGE);
    // A root or a snare is not worth a kick; a buff is filler.
    CastShape nova; nova.spellId = 122; nova.mechanicMask = MechBit(MECH_ROOT);
    CHECK(Classify(nova) == CastClass::FILLER);
    CastShape buff; buff.spellId = 1459; buff.positive = true; buff.castMs = 1500;
    CHECK(Classify(buff) == CastClass::FILLER);
}

TEST(pvp_int_control_on_our_side_is_kicked_healer_first)
{
    CastFacts f = Base();
    f.shape = Polymorph();
    f.onOurHealer = true; f.onOurSide = true;
    Decision d = Decide(f);
    CHECK(d.wants && d.score == SCORE_CC_HEALER);
    f.onOurHealer = false; f.onUs = true;
    d = Decide(f);
    CHECK(d.wants && d.score == SCORE_CC_SELF);
    f.onUs = false;
    d = Decide(f);
    CHECK(d.wants && d.score == SCORE_CC_FRIEND);
    CHECK(SCORE_CC_HEALER > SCORE_CC_SELF && SCORE_CC_SELF > SCORE_CC_FRIEND && SCORE_CC_FRIEND > SCORE_HEAL);
}

TEST(pvp_int_control_not_aimed_at_our_side_is_not_ours_to_stop)
{
    CastFacts f = Base();
    f.shape = Polymorph();
    f.castKey = HoldingKey(Tier::EXPERT, f.botGuid);
    Decision const d = Decide(f);
    CHECK(!d.wants);
    CHECK(d.score == 0);
}

TEST(pvp_int_a_heal_is_kicked_only_inside_the_kill_window)
{
    CastFacts f = Base();
    f.castKey = HoldingKey(Tier::EXPERT, f.botGuid);
    f.shape = Heal();
    f.healTargetHp = 35.0f;
    Decision d = Decide(f);
    CHECK(d.wants && d.score == SCORE_HEAL);
    f.healTargetHp = 55.0f;
    CHECK(!Decide(f).wants);                       // above the window, no burst: filler
    f.ourBurst = true;
    d = Decide(f);
    CHECK(d.wants && d.score == SCORE_BURST_HEAL); // our burst widens the window
    f.healTargetHp = 80.0f;
    CHECK(!Decide(f).wants);
    // The enemy healer healing itself while it is the kill target: the best heal to stop.
    f.ourBurst = false;
    f.healTargetHp = 30.0f;
    f.healOnCasterItself = true;
    f.healTargetIsKillTarget = true;
    d = Decide(f);
    CHECK(d.wants && d.score == SCORE_HEAL_HEALER);
}

TEST(pvp_int_large_damage_on_a_hurt_friend_is_kicked_not_on_a_healthy_one)
{
    CastFacts f = Base();
    f.castKey = HoldingKey(Tier::EXPERT, f.botGuid);
    f.shape = Nuke();
    f.onOurSide = true;
    f.allyHp = 55.0f;
    Decision d = Decide(f);
    CHECK(d.wants && d.score == SCORE_BIG_DAMAGE);
    f.allyHp = 90.0f;
    CHECK(!Decide(f).wants);
    f.onOurSide = false;
    f.allyHp = 30.0f;
    CHECK(!Decide(f).wants);
}

TEST(pvp_int_a_healer_casting_the_wrong_school_keeps_its_lock_unspent)
{
    // A restoration shaman casting Lava Burst (Fire) at our hurt friend: Fire closed, Nature (its heals) open.
    CastFacts f = Base();
    f.castKey = HoldingKey(Tier::EXPERT, f.botGuid);
    f.shape = Nuke();
    f.castSchool = SCHOOL_FIRE;
    f.casterIsHealer = true;
    f.casterClass = CLS_SHAMAN;
    f.onOurSide = true;
    f.allyHp = 40.0f;
    Decision d = Decide(f);
    CHECK(!d.wants);
    // The same cast in the school it heals with is worth the lock.
    f.castSchool = SCHOOL_NATURE;
    d = Decide(f);
    CHECK(d.wants && d.score == SCORE_BIG_DAMAGE);
    // A priest healer: Holy Fire (Holy) locks the heal school, Shadow Word: Pain does not.
    f.casterClass = CLS_PRIEST;
    f.castSchool = SCHOOL_SHADOW;
    CHECK(!Decide(f).wants);
    f.castSchool = SCHOOL_HOLY;
    CHECK(Decide(f).wants);
    // A control cast is stopped whatever its school.
    CastFacts c = Base();
    c.shape = Polymorph();
    c.castSchool = SCHOOL_ARCANE;
    c.casterIsHealer = true;
    c.casterClass = CLS_SHAMAN;
    c.onOurSide = true;
    CHECK(Decide(c).wants);
    CHECK(HealSchoolsOfClass(CLS_WARRIOR) == 0);
}

TEST(pvp_int_a_school_already_closed_by_a_partner_is_not_kicked_again)
{
    CastFacts f = Base();
    f.shape = Polymorph();
    f.onUs = true;
    f.schoolAlreadyLocked = true;
    Decision const d = Decide(f);
    CHECK(!d.wants);
    CHECK(std::string(d.why) == "school-locked");
}

TEST(pvp_int_tiers_see_different_casts)
{
    CastFacts f = Base(Tier::EXPERT);
    f.shape = Polymorph();
    f.onUs = true;
    f.ownTarget = false;
    CHECK(Sees(f));
    f.tier = Tier::GOOD;
    CHECK(Sees(f));                 // it is aimed at the bot itself
    f.onUs = false; f.onOurSide = true;
    CHECK(!Sees(f));                // a friend, a caster that is neither its target nor a healer
    f.casterIsHealer = true;
    CHECK(Sees(f));
    f.tier = Tier::CASUAL;
    CHECK(!Sees(f));
    f.ownTarget = true;
    CHECK(Sees(f));
}

TEST(pvp_int_filler_kicks_follow_the_tier_rate)
{
    int expert = 0, good = 0, casual = 0;
    for (std::uint32_t key = 1; key <= 3000; ++key)
    {
        CastFacts f = Base(Tier::EXPERT);
        f.shape = Filler();
        f.castKey = key;
        if (Decide(f).wants) ++expert;
        f.tier = Tier::GOOD;
        if (Decide(f).wants) ++good;
        f.tier = Tier::CASUAL;
        if (Decide(f).wants) ++casual;
    }
    CHECK(expert > 90 && expert < 220);     // about 5 percent
    CHECK(good > 780 && good < 1020);       // about 30 percent
    CHECK(casual > 1900 && casual < 2100);  // every turn it takes: it misses one in three
}

TEST(pvp_int_a_casual_bot_misses_one_turn_in_three)
{
    int missed = 0;
    for (std::uint32_t key = 1; key <= 3000; ++key)
    {
        CastFacts f = Base(Tier::CASUAL);
        f.shape = Polymorph();
        f.onUs = true;
        f.castKey = key;
        Decision const d = Decide(f);
        if (!d.wants)
        {
            ++missed;
            CHECK(std::string(d.why) == "missed-turn");
        }
    }
    CHECK(missed > 850 && missed < 1150);
}

TEST(pvp_int_expert_kicks_between_sixty_and_ninety_percent_and_varies_per_cast)
{
    int low = 0, high = 0;
    for (std::uint32_t key = 1; key <= 400; ++key)
    {
        std::uint32_t const pct = KickPercent(Tier::EXPERT, 4242, key);
        CHECK(pct >= 60 && pct <= 90);
        if (pct < 70) ++low;
        if (pct > 80) ++high;
        // Fixed per cast: the same cast key answers the same.
        CHECK(KickPercent(Tier::EXPERT, 4242, key) == pct);
    }
    CHECK(low > 60 && high > 60);
    CHECK(KickPercent(Tier::GOOD, 4242, 7) == 70);
    CHECK(KickPercent(Tier::CASUAL, 4242, 7) == 0);
}

TEST(pvp_int_the_kick_waits_for_the_late_part_of_a_long_cast)
{
    std::uint32_t const react = OneVsOne::ReactionMs(Tier::GOOD, 4242);
    std::uint32_t const at = KickAtMs(Tier::GOOD, 4242, 3, 2000, false, 0);
    CHECK(at == 1400);                        // 70 percent of 2 s
    CHECK(at >= react);
    CHECK(KickAtMs(Tier::CASUAL, 4242, 3, 2000, false, 0) == OneVsOne::ReactionMs(Tier::CASUAL, 4242));
    for (std::uint32_t key = 1; key <= 200; ++key)
    {
        std::uint32_t const e = KickAtMs(Tier::EXPERT, 4242, key, 2500, false, 0);
        CHECK(e >= OneVsOne::ReactionMs(Tier::EXPERT, 4242));
        CHECK(e <= 2500 - SAFETY_MS);
        CHECK(e >= 1500);
    }
}

TEST(pvp_int_a_cast_shorter_than_a_reaction_is_kicked_at_once)
{
    std::uint32_t const react = OneVsOne::ReactionMs(Tier::CASUAL, 4242);
    CHECK(KickAtMs(Tier::CASUAL, 4242, 1, react + SAFETY_MS, false, 0) == react);
    std::uint32_t const reactE = OneVsOne::ReactionMs(Tier::EXPERT, 4242);
    CHECK(KickAtMs(Tier::EXPERT, 4242, 1, reactE + 100, false, 0) == reactE);
    // A channel is kicked once the bot noticed it.
    CHECK(KickAtMs(Tier::GOOD, 4242, 1, 0, true, 0) == OneVsOne::ReactionMs(Tier::GOOD, 4242));
}

TEST(pvp_int_a_backup_kicker_waits_behind_the_first)
{
    std::uint32_t const first = KickAtMs(Tier::GOOD, 4242, 3, 3000, false, 0);
    CHECK(KickAtMs(Tier::GOOD, 4242, 3, 3000, false, 1) == first + STAGGER_MS);
    CHECK(KickAtMs(Tier::GOOD, 4242, 3, 3000, false, 2) == first + 2 * STAGGER_MS);
    // The backup of a short cast would kick after it ended: due() never says yes in time, so the first kick stands alone.
    std::uint32_t const backup = KickAtMs(Tier::GOOD, 4242, 3, 1000, false, 1);
    CHECK(backup > 1000 - SAFETY_MS + 400);
}

TEST(pvp_int_due_is_time_zero_at_the_first_look)
{
    CastFacts f = Base(Tier::GOOD);
    f.shape = Polymorph();
    f.onUs = true;
    f.castKey = HoldingKey(Tier::GOOD, f.botGuid);
    Decision const d = Decide(f);
    CHECK(d.wants);
    CHECK(!Due(d, 0));
    CHECK(!Due(d, d.kickAtMs - 1));
    CHECK(Due(d, d.kickAtMs));
    CHECK(Due(d, d.kickAtMs + 200));
    Decision hold;
    CHECK(!Due(hold, 100000));
}

TEST(pvp_int_every_decision_is_deterministic)
{
    for (std::uint32_t key = 1; key <= 100; ++key)
    {
        CastFacts f = Base(Tier::EXPERT);
        f.shape = Heal();
        f.healTargetHp = 30.0f;
        f.castKey = key;
        Decision const a = Decide(f);
        Decision const b = Decide(f);
        CHECK(a.wants == b.wants && a.score == b.score && a.kickAtMs == b.kickAtMs);
    }
}
