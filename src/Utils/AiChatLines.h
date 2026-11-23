/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef AWARE_BOTS_AI_CHAT_LINES_H
#define AWARE_BOTS_AI_CHAT_LINES_H

#include "AiChatPolicy.h"

#include <array>
#include <iterator>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// The free lines of the AI chat (design decision 0031): the template layer that answers trivial human lines ("hi", "ty", "gg",
// "lol") without asking the sidecar, the canned lines a bot says when the sidecar has no line for it, and the obvious
// thanks and insults the module reports to the sidecar's memory. Pools per archetype; quiet and rough bots type in
// lower case and now and then with a typo. No game core types, so tests/run.sh checks all of it.
namespace AiChatLines
{
using AiChatPolicy::Archetype;

enum class Trivial : uint8_t
{
    NONE,
    GREETING,
    THANKS,
    GG,
    LAUGH,
    NP,
    OK,
    BYE
};

// Lower case ASCII words: any other character but a letter or a digit splits words, apostrophes vanish ("you're" is
// "youre"), so "Hey!!", "hey :)" and "HEY" read the same.
inline std::vector<std::string> Words(std::string_view text)
{
    std::vector<std::string> words;
    std::string word;
    for (char ch : text)
    {
        unsigned char const c = static_cast<unsigned char>(ch);
        if (c == '\'')
            continue;

        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
            word.push_back(ch);
        else if (c >= 'A' && c <= 'Z')
            word.push_back(static_cast<char>(c - 'A' + 'a'));
        else if (!word.empty())
        {
            words.push_back(word);
            word.clear();
        }
    }

    if (!word.empty())
        words.push_back(word);

    return words;
}

// Runs of minRun or more of the same character shrink to keep characters: "heyyy" with (3, 1) is "hey".
inline std::string Squeeze(std::string_view text, size_t minRun, size_t keep)
{
    std::string out;
    size_t i = 0;
    while (i < text.size())
    {
        size_t j = i;
        while (j < text.size() && text[j] == text[i])
            ++j;

        size_t const run = j - i;
        out.append(run >= minRun ? keep : run, text[i]);
        i = j;
    }

    return out;
}

// "haha", "hahaha", "hehe", "lolol": a unit said again and again, with an optional last letter of the unit.
inline bool IsRepeated(std::string_view text, std::string_view unit)
{
    if (text.size() < unit.size() * 2)
        return false;

    for (size_t i = 0; i < text.size(); ++i)
        if (text[i] != unit[i % unit.size()])
            return false;

    return true;
}

struct TrivialPhrase
{
    std::string_view phrase;
    Trivial kind;
};

inline constexpr TrivialPhrase TRIVIAL_PHRASES[] = {
    { "hi", Trivial::GREETING }, { "hey", Trivial::GREETING }, { "hello", Trivial::GREETING },
    { "helo", Trivial::GREETING }, { "yo", Trivial::GREETING }, { "sup", Trivial::GREETING },
    { "wassup", Trivial::GREETING }, { "whassup", Trivial::GREETING }, { "whats up", Trivial::GREETING },
    { "hiya", Trivial::GREETING }, { "heya", Trivial::GREETING }, { "howdy", Trivial::GREETING },
    { "hai", Trivial::GREETING }, { "o", Trivial::GREETING }, { "greetings", Trivial::GREETING },
    { "morning", Trivial::GREETING }, { "good morning", Trivial::GREETING }, { "evening", Trivial::GREETING },
    { "good evening", Trivial::GREETING }, { "hey hey", Trivial::GREETING }, { "hi hi", Trivial::GREETING },
    { "ello", Trivial::GREETING },
    { "ty", Trivial::THANKS }, { "thx", Trivial::THANKS }, { "thanks", Trivial::THANKS },
    { "thank you", Trivial::THANKS }, { "thank u", Trivial::THANKS }, { "tyvm", Trivial::THANKS },
    { "tysm", Trivial::THANKS }, { "thnx", Trivial::THANKS }, { "thanx", Trivial::THANKS },
    { "thnks", Trivial::THANKS }, { "cheers", Trivial::THANKS }, { "tyty", Trivial::THANKS },
    { "many thanks", Trivial::THANKS }, { "ty ty", Trivial::THANKS },
    { "gg", Trivial::GG }, { "gj", Trivial::GG }, { "wp", Trivial::GG }, { "gg wp", Trivial::GG },
    { "ggwp", Trivial::GG }, { "gj wp", Trivial::GG }, { "good game", Trivial::GG }, { "good job", Trivial::GG },
    { "well played", Trivial::GG },
    { "lol", Trivial::LAUGH }, { "lmao", Trivial::LAUGH }, { "rofl", Trivial::LAUGH }, { "xd", Trivial::LAUGH },
    { "lul", Trivial::LAUGH }, { "heh", Trivial::LAUGH }, { "lmfao", Trivial::LAUGH },
    { "np", Trivial::NP }, { "no problem", Trivial::NP }, { "no prob", Trivial::NP }, { "yw", Trivial::NP },
    { "youre welcome", Trivial::NP }, { "ur welcome", Trivial::NP }, { "no worries", Trivial::NP },
    { "nw", Trivial::NP },
    { "k", Trivial::OK }, { "kk", Trivial::OK }, { "ok", Trivial::OK }, { "okay", Trivial::OK },
    { "okey", Trivial::OK }, { "sure", Trivial::OK }, { "alright", Trivial::OK }, { "aight", Trivial::OK },
    { "cool", Trivial::OK }, { "fine", Trivial::OK }, { "yep", Trivial::OK }, { "yes", Trivial::OK },
    { "yeah", Trivial::OK }, { "ya", Trivial::OK }, { "yup", Trivial::OK }, { "roger", Trivial::OK },
    { "bye", Trivial::BYE }, { "cya", Trivial::BYE }, { "see ya", Trivial::BYE }, { "see you", Trivial::BYE },
    { "gn", Trivial::BYE }, { "good night", Trivial::BYE }, { "night", Trivial::BYE }, { "later", Trivial::BYE },
    { "laters", Trivial::BYE }, { "bb", Trivial::BYE }, { "bbl", Trivial::BYE }, { "take care", Trivial::BYE },
    { "bye bye", Trivial::BYE }, { "cu", Trivial::BYE }
};

// Words that do not change what a short line means: "hi all", "thanks a lot", "ty bro", "hey there".
inline constexpr std::string_view FILLER_WORDS[] = { "man", "bro", "mate", "dude", "all", "guys", "everyone",
    "everybody", "there", "buddy", "m8", "again", "so", "much", "a", "lot", "very", "then", "yall", "friend" };

// A trivial line has at most this many words once the fillers and the bot's name are gone.
constexpr size_t MAX_TRIVIAL_WORDS = 3;

inline Trivial LookUpTrivial(std::string const& phrase)
{
    for (TrivialPhrase const& entry : TRIVIAL_PHRASES)
        if (entry.phrase == phrase)
            return entry.kind;

    if (IsRepeated(phrase, "ha") || IsRepeated(phrase, "he") || IsRepeated(phrase, "lo") ||
        (phrase.size() > 2 && IsRepeated(phrase.substr(0, phrase.size() - 1), "ha")) ||
        (phrase.size() > 2 && IsRepeated(phrase.substr(0, phrase.size() - 1), "lo")))
        return Trivial::LAUGH;

    return Trivial::NONE;
}

// The trivial kind of a human's line, case and punctuation insensitive; botName (the bot it goes to) is ignored as a
// word, so "hey Taran!" is a greeting. Repeated letters count once ("heyyy", "thxxx", "gggg").
inline Trivial ClassifyTrivial(std::string_view text, std::string_view botName = {})
{
    std::vector<std::string> const nameWords = Words(botName);
    std::string phrase;
    size_t kept = 0;
    for (std::string const& word : Words(text))
    {
        bool skip = false;
        for (std::string const& name : nameWords)
            skip = skip || word == name;
        for (std::string_view filler : FILLER_WORDS)
            skip = skip || word == filler;
        if (skip)
            continue;

        if (++kept > MAX_TRIVIAL_WORDS)
            return Trivial::NONE;

        if (!phrase.empty())
            phrase.push_back(' ');
        phrase += word;
    }

    if (phrase.empty())
        return Trivial::NONE;

    for (std::string const& candidate : { phrase, Squeeze(phrase, 3, 2), Squeeze(phrase, 3, 1), Squeeze(phrase, 2, 1) })
        if (Trivial const kind = LookUpTrivial(candidate); kind != Trivial::NONE)
            return kind;

    return Trivial::NONE;
}

// Obvious tone of a human's line, for the sidecar's memory: thanks and insults, nothing subtle. An insult needs no
// first-person word in the line, so "sorry, I'm a noob" is no insult.
enum class Tone : uint8_t
{
    NONE,
    THANKED,
    INSULTED
};

inline constexpr std::string_view INSULT_PHRASES[] = { "noob", "nub", "newb", "idiot", "moron", "loser", "stupid",
    "dumb", "dumbass", "trash", "garbage", "useless", "pathetic", "clown", "scrub", "jerk", "you suck", "u suck",
    "ur bad", "youre bad", "stfu", "shut up", "fuck you", "fuck off", "f u", "fu", "screw you", "piss off", "kys",
    "go away" };
inline constexpr std::string_view FIRST_PERSON_WORDS[] = { "i", "im", "me", "my", "myself", "ive", "id", "ill" };
inline constexpr std::string_view THANKS_WORDS[] = { "ty", "thx", "thanks", "tyvm", "tysm", "thnx", "thanx", "cheers" };

inline bool HasPhrase(std::vector<std::string> const& words, std::string_view phrase)
{
    std::vector<std::string> const parts = Words(phrase);
    if (parts.empty() || parts.size() > words.size())
        return false;

    for (size_t start = 0; start + parts.size() <= words.size(); ++start)
    {
        bool match = true;
        for (size_t i = 0; i < parts.size() && match; ++i)
            match = words[start + i] == parts[i];
        if (match)
            return true;
    }

    return false;
}

inline Tone ToneOf(std::string_view text)
{
    std::vector<std::string> const words = Words(text);
    bool firstPerson = false;
    for (std::string_view word : FIRST_PERSON_WORDS)
        firstPerson = firstPerson || HasPhrase(words, word);

    if (!firstPerson)
        for (std::string_view insult : INSULT_PHRASES)
            if (HasPhrase(words, insult))
                return Tone::INSULTED;

    for (std::string_view thanks : THANKS_WORDS)
        if (HasPhrase(words, thanks))
            return Tone::THANKED;

    return HasPhrase(words, "thank you") || HasPhrase(words, "thank u") ? Tone::THANKED : Tone::NONE;
}

using Pool = std::array<std::string_view, 4>;

// Pools hold up to four lines; an empty slot ends the pool.
inline std::string_view Pick(Pool const& pool, uint32_t pick)
{
    size_t size = 0;
    while (size < pool.size() && !pool[size].empty())
        ++size;

    return size ? pool[pick % size] : std::string_view();
}

inline Pool const& TemplatePool(Trivial kind, Archetype archetype)
{
    static Pool const empty{};
    static Pool const quiet[] = {
        { "hi", "hey", "o/", "yo" },
        { "np", "yw", "np np" },
        { "gg", "gg wp" },
        { "lol", "heh", ":)" },
        { ":)", "k", "ya" },
        { "k", "ok", "kk" },
        { "cya", "bye", "l8r" } };
    static Pool const rough[] = {
        { "sup", "yo", "what", "ya?" },
        { "sure", "whatever", "yeah yeah", "ya ya" },
        { "gg ez", "gg", "lol ok", "ez" },
        { "lol ok", "lmao", "lol" },
        { "whatever", "sure", "k" },
        { "k", "sure", "whatevr" },
        { "later", "cya", "bye i guess" } };
    static Pool const friendly[] = {
        { "Hey there!", "Hi! How's it going?", "Hello :)", "Hey, good to see you" },
        { "No problem!", "Anytime :)", "Glad to help!", "np, happy to help" },
        { "gg! That was fun", "gg wp!", "Good game!" },
        { "haha", "lol :D", "hehe" },
        { ":)", "Cool", "All good" },
        { "Sounds good", "Alright!", "Cool, got it" },
        { "See you around!", "Take care!", "Bye, have fun!" } };
    static Pool const chatty[] = {
        { "Heyyy! What's up?", "Hi hi! Good to see you", "Yo! How's your day going?", "Oh hey! What are you up to?" },
        { "Anytime! Seriously, just ask", "No worries at all :D", "Haha np, that's what friends are for" },
        { "gg!! That was a good one", "gg wp, rematch sometime?", "Haha gg, close one" },
        { "hahaha", "lmao right?", "lol I know right" },
        { "All good!", ":D", "Yep yep" },
        { "Awesome!", "Cool cool", "Perfect, let's go" },
        { "Cya! Have fun out there", "Bye bye! :)", "Later! Don't die too much lol" } };

    static_assert(std::size(quiet) == size_t(Trivial::BYE) && std::size(rough) == size_t(Trivial::BYE) &&
        std::size(friendly) == size_t(Trivial::BYE) && std::size(chatty) == size_t(Trivial::BYE));
    if (kind == Trivial::NONE)
        return empty;

    size_t const index = size_t(kind) - 1;
    switch (archetype)
    {
        case Archetype::QUIET: return quiet[index];
        case Archetype::ROUGH: return rough[index];
        case Archetype::CHATTY: return chatty[index];
        default: return friendly[index];
    }
}

// "ok" and "np" end a talk: outside a whisper a bot leaves half of them unanswered, as players do.
constexpr uint32_t SILENT_ENDER_PCT = 50;
// variationRoll 0-99: below SILENT_ENDER_PCT it may silence an ender; then for friendly and chatty bots a band drops
// the last ! or . and another starts the line in lower case.
constexpr uint32_t DROP_PUNCTUATION_FROM = 50;
constexpr uint32_t LOWER_CASE_FROM = 65;
constexpr uint32_t VARIATION_UNTIL = 80;

// The template answer to a trivial line, or empty for silence. whisper: a whisper is always answered.
inline std::string TemplateReply(Trivial kind, Archetype archetype, uint32_t pick, uint32_t variationRoll, bool whisper)
{
    if (kind == Trivial::NONE)
        return {};

    if (!whisper && (kind == Trivial::OK || kind == Trivial::NP) && variationRoll < SILENT_ENDER_PCT)
        return {};

    std::string line(Pick(TemplatePool(kind, archetype), pick));
    bool const polished = archetype == Archetype::FRIENDLY || archetype == Archetype::CHATTY;
    if (!polished || line.empty() || variationRoll >= VARIATION_UNTIL)
        return line;

    if (variationRoll >= LOWER_CASE_FROM)
    {
        if (line[0] >= 'A' && line[0] <= 'Z')
            line[0] = static_cast<char>(line[0] - 'A' + 'a');
    }
    else if (variationRoll >= DROP_PUNCTUATION_FROM && (line.back() == '!' || line.back() == '.'))
    {
        while (!line.empty() && (line.back() == '!' || line.back() == '.'))
            line.pop_back();
    }

    return line;
}

// The moments a canned line stands in for when the sidecar has no line (timeout, error, rate limit, budget, empty).
// NONE: say nothing (a bot's own friendly whisper is never canned).
enum class Canned : uint8_t
{
    NONE,
    DUEL_ACCEPT,
    DUEL_DECLINE,
    DUEL_WIN,
    DUEL_LOSS,
    WHISPER,
    NEARBY,
    PARTY,
    GUILD,
    GREET,
    GRATS,
    DUEL_ASK,
    BG_WIN,
    BG_LOSS,
    BG_CAPTURE
};

inline Pool const& CannedPool(Canned moment, Archetype archetype)
{
    static Pool const empty{};
    static Pool const quiet[] = {
        { "k", "ok" },
        { "not now", "nah" },
        { "gg" },
        { "gg", "gg wp" },
        { "busy rn", "?", "hm?" },
        { "lol", "heh" },
        { "ok", "lol" },
        { "lol", "nice" },
        { "hi", "hey" },
        { "gz", "grats" },
        { "duel?" },
        { "gg", "gg wp", "gg all" },
        { "gg", "gg, next" },
        { "nice", "nice cap" } };
    static Pool const rough[] = {
        { "bring it", "sure", "k" },
        { "nah", "not now" },
        { "gg ez", "gg" },
        { "gg, lucky", "gg" },
        { "busy", "nah", "wat" },
        { "lol", "k", "sure" },
        { "lol", "k" },
        { "lol", "k" },
        { "yo", "sup" },
        { "gz", "grats i guess" },
        { "duel?", "1v1 me", "duel me" },
        { "ez", "gg ez", "too easy" },
        { "gg, what a team", "gg i guess", "wow. gg" },
        { "ez cap", "about time" } };
    static Pool const friendly[] = {
        { "Sure, good luck!", "Sure!" },
        { "Not right now, sorry", "Maybe later!" },
        { "gg wp!", "gg!" },
        { "gg, you got me", "gg wp!" },
        { "sorry, busy rn", "brb, one sec" },
        { "haha", "true" },
        { "haha", "sounds good" },
        { "haha nice", "nice!" },
        { "Hey all!", "Hi!" },
        { "gz!", "Grats!" },
        { "Wanna duel?", "Duel?" },
        { "gg all, nice work!", "gg! Well played everyone" },
        { "gg all, close one", "gg, we'll get them next time" },
        { "Nice cap!", "Great job on the cap!" } };
    static Pool const chatty[] = {
        { "Oh you're on!", "Let's go!" },
        { "Ahh not now, maybe later?" },
        { "gg! good fight", "gg, that was fun!" },
        { "gg! rematch?", "gg wp, nice one" },
        { "sorry kinda busy, talk later?", "brb!" },
        { "lol true", "haha yeah" },
        { "haha yeah", "lol" },
        { "haha nice", "lol true" },
        { "Heyy, welcome!", "Hi hi!" },
        { "gz!! nice one", "Grats!!" },
        { "Hey, wanna duel?", "Duel? Just for fun" },
        { "GG!! What a team :D", "gg everyone, that was awesome!" },
        { "gg, that one hurt lol", "gg! rematch? lol" },
        { "Woo nice cap!!", "Yesss great cap!" } };

    static_assert(std::size(quiet) == size_t(Canned::BG_CAPTURE) && std::size(rough) == size_t(Canned::BG_CAPTURE) &&
        std::size(friendly) == size_t(Canned::BG_CAPTURE) && std::size(chatty) == size_t(Canned::BG_CAPTURE));
    if (moment == Canned::NONE)
        return empty;

    size_t const index = size_t(moment) - 1;
    switch (archetype)
    {
        case Archetype::QUIET: return quiet[index];
        case Archetype::ROUGH: return rough[index];
        case Archetype::CHATTY: return chatty[index];
        default: return friendly[index];
    }
}

inline std::string CannedLine(Canned moment, Archetype archetype, uint32_t pick)
{
    return std::string(Pick(CannedPool(moment, archetype), pick));
}
}

#endif
