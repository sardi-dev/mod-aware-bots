/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_ARENANAMES_H
#define _PLAYERBOT_ARENANAMES_H

// BT-154: names of the bot arena teams. Original word lists of this module, no game or brand names, no persons:
// "Gilded Lanterns", "Ember & Ash", "The Ravens of Saltmere", "Wandering Foxes". A name is picked by a hash of
// (faction, team counter, attempt), so the same input gives the same name and a restart changes nothing. Free of game
// types so that tests/ can run it over every combination.

#include "BotSkillMath.h"

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <string>
#include <vector>

namespace ArenaNames
{
// The table column is VARCHAR(24) and a gossip line has to fit.
constexpr std::size_t NAME_MIN_LENGTH = 3;
constexpr std::size_t NAME_MAX_LENGTH = 24;
constexpr std::uint32_t MAX_ATTEMPTS = 25;

enum class Pattern : std::uint8_t
{
    ADJECTIVE_PLURAL,    // Gilded Lanterns
    NOUN_AND_NOUN,       // Ember & Ash (2v2 only)
    THE_PLURAL_OF_PLACE, // The Ravens of Saltmere
    PARTICIPLE_PLURAL    // Wandering Foxes
};
constexpr std::uint32_t PATTERN_COUNT = 4;

constexpr std::uint8_t FACTION_ALLIANCE = 0;
constexpr std::uint8_t FACTION_HORDE = 1;

// Words every team may use.
inline constexpr char const* SHARED_ADJECTIVES[] = {
    "Ashen", "Brazen", "Crimson", "Hollow", "Silent", "Verdant", "Hushed", "Restless", "Pale", "Stormlit",
    "Amber", "Azure", "Bitter", "Bright", "Copper", "Dusky", "Faded", "Frozen", "Gentle", "Hidden",
    "Jade", "Lone", "Mossy", "Narrow", "Quiet", "Rapid", "Silver", "Swift", "Tawny", "Wild",
};
inline constexpr char const* ALLIANCE_ADJECTIVES[] = {
    "Gilded", "Valiant", "Steadfast", "Dawnlit", "Oathbound", "Loyal", "Honest", "Radiant", "Sworn", "Stalwart",
    "Shining", "Fair",
};
inline constexpr char const* HORDE_ADJECTIVES[] = {
    "Rusted", "Bloodied", "Scorched", "Tusked", "Grim", "Savage", "Burnt", "Jagged", "Barbed", "Gnarled",
    "Smoldering", "Hungry", "Howling", "Charred",
};

inline constexpr char const* SHARED_PLURALS[] = {
    "Foxes", "Ravens", "Wolves", "Hawks", "Owls", "Otters", "Herons", "Badgers", "Stags", "Falcons",
    "Crows", "Moths", "Lynxes", "Adders", "Boars", "Bears", "Hares", "Larks", "Rooks", "Swans",
    "Vipers", "Jackals", "Pikes", "Gulls", "Wasps", "Hornets", "Beetles", "Mantises",
};
inline constexpr char const* ALLIANCE_PLURALS[] = {
    "Lanterns", "Banners", "Anvils", "Beacons", "Bulwarks", "Harbors", "Compasses", "Wardens", "Heralds", "Sentries",
    "Shields", "Spires", "Keystones", "Bells",
};
inline constexpr char const* HORDE_PLURALS[] = {
    "Fangs", "Tusks", "Embers", "Cinders", "Marauders", "Skulls", "Thorns", "Talons", "Claws", "Hammers",
    "Drums", "Pyres", "Bones", "Spears",
};

inline constexpr char const* SHARED_SINGULARS[] = {
    "Ash", "Thorn", "Thistle", "Dawn", "Dusk", "Salt", "Smoke", "Iron", "Oak", "Birch",
    "Moss", "Flint", "Reed", "Storm", "Frost", "Sun", "Moon", "Tide", "Gale", "Briar",
    "Hawk", "Fox", "Wolf", "Crow",
};
inline constexpr char const* ALLIANCE_SINGULARS[] = {
    "Lantern", "Anvil", "Anchor", "Harbor", "Compass", "Banner", "Beacon", "Oath",
};
inline constexpr char const* HORDE_SINGULARS[] = {
    "Ember", "Cinder", "Fang", "Tusk", "Bone", "Spear", "Pyre", "Drum",
};

inline constexpr char const* PARTICIPLES[] = {
    "Wandering", "Burning", "Roaming", "Prowling", "Soaring", "Creeping", "Drifting", "Waking", "Hunting", "Gliding",
    "Sleeping", "Stalking", "Rising", "Falling", "Singing", "Howling", "Gleaming", "Lurking", "Running", "Watching",
};

// Invented places: none is a place of the game.
inline constexpr char const* PLACES[] = {
    "Ashcombe", "Brackenfell", "Cragwick", "Dunmere", "Foxglen", "Greywater", "Hollowmere", "Ironwick", "Larkspire",
    "Marrowgate", "Mistwick", "Oakenreach", "Ravenmoor", "Saltmere", "Thornby", "Wyndmoor", "Duskhollow", "Embermere",
    "Fennhollow", "Harrowmere",
};

// Words that must never appear, as a whole word or as two adjacent words written together (so that two harmless
// words cannot form one of them).
inline constexpr char const* BLOCKED_WORDS[] = {
    "ass", "damn", "hell", "shit", "fuck", "cunt", "dick", "cock", "piss", "slut", "whore", "bitch", "bastard", "rape",
    "nazi", "sex", "porn", "suck", "fag", "nude", "naked", "kill", "murder",
};

inline std::size_t AdjectiveCount(std::uint8_t faction)
{
    return std::size(SHARED_ADJECTIVES) + (faction == FACTION_HORDE ? std::size(HORDE_ADJECTIVES) : std::size(ALLIANCE_ADJECTIVES));
}
inline char const* AdjectiveAt(std::uint8_t faction, std::size_t index)
{
    if (index < std::size(SHARED_ADJECTIVES))
        return SHARED_ADJECTIVES[index];
    index -= std::size(SHARED_ADJECTIVES);
    return faction == FACTION_HORDE ? HORDE_ADJECTIVES[index] : ALLIANCE_ADJECTIVES[index];
}

inline std::size_t PluralCount(std::uint8_t faction)
{
    return std::size(SHARED_PLURALS) + (faction == FACTION_HORDE ? std::size(HORDE_PLURALS) : std::size(ALLIANCE_PLURALS));
}
inline char const* PluralAt(std::uint8_t faction, std::size_t index)
{
    if (index < std::size(SHARED_PLURALS))
        return SHARED_PLURALS[index];
    index -= std::size(SHARED_PLURALS);
    return faction == FACTION_HORDE ? HORDE_PLURALS[index] : ALLIANCE_PLURALS[index];
}

inline std::size_t SingularCount(std::uint8_t faction)
{
    return std::size(SHARED_SINGULARS) + (faction == FACTION_HORDE ? std::size(HORDE_SINGULARS) : std::size(ALLIANCE_SINGULARS));
}
inline char const* SingularAt(std::uint8_t faction, std::size_t index)
{
    if (index < std::size(SHARED_SINGULARS))
        return SHARED_SINGULARS[index];
    index -= std::size(SHARED_SINGULARS);
    return faction == FACTION_HORDE ? HORDE_SINGULARS[index] : ALLIANCE_SINGULARS[index];
}

inline std::string Lower(std::string text)
{
    for (char& c : text)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

inline std::vector<std::string> Words(std::string const& name)
{
    std::vector<std::string> words;
    std::string current;
    for (char const c : name)
    {
        if (std::isalpha(static_cast<unsigned char>(c)))
            current += char(std::tolower(static_cast<unsigned char>(c)));
        else if (!current.empty())
        {
            words.push_back(current);
            current.clear();
        }
    }
    if (!current.empty())
        words.push_back(current);
    return words;
}

inline bool IsBlockedWord(std::string const& word)
{
    for (char const* blocked : BLOCKED_WORDS)
        if (word == blocked)
            return true;
    return false;
}

// True when a whole word, or two adjacent words joined, is on the block list.
inline bool ContainsBlockedWord(std::string const& name)
{
    std::vector<std::string> const words = Words(name);
    for (std::size_t i = 0; i < words.size(); ++i)
    {
        if (IsBlockedWord(words[i]))
            return true;
        if (i + 1 < words.size() && IsBlockedWord(words[i] + words[i + 1]))
            return true;
    }
    return false;
}

// Letters, spaces and "&" only, within the length limits, no leading, trailing or doubled space.
inline bool IsValidName(std::string const& name)
{
    if (name.size() < NAME_MIN_LENGTH || name.size() > NAME_MAX_LENGTH)
        return false;
    if (name.front() == ' ' || name.back() == ' ')
        return false;
    for (std::size_t i = 0; i < name.size(); ++i)
    {
        char const c = name[i];
        bool const letter = std::isalpha(static_cast<unsigned char>(c)) != 0;
        if (!letter && c != ' ' && c != '&')
            return false;
        if (c == ' ' && i + 1 < name.size() && name[i + 1] == ' ')
            return false;
    }
    return true;
}

inline std::string Build(Pattern pattern, std::uint8_t faction, std::size_t first, std::size_t second)
{
    switch (pattern)
    {
        case Pattern::ADJECTIVE_PLURAL:
            return std::string(AdjectiveAt(faction, first)) + " " + PluralAt(faction, second);
        case Pattern::NOUN_AND_NOUN:
            return std::string(SingularAt(faction, first)) + " & " + SingularAt(faction, second);
        case Pattern::THE_PLURAL_OF_PLACE:
            return std::string("The ") + PluralAt(faction, first) + " of " + PLACES[second % std::size(PLACES)];
        case Pattern::PARTICIPLE_PLURAL:
            return std::string(PARTICIPLES[first % std::size(PARTICIPLES)]) + " " + PluralAt(faction, second);
    }
    return std::string();
}

inline std::size_t FirstCount(Pattern pattern, std::uint8_t faction)
{
    switch (pattern)
    {
        case Pattern::ADJECTIVE_PLURAL:
            return AdjectiveCount(faction);
        case Pattern::NOUN_AND_NOUN:
            return SingularCount(faction);
        case Pattern::THE_PLURAL_OF_PLACE:
            return PluralCount(faction);
        case Pattern::PARTICIPLE_PLURAL:
            return std::size(PARTICIPLES);
    }
    return 0;
}

inline std::size_t SecondCount(Pattern pattern, std::uint8_t faction)
{
    switch (pattern)
    {
        case Pattern::ADJECTIVE_PLURAL:
            return PluralCount(faction);
        case Pattern::NOUN_AND_NOUN:
            return SingularCount(faction);
        case Pattern::THE_PLURAL_OF_PLACE:
            return std::size(PLACES);
        case Pattern::PARTICIPLE_PLURAL:
            return PluralCount(faction);
    }
    return 0;
}

// Calls `visit` for every combination of every pattern for this faction (the 2v2 pattern included). The test runs
// this over both factions.
inline void ForEachCombination(std::uint8_t faction, std::function<void(Pattern, std::string const&)> const& visit)
{
    for (std::uint32_t p = 0; p < PATTERN_COUNT; ++p)
    {
        Pattern const pattern = Pattern(p);
        for (std::size_t i = 0; i < FirstCount(pattern, faction); ++i)
            for (std::size_t j = 0; j < SecondCount(pattern, faction); ++j)
            {
                if (pattern == Pattern::NOUN_AND_NOUN && i == j)
                    continue;
                visit(pattern, Build(pattern, faction, i, j));
            }
    }
}

inline std::uint32_t HashOf(std::uint8_t faction, std::uint32_t counter, std::uint32_t attempt)
{
    return BotSkillMath::Mix(counter * 977u + std::uint32_t(faction) * 40503u + attempt * 7919u + 1u);
}

// The name for (faction, counter, attempt), or an empty string when that combination is not usable (too long, on the
// block list, or a pattern that does not fit the bracket). `pairBracket` is true for 2v2, where "A & B" is allowed.
inline std::string NameFor(std::uint8_t faction, std::uint32_t counter, std::uint32_t attempt, bool pairBracket)
{
    std::uint32_t const hash = HashOf(faction, counter, attempt);
    Pattern pattern = Pattern(hash % PATTERN_COUNT);
    if (pattern == Pattern::NOUN_AND_NOUN && !pairBracket)
        pattern = Pattern::ADJECTIVE_PLURAL;

    std::size_t const first = BotSkillMath::Mix(hash ^ 0x9e3779b9u) % FirstCount(pattern, faction);
    std::size_t second = BotSkillMath::Mix(hash * 31u + 7u) % SecondCount(pattern, faction);
    if (pattern == Pattern::NOUN_AND_NOUN && first == second)
        second = (second + 1) % SecondCount(pattern, faction);

    std::string const name = Build(pattern, faction, first, second);
    return IsValidName(name) && !ContainsBlockedWord(name) ? name : std::string();
}

inline std::string UniqueKey(std::string const& name)
{
    return Lower(name);
}

// A name nobody has (retired teams keep theirs, so `taken` must know them). Tries MAX_ATTEMPTS names, then a Roman
// numeral behind the first usable one. Empty when nothing fits.
inline std::string GenerateUnique(std::uint8_t faction, std::uint32_t counter, bool pairBracket,
    std::function<bool(std::string const&)> const& taken)
{
    static char const* const ROMAN[] = { "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X" };

    std::string firstUsable;
    for (std::uint32_t attempt = 0; attempt < MAX_ATTEMPTS; ++attempt)
    {
        std::string const name = NameFor(faction, counter, attempt, pairBracket);
        if (name.empty())
            continue;
        if (firstUsable.empty())
            firstUsable = name;
        if (!taken(name))
            return name;
    }

    for (char const* numeral : ROMAN)
    {
        std::string const name = firstUsable + " " + numeral;
        if (!firstUsable.empty() && IsValidName(name) && !taken(name))
            return name;
    }

    return std::string();
}
}  // namespace ArenaNames

#endif
