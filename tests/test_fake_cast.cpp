#include "TestHarness.h"
#include "FakeCastPolicy.h"

// BT-523: fake casting: experts only, 60 to 70 percent of the important casts under a threat, never in an emergency, the
// cancel point fixed per cast, and the next cast real.

using namespace FakeCast;

namespace
{
PvpInterrupt::CastShape Polymorph() { PvpInterrupt::CastShape s; s.spellId = 118; s.castMs = 1700; return s; }
PvpInterrupt::CastShape Heal(std::uint32_t castMs = 2500) { PvpInterrupt::CastShape s; s.spellId = 2061; s.positive = true; s.heal = true; s.castMs = castMs; return s; }
PvpInterrupt::CastShape Nuke(std::uint32_t castMs = 2250) { PvpInterrupt::CastShape s; s.spellId = 133; s.damage = true; s.castMs = castMs; return s; }

Facts Under(PvpInterrupt::CastShape shape)
{
    Facts f;
    f.botGuid = 4242;
    f.castKey = 1;
    f.shape = shape;
    f.healTargetHp = 50.0f;
    f.kickerInRange = true;
    return f;
}

std::uint32_t FakingKey(Facts f)
{
    for (std::uint32_t k = 1; k < 5000; ++k)
    {
        f.castKey = k;
        if (Decide(f).fake)
            return k;
    }
    return 0;
}
}  // namespace

TEST(fake_only_experts_fake)
{
    Facts f = Under(Polymorph());
    std::uint32_t const key = FakingKey(f);
    CHECK(key != 0);
    f.castKey = key;
    CHECK(Decide(f).fake);
    f.tier = Tier::GOOD;
    CHECK(!Decide(f).fake);
    CHECK(std::string(Decide(f).why) == "not-expert");
    f.tier = Tier::CASUAL;
    CHECK(!Decide(f).fake);
}

TEST(fake_important_casts_are_control_big_heals_and_long_damage)
{
    CHECK(Important(Polymorph(), 100.0f));
    CHECK(Important(Heal(), 60.0f));
    CHECK(!Important(Heal(), 90.0f));                 // a heal on a healthy target is no loss
    CHECK(Important(Nuke(2250), 100.0f));
    CHECK(!Important(Nuke(1800), 100.0f));            // a short nuke
    CHECK(!Important(Heal(1200), 40.0f));             // shorter than 1.5 s: no time to fake
    PvpInterrupt::CastShape channel = Nuke(0);
    channel.channeled = true;
    CHECK(!Important(channel, 100.0f));               // channels are not cancelled
    PvpInterrupt::CastShape buff; buff.spellId = 1459; buff.positive = true; buff.castMs = 2000;
    CHECK(!Important(buff, 100.0f));
}

TEST(fake_never_in_an_emergency)
{
    Facts f = Under(Heal());
    f.healTargetHp = 20.0f;                           // the heal the target needs now
    f.castKey = 1;
    for (std::uint32_t k = 1; k <= 500; ++k)
    {
        f.castKey = k;
        Verdict const v = Decide(f);
        CHECK(!v.fake);
        CHECK(std::string(v.why) == "emergency");
    }
    f.healTargetHp = 50.0f;
    f.selfHp = 15.0f;                                 // the bot itself is nearly dead
    CHECK(std::string(Decide(f).why) == "emergency");
    CHECK(!StillWorthCancelling(true, Heal(), 20.0f, 100.0f));
    CHECK(StillWorthCancelling(true, Heal(), 50.0f, 100.0f));
    CHECK(!StillWorthCancelling(false, Heal(), 50.0f, 100.0f));
}

TEST(fake_only_while_a_kicker_is_in_range)
{
    Facts f = Under(Polymorph());
    f.kickerInRange = false;
    for (std::uint32_t k = 1; k <= 300; ++k)
    {
        f.castKey = k;
        CHECK(!Decide(f).fake);
    }
    CHECK(std::string(Decide(f).why) == "no-kicker");
}

TEST(fake_the_share_of_important_casts_under_threat_is_sixty_five_percent)
{
    int fakes = 0;
    int total = 0;
    for (std::uint32_t k = 1; k <= 4000; ++k)
    {
        Facts f = Under(Polymorph());
        f.castKey = k;
        ++total;
        if (Decide(f).fake)
            ++fakes;
    }
    double const share = 100.0 * fakes / total;
    CHECK(share >= 60.0 && share <= 70.0);
    // The chance is a setting.
    int none = 0, all = 0;
    for (std::uint32_t k = 1; k <= 500; ++k)
    {
        Facts f = Under(Polymorph());
        f.castKey = k;
        f.chancePercent = 0;
        if (Decide(f).fake) ++none;
        f.chancePercent = 100;
        if (Decide(f).fake) ++all;
    }
    CHECK(none == 0 && all == 500);
}

TEST(fake_the_cancel_point_is_between_45_and_85_percent_and_fixed_per_cast)
{
    int low = 0, high = 0;
    for (std::uint32_t k = 1; k <= 400; ++k)
    {
        std::uint32_t const at = CancelAtMs(4242, k, 2500);
        CHECK(at >= 2500 * 45 / 100);
        CHECK(at <= 2500 * 85 / 100);
        CHECK(at == CancelAtMs(4242, k, 2500));
        if (at < 1500) ++low;
        if (at > 1900) ++high;
    }
    CHECK(low > 40 && high > 40);
    // Never after the cast would have ended, and not at once on a short cast.
    CHECK(CancelAtMs(4242, 7, 1500) <= 1500 - SAFETY_MS);
    CHECK(CancelAtMs(4242, 7, 1500) >= MIN_CANCEL_MS);
}

TEST(fake_the_cast_after_a_fake_is_real)
{
    Facts f = Under(Polymorph());
    f.castKey = FakingKey(f);
    CHECK(Decide(f).fake);
    f.sinceLastFakeMs = 3000;
    Verdict v = Decide(f);
    CHECK(!v.fake && std::string(v.why) == "real-after-fake");
    f.sinceLastFakeMs = 9000;
    CHECK(Decide(f).fake);
}

TEST(fake_the_cancel_is_due_at_its_point_and_not_for_a_real_cast)
{
    Facts f = Under(Polymorph());
    f.castKey = FakingKey(f);
    Verdict const v = Decide(f);
    CHECK(v.fake);
    CHECK(!CancelDue(v, v.cancelAtMs - 1));
    CHECK(CancelDue(v, v.cancelAtMs));
    CHECK(CancelDue(v, v.cancelAtMs + 100));
    Verdict real;
    CHECK(!CancelDue(real, 100000));
}

TEST(fake_decisions_are_deterministic)
{
    for (std::uint32_t k = 1; k <= 200; ++k)
    {
        Facts f = Under(Nuke());
        f.castKey = k;
        Verdict const a = Decide(f);
        Verdict const b = Decide(f);
        CHECK(a.fake == b.fake && a.cancelAtMs == b.cancelAtMs);
    }
}
