/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef AWARE_BOTS_AI_CHAT_POLICY_H
#define AWARE_BOTS_AI_CHAT_POLICY_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

// The pure decisions of the AI layer's player-to-player chat (design decision 0029, phase 1): which moments ask the sidecar,
// which whispers are commands and never get a chat reply, which line is said in the end (the AI's or the rule line), how
// long a bot "reads and types" before it says it, and the game-only context tokens of the request. No game core types,
// so tests/run.sh checks all of it. The game side is src/AwareAi/AiChat.cpp.
namespace AiChatPolicy
{
// The sidecar's chat kinds (services/aware-ai/schemas/v1/chat_request.json): phase 1's seven and phase 1b's opener (a
// bot speaks first at a small moment) and friend_whisper (a bot whispers a human it likes); phase 2's bg_win, bg_loss
// and bg_capture (a bot's line in battleground chat at the end of a match and when its team scores).
enum class Kind : uint8_t
{
    DUEL_CHALLENGE,
    DUEL_WIN,
    DUEL_LOSS,
    WHISPER_REPLY,
    NEARBY_REPLY,
    PARTY_MOMENT,
    GUILD_REPLY,
    OPENER,
    FRIEND_WHISPER,
    BG_WIN,
    BG_LOSS,
    BG_CAPTURE
};

inline char const* KindName(Kind kind)
{
    switch (kind)
    {
        case Kind::DUEL_CHALLENGE: return "duel_challenge";
        case Kind::DUEL_WIN: return "duel_win";
        case Kind::DUEL_LOSS: return "duel_loss";
        case Kind::WHISPER_REPLY: return "whisper_reply";
        case Kind::NEARBY_REPLY: return "nearby_reply";
        case Kind::PARTY_MOMENT: return "party_moment";
        case Kind::GUILD_REPLY: return "guild_reply";
        case Kind::OPENER: return "opener";
        case Kind::FRIEND_WHISPER: return "friend_whisper";
        case Kind::BG_WIN: return "bg_win";
        case Kind::BG_LOSS: return "bg_loss";
        case Kind::BG_CAPTURE: return "bg_capture";
    }

    return "whisper_reply";
}

enum class Channel : uint8_t
{
    WHISPER,
    SAY,
    PARTY,
    RAID,
    GUILD,
    BATTLEGROUND  // the team's battleground chat (instance chat in 5.4.8)
};

inline char const* ChannelName(Channel channel)
{
    switch (channel)
    {
        case Channel::WHISPER: return "whisper";
        case Channel::SAY: return "say";
        case Channel::PARTY: return "party";
        case Channel::RAID: return "raid";
        case Channel::GUILD: return "guild";
        case Channel::BATTLEGROUND: return "battleground";
    }

    return "whisper";
}

// What the bot finally said, for the events log: the sidecar's line (ai, fake), the moment's rule line (rules), the
// free template answer to a trivial line (template) or a short line when the sidecar had none (canned).
enum class Source : uint8_t
{
    AI,
    FAKE,
    RULES,
    TEMPLATE,
    CANNED
};

inline char const* SourceName(Source source)
{
    switch (source)
    {
        case Source::AI: return "ai";
        case Source::FAKE: return "fake";
        case Source::RULES: return "rules";
        case Source::TEMPLATE: return "template";
        case Source::CANNED: return "canned";
    }

    return "rules";
}

constexpr uint32_t PERCENT = 100;

// The sidecar's 0-100 traits for the bot voice's four personalities (BotVoice::Personality order: quiet, friendly,
// joker, grumpy). The sidecar turns values of 66 and more or 33 and less into the persona's voice, and both sides read
// the chat archetype from them (ArchetypeOf).
struct Traits
{
    uint32_t aggression = 50;
    uint32_t sociability = 50;
    uint32_t chattiness = 50;
    uint32_t honor = 50;
};

inline Traits PersonalityTraits(uint8_t personality)
{
    switch (personality)
    {
        case 0: return { 30, 40, 20, 60 };  // quiet: terse
        case 1: return { 25, 85, 60, 70 };  // friendly: open, fair
        case 2: return { 50, 75, 85, 50 };  // joker: talkative
        case 3: return { 75, 20, 45, 35 };  // grumpy: cold, competitive, rough
        default: return {};
    }
}

// The chat spec's four archetypes (design decision 0031). The sidecar applies the same rule to the traits a request carries, so
// a bot is the same kind of person in its template, canned and AI lines.
enum class Archetype : uint8_t
{
    FRIENDLY,
    QUIET,
    ROUGH,
    CHATTY
};

constexpr uint32_t QUIET_MAX_CHATTINESS = 25;
constexpr uint32_t ROUGH_MIN_AGGRESSION = 70;
constexpr uint32_t ROUGH_MAX_HONOR = 40;
constexpr uint32_t CHATTY_MIN_CHATTINESS = 80;

inline Archetype ArchetypeOf(Traits const& traits)
{
    if (traits.chattiness <= QUIET_MAX_CHATTINESS)
        return Archetype::QUIET;

    if (traits.aggression >= ROUGH_MIN_AGGRESSION && traits.honor <= ROUGH_MAX_HONOR)
        return Archetype::ROUGH;

    if (traits.chattiness >= CHATTY_MIN_CHATTINESS && traits.aggression < ROUGH_MIN_AGGRESSION)
        return Archetype::CHATTY;

    return Archetype::FRIENDLY;
}

inline char const* ArchetypeName(Archetype archetype)
{
    switch (archetype)
    {
        case Archetype::FRIENDLY: return "friendly";
        case Archetype::QUIET: return "quiet";
        case Archetype::ROUGH: return "rough";
        case Archetype::CHATTY: return "chatty";
    }

    return "friendly";
}

// SplitMix64: cheap, well mixed and stable across builds and restarts. The bot voice derives its personality from it.
inline uint64_t MixGuid(uint64_t value)
{
    value += 0x9E3779B97F4A7C15ULL;
    value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ULL;
    value = (value ^ (value >> 27)) * 0x94D049BB133111EBULL;
    return value ^ (value >> 31);
}

constexpr uint32_t PERSONALITY_ROLL_RANGE = 100;
// Upper bounds of the guid roll for quiet, friendly and joker; the rest is grumpy. Random bots follow the chat spec's
// mix (25 quiet, 50 friendly, 10 joker or chatty, 15 grumpy or rough). A human's alt bots keep the older 30, 35, 15, 20
// split, so none of them changes character.
constexpr uint32_t RANDOM_BOT_PERSONALITY_BELOW[3] = { 25, 75, 85 };
constexpr uint32_t ALT_BOT_PERSONALITY_BELOW[3] = { 30, 65, 80 };

// The BotVoice::Personality index (0 quiet, 1 friendly, 2 joker, 3 grumpy) of a character: nothing is stored.
inline uint8_t PersonalityIndex(uint64_t guid, bool randomBot)
{
    uint32_t const (&below)[3] = randomBot ? RANDOM_BOT_PERSONALITY_BELOW : ALT_BOT_PERSONALITY_BELOW;
    uint32_t const roll = uint32_t(MixGuid(guid) % PERSONALITY_ROLL_RANGE);
    for (uint8_t index = 0; index < 3; ++index)
        if (roll < below[index])
            return index;

    return 3;
}

struct Gate
{
    bool layerOn = false;    // AwareAi.Enable
    bool chatOn = false;     // AwareAi.Chat.Enable
    bool linkUp = false;     // the sidecar client runs and its breaker is closed
    uint32_t nearbyChancePct = 0;
    uint32_t partyChancePct = 0;
};

// A human talking near a bot or in its group is answered only sometimes; a line that names the bot, a whisper, a duel,
// a bot voice moment that already passed its own roll and a guild line are always asked (the sidecar's rate limits and
// the guild window still apply).
inline uint32_t ChancePct(Gate const& gate, Kind kind, bool certain)
{
    if (certain)
        return PERCENT;

    switch (kind)
    {
        case Kind::NEARBY_REPLY: return std::min(gate.nearbyChancePct, PERCENT);
        case Kind::PARTY_MOMENT: return std::min(gate.partyChancePct, PERCENT);
        default: return PERCENT;
    }
}

// roll is 1-100.
inline bool ShouldAsk(Gate const& gate, Kind kind, bool certain, uint32_t roll)
{
    if (!gate.layerOn || !gate.chatOn || !gate.linkUp)
        return false;

    return roll <= ChancePct(gate, kind, certain);
}

// A human's /say line near bots (design decision 0031 item 1). Unprompted, a bot answers at the configured base chance scaled
// by its personality; a line that names it (or comes while the human targets it) gets 75 to 90 percent, never certain,
// the rough bots at the low end; standing very close doubles the chance up to 90 percent.
struct NearbyCue
{
    uint32_t basePct = 25;  // AwareAi.Chat.NearbyChance
    Archetype archetype = Archetype::FRIENDLY;
    bool named = false;     // named in the line, or the human's target
    bool veryClose = false; // within VERY_CLOSE_YARDS of the human
};

constexpr float VERY_CLOSE_YARDS = 5.0f;
constexpr uint32_t CLOSE_FACTOR = 2;
constexpr uint32_t MAX_RAISED_NEARBY_PCT = 90;

inline uint32_t NamedChancePct(Archetype archetype)
{
    switch (archetype)
    {
        case Archetype::ROUGH: return 75;
        case Archetype::QUIET: return 80;
        case Archetype::FRIENDLY: return 85;
        case Archetype::CHATTY: return 90;
    }

    return 85;
}

// How the personality scales the unprompted base chance, in percent of it.
inline uint32_t BaseScalePct(Archetype archetype)
{
    switch (archetype)
    {
        case Archetype::QUIET: return 50;
        case Archetype::ROUGH: return 80;
        case Archetype::FRIENDLY: return 100;
        case Archetype::CHATTY: return 150;
    }

    return 100;
}

inline uint32_t NearbyChancePct(NearbyCue const& cue)
{
    uint32_t const scaled = std::min(cue.basePct, PERCENT) * BaseScalePct(cue.archetype) / PERCENT;
    uint32_t pct = cue.named ? NamedChancePct(cue.archetype) : std::min(PERCENT, scaled);
    if (cue.veryClose)
        pct = std::max(pct, std::min(pct * CLOSE_FACTOR, MAX_RAISED_NEARBY_PCT));

    return pct;
}

inline std::string Lower(std::string_view text)
{
    std::string out(text);
    std::transform(out.begin(), out.end(), out.begin(),
        [](unsigned char c) { return static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c); });
    return out;
}

