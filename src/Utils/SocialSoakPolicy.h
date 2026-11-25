/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef AWARE_BOTS_SOCIAL_SOAK_POLICY_H
#define AWARE_BOTS_SOCIAL_SOAK_POLICY_H

#include "EmoteReplyPolicy.h"
#include "LikingPolicy.h"
#include "PlaystylePolicy.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

// The soak support of the social layer (docs/social/social.md, section 13): a list of named bots that limits every social
// behaviour to them (AiPlayerbot.Social.OnlyBots, empty means no limit, like Auction.OnlyBots), one grep-friendly log line
// per social action, and a counter set whose summary line the morning report reads. Pure logic: the clock and the names
// are arguments, nothing here reads a game object. Humans appear as the low guid only, never as a name.
namespace SocialSoak
{
// A bot name list. Names compare case-insensitively on plain ASCII letters (the bot names of the random accounts are ASCII).
class Scope
{
public:
    // Splits on commas, semicolons and spaces; empty items and repeats are dropped.
    static Scope Parse(std::string const& list)
    {
        Scope scope;
        std::string word;
        auto flush = [&]()
        {
            if (!word.empty() && !std::binary_search(scope.names.begin(), scope.names.end(), word))
            {
                scope.names.push_back(word);
                std::sort(scope.names.begin(), scope.names.end());
            }
            word.clear();
        };

        for (char const c : list)
        {
            if (c == ',' || c == ';' || c == ' ' || c == '\t' || c == '\n' || c == '\r')
                flush();
            else
                word.push_back(Lower(c));
        }
        flush();
        return scope;
    }

    // True when only the listed bots may act socially.
    bool Limited() const
    {
        return !names.empty();
    }

    // An unlimited scope allows every bot; a limited one only the listed names.
    bool Allows(std::string const& name) const
    {
        if (names.empty())
            return true;

        std::string lowered;
        lowered.reserve(name.size());
        for (char const c : name)
            lowered.push_back(Lower(c));
        return std::binary_search(names.begin(), names.end(), lowered);
    }

    size_t Size() const
    {
        return names.size();
    }

    // For the start-up log: "all bots" or "3 named bots".
    std::string Describe() const
    {
        return names.empty() ? std::string("all bots") : std::to_string(names.size()) + " named bots";
    }

private:
    static char Lower(char c)
    {
        return (c >= 'A' && c <= 'Z') ? char(c - 'A' + 'a') : c;
    }

