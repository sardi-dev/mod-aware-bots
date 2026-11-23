/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef AWARE_BOTS_LIKING_BOOK_H
#define AWARE_BOTS_LIKING_BOOK_H

#include "LikingPolicy.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// Design decision 0033 point 14, slice 2: the memory side of the liking store, free of game and database types so it is tested on
// its own (tests/test_liking_book.cpp). LikingStore.cpp holds the one instance, the lock around it, the database calls and
// the game hooks. A human's rows are loaded when the human logs in (the answer comes later, from the database's async
// threads), so events that arrive first wait in a short list and replay once the rows are in. Changed rows are marked dirty
// and handed out by TakeDirty for a batched write; nothing here talks to a database.
namespace LikingBook
{
// One row of the table.
struct Row
{
    uint32_t bot = 0;
    uint32_t human = 0;
    LikingPolicy::Pair pair;
};

// Longest list of events that wait for a human's rows; the oldest are dropped past it.
constexpr size_t MAX_WAITING = 64;
// Pair cooldowns of quest offers kept per human; the ones that ran out are dropped past it.
constexpr size_t MAX_PAIR_OFFERS = 64;

class Book
{
public:
    // A human logged in (or was found online with no entry): rows are expected from Load. A known human stays as it is.
    void Begin(uint32_t human)
    {
        humans.try_emplace(human);
    }

    bool Has(uint32_t human) const
    {
        return humans.count(human) != 0;
    }

    bool Loaded(uint32_t human) const
    {
        auto const itr = humans.find(human);
        return itr != humans.end() && itr->second.loaded;
    }

    // The rows of the database arrived (offerAt is the stored time of the last guild offer, 0 for none). Events that
    // waited replay on them in order, and the rows they change turn dirty.
    void Load(LikingPolicy::Tuning const& tuning, uint32_t human, std::vector<Row> const& rows, uint32_t offerAt)
    {
        Human& book = humans[human];
        book.rows.clear();
        for (Row const& row : rows)
            book.rows[row.bot] = Entry{ row.pair, false };

        book.guildOfferAt = std::max(book.guildOfferAt, offerAt);

        book.loaded = true;
        std::vector<Waiting> waiting;
        waiting.swap(book.waiting);
        for (Waiting const& event : waiting)
            Change(tuning, book, event.bot, event.event, event.steps, event.at);
    }

    // One event for a pair. Returns false when the human is unknown to the book. Before the human's rows are in, the event
    // waits.
    bool Note(LikingPolicy::Tuning const& tuning, uint32_t human, uint32_t bot, LikingPolicy::Event event, uint32_t steps,
        uint32_t now)
    {
        auto const itr = humans.find(human);
        if (itr == humans.end())
            return false;

        Human& book = itr->second;
        if (!book.loaded)
        {
            if (book.waiting.size() >= MAX_WAITING)
                book.waiting.erase(book.waiting.begin());
            book.waiting.push_back({ bot, event, steps, now });
            return true;
        }

        Change(tuning, book, bot, event, steps, now);
        return true;
    }

    // The score at `now`. A pair with no row, an unknown human or rows still on their way sit at neutral.
    double Value(LikingPolicy::Tuning const& tuning, uint32_t human, uint32_t bot, uint32_t now) const
    {
        double const neutral = LikingPolicy::Clamp01(tuning.neutral);
        auto const itr = humans.find(human);
        if (itr == humans.end() || !itr->second.loaded)
            return neutral;

        auto const row = itr->second.rows.find(bot);
        return row == itr->second.rows.end() ? neutral : LikingPolicy::ValueAt(tuning, row->second.pair, now);
    }

    // The last guild offer to the human (0 for none) and the in-memory window of offers per hour.
    uint32_t GuildOfferAt(uint32_t human) const
    {
        auto const itr = humans.find(human);
        return itr == humans.end() ? 0 : itr->second.guildOfferAt;
    }