inline std::string_view Trim(std::string_view text)
{
    size_t const begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string_view::npos)
        return {};

    size_t const end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

inline std::string_view StripTrailingPunctuation(std::string_view word)
{
    while (!word.empty() && std::string_view(".,!?;:").find(word.back()) != std::string_view::npos)
        word.remove_suffix(1);

    return word;
}

struct CommandSyntax
{
    std::string separator;  // AiPlayerbot.CommandSeparator: several commands in one line
    std::string prefix;     // AiPlayerbot.CommandPrefix: when set, only lines starting with it are commands
};

// Player shorthand that asks a bot to do something. The module's own triggers do not know most of these ("inv" is not a
// trigger), but an AI line such as "sure, coming" to them would promise an action no rule performs.
inline bool IsRequestWord(std::string_view word, bool firstWord)
{
    static constexpr std::string_view ANYWHERE[] = { "inv", "invite", "summon", "trade" };
    static constexpr std::string_view FIRST[] = { "follow", "come", "stay", "wait", "attack", "pull", "release", "res",
        "rez", "buff", "join", "leave", "stop", "port" };
    for (std::string_view request : ANYWHERE)
        if (word == request)
            return true;

    if (!firstWord)
        return false;

    for (std::string_view request : FIRST)
        if (word == request)
            return true;

    return false;
}