    // Sorted, lower case, no repeats.
    std::vector<std::string> names;
};

// What the soak counts. One counter per kind of outcome, so the summary tells the morning reader what happened and what was
// held back (a cap, a roll, a gate, the scope).
enum class Counter : uint8_t
{
    LikingUp,         // a score went up
    LikingDown,       // a score went down
    LikingCapped,     // a positive move that the daily cap cut to nothing
    LikingQueued,     // an event that waits for the human's rows to load
    EmoteReply,       // a gesture played
    EmoteNoGesture,   // the reaction table had no gesture for that emote
    EmoteChance,      // an untargeted emote that lost its roll
    EmoteLimited,     // the human already got a reply this minute
    EmoteCold,        // the bot does not like the human
    EmoteBusy,        // the bot was busy
    QuestOffer,       // a shared-quest group offer made
    QuestOfferRefused,// a candidate the liking gate, the pair cooldown or the hour cap refused
    GuildOffer,       // a guild offer made
    GuildOfferRefused,// a leader the liking gate, the cooldown or the hour cap refused
    OutOfScope,       // a bot left out because it is not on the OnlyBots list
    QuestPairCooldown, QuestRollingLimit, DbWrites,
    TradeChatterSent, TradeChatterBotCap, TradeChatterChannelCap, TradeChatterFactionCap,
    UpGroupTime, UpDungeonDone, UpTraded, UpDueled, UpGuildTime, UpThanked, UpInsulted, UpKicked, UpLootNinja,
    DownGroupTime, DownDungeonDone, DownTraded, DownDueled, DownGuildTime, DownThanked, DownInsulted, DownKicked, DownLootNinja,
    Count
};

constexpr size_t COUNTER_COUNT = size_t(Counter::Count);

inline char const* CounterName(Counter counter)
{
    switch (counter)
    {
        case Counter::LikingUp: return "liking_up";
        case Counter::LikingDown: return "liking_down";
        case Counter::LikingCapped: return "liking_capped";
        case Counter::LikingQueued: return "liking_queued";
        case Counter::EmoteReply: return "emote_reply";
        case Counter::EmoteNoGesture: return "emote_no_gesture";
        case Counter::EmoteChance: return "emote_chance";
        case Counter::EmoteLimited: return "emote_limited";
        case Counter::EmoteCold: return "emote_cold";
        case Counter::EmoteBusy: return "emote_busy";
        case Counter::QuestOffer: return "quest_offer";
        case Counter::QuestOfferRefused: return "quest_offer_refused";
        case Counter::GuildOffer: return "guild_offer";
        case Counter::GuildOfferRefused: return "guild_offer_refused";
        case Counter::OutOfScope: return "out_of_scope";
        case Counter::QuestPairCooldown: return "quest_pair_cooldown";
        case Counter::QuestRollingLimit: return "quest_rolling_limit";
        case Counter::DbWrites: return "db_writes";
        case Counter::TradeChatterSent: return "trade_chatter_sent";
        case Counter::TradeChatterBotCap: return "trade_chatter_bot_cap";
        case Counter::TradeChatterChannelCap: return "trade_chatter_channel_cap";
        case Counter::TradeChatterFactionCap: return "trade_chatter_faction_cap";
        case Counter::UpGroupTime: return "up_group_time";
        case Counter::UpDungeonDone: return "up_dungeon_done";
        case Counter::UpTraded: return "up_traded";
        case Counter::UpDueled: return "up_dueled";
        case Counter::UpGuildTime: return "up_guild_time";
        case Counter::UpThanked: return "up_thanked";
        case Counter::UpInsulted: return "up_insulted";
        case Counter::UpKicked: return "up_kicked";
        case Counter::UpLootNinja: return "up_loot_ninja";
        case Counter::DownGroupTime: return "down_group_time";
        case Counter::DownDungeonDone: return "down_dungeon_done";
        case Counter::DownTraded: return "down_traded";
        case Counter::DownDueled: return "down_dueled";
        case Counter::DownGuildTime: return "down_guild_time";
        case Counter::DownThanked: return "down_thanked";
        case Counter::DownInsulted: return "down_insulted";
        case Counter::DownKicked: return "down_kicked";
        case Counter::DownLootNinja: return "down_loot_ninja";
        case Counter::Count: break;
    }
    return "unknown";
}

// The counter an emote verdict feeds, if the verdict is worth counting. An emote for someone else or from too far is the
// normal case for most bots in a city and says nothing about the feature.
inline std::optional<Counter> EmoteCounter(EmoteReplyPolicy::Verdict verdict)
{
    switch (verdict)
    {
        case EmoteReplyPolicy::Verdict::Reply: return Counter::EmoteReply;
        case EmoteReplyPolicy::Verdict::Chance: return Counter::EmoteChance;
        case EmoteReplyPolicy::Verdict::Limited: return Counter::EmoteLimited;
        case EmoteReplyPolicy::Verdict::Cold: return Counter::EmoteCold;
        case EmoteReplyPolicy::Verdict::Busy: return Counter::EmoteBusy;
        case EmoteReplyPolicy::Verdict::OutOfScope: return Counter::OutOfScope;
        case EmoteReplyPolicy::Verdict::NotAimed:
        case EmoteReplyPolicy::Verdict::TooFar:
            break;
    }
    return std::nullopt;
}

class Tally
{
public:
    void Add(Counter counter, uint64_t amount = 1)
    {
        counts[size_t(counter)] += amount;
    }

    uint64_t Get(Counter counter) const
    {
        return counts[size_t(counter)];
    }

    uint64_t Total() const
    {
        uint64_t sum = 0;
        for (uint64_t const count : counts)
            sum += count;
        return sum;
    }

