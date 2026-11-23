/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "ArenaText.h"
#include "TestHarness.h"

using BotSkillMath::Tier;

TEST(arena_text_ladder_line_shape)
{
    CHECK(ArenaText::LadderLine(1, "Gilded Lanterns", 1684, 31, 12, 0, 4, Tier::EXPERT) ==
        "1. Gilded Lanterns - 1684 (31 W / 12 L) 4 in a row [expert]");
    CHECK(ArenaText::LadderLine(7, "Ashen Fangs", 1290, 3, 9, 1, -3, Tier::CASUAL) ==
        "7. Ashen Fangs - 1290 (3 W / 9 L / 1 D) 3 losses in a row [casual]");
    CHECK(ArenaText::LadderLine(2, "Ember & Ash", 1500, 0, 0, 0, 0, Tier::GOOD) == "2. Ember & Ash - 1500 (0 W / 0 L) [good]");
}

TEST(arena_text_the_longest_ladder_line_fits_a_gossip_line)
{
    std::string const line = ArenaText::LadderLine(10, "The Mantises of Fennhollow", 1999, 9999, 9999, 99, -99, Tier::CASUAL);
    CHECK(line.size() > ArenaText::GOSSIP_LINE_MAX);  // the extreme case does not fit ...
    CHECK(ArenaText::Fit(line).size() <= ArenaText::GOSSIP_LINE_MAX);  // ... and is cut
    std::string const normal = ArenaText::LadderLine(10, "Smoldering Mantises", 1684, 131, 112, 3, 14, Tier::EXPERT);
    CHECK(normal.size() <= ArenaText::GOSSIP_LINE_MAX);
}

TEST(arena_text_fit_cuts_at_a_word)
{
    std::string const cut = ArenaText::Fit("alpha beta gamma delta epsilon zeta eta theta iota kappa lambda mu nu xi omicron pi rho", 40);
    CHECK(cut.size() <= 40);
    CHECK(cut.size() > 3 && cut.substr(cut.size() - 3) == "...");
    CHECK(ArenaText::Fit("short", 40) == "short");
}

TEST(arena_text_streak_text)
{
    CHECK(ArenaText::StreakText(1).empty());
    CHECK(ArenaText::StreakText(0).empty());
    CHECK(ArenaText::StreakText(-1).empty());
    CHECK(ArenaText::StreakText(2) == "2 in a row");
    CHECK(ArenaText::StreakText(-5) == "5 losses in a row");
}

TEST(arena_text_roster_lines)
{
    CHECK(ArenaText::RosterLine("Mira", 4, 261, false) == "Mira: Subtlety Rogue, damage");
    CHECK(ArenaText::RosterLine("Tor", 11, 105, true) == "Tor: Restoration Druid, healer");
    CHECK(ArenaText::SpecAndClass(8, 0) == "Mage");
}

TEST(arena_text_every_profile_has_a_spec_name_and_a_class_name)
{
    for (ArenaComp::SpecProfile const& profile : ArenaComp::PROFILES)
    {
        CHECK(std::string(ArenaText::SpecName(profile.specId)) != "");
        CHECK(std::string(ArenaText::ClassName(profile.classId)) != "Adventurer");
    }
}

TEST(arena_text_has_no_long_dashes)
{
    for (ArenaComp::SpecProfile const& profile : ArenaComp::PROFILES)
    {
        std::string const text = ArenaText::SpecAndClass(profile.classId, profile.specId);
        CHECK(text.find("\xE2\x80") == std::string::npos);
    }
}