using NameLookup = std::function<bool(std::string const&)>;

// The whole text, then every shorter run of its leading words, as ExternalEventHelper::ParseChatCommand tries them.
inline bool LeadingWordsMatch(std::string const& text, NameLookup const& lookup)
{
    if (lookup(text))
        return true;

    for (size_t space = text.rfind(' '); space != std::string::npos && space > 0; space = text.rfind(' ', space - 1))
        if (lookup(text.substr(0, space)))
            return true;

    return false;
}

// True when the module would read the line as a command (or it asks for an action), so the bot never also answers it as
// chat. The trigger part mirrors ExternalEventHelper::ParseChatCommand: the whole line, then every shorter run of leading
// words, is looked up as a trigger name; it is also tried in lower case and with the first word's trailing punctuation
// dropped, so "Follow!" is a command here even where the module itself would miss it. "d <action>" and "do <action>"
// (PlayerbotAI::HandleCommand) count only when an action of that name exists, so "do you duel?" stays a question.
// isTrigger and isAction may be empty (no bot context: only the syntax and the request words count). An empty line
// counts as a command: there is nothing to answer.
inline bool IsCommandLike(std::string_view text, CommandSyntax const& syntax, NameLookup const& isTrigger,
    NameLookup const& isAction = {})
{
    std::string_view const line = Trim(text);
    if (line.empty())
        return true;

    // Item, spell and quest links and colour codes: trade and loot commands, never small talk.
    if (line.find('|') != std::string_view::npos)
        return true;

    if (!syntax.separator.empty() && line.find(syntax.separator) != std::string_view::npos)
        return true;

    if (!syntax.prefix.empty() && line.substr(0, syntax.prefix.size()) == syntax.prefix)
        return true;

    // "#w ", "#p " (PlayerbotAI::HandleCommand), "@tank ..." (the chat filter) and stray client or GM commands.
    if (std::string_view("#@./!").find(line.front()) != std::string_view::npos)
        return true;

    std::string const lower = Lower(line);
    if (lower == "reset" || lower.rfind("queue ", 0) == 0)
        return true;

    for (std::string_view doForm : { std::string_view("d "), std::string_view("do ") })
        if (isAction && lower.rfind(doForm, 0) == 0 &&
            LeadingWordsMatch(std::string(Trim(std::string_view(lower).substr(doForm.size()))), isAction))
            return true;

    if (isTrigger && (LeadingWordsMatch(std::string(line), isTrigger) || LeadingWordsMatch(lower, isTrigger)))
        return true;

    bool first = true;
    size_t start = 0;
    while (start < lower.size())
    {
        size_t const end = std::min(lower.find(' ', start), lower.size());
        std::string_view const word = StripTrailingPunctuation(std::string_view(lower).substr(start, end - start));
        if (!word.empty())
        {
            if ((first && isTrigger && isTrigger(std::string(word))) || IsRequestWord(word, first))
                return true;

            first = false;
        }

        start = end + 1;
    }

    return false;
}

