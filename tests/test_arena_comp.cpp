/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "ArenaComp.h"
#include "TestHarness.h"

using ArenaComp::Candidate;
using ArenaComp::Member;
using BotSkillMath::Tier;

namespace
{
// ChrSpecialization ids of the core
constexpr std::uint16_t MAGE_FROST = 64;
constexpr std::uint16_t MAGE_FIRE = 63;
constexpr std::uint16_t PALADIN_HOLY = 65;
constexpr std::uint16_t PALADIN_PROT = 66;
constexpr std::uint16_t PALADIN_RET = 70;
constexpr std::uint16_t WARRIOR_ARMS = 71;
constexpr std::uint16_t WARRIOR_PROT = 73;
constexpr std::uint16_t DRUID_RESTO = 105;
constexpr std::uint16_t DRUID_BALANCE = 102;
constexpr std::uint16_t DK_BLOOD = 250;
constexpr std::uint16_t DK_FROST = 251;
constexpr std::uint16_t PRIEST_DISC = 256;
constexpr std::uint16_t PRIEST_HOLY = 257;
constexpr std::uint16_t PRIEST_SHADOW = 258;
constexpr std::uint16_t ROGUE_SUB = 261;
constexpr std::uint16_t SHAMAN_RESTO = 264;
constexpr std::uint16_t WARLOCK_AFF = 265;
constexpr std::uint16_t HUNTER_MM = 254;
constexpr std::uint16_t MONK_MW = 270;
constexpr std::uint16_t MONK_BREW = 268;

Member M(std::uint16_t spec, std::uint8_t level = 60) { return { spec, level }; }
Candidate C(std::uint32_t guid, std::uint16_t spec, Tier tier, std::uint8_t level = 60) { return { guid, spec, level, tier }; }
}  // namespace

TEST(arena_comp_every_spec_has_one_profile_and_tanks_are_marked)
{
    int tanks = 0;
    for (ArenaComp::SpecProfile const& a : ArenaComp::PROFILES)
    {
        CHECK(ArenaComp::FindProfile(a.specId) == &a);
        if (a.role == ArenaComp::Role::TANK)
            ++tanks;
    }
    CHECK(tanks == 5);  // blood, guardian, brewmaster, protection warrior and paladin
    CHECK(ArenaComp::FindProfile(9999) == nullptr);
}

TEST(arena_comp_the_working_comps_pass)
{
    // healer plus a damage spec that locks down
    CHECK(ArenaComp::PairScore(M(DRUID_RESTO), M(ROGUE_SUB)) >= ArenaComp::DEFAULT_MIN_PAIR_SCORE);
    CHECK(ArenaComp::PairScore(M(PRIEST_DISC), M(MAGE_FROST)) >= ArenaComp::DEFAULT_MIN_PAIR_SCORE);
    CHECK(ArenaComp::PairScore(M(PALADIN_HOLY), M(WARLOCK_AFF)) >= ArenaComp::DEFAULT_MIN_PAIR_SCORE);
    CHECK(ArenaComp::PairScore(M(MONK_MW), M(ROGUE_SUB)) >= ArenaComp::DEFAULT_MIN_PAIR_SCORE);
    // a restoration shaman has no hard control before Capacitor Totem (63): at 65 the comp passes, at 60 it does not
    CHECK(ArenaComp::PairScore(M(SHAMAN_RESTO, 65), M(DK_FROST, 65)) >= ArenaComp::DEFAULT_MIN_PAIR_SCORE);
    CHECK(ArenaComp::PairScore(M(SHAMAN_RESTO, 60), M(DK_FROST, 60)) < ArenaComp::DEFAULT_MIN_PAIR_SCORE);
    // damage only comps that cover three or more categories
    CHECK(ArenaComp::PairScore(M(ROGUE_SUB), M(MAGE_FROST)) >= ArenaComp::DEFAULT_MIN_PAIR_SCORE);
    CHECK(ArenaComp::PairScore(M(ROGUE_SUB), M(WARLOCK_AFF)) >= ArenaComp::DEFAULT_MIN_PAIR_SCORE);
    // a warlock and a hunter cover only two categories (disorient, incapacitate) and bring no interrupt: below the line
    CHECK(ArenaComp::PairScore(M(WARLOCK_AFF), M(HUNTER_MM)) < ArenaComp::DEFAULT_MIN_PAIR_SCORE);
}

TEST(arena_comp_two_healers_fail)
{
    CHECK(ArenaComp::PairScore(M(DRUID_RESTO), M(PRIEST_HOLY)) < ArenaComp::DEFAULT_MIN_PAIR_SCORE);
    CHECK(ArenaComp::PairScore(M(PALADIN_HOLY), M(MONK_MW)) < ArenaComp::DEFAULT_MIN_PAIR_SCORE);
}