    // A guild offer was made now: the cooldown starts and the hour window takes one. False when the window is full.
    bool TakeGuildOffer(uint32_t human, uint32_t now, uint32_t capPerHour)
    {
        auto const itr = humans.find(human);
        if (itr == humans.end() || !itr->second.loaded)
            return false;

        Human& book = itr->second;
        if (!book.guildWindow.TryTake(now, capPerHour))
            return false;

        book.guildOfferAt = now;
        book.guildOfferDirty = true;
        return true;
    }

    // A shared-quest group offer from `bot` to the human at `now`. All gates in one place so the order is the same everywhere:
    // the human's rows are in, the bot likes the human enough, this bot did not offer them lately (pair cooldown) and the
    // human's hour window has room for another offer from any bot. True counts the offer: the window takes one and the
    // pair cooldown starts. Nothing changes when it is false.
    bool TakeQuestOffer(LikingPolicy::Tuning const& tuning, uint32_t human, uint32_t bot, uint32_t now,
        uint32_t pairCooldownSeconds)
    {
        auto const itr = humans.find(human);
        if (itr == humans.end() || !itr->second.loaded)
            return false;

        Human& book = itr->second;
        if (!LikingPolicy::QuestOfferAllowed(tuning, Value(tuning, human, bot, now)))
            return false;

        auto const last = book.questOfferAt.find(bot);
        if (last != book.questOfferAt.end() && !LikingPolicy::OfferCooldownPassed(last->second, now, pairCooldownSeconds))
            return false;

        if (!book.questWindow.TryTake(now, tuning.questOffersPerHour))
            return false;

        if (book.questOfferAt.size() >= MAX_PAIR_OFFERS)
        {
            for (auto old = book.questOfferAt.begin(); old != book.questOfferAt.end();)
                old = LikingPolicy::OfferCooldownPassed(old->second, now, pairCooldownSeconds) ? book.questOfferAt.erase(old)
                                                                                                : std::next(old);
        }

        book.questOfferAt[bot] = now;
        return true;
    }

    // The changed rows of one human (0: of everyone), at most `limit` of them, marked clean. The second list carries the
    // humans whose last guild offer time changed.
    std::vector<Row> TakeDirty(uint32_t human, size_t limit, std::vector<std::pair<uint32_t, uint32_t>>* offers = nullptr)
    {
        std::vector<Row> out;
        for (auto& [id, book] : humans)
        {
            if (human != 0 && id != human)
                continue;
            if (book.guildOfferDirty && offers)
            {
                offers->push_back({ id, book.guildOfferAt });
                book.guildOfferDirty = false;
            }
            for (auto& [bot, entry] : book.rows)
            {
                if (!entry.dirty)
                    continue;
                if (out.size() >= limit)
                    return out;
                entry.dirty = false;
                out.push_back({ bot, id, entry.pair });
            }
        }
        return out;
    }

    size_t DirtyCount() const
    {
        size_t count = 0;
        for (auto const& [id, book] : humans)
            for (auto const& [bot, entry] : book.rows)
                count += entry.dirty ? 1 : 0;
        return count;
    }

    // The human logs out: its changed rows (and offer time) come out for a last write and the memory is released.
    std::vector<Row> End(uint32_t human, std::vector<std::pair<uint32_t, uint32_t>>* offers = nullptr)
    {
        std::vector<Row> out = TakeDirty(human, size_t(-1), offers);
        humans.erase(human);
        return out;
    }

    // The wipe command: everything of the human is forgotten. A human that is online stays known and loaded (empty), so the
    // next events start from neutral.
    void Wipe(uint32_t human)
    {
        auto const itr = humans.find(human);
        if (itr == humans.end())
            return;

        bool const wasLoaded = itr->second.loaded;
        itr->second = Human();
        itr->second.loaded = wasLoaded;
    }

    void WipeAll()
    {
        std::vector<uint32_t> ids;
        for (auto const& [id, book] : humans)
            ids.push_back(id);
        for (uint32_t id : ids)
            Wipe(id);
    }

    // A bot's character was deleted: its rows go from every human's memory.
    void ForgetBot(uint32_t bot)
    {
        for (auto& [id, book] : humans)
        {
            book.rows.erase(bot);
            book.waiting.erase(std::remove_if(book.waiting.begin(), book.waiting.end(),
                                   [bot](Waiting const& event) { return event.bot == bot; }),
                book.waiting.end());
        }
    }