// Case-insensitive (ASCII) match of a character name as a whole word: "hey Taran!" names Taran, "Taranis" does not.
inline bool MentionsName(std::string_view text, std::string_view name)
{
    if (name.empty())
        return false;

    std::string const haystack = Lower(text);
    std::string const needle = Lower(name);
    auto const isWordChar = [](unsigned char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c >= 0x80; };
    for (size_t at = haystack.find(needle); at != std::string::npos; at = haystack.find(needle, at + 1))
    {
        bool const before = at == 0 || !isWordChar(static_cast<unsigned char>(haystack[at - 1]));
        size_t const after = at + needle.size();
        bool const behind = after >= haystack.size() || !isWordChar(static_cast<unsigned char>(haystack[after]));
        if (before && behind)
            return true;
    }

    return false;
}

// Cuts at a UTF-8 character boundary so a multi-byte name or word is never split.
inline std::string TruncateUtf8(std::string_view text, size_t maxBytes)
{
    if (text.size() <= maxBytes)
        return std::string(text);

    size_t cut = maxBytes;
    while (cut > 0 && (static_cast<unsigned char>(text[cut]) & 0xC0) == 0x80)
        --cut;

    return std::string(text.substr(0, cut));
}

// A game chat line holds 255 bytes.
constexpr size_t MAX_LINE_BYTES = 255;
// A cut line ends at the last space when one is this close to the limit, else mid-word.
constexpr size_t WORD_CUT_WINDOW = 40;

// The AI's text made safe for the chat packet: '|' starts the client's escape codes (colours, links) and is dropped, line
// breaks and other control characters become spaces, runs of spaces collapse, and the result fits one chat line.
inline std::string SanitizeReply(std::string_view text)
{
    std::string clean;
    clean.reserve(text.size());
    bool space = false;
    for (char ch : text)
    {
        unsigned char const c = static_cast<unsigned char>(ch);
        if (c == '|')
            continue;

        if (c < 0x20 || c == 0x7F || c == ' ')
        {
            space = !clean.empty();
            continue;
        }

        if (space)
            clean.push_back(' ');

        space = false;
        clean.push_back(ch);
    }

    if (clean.size() <= MAX_LINE_BYTES)
        return clean;

    std::string cut = TruncateUtf8(clean, MAX_LINE_BYTES);
    size_t const lastSpace = cut.rfind(' ');
    if (lastSpace != std::string::npos && lastSpace + WORD_CUT_WINDOW >= cut.size())
        cut.resize(lastSpace);

    return cut;
}

struct Line
{
    std::string text;  // empty: the bot stays silent
    Source source = Source::RULES;
};

// The module's own source for a template answer: it never reaches the sidecar.
constexpr std::string_view TEMPLATE_SOURCE = "template";

// responseSource is the sidecar's "source" ("llm" or "fake" carry a reply; "rules", "rules_hint" and "error" do not,
// and the client uses "error" for a timeout, a transport failure or an unreadable answer), or TEMPLATE_SOURCE. Any
// reply that is empty once sanitised falls back too: first to the moment's rule line, then to the canned line (both
// empty: silence, as for a bot's own friendly whisper). A template answer stands as it is, silence included.
inline Line ChooseLine(std::string_view responseSource, std::string_view reply, std::string const& fallback,
    std::string const& canned = {})
{
    if (responseSource == TEMPLATE_SOURCE)
        return { SanitizeReply(reply), Source::TEMPLATE };

    if (responseSource == "llm" || responseSource == "fake")
    {
        std::string text = SanitizeReply(reply);
        if (!text.empty())
            return { std::move(text), responseSource == "fake" ? Source::FAKE : Source::AI };
    }

    if (fallback.empty() && !canned.empty())
        return { canned, Source::CANNED };

    return { fallback, Source::RULES };
}