    bool operator==(Tally const& other) const
    {
        return counts == other.counts;
    }

    // "liking_up=3 liking_down=0 ...": every counter, in the enum's order, so two summaries line up and a grep for one
    // counter always finds it.
    std::string Fields() const
    {
        std::string out;
        for (size_t i = 0; i < COUNTER_COUNT; ++i)
        {
            if (i)
                out += ' ';
            out += CounterName(Counter(i));
            out += '=';
            out += std::to_string(counts[i]);
        }
        return out;
    }

private:
    std::array<uint64_t, COUNTER_COUNT> counts{};
};

// World-thread measurements of one candidate scan and ordering pass per eligible human.
struct PassStats
{
    uint64_t passes = 0, totalMs = 0, maxMs = 0, totalCandidates = 0, maxCandidates = 0;
    void Add(uint32_t ms, uint64_t candidates)
    {
        ++passes;
        totalMs += ms;
        maxMs = std::max(maxMs, uint64_t(ms));
        totalCandidates += candidates;
        maxCandidates = std::max(maxCandidates, candidates);
    }
    double AverageMs() const { return passes ? double(totalMs) / passes : 0.0; }
    double AverageCandidates() const { return passes ? double(totalCandidates) / passes : 0.0; }
};

using StyleCounts = std::array<uint64_t, PlaystylePolicy::STYLE_COUNT>;
inline void CountStyle(StyleCounts& counts, uint32_t guid, PlaystylePolicy::Shares const& shares)
{
    ++counts[size_t(PlaystylePolicy::StyleFor(guid, shares))];
}

// The next fixed wall-clock boundary, or zero when periodic summaries are disabled.
inline uint32_t NextSummaryBoundary(uint32_t now, uint32_t interval)
{
    return interval ? now + interval - now % interval : 0;
}

// Interval 0 turns the periodic line off (the line at shutdown still comes).
inline bool SummaryDue(uint32_t lastAt, uint32_t now, uint32_t intervalSeconds, bool changed)
{
    (void)changed;
    if (intervalSeconds == 0)
        return false;
    if (now < lastAt)
        return true;
    return now - lastAt >= intervalSeconds;
}

// One line of the summary. `windowSeconds` is the time since the previous summary (or the start), the counters are totals since
// the interval.
inline std::string SummaryLine(Scope const& scope, Tally const& tally, uint32_t windowSeconds,
    std::array<uint64_t, 4> const& tiers = {}, uint64_t totalQuestOffers = 0, uint64_t totalDbWrites = 0,
    PassStats const& passes = {}, StyleCounts const& styles = {})
{
    char metrics[256];
    std::snprintf(metrics, sizeof(metrics), " quest_passes=%llu quest_pass_avg_ms=%.2f quest_pass_max_ms=%llu quest_candidates_avg=%.2f quest_candidates_max=%llu",
        (unsigned long long)passes.passes, passes.AverageMs(), (unsigned long long)passes.maxMs, passes.AverageCandidates(),
        (unsigned long long)passes.maxCandidates);
    return "[SOCIAL] summary (interval): scope=" + scope.Describe() + " window=" + std::to_string(windowSeconds) + "s " +
        tally.Fields() + " pairs_cold_or_above=" + std::to_string(tiers[0]) +
        " pairs_stranger_or_above=" + std::to_string(tiers[1]) + " pairs_friendly_or_above=" + std::to_string(tiers[2]) +
        " pairs_close=" + std::to_string(tiers[3]) + " total_quest_offer=" + std::to_string(totalQuestOffers) +
        " total_db_writes=" + std::to_string(totalDbWrites) + metrics +
        " style_quester=" + std::to_string(styles[0]) + " style_gatherer_crafter=" + std::to_string(styles[1]) +
        " style_trader=" + std::to_string(styles[2]) + " style_socializer=" + std::to_string(styles[3]) +
        " style_pvper=" + std::to_string(styles[4]);
}

// Event enums are contiguous in LikingPolicy; keep the counter ranges in the same order.
inline Counter CauseCounter(LikingPolicy::Event event, bool up)
{
    return Counter(size_t(up ? Counter::UpGroupTime : Counter::DownGroupTime) + size_t(event));
}

// The score moves a liking line names.
enum class Move : uint8_t
{
    Up,
    Down,
    Capped,  // a positive step that left the score as it was (the daily cap)
    Queued,  // the human's rows are not loaded yet, the event waits
};

inline char const* MoveName(Move move)
{
    switch (move)
    {
        case Move::Up: return "up";
        case Move::Down: return "down";
        case Move::Capped: return "capped";
        case Move::Queued: return "queued";
    }
    return "up";
}

inline Counter MoveCounter(Move move)
{
    switch (move)
    {
        case Move::Up: return Counter::LikingUp;
        case Move::Down: return Counter::LikingDown;
        case Move::Capped: return Counter::LikingCapped;
        case Move::Queued: return Counter::LikingQueued;
    }
    return Counter::LikingUp;
}

// What an event did to a score: `rawDelta` is the change the event asked for (positive or negative), before and after are the
// scores (0 to 1) around it. A positive request that moved less than a tenth of a point was cut by the daily cap.
inline Move ClassifyMove(bool rowsLoaded, double rawDelta, double before, double after)
{
    constexpr double MIN_STEP = 0.001;
    if (!rowsLoaded)
        return Move::Queued;
    if (after > before + MIN_STEP)
        return Move::Up;
    if (after < before - MIN_STEP)
        return Move::Down;
    return rawDelta > 0.0 ? Move::Capped : Move::Down;
}

inline char const* EventName(LikingPolicy::Event event)
{
    switch (event)
    {
        case LikingPolicy::Event::GroupTime: return "group_time";
        case LikingPolicy::Event::DungeonDone: return "dungeon_done";
        case LikingPolicy::Event::Traded: return "traded";
        case LikingPolicy::Event::Dueled: return "dueled";
        case LikingPolicy::Event::GuildTime: return "guild_time";
        case LikingPolicy::Event::Thanked: return "thanked";
        case LikingPolicy::Event::Insulted: return "insulted";
        case LikingPolicy::Event::Kicked: return "kicked";
        case LikingPolicy::Event::LootNinja: return "loot_ninja";
    }
    return "unknown";
}

// The log lines, one per social action. All start with "[SOCIAL] <action>" and carry key=value pairs, so the morning grep is
// `grep '\[SOCIAL\] liking'`. Scores are whole percent (0 to 100). A bot appears by name, a human by low guid.
inline std::string LikingLine(std::string const& bot, uint32_t human, LikingPolicy::Event event, uint32_t steps, Move move,
    double before, double after)
{
    char buffer[256];
    std::snprintf(buffer, sizeof(buffer), "[SOCIAL] liking bot=%s human=%u event=%s steps=%u result=%s before=%u after=%u",
        bot.c_str(), unsigned(human), EventName(event), unsigned(steps), MoveName(move), unsigned(LikingPolicy::Percent(before)),
        unsigned(LikingPolicy::Percent(after)));
    return buffer;
}

inline std::string EmoteLine(std::string const& bot, uint32_t human, uint32_t textEmote, bool targeted, float yards)
{
    char buffer[200];
    std::snprintf(buffer, sizeof(buffer), "[SOCIAL] emote bot=%s human=%u text_emote=%u aim=%s yards=%.0f", bot.c_str(),
        unsigned(human), unsigned(textEmote), targeted ? "bot" : "nobody", double(yards));
    return buffer;
}

// kind is "quest" or "guild"; detail is the quest id (0 for a guild offer).
inline std::string OfferLine(char const* kind, std::string const& bot, uint32_t human, uint32_t detail, bool made)
{
    char buffer[200];
    std::snprintf(buffer, sizeof(buffer), "[SOCIAL] offer kind=%s bot=%s human=%u quest=%u result=%s", kind, bot.c_str(),
        unsigned(human), unsigned(detail), made ? "made" : "refused");
    return buffer;
}
}  // namespace SocialSoak

#endif