    size_t HumanCount() const
    {
        return humans.size();
    }

    size_t RowCount(uint32_t human) const
    {
        auto const itr = humans.find(human);
        return itr == humans.end() ? 0 : itr->second.rows.size();
    }

private:
    struct Entry
    {
        LikingPolicy::Pair pair;
        bool dirty = false;
    };

    struct Waiting
    {
        uint32_t bot = 0;
        LikingPolicy::Event event = LikingPolicy::Event::GroupTime;
        uint32_t steps = 0;
        uint32_t at = 0;
    };

    struct Human
    {
        bool loaded = false;
        std::unordered_map<uint32_t, Entry> rows;
        std::vector<Waiting> waiting;
        uint32_t guildOfferAt = 0;
        bool guildOfferDirty = false;
        LikingPolicy::OfferWindow guildWindow;
        // Shared-quest offers live in memory only: a restart forgets them, and a human who logs out loses them.
        LikingPolicy::OfferWindow questWindow;
        std::unordered_map<uint32_t, uint32_t> questOfferAt;
    };

    static void Change(LikingPolicy::Tuning const& tuning, Human& book, uint32_t bot, LikingPolicy::Event event,
        uint32_t steps, uint32_t at)
    {
        auto itr = book.rows.find(bot);
        if (itr == book.rows.end())
            itr = book.rows.emplace(bot, Entry{ LikingPolicy::Fresh(tuning, at), false }).first;

        itr->second.pair = LikingPolicy::Apply(tuning, itr->second.pair, event, steps, at);
        itr->second.dirty = true;
    }

    std::unordered_map<uint32_t, Human> humans;
};

// Seconds of time spent together per pair (a group, or one guild), turned into whole steps. The remainder stays for the
// next pass, and a pair not seen in a pass is dropped at EndPass so the list follows who is grouped now.
class PairClock
{
public:
    static uint64_t KeyOf(uint32_t bot, uint32_t human)
    {
        return (uint64_t(human) << 32) | bot;
    }

    // Adds `seconds` for the pair and returns the whole steps of `stepSeconds` that are now complete.
    uint32_t Add(uint64_t key, uint32_t seconds, uint32_t stepSeconds)
    {
        seen.insert(key);
        if (stepSeconds == 0)
            return 0;

        uint32_t& total = held[key];
        total += seconds;
        uint32_t const steps = total / stepSeconds;
        total -= steps * stepSeconds;
        return steps;
    }

    void EndPass()
    {
        for (auto itr = held.begin(); itr != held.end();)
            itr = seen.count(itr->first) ? std::next(itr) : held.erase(itr);
        seen.clear();
    }

    size_t Size() const
    {
        return held.size();
    }

    void Clear()
    {
        held.clear();
        seen.clear();
    }

private:
    std::unordered_map<uint64_t, uint32_t> held;
    std::unordered_set<uint64_t> seen;
};

// The same line of tone (thanks, rudeness) from the same pair counts once in a window, so repeating it does not farm the
// score or drive it down.
class RepeatGate
{
public:
    // True when the key may count now; the window then restarts.
    bool Allow(uint64_t key, uint32_t now, uint32_t windowSeconds)
    {
        if (stamps.size() >= MAX_KEPT)
            stamps.clear();

        auto const [itr, fresh] = stamps.try_emplace(key, now);
        if (fresh)
            return true;
        if (now >= itr->second && now - itr->second < windowSeconds)
            return false;

        itr->second = now;
        return true;
    }