// A player reads the line, thinks and types the answer: no reply comes at once, and none takes longer than 5 s to type
// (design decision 0031 item 11).
constexpr uint32_t REACTION_MS = 500;
constexpr uint32_t READ_MS_PER_CHAR = 20;
constexpr uint32_t TYPE_MS_PER_CHAR = 55;
constexpr uint32_t MIN_DELAY_MS = 1000;
constexpr uint32_t MAX_DELAY_MS = 5000;
constexpr uint32_t JITTER_PCT = 20;

// jitterRoll 0-100 spreads the delay over +-JITTER_PCT percent. humanChars is 0 for a moment without a human line.
inline uint32_t ReplyDelayMs(size_t humanChars, size_t replyChars, uint32_t jitterRoll)
{
    uint64_t const base = REACTION_MS + uint64_t(humanChars) * READ_MS_PER_CHAR + uint64_t(replyChars) * TYPE_MS_PER_CHAR;
    uint64_t const scalePct = PERCENT - JITTER_PCT + (2 * JITTER_PCT * std::min(jitterRoll, PERCENT)) / PERCENT;
    uint64_t const delay = base * scalePct / PERCENT;
    return uint32_t(std::clamp<uint64_t>(delay, MIN_DELAY_MS, MAX_DELAY_MS));
}

// Conversation mode (design decision 0031 item 12): a human whose last line to a bot is under 2 minutes old talks with it, and
// it may answer every line (the sidecar keeps the pace near 4 a minute). Lines from that human that arrive while the
// bot is still reading or typing merge into one request, joined by " / ".
constexpr uint32_t CONVERSATION_WINDOW_MS = 120000;
constexpr size_t MAX_MERGED_LINES = 4;
constexpr std::string_view MERGE_SEPARATOR = " / ";
// The request waits this long for more lines before it goes out (it reads them meanwhile), and a merged line holds it
// a little longer, never past MAX_GATHER_MS after the first line.
constexpr uint32_t MIN_GATHER_MS = 400;
constexpr uint32_t MAX_FIRST_GATHER_MS = 1500;
constexpr uint32_t GATHER_EXTEND_MS = 700;
constexpr uint32_t MAX_GATHER_MS = 2500;

// sinceLastLineMs: how long ago the human's previous line to the bot came; hadLine false when there was none.
inline bool InConversation(bool hadLine, uint32_t sinceLastLineMs)
{
    return hadLine && sinceLastLineMs < CONVERSATION_WINDOW_MS;
}

// The last MAX_MERGED_LINES lines, joined.
inline std::string JoinLines(std::vector<std::string> const& lines)
{
    std::string joined;
    size_t const first = lines.size() > MAX_MERGED_LINES ? lines.size() - MAX_MERGED_LINES : 0;
    for (size_t i = first; i < lines.size(); ++i)
    {
        if (!joined.empty())
            joined += MERGE_SEPARATOR;
        joined += lines[i];
    }

    return joined;
}

inline uint32_t FirstGatherMs(size_t humanChars)
{
    uint64_t const read = REACTION_MS + uint64_t(humanChars) * READ_MS_PER_CHAR;
    return uint32_t(std::clamp<uint64_t>(read, MIN_GATHER_MS, MAX_FIRST_GATHER_MS));
}

// The new send time (from the first line) when another line merges in at ageMs.
inline uint32_t ExtendedGatherMs(uint32_t currentMs, uint32_t ageMs)
{
    return std::min(MAX_GATHER_MS, std::max(currentMs, ageMs + GATHER_EXTEND_MS));
}

// When the answer to a human's line comes (design decision 0031 item 11): after the typing time, now and then 30 to 90 s late
// ("was fighting"), and while the bot is in combat only after the fight or not at all. A whisper is never left
// unanswered (design decision 0031): in combat it waits for the end of the fight and gets its one late line then.
enum class Timing : uint8_t
{
    NOW,
    LATE,
    AFTER_COMBAT,
    SKIP
};

struct TimingCue
{
    bool humanLine = false;    // an answer to a human's line; moments (duels, openers, voice) always come now
    bool whisper = false;
    bool lateAllowed = true;   // false for guild lines, which go stale in a minute
    bool inCombat = false;
    uint32_t latePct = 7;      // AwareAi.Chat.LateChance
    uint32_t combatWaitPct = 60;  // AwareAi.Chat.CombatWaitChance
    uint32_t combatSkipPct = 20;  // AwareAi.Chat.CombatSkipChance
};

constexpr uint32_t LATE_MIN_MS = 30000;
constexpr uint32_t LATE_MAX_MS = 90000;
// A bot still fighting after this long drops the line it held for after the fight.
constexpr uint32_t COMBAT_HOLD_MAX_MS = 120000;

inline char const* TimingName(Timing timing)
{
    switch (timing)
    {
        case Timing::NOW: return "now";
        case Timing::LATE: return "late";
        case Timing::AFTER_COMBAT: return "after_combat";
        case Timing::SKIP: return "skip";
    }

    return "now";
}

