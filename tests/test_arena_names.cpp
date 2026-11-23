/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "ArenaNames.h"
#include "TestHarness.h"

#include <set>

namespace
{
bool HasDash(std::string const& text)
{
    return text.find("\xE2\x80\x93") != std::string::npos || text.find("\xE2\x80\x94") != std::string::npos;
}
}  // namespace

TEST(arena_names_every_combination_passes_the_block_list)
{
    int checked = 0;
    for (std::uint8_t faction : { ArenaNames::FACTION_ALLIANCE, ArenaNames::FACTION_HORDE })
        ArenaNames::ForEachCombination(faction, [&](ArenaNames::Pattern, std::string const& name)
        {
            ++checked;
            CHECK(!ArenaNames::ContainsBlockedWord(name));
            CHECK(!HasDash(name));
        });
    CHECK(checked > 5000);
}

TEST(arena_names_every_usable_combination_fits_the_column)
{
    int usable = 0;
    int tooLong = 0;
    for (std::uint8_t faction : { ArenaNames::FACTION_ALLIANCE, ArenaNames::FACTION_HORDE })
        ArenaNames::ForEachCombination(faction, [&](ArenaNames::Pattern, std::string const& name)
        {
            if (name.size() > ArenaNames::NAME_MAX_LENGTH)
            {
                ++tooLong;
                CHECK(!ArenaNames::IsValidName(name));
                return;
            }
            ++usable;
            CHECK(ArenaNames::IsValidName(name));
        });
    CHECK(usable > 4000);
    // only the "The X of Y" pattern can run over the limit, and most of its combinations still fit
    CHECK(tooLong < usable / 10);
}

TEST(arena_names_patterns_other_than_the_place_one_always_fit)
{
    for (std::uint8_t faction : { ArenaNames::FACTION_ALLIANCE, ArenaNames::FACTION_HORDE })
        ArenaNames::ForEachCombination(faction, [&](ArenaNames::Pattern pattern, std::string const& name)
        {
            if (pattern != ArenaNames::Pattern::THE_PLURAL_OF_PLACE)
                CHECK(name.size() <= ArenaNames::NAME_MAX_LENGTH);
        });
}

TEST(arena_names_block_list_catches_whole_words_and_joined_pairs)
{
    CHECK(ArenaNames::ContainsBlockedWord("Mad Shit Ravens"));
    CHECK(ArenaNames::ContainsBlockedWord("Ass"));
    CHECK(ArenaNames::ContainsBlockedWord("Pi Ss"));  // two harmless words that join into a blocked one
    CHECK(!ArenaNames::ContainsBlockedWord("Gilded Lanterns"));
    CHECK(!ArenaNames::ContainsBlockedWord("Compass & Brass"));  // "brass" contains a short word but is not one
    CHECK(!ArenaNames::ContainsBlockedWord("Ashen Fangs"));
}

TEST(arena_names_validity_rules)
{
    CHECK(ArenaNames::IsValidName("Ember & Ash"));
    CHECK(ArenaNames::IsValidName("The Ravens of Saltmere"));
    CHECK(!ArenaNames::IsValidName("Ab"));
    CHECK(!ArenaNames::IsValidName(" Leading"));
    CHECK(!ArenaNames::IsValidName("Trailing "));
    CHECK(!ArenaNames::IsValidName("Two  Spaces"));
    CHECK(!ArenaNames::IsValidName("Digit 4"));
    CHECK(!ArenaNames::IsValidName("Semi;colon"));
    CHECK(!ArenaNames::IsValidName("The Skullcaps of Thistledown Hollow"));
}

TEST(arena_names_are_deterministic)
{
    for (std::uint32_t counter = 1; counter < 200; ++counter)
        for (std::uint32_t attempt = 0; attempt < 5; ++attempt)
            CHECK(ArenaNames::NameFor(0, counter, attempt, true) == ArenaNames::NameFor(0, counter, attempt, true));
}

TEST(arena_names_faction_flavour_and_bracket_patterns)
{
    // a horde team never gets an alliance word and the pair pattern is for 2v2 only
    for (std::uint32_t counter = 1; counter < 400; ++counter)
    {
        std::string const horde = ArenaNames::NameFor(ArenaNames::FACTION_HORDE, counter, 0, false);
        if (horde.empty())
            continue;
        CHECK(horde.find(" & ") == std::string::npos);
        for (std::string const& word : ArenaNames::Words(horde))
            for (char const* a : ArenaNames::ALLIANCE_ADJECTIVES)
                CHECK(word != ArenaNames::Lower(a));
    }
}

TEST(arena_names_unique_generator_avoids_taken_names)
{
    std::set<std::string> taken;
    for (std::uint32_t counter = 1; counter <= 60; ++counter)
    {
        std::string const name = ArenaNames::GenerateUnique(ArenaNames::FACTION_ALLIANCE, counter, true,
            [&](std::string const& candidate) { return taken.count(ArenaNames::UniqueKey(candidate)) != 0; });
        CHECK(!name.empty());
        CHECK(ArenaNames::IsValidName(name));
        CHECK(taken.insert(ArenaNames::UniqueKey(name)).second);
    }
}

TEST(arena_names_unique_generator_is_case_insensitive_through_the_key)
{
    std::set<std::string> taken = { ArenaNames::UniqueKey("GILDED LANTERNS") };
    CHECK(taken.count(ArenaNames::UniqueKey("Gilded Lanterns")) == 1);
}

TEST(arena_names_unique_generator_falls_back_to_a_roman_numeral)
{
    // everything is taken except a numbered name
    std::string base;
    for (std::uint32_t attempt = 0; attempt < ArenaNames::MAX_ATTEMPTS && base.empty(); ++attempt)
        base = ArenaNames::NameFor(ArenaNames::FACTION_HORDE, 5, attempt, true);
    CHECK(!base.empty());

    std::string const name = ArenaNames::GenerateUnique(ArenaNames::FACTION_HORDE, 5, true,
        [&](std::string const& candidate) { return candidate.find(" II") == std::string::npos || candidate.size() != base.size() + 3; });
    // the predicate accepts only "<base> II"
    CHECK(name == base + " II");
}

TEST(arena_names_unique_generator_gives_up_when_nothing_is_free)
{
    std::string const name = ArenaNames::GenerateUnique(ArenaNames::FACTION_ALLIANCE, 1, true, [](std::string const&) { return true; });
    CHECK(name.empty());
}

TEST(arena_names_enough_distinct_names_for_many_teams)
{
    std::set<std::string> seen;
    for (std::uint32_t counter = 1; counter <= 300; ++counter)
    {
        std::string const name = ArenaNames::NameFor(ArenaNames::FACTION_HORDE, counter, 0, true);
        if (!name.empty())
            seen.insert(name);
    }
    CHECK(seen.size() > 150);
}
