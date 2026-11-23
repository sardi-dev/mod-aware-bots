/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "BotSkillMath.h"
#include "TestHarness.h"

using BotSkillMath::Tier;

namespace
{
BotSkillMath::Shares const DEFAULT_SHARES = { 10, 60 };
}

TEST(skill_tier_is_fixed_per_bot)
{
    for (std::uint32_t guid = 1; guid < 500; ++guid)
        CHECK(BotSkillMath::TierOf(guid, DEFAULT_SHARES) == BotSkillMath::TierOf(guid, DEFAULT_SHARES));
}

TEST(skill_tiers_follow_the_shares)
{
    int counts[3] = { 0, 0, 0 };
    for (std::uint32_t guid = 1; guid <= 20000; ++guid)
        ++counts[static_cast<int>(BotSkillMath::TierOf(guid, DEFAULT_SHARES))];

    CHECK_NEAR(counts[0] / 200.0, 10.0, 1.5);  // percent of 20000
    CHECK_NEAR(counts[1] / 200.0, 60.0, 1.5);
    CHECK_NEAR(counts[2] / 200.0, 30.0, 1.5);
}

TEST(skill_tiers_mix_among_the_first_bots)
{
    // The bots' GUIDs are low and consecutive: the first 150 must not all land in one tier.
    int counts[3] = { 0, 0, 0 };
    for (std::uint32_t guid = 10; guid < 160; ++guid)
        ++counts[static_cast<int>(BotSkillMath::TierOf(guid, DEFAULT_SHARES))];

    CHECK(counts[0] >= 5 && counts[0] <= 30);
    CHECK(counts[1] >= 65 && counts[1] <= 115);
    CHECK(counts[2] >= 25 && counts[2] <= 70);
}

TEST(skill_shares_at_the_edges)
{
    for (std::uint32_t guid = 1; guid < 300; ++guid)
    {
        CHECK(BotSkillMath::TierOf(guid, { 100, 0 }) == Tier::EXPERT);
        CHECK(BotSkillMath::TierOf(guid, { 0, 100 }) == Tier::GOOD);
        CHECK(BotSkillMath::TierOf(guid, { 0, 0 }) == Tier::CASUAL);
    }
}

TEST(skill_lines_by_tier)
{
    // a casual bot plays the core lines only
    CHECK(BotSkillMath::Uses(Tier::CASUAL, Tier::CASUAL));
    CHECK(!BotSkillMath::Uses(Tier::CASUAL, Tier::GOOD));
    CHECK(!BotSkillMath::Uses(Tier::CASUAL, Tier::EXPERT));
    // a good bot adds the good lines
    CHECK(BotSkillMath::Uses(Tier::GOOD, Tier::CASUAL));
    CHECK(BotSkillMath::Uses(Tier::GOOD, Tier::GOOD));
    CHECK(!BotSkillMath::Uses(Tier::GOOD, Tier::EXPERT));
    // an expert plays every line
    CHECK(BotSkillMath::Uses(Tier::EXPERT, Tier::EXPERT));
}