// roll is 1-100. In combat the skip chance comes first, then the wait; out of combat only the late chance counts.
inline Timing PlanTiming(TimingCue const& cue, uint32_t roll)
{
    if (!cue.humanLine)
        return Timing::NOW;

    if (cue.inCombat)
    {
        uint32_t const skip = std::min(cue.combatSkipPct, PERCENT);
        uint32_t const wait = std::min(cue.combatWaitPct, PERCENT - skip);
        if (roll <= skip)
            return cue.whisper ? Timing::AFTER_COMBAT : Timing::SKIP;

        if (roll <= skip + wait)
            return Timing::AFTER_COMBAT;

        return Timing::NOW;
    }

    if (cue.lateAllowed && roll <= std::min(cue.latePct, PERCENT))
        return Timing::LATE;

    return Timing::NOW;
}

// The moments a bot shares with a human that go to the sidecar's memory (design decision 0031 items 5 and 6).
enum class MemoryEvent : uint8_t
{
    GROUPED,
    DUNGEON_DONE,
    BOSS_KILLED_TOGETHER,
    WIPE_TOGETHER,
    DUEL_WON,      // the bot won
    DUEL_LOST,     // the bot lost
    LOOT_NINJA,    // the human took loot the bot needed
    LOOT_PASSED,   // the human passed on loot the bot then won
    INSULTED,
    THANKED
};

inline char const* MemoryEventName(MemoryEvent event)
{
    switch (event)
    {
        case MemoryEvent::GROUPED: return "grouped";
        case MemoryEvent::DUNGEON_DONE: return "dungeon_done";
        case MemoryEvent::BOSS_KILLED_TOGETHER: return "boss_killed_together";
        case MemoryEvent::WIPE_TOGETHER: return "wipe_together";
        case MemoryEvent::DUEL_WON: return "duel_won";
        case MemoryEvent::DUEL_LOST: return "duel_lost";
        case MemoryEvent::LOOT_NINJA: return "loot_ninja";
        case MemoryEvent::LOOT_PASSED: return "loot_passed";
        case MemoryEvent::INSULTED: return "insulted";
        case MemoryEvent::THANKED: return "thanked";
    }

    return "grouped";
}

// Item data as the core numbers it (ItemPrototype.h): armor is class 4, its subclasses cloth 1 to plate 4, a cloak is
// inventory type 16 (cloth for every class).
constexpr uint32_t ITEM_CLASS_ARMOR_ID = 4;
constexpr uint32_t ARMOR_CLOTH = 1;
constexpr uint32_t ARMOR_MAIL = 3;
constexpr uint32_t ARMOR_PLATE = 4;
constexpr uint32_t INVTYPE_CLOAK_ID = 16;
// Warriors, paladins and death knights wear plate, hunters and shamans mail from this level; before it one type less.
constexpr uint32_t HEAVY_ARMOR_LEVEL = 40;

// The armor type a class levels in (0 for an unknown class).
inline uint32_t MainArmorSubclass(uint8_t classId, uint32_t level)
{
    uint32_t armor = 0;
    switch (classId)
    {
        case 1: case 2: case 6: armor = ARMOR_PLATE; break;
        case 3: case 7: armor = ARMOR_MAIL; break;
        case 4: case 10: case 11: armor = ARMOR_MAIL - 1; break;
        case 5: case 8: case 9: armor = ARMOR_CLOTH; break;
        default: return 0;
    }

    return level < HEAVY_ARMOR_LEVEL && armor >= ARMOR_MAIL ? armor - 1 : armor;
}

// A Need roll won on armor of a lighter type than the human's own (a plate wearer needing cloth): the 5.4.8 client lets
// a class need what it can wear, so this is the plain "ninja" a bot that needed it holds against the human.
inline bool IsOffArmorNeed(uint8_t humanClass, uint32_t humanLevel, uint32_t itemClass, uint32_t itemSubClass,
    uint32_t inventoryType)
{
    if (itemClass != ITEM_CLASS_ARMOR_ID || itemSubClass < ARMOR_CLOTH || itemSubClass > ARMOR_PLATE ||
        inventoryType == INVTYPE_CLOAK_ID)
        return false;

    uint32_t const main = MainArmorSubclass(humanClass, humanLevel);
    return main && itemSubClass < main;
}

// Openers (design decision 0031 item 3): small moments where a bot speaks first, rare and rule-gated. A human gets at most one
// opener per cooldown, whatever its kind; a duel ask needs a rough or chatty bot and an idle human.
struct OpenerCue
{
    uint32_t chancePct = 0;
    uint32_t sinceLastOpenerMs = 0;
    bool hadOpener = false;
    uint32_t cooldownMs = 0;
};