    size_t Size() const
    {
        return stamps.size();
    }

private:
    static constexpr size_t MAX_KEPT = 4096;
    std::unordered_map<uint64_t, uint32_t> stamps;
};

// Seconds a group pass may credit at most: a stalled world tick does not hand out hours.
constexpr uint32_t MAX_PASS_CREDIT_SECONDS = 180;

inline uint32_t PassCredit(uint32_t lastPassAt, uint32_t now)
{
    if (lastPassAt == 0 || now <= lastPassAt)
        return 0;
    return std::min(now - lastPassAt, MAX_PASS_CREDIT_SECONDS);
}

// SQL text. Every value is a number the module computed, so the statements are built as plain strings (the arena store
// does the same, see ArenaStore.cpp).
constexpr size_t ROWS_PER_STATEMENT = 100;

// One REPLACE for rows [first, last) of the list.
inline std::string ReplaceSql(std::vector<Row> const& rows, size_t first, size_t last)
{
    std::string sql =
        "REPLACE INTO `playerbots_social_liking` (`bot_guid`, `player_guid`, `value`, `updated_at`, `gain_day`, `gain_today`) VALUES ";
    char buffer[160];
    for (size_t i = first; i < last && i < rows.size(); ++i)
    {
        Row const& row = rows[i];
        std::snprintf(buffer, sizeof(buffer), "%s(%u, %u, %.6f, %u, %u, %.6f)", i == first ? "" : ", ", unsigned(row.bot),
            unsigned(row.human), row.pair.value, unsigned(row.pair.updatedAt), unsigned(row.pair.gainDay), row.pair.gainToday);
        sql += buffer;
    }
    return sql;
}

// The kind column of the offer table: one kind so far.
constexpr unsigned OFFER_KIND_GUILD = 1;

inline std::string OfferReplaceSql(uint32_t human, uint32_t offerAt)
{
    char buffer[200];
    std::snprintf(buffer, sizeof(buffer),
        "REPLACE INTO `playerbots_social_offer` (`player_guid`, `kind`, `last_at`) VALUES (%u, %u, %u)", unsigned(human),
        OFFER_KIND_GUILD, unsigned(offerAt));
    return buffer;
}

// The cleanup: rows older than retentionDays whose decayed score sits within FORGET_DUST of neutral (the SQL form of
// LikingPolicy::Forgettable), and offer times older than retentionDays.
inline std::vector<std::string> RetentionSql(LikingPolicy::Tuning const& tuning, uint32_t now, uint32_t retentionDays)
{
    uint64_t const span = uint64_t(retentionDays) * LikingPolicy::SECONDS_PER_DAY;
    uint32_t const cutoff = now > span ? uint32_t(now - span) : 0;
    char buffer[400];
    std::vector<std::string> statements;

    if (tuning.decayHalfLifeDays > 0.0)
        std::snprintf(buffer, sizeof(buffer),
            "DELETE FROM `playerbots_social_liking` WHERE `updated_at` < %u AND ABS(`value` - %.6f) * POW(2, -((%u - CAST(`updated_at` AS SIGNED)) / %.1f)) <= %.4f",
            unsigned(cutoff), LikingPolicy::Clamp01(tuning.neutral), unsigned(now),
            tuning.decayHalfLifeDays * double(LikingPolicy::SECONDS_PER_DAY), LikingPolicy::FORGET_DUST);
    else
        std::snprintf(buffer, sizeof(buffer), "DELETE FROM `playerbots_social_liking` WHERE `updated_at` < %u", unsigned(cutoff));
    statements.push_back(buffer);

    std::snprintf(buffer, sizeof(buffer), "DELETE FROM `playerbots_social_offer` WHERE `last_at` < %u", unsigned(cutoff));
    statements.push_back(buffer);
    return statements;
}

// The removal of one character's rows, as the human and as the bot (a deleted character, the wipe command).
inline std::vector<std::string> ForgetPlayerSql(uint32_t guid, bool asBotToo)
{
    char buffer[200];
    std::vector<std::string> statements;
    std::snprintf(buffer, sizeof(buffer), "DELETE FROM `playerbots_social_liking` WHERE `player_guid` = %u", unsigned(guid));
    statements.push_back(buffer);
    std::snprintf(buffer, sizeof(buffer), "DELETE FROM `playerbots_social_offer` WHERE `player_guid` = %u", unsigned(guid));
    statements.push_back(buffer);
    if (asBotToo)
    {
        std::snprintf(buffer, sizeof(buffer), "DELETE FROM `playerbots_social_liking` WHERE `bot_guid` = %u", unsigned(guid));
        statements.push_back(buffer);
    }
    return statements;
}
}  // namespace LikingBook

#endif