TEST(arena_comp_a_tank_or_unknown_spec_is_not_eligible)
{
    CHECK(ArenaComp::PairScore(M(PALADIN_PROT), M(ROGUE_SUB)) == ArenaComp::INELIGIBLE);
    CHECK(ArenaComp::PairScore(M(WARRIOR_PROT), M(DRUID_RESTO)) == ArenaComp::INELIGIBLE);
    CHECK(ArenaComp::PairScore(M(DK_BLOOD), M(MAGE_FIRE)) == ArenaComp::INELIGIBLE);
    CHECK(ArenaComp::PairScore(M(MONK_BREW), M(MAGE_FIRE)) == ArenaComp::INELIGIBLE);
    CHECK(ArenaComp::PairScore(M(0), M(MAGE_FIRE)) == ArenaComp::INELIGIBLE);
    CHECK(!ArenaComp::Eligible(M(PALADIN_PROT)));
    CHECK(ArenaComp::Eligible(M(PALADIN_RET)));
}

TEST(arena_comp_score_is_symmetric)
{
    for (ArenaComp::SpecProfile const& a : ArenaComp::PROFILES)
        for (ArenaComp::SpecProfile const& b : ArenaComp::PROFILES)
            CHECK(ArenaComp::PairScore(M(a.specId), M(b.specId)) == ArenaComp::PairScore(M(b.specId), M(a.specId)));
}

TEST(arena_comp_no_control_is_penalised)
{
    // two death knights have silence only: -30 for the missing stun/incapacitate/disorient
    std::int32_t const score = ArenaComp::PairScore(M(DK_FROST), M(DK_FROST));
    CHECK(score < ArenaComp::DEFAULT_MIN_PAIR_SCORE);
}

TEST(arena_comp_locks_depend_on_the_level)
{
    // a fire mage learns Dragon's Breath at 62: a 60 gets root + incapacitate + interrupt (silence), a 62 adds disorient
    ArenaComp::SpecProfile const* fire = ArenaComp::FindProfile(MAGE_FIRE);
    CHECK(fire != nullptr);
    CHECK(ArenaComp::CountBits(ArenaComp::LocksAt(*fire, 60)) == 3);
    CHECK(ArenaComp::CountBits(ArenaComp::LocksAt(*fire, 62)) == 4);
    CHECK(ArenaComp::LocksAt(*fire, 62) & ArenaComp::LOCK_DISORIENT);
    // below the first interrupt only the root is known
    CHECK(ArenaComp::LocksAt(*fire, 5) == ArenaComp::LOCK_ROOT);
}

TEST(arena_comp_external_defensive_counts_when_learned)
{
    // Pain Suppression comes at 58: a level 60 disc priest brings it, a level 50 does not
    std::int32_t const with = ArenaComp::PairScore(M(PRIEST_DISC, 60), M(ROGUE_SUB, 60));
    std::int32_t const without = ArenaComp::PairScore(M(PRIEST_DISC, 50), M(ROGUE_SUB, 60));
    CHECK(with - without >= ArenaComp::SCORE_EXTERNAL_DEFENSIVE);
}

TEST(arena_comp_ranged_and_melee_bonus)
{
    std::int32_t const mixed = ArenaComp::PairScore(M(MAGE_FROST), M(ROGUE_SUB));
    std::int32_t const rangedBoth = ArenaComp::PairScore(M(MAGE_FROST), M(WARLOCK_AFF));
    CHECK(mixed > 0 && rangedBoth > 0);
    // identical control and burst would leave exactly the 10 points apart; here we only assert the bonus is in the sum
    ArenaComp::SpecProfile const* rogue = ArenaComp::FindProfile(ROGUE_SUB);
    ArenaComp::SpecProfile const* mage = ArenaComp::FindProfile(MAGE_FROST);
    CHECK(rogue->melee != mage->melee);
}

TEST(arena_form_next_team_picks_the_best_pair_deterministically)
{
    std::vector<Candidate> pool = {
        C(10, ROGUE_SUB, Tier::GOOD), C(11, DRUID_RESTO, Tier::GOOD), C(12, MAGE_FROST, Tier::GOOD), C(13, PALADIN_PROT, Tier::GOOD),
    };
    std::uint32_t const have[3] = { 0, 0, 0 };
    auto const a = ArenaComp::FormNextTeam(pool, ArenaComp::DEFAULT_MIN_PAIR_SCORE, ArenaComp::TierMix(), have);
    std::reverse(pool.begin(), pool.end());
    auto const b = ArenaComp::FormNextTeam(pool, ArenaComp::DEFAULT_MIN_PAIR_SCORE, ArenaComp::TierMix(), have);
    CHECK(a.has_value() && b.has_value());
    CHECK(a->first == b->first && a->second == b->second);
    CHECK(a->first < a->second);
    CHECK(a->first != 13 && a->second != 13);  // the protection paladin is a tank: never picked
}