// roll is 1-100.
inline bool MayOpen(OpenerCue const& cue, uint32_t roll)
{
    if (cue.hadOpener && cue.sinceLastOpenerMs < cue.cooldownMs)
        return false;

    return roll <= std::min(cue.chancePct, PERCENT);
}

inline bool AsksForDuels(Archetype archetype)
{
    return archetype == Archetype::ROUGH || archetype == Archetype::CHATTY;
}

// Friendly whispers (design decision 0031 item 14): a bot that grouped or duelled with a human before whispers them first, at
// most 1 to 2 times per play session (from their login), never while they fight or are in a dungeon, raid or
// battleground. A rare timer rolls it, so it never comes on a schedule; the sidecar checks the relation and may refuse.
struct FriendWhisperCue
{
    uint32_t saidThisSession = 0;
    uint32_t sessionCap = 0;      // rolled 1 to AwareAi.Chat.FriendWhisper.MaxPerSession at login
    bool humanInCombat = false;
    bool humanInInstance = false;
    uint32_t chancePct = 0;
};

constexpr uint32_t MAX_FRIEND_WHISPERS_PER_SESSION = 2;

// roll is 1-100.
inline bool MayFriendWhisper(FriendWhisperCue const& cue, uint32_t roll)
{
    if (cue.saidThisSession >= std::min(cue.sessionCap, MAX_FRIEND_WHISPERS_PER_SESSION) || cue.humanInCombat ||
        cue.humanInInstance)
        return false;

    return roll <= std::min(cue.chancePct, PERCENT);
}

// Guild: one or two bots answer a human's line, and only while it is fresh.
constexpr uint32_t MIN_GUILD_ANSWERERS = 1;
constexpr uint32_t MAX_GUILD_ANSWERERS = 2;

inline uint32_t GuildAnswerers(int32_t configured)
{
    return uint32_t(std::clamp<int32_t>(configured, MIN_GUILD_ANSWERERS, MAX_GUILD_ANSWERERS));
}

inline bool StillFresh(uint32_t ageMs, uint32_t windowMs)
{
    return ageMs <= windowMs;
}

// Battleground chat (design decision 0029, phase 2): at the end of a match one or two bots of each team say a line in their
// team's battleground chat (bg_win, bg_loss), and a bot that calls its team's capture may word it through the sidecar
// (bg_capture). Only a team with a human hears it (instance chat reaches one team), never in an arena.
// The core's BattlegroundWinner (Battleground.h); any other value is a draw or no result.
constexpr uint8_t WINNER_HORDE_ID = 0;
constexpr uint8_t WINNER_ALLIANCE_ID = 1;

enum class BgResult : uint8_t
{
    NONE,
    WIN,
    LOSS
};

inline BgResult ResultFor(uint8_t winner, bool alliance)
{
    if (winner != WINNER_HORDE_ID && winner != WINNER_ALLIANCE_ID)
        return BgResult::NONE;

    return (winner == WINNER_ALLIANCE_ID) == alliance ? BgResult::WIN : BgResult::LOSS;
}

struct BgChatCue
{
    bool chatOn = false;          // AwareAi.Enable and AwareAi.Chat.Enable
    bool battlegroundOn = false;  // AwareAi.Chat.Battleground.Enable
    bool arena = false;
    bool humanOnTeam = false;     // a human of the bot's team is in the match, on its map
};

// Battleground moments also use canned lines when the sidecar is down.
inline bool MayBgChat(BgChatCue const& cue, Kind kind)
{
    if (!cue.chatOn || !cue.battlegroundOn || cue.arena || !cue.humanOnTeam)
        return false;

    switch (kind)
    {
        case Kind::BG_WIN:
        case Kind::BG_LOSS:
        case Kind::BG_CAPTURE: return true;
        default: return false;
    }
}

constexpr uint32_t MIN_BG_SPEAKERS = 1;
constexpr uint32_t MAX_BG_SPEAKERS = 2;
// With two speakers allowed, the second one speaks in this percent of the matches.
constexpr uint32_t SECOND_BG_SPEAKER_PCT = 50;

// roll is 1-100.
inline uint32_t BgSpeakerCount(int32_t configured, uint32_t roll)
{
    uint32_t const most = uint32_t(std::clamp<int32_t>(configured, MIN_BG_SPEAKERS, MAX_BG_SPEAKERS));
    return most > MIN_BG_SPEAKERS && roll <= SECOND_BG_SPEAKER_PCT ? most : MIN_BG_SPEAKERS;
}

// Up to count distinct candidates, the heavier weights (the voice's chattiness) more often. rolls are any random
// numbers, one per pick (reused round when short); a candidate of weight 0 is never picked.
inline std::vector<size_t> PickSpeakers(std::vector<uint32_t> weights, size_t count, std::vector<uint32_t> const& rolls)
{
    std::vector<size_t> picked;
    for (size_t pick = 0; pick < count; ++pick)
    {
        uint64_t total = 0;
        for (uint32_t weight : weights)
            total += weight;

        if (!total)
            break;

        uint64_t roll = (rolls.empty() ? 0 : rolls[pick % rolls.size()]) % total;
        for (size_t i = 0; i < weights.size(); ++i)
        {
            if (roll < weights[i])
            {
                picked.push_back(i);
                weights[i] = 0;
                break;
            }

            roll -= weights[i];
        }
    }

    return picked;
}

// The first bot of a team that sees the match end files the team's lines; the others find the claim. A claim is kept
// long enough that the team's late bots never file again, and dropped later because the core reuses instance ids.
constexpr uint32_t MATCH_END_KEEP_MS = 600000;
constexpr size_t MATCH_END_TRIM_SIZE = 64;

class MatchEndBook
{
public:
    // True once per team and match. teamIndex is 0 or 1; nowMs a wrapping millisecond clock.
    bool Claim(uint32_t instanceId, uint32_t teamIndex, uint32_t nowMs)
    {
        if (_claims.size() >= MATCH_END_TRIM_SIZE)
            for (auto itr = _claims.begin(); itr != _claims.end();)
                itr = nowMs - itr->second >= MATCH_END_KEEP_MS ? _claims.erase(itr) : std::next(itr);

        auto const [itr, fresh] = _claims.try_emplace({ instanceId, teamIndex }, nowMs);
        if (!fresh && nowMs - itr->second < MATCH_END_KEEP_MS)
            return false;

        itr->second = nowMs;
        return true;
    }

    size_t Size() const { return _claims.size(); }

private:
    std::map<std::pair<uint32_t, uint32_t>, uint32_t> _claims;
};

// Each team's first observation establishes a baseline, so joining late does not replay old captures.
class BgCaptureBook
{
public:
    bool Observe(uint32_t instanceId, uint32_t team, uint32_t score, uint32_t nowMs)
    {
        if (_scores.size() >= MATCH_END_TRIM_SIZE)
            for (auto itr = _scores.begin(); itr != _scores.end();)
                itr = nowMs - itr->second.seen >= MATCH_END_KEEP_MS ? _scores.erase(itr) : std::next(itr);
        auto const [itr, fresh] = _scores.try_emplace({ instanceId, team }, Entry{ score, nowMs });
        bool const captured = !fresh && score > itr->second.score;
        itr->second = { score, nowMs };
        return captured;
    }
private:
    struct Entry { uint32_t score; uint32_t seen; };
    std::map<std::pair<uint32_t, uint32_t>, Entry> _scores;
};

// A speaker stays in the ended match until it said its line, never longer than this (the core closes the match after
// 120 s; the deadline, the typing time and a second speaker's wait stay under it).
constexpr uint32_t BG_END_HOLD_MS = 20000;

inline bool HoldsLeave(bool endLineOpen, uint32_t ageMs)
{
    return endLineOpen && ageMs < BG_END_HOLD_MS;
}

// Why a battleground line is dropped before it is said, or nullptr when it may go out: the bot must still be in the
// match the line is about and the human it was meant for still on its team there.
inline char const* BgDeliveryBlock(bool inBattleground, uint32_t botInstanceId, uint32_t lineInstanceId,
    bool humanOnTeam)
{
    if (!inBattleground || !lineInstanceId || botInstanceId != lineInstanceId)
        return "left_battleground";

    return humanOnTeam ? nullptr : "human_gone";
}

// The sidecar's class and race fields are lower-case identifiers.
inline char const* ClassToken(uint8_t classId)
{
    switch (classId)
    {
        case 1: return "warrior";
        case 2: return "paladin";
        case 3: return "hunter";
        case 4: return "rogue";
        case 5: return "priest";
        case 6: return "death_knight";
        case 7: return "shaman";
        case 8: return "mage";
        case 9: return "warlock";
        case 10: return "monk";
        case 11: return "druid";
        default: return "adventurer";
    }
}

inline char const* RaceToken(uint8_t raceId)
{
    switch (raceId)
    {
        case 1: return "human";
        case 2: return "orc";
        case 3: return "dwarf";
        case 4: return "night_elf";
        case 5: return "undead";
        case 6: return "tauren";
        case 7: return "gnome";
        case 8: return "troll";
        case 9: return "goblin";
        case 10: return "blood_elf";
        case 11: return "draenei";
        case 22: return "worgen";
        case 24:
        case 25:
        case 26: return "pandaren";
        default: return "unknown";
    }
}
}

#endif