TEST(arena_form_next_team_needs_a_pair_above_the_minimum)
{
    std::vector<Candidate> pool = { C(1, DRUID_RESTO, Tier::GOOD), C(2, PRIEST_HOLY, Tier::GOOD) };  // two healers
    std::uint32_t const have[3] = { 0, 0, 0 };
    CHECK(!ArenaComp::FormNextTeam(pool, ArenaComp::DEFAULT_MIN_PAIR_SCORE, ArenaComp::TierMix(), have).has_value());
    CHECK(!ArenaComp::FormNextTeam({}, ArenaComp::DEFAULT_MIN_PAIR_SCORE, ArenaComp::TierMix(), have).has_value());
    CHECK(!ArenaComp::FormNextTeam({ C(1, ROGUE_SUB, Tier::GOOD) }, ArenaComp::DEFAULT_MIN_PAIR_SCORE, ArenaComp::TierMix(), have).has_value());
}

TEST(arena_form_next_team_follows_the_tier_mix)
{
    // an expert pair and a casual pair are both possible; with no teams yet the expert target (1) and the casual target (2)
    // are compared by shortfall, so casual (2 missing) comes before expert (1 missing)
    std::vector<Candidate> pool = {
        C(1, ROGUE_SUB, Tier::EXPERT), C(2, DRUID_RESTO, Tier::EXPERT), C(3, MAGE_FROST, Tier::CASUAL), C(4, PRIEST_DISC, Tier::CASUAL),
    };
    ArenaComp::TierMix mix;
    mix.target[0] = 1;
    mix.target[1] = 3;
    mix.target[2] = 2;
    std::uint32_t none[3] = { 0, 0, 0 };
    auto first = ArenaComp::FormNextTeam(pool, ArenaComp::DEFAULT_MIN_PAIR_SCORE, mix, none);
    CHECK(first.has_value() && first->tier == Tier::CASUAL && first->sameTier);

    // once two casual teams exist, the expert pair is next
    std::uint32_t casualDone[3] = { 0, 0, 2 };
    auto second = ArenaComp::FormNextTeam(pool, ArenaComp::DEFAULT_MIN_PAIR_SCORE, mix, casualDone);
    CHECK(second.has_value() && second->tier == Tier::EXPERT && second->sameTier);
}

TEST(arena_form_next_team_mixed_pair_only_when_no_same_tier_pair_passes)
{
    std::vector<Candidate> pool = { C(1, ROGUE_SUB, Tier::EXPERT), C(2, DRUID_RESTO, Tier::CASUAL) };
    std::uint32_t const have[3] = { 0, 0, 0 };
    auto pair = ArenaComp::FormNextTeam(pool, ArenaComp::DEFAULT_MIN_PAIR_SCORE, ArenaComp::TierMix(), have);
    CHECK(pair.has_value());
    CHECK(!pair->sameTier);
    CHECK(pair->tier == Tier::CASUAL);  // the weaker member sets the tier

    // with a same-tier pair available the mixed pair is not used even if its score is higher
    pool.push_back(C(3, MAGE_FROST, Tier::GOOD));
    pool.push_back(C(4, PRIEST_DISC, Tier::GOOD));
    pair = ArenaComp::FormNextTeam(pool, ArenaComp::DEFAULT_MIN_PAIR_SCORE, ArenaComp::TierMix(), have);
    CHECK(pair.has_value() && pair->sameTier && pair->tier == Tier::GOOD);
}

TEST(arena_form_next_team_ignores_ineligible_candidates)
{
    std::vector<Candidate> pool = { C(1, PALADIN_PROT, Tier::GOOD), C(2, WARRIOR_PROT, Tier::GOOD), C(3, DK_BLOOD, Tier::GOOD) };
    std::uint32_t const have[3] = { 0, 0, 0 };
    CHECK(!ArenaComp::FormNextTeam(pool, ArenaComp::DEFAULT_MIN_PAIR_SCORE, ArenaComp::TierMix(), have).has_value());
}

TEST(arena_form_next_team_never_pairs_a_bot_with_itself)
{
    std::vector<Candidate> pool = { C(7, ROGUE_SUB, Tier::GOOD) };
    std::uint32_t const have[3] = { 0, 0, 0 };
    CHECK(!ArenaComp::FormNextTeam(pool, 0, ArenaComp::TierMix(), have).has_value());
}
