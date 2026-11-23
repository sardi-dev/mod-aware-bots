/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_BGPLANCOMMON_H
#define _PLAYERBOT_BGPLANCOMMON_H

// BT-156: the battleground plan pieces that are not specific to one map, moved out of WsgPlanLogic.h so that Warsong
// Gulch, Arathi Basin and the maps that follow (Twin Peaks, Battle for Gilneas, Eye of the Storm) share one copy: the
// per-team plan draw, the tier lag, the call-out limiter and the call chance and delay, the reading of a human's call
// with its place or node words, the candidate / score / assign / tier-fit pattern, a largest-remainder split for group
// sizes and the per-match record table.
//
// Free of game types so that tests/ can run it (tests/test_bg_plan_common.cpp). Behavior is exactly what
// WsgPlanLogic.h had before the move; the Warsong tests stayed unchanged to prove it.

#include "BotSkillMath.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace BgPlan
{
using Tier = BotSkillMath::Tier;

// ---------------------------------------------------------------------------------------------------------------------
// Plan draw (owner, 2026-11-12): one draw per team per match, offense 20, balanced 60, defense 20 percent.
// ---------------------------------------------------------------------------------------------------------------------
constexpr std::uint32_t PLAN_OFFENSE_PERCENT = 20;
constexpr std::uint32_t PLAN_BALANCED_PERCENT = 60;

// Same numbers as WSBotStrategy and ABBotStrategy in BattleGroundTactics.h (balanced 0, offensive 1, defensive 2).
enum class Plan : std::uint8_t
{
    BALANCED = 0,
    OFFENSE = 1,
    DEFENSE = 2
};

// roll is 0 to 99.
inline Plan DrawPlan(std::uint32_t roll)
{
    if (roll < PLAN_OFFENSE_PERCENT)
        return Plan::OFFENSE;
    if (roll < PLAN_OFFENSE_PERCENT + PLAN_BALANCED_PERCENT)
        return Plan::BALANCED;
    return Plan::DEFENSE;
}

// A strategy number from the tactics class (0 to 2) as a plan; anything else plays balanced.
inline Plan PlanFromStrategy(std::uint8_t strategy)
{
    return strategy <= static_cast<std::uint8_t>(Plan::DEFENSE) ? static_cast<Plan>(strategy) : Plan::BALANCED;
}

// ---------------------------------------------------------------------------------------------------------------------
// Tier lag: a bot adopts a new plan state this long after it began (casual bots react late on purpose).
// ---------------------------------------------------------------------------------------------------------------------
constexpr std::uint32_t ADOPT_LAG_EXPERT_MS = 0;
constexpr std::uint32_t ADOPT_LAG_GOOD_MS = 1000;
constexpr std::uint32_t ADOPT_LAG_CASUAL_MS = 3000;

inline std::uint32_t AdoptLagMs(Tier tier)
{
    return tier == Tier::EXPERT ? ADOPT_LAG_EXPERT_MS : tier == Tier::GOOD ? ADOPT_LAG_GOOD_MS : ADOPT_LAG_CASUAL_MS;
}

// ---------------------------------------------------------------------------------------------------------------------
// Classes as the client numbers them (SharedDefines.h Classes).
// ---------------------------------------------------------------------------------------------------------------------
enum ClassId : std::uint8_t
{
    CLS_WARRIOR = 1,
    CLS_PALADIN = 2,
    CLS_HUNTER = 3,
    CLS_ROGUE = 4,
    CLS_PRIEST = 5,
    CLS_DEATH_KNIGHT = 6,
    CLS_SHAMAN = 7,
    CLS_MAGE = 8,
    CLS_WARLOCK = 9,
    CLS_MONK = 10,
    CLS_DRUID = 11
};

// ---------------------------------------------------------------------------------------------------------------------
// Call-outs: chance, delay and the limiter. Limits per sheet section 8 of the Warsong and Arathi sheets.
// ---------------------------------------------------------------------------------------------------------------------
constexpr std::uint32_t CALL_TEAM_GAP_MS = 6000;
constexpr std::uint32_t CALL_BOT_GAP_MS = 20000;
constexpr std::uint32_t CALL_WINDOW_MS = 60000;
constexpr std::uint32_t CALL_MAX_PER_WINDOW = 8;
constexpr std::uint32_t CALL_CHANCE_EXPERT = 100;
constexpr std::uint32_t CALL_CHANCE_GOOD = 70;
constexpr std::uint32_t CALL_CHANCE_CASUAL = 30;
// Multiplier of the per-event roll hash (Knuth), the same number BotSkillMath::Mix starts with.
constexpr std::uint32_t CALL_SEQ_MIX = 2654435761u;

inline std::uint32_t CallChancePercent(Tier tier)
{
    return tier == Tier::EXPERT ? CALL_CHANCE_EXPERT : tier == Tier::GOOD ? CALL_CHANCE_GOOD : CALL_CHANCE_CASUAL;
}

// Casual bots are late (and sometimes wrong, by their place name), expert bots at once. roll is 0 to 999.
inline std::uint32_t CallDelayMs(Tier tier, std::uint32_t roll)
{
    roll %= 1000;
    if (tier == Tier::EXPERT)
        return 0;
    if (tier == Tier::GOOD)
        return 150 + roll * 250 / 1000;
    return 400 + roll * 500 / 1000;
}

// Whether a bot says a line for an event, and when: a fixed roll per event (speaker and event number), so a test or a
// replay gets the same answer. Expert bots call everything, good 70 percent, casual 30 (and late).
struct CallDraw
{
    bool pass = false;
    std::uint32_t delayMs = 0;
};

inline CallDraw DrawCall(std::uint32_t speakerGuidLow, std::uint32_t seq, Tier tier)
{
    std::uint32_t const mixed = BotSkillMath::Mix(speakerGuidLow ^ (seq * CALL_SEQ_MIX));
    CallDraw draw;
    draw.pass = mixed % 100 < CallChancePercent(tier);
    draw.delayMs = CallDelayMs(tier, (mixed >> 8) % 1000);
    return draw;
}

// A call kind that must also keep its distance to an earlier call of another (or its own) kind.
struct KindGap
{
    bool active = false;
    std::uint8_t kind = 0;
    std::uint32_t ms = 0;
};

// One line per team every 6 s, at most 8 lines a minute per team, 20 s per bot, and a per-kind gap when the caller asks
// for one. Pure bookkeeping: Allow() says yes or no, Commit() records a line that was sent. Call kinds are the caller's
// enum values as numbers.
struct CallLimiter
{
    std::uint32_t lastTeamMs = 0;
    std::vector<std::uint32_t> window;                          // times of the lines of the last minute
    std::unordered_map<std::uint32_t, std::uint32_t> botLast;   // guid low to time of its last line
    std::unordered_map<std::uint8_t, std::uint32_t> kindLast;   // call kind to time of its last line

    bool Allow(std::uint32_t guidLow, std::uint8_t /*kind*/, std::uint32_t nowMs, KindGap const& gap = KindGap()) const
    {
        if (lastTeamMs && nowMs - lastTeamMs < CALL_TEAM_GAP_MS)
            return false;

        std::uint32_t inWindow = 0;
        for (std::uint32_t t : window)
            if (nowMs - t < CALL_WINDOW_MS)
                ++inWindow;
        if (inWindow >= CALL_MAX_PER_WINDOW)
            return false;

        auto const bot = botLast.find(guidLow);
        if (bot != botLast.end() && nowMs - bot->second < CALL_BOT_GAP_MS)
            return false;

        if (gap.active)
        {
            auto const k = kindLast.find(gap.kind);
            if (k != kindLast.end() && nowMs - k->second < gap.ms)
                return false;
        }
        return true;
    }

    void Commit(std::uint32_t guidLow, std::uint8_t kind, std::uint32_t nowMs)
    {
        std::uint32_t const stamp = nowMs ? nowMs : 1;
        lastTeamMs = stamp;
        botLast[guidLow] = stamp;
        kindLast[kind] = stamp;

        window.erase(std::remove_if(window.begin(), window.end(),
                                    [nowMs](std::uint32_t t) { return nowMs - t >= CALL_WINDOW_MS; }),
                     window.end());
        window.push_back(stamp);
    }
};

// ---------------------------------------------------------------------------------------------------------------------
// A human's call: the words of a short chat line, read against a verb table and a place or node table.
// ---------------------------------------------------------------------------------------------------------------------
constexpr std::size_t MAX_CALL_LENGTH = 40;
constexpr std::uint8_t WORD_NONE = 0xFF;

// A word and what it means; id is the caller's enum value as a number.
struct WordId
{
    char const* word;
    std::uint8_t id;
};

// The lower case words of a short line (letters and digits). A line longer than a short call is not read: empty list.
inline std::vector<std::string> CallWords(std::string const& text)
{
    std::vector<std::string> words;
    if (text.empty() || text.size() > MAX_CALL_LENGTH)
        return words;

    std::string word;
    for (char ch : text)
    {
        char const lower = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        if (std::isalnum(static_cast<unsigned char>(lower)))
            word.push_back(lower);
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

inline std::uint8_t LookupWord(std::string const& word, WordId const* table, std::size_t count)
{
    for (std::size_t i = 0; i < count; ++i)
        if (word == table[i].word)
            return table[i].id;
    return WORD_NONE;
}

struct ParsedCall
{
    std::uint8_t verb = WORD_NONE;   // the first word found in the verb table
    std::uint8_t place = WORD_NONE;  // the first word found in the place (or node) table
    std::uint32_t number = 0;        // the first number in the line ("inc bs 3"), 0 when there is none
};

inline ParsedCall ParseCall(std::string const& text, WordId const* verbs, std::size_t verbCount, WordId const* places,
                            std::size_t placeCount)
{
    ParsedCall call;
    for (std::string const& w : CallWords(text))
    {
        if (call.verb == WORD_NONE)
            call.verb = LookupWord(w, verbs, verbCount);
        if (call.place == WORD_NONE)
            call.place = LookupWord(w, places, placeCount);
        if (!call.number && w.size() <= 2 && std::all_of(w.begin(), w.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; }))
            call.number = static_cast<std::uint32_t>(std::stoul(w));
    }
    return call;
}

// ---------------------------------------------------------------------------------------------------------------------
// Candidate, score, assign and tier fit: the pattern every map's assignment uses. The map adds its own slot type and its
// own class fit; the tie-break and the tier profiles are shared.
// ---------------------------------------------------------------------------------------------------------------------

// Fields every map's candidate has.
struct CandidateBase
{
    std::uint32_t guidLow = 0;
    std::uint8_t cls = 0;
    bool healer = false;
    bool tank = false;
    bool melee = false;
    bool feral = false;    // cat or bear druid spec: can stealth in cat form
    std::uint8_t level = 0;
    Tier tier = Tier::GOOD;
};

// A stealth class takes the flag-taker or raider slots whatever else is true (owner).
inline bool IsStealthClass(CandidateBase const& c)
{
    return c.cls == CLS_ROGUE || (c.cls == CLS_DRUID && c.feral);
}

struct LevelSpan
{
    std::uint8_t minLevel = 0;
    std::uint8_t maxLevel = 0;
};

template <class C>
LevelSpan SpanOf(std::vector<C> const& candidates)
{
    LevelSpan span;
    if (candidates.empty())
        return span;

    span.minLevel = candidates.front().level;
    span.maxLevel = candidates.front().level;
    for (C const& c : candidates)
    {
        span.minLevel = std::min(span.minLevel, c.level);
        span.maxLevel = std::max(span.maxLevel, c.level);
    }
    return span;
}

// Tier profiles of a slot. A critical slot is one where a late reaction costs most: expert bots take them. A post is
// where a late reaction hurts least: casual bots take it.
inline int TierFitCritical(Tier tier)
{
    return tier == Tier::EXPERT ? 10 : tier == Tier::GOOD ? 5 : 0;
}

inline int TierFitPost(Tier tier)
{
    return tier == Tier::CASUAL ? 8 : tier == Tier::GOOD ? 4 : 0;
}

// The best candidate for one slot: the highest score, ties by the GUID hash (stable, like the skill tiers) and then the
// GUID. `taken` marks candidates that are placed already. A candidate whose score is at or below `notEligible` cannot
// take the slot. Returns candidates.size() when nobody can.
constexpr int NOT_ELIGIBLE = -1000000;

template <class C, class ScoreFn>
std::size_t PickBest(std::vector<C> const& candidates, std::vector<bool> const& taken, ScoreFn score)
{
    std::size_t best = candidates.size();
    int bestScore = 0;
    for (std::size_t i = 0; i < candidates.size(); ++i)
    {
        if (taken[i])
            continue;

        int const s = score(candidates[i]);
        if (s <= NOT_ELIGIBLE)
            continue;

        bool better = best == candidates.size() || s > bestScore;
        if (!better && s == bestScore)
        {
            std::uint32_t const hashNew = BotSkillMath::Mix(candidates[i].guidLow);
            std::uint32_t const hashOld = BotSkillMath::Mix(candidates[best].guidLow);
            better = hashNew < hashOld || (hashNew == hashOld && candidates[i].guidLow < candidates[best].guidLow);
        }
        if (better)
        {
            best = i;
            bestScore = s;
        }
    }
    return best;
}

// The n nearest of a list of (guid, distance); ties by guid. Returns their guids.
inline std::vector<std::uint32_t> NearestN(std::vector<std::pair<std::uint32_t, float>> list, std::size_t n)
{
    std::sort(list.begin(), list.end(), [](std::pair<std::uint32_t, float> const& a, std::pair<std::uint32_t, float> const& b)
    {
        if (a.second != b.second)
            return a.second < b.second;
        return a.first < b.first;
    });
    std::vector<std::uint32_t> result;
    for (std::size_t i = 0; i < list.size() && i < n; ++i)
        result.push_back(list[i].first);
    return result;
}

// ---------------------------------------------------------------------------------------------------------------------
// Group sizes: split `total` over groups by weight. Groups with a minimum get it first, in the order of the list, while
// the total lasts; the rest goes one bot at a time to the group farthest below its exact share (total * weight / sum of
// weights), ties to the earlier group. At a total equal to the sum of the weights the result is the weights. The result
// always adds up to `total` when at least one weight is positive.
// ---------------------------------------------------------------------------------------------------------------------
struct GroupWish
{
    std::uint32_t weight;   // 0 = the group is not opened
    std::uint32_t minimum;  // 0 when the group needs no head start
};

inline std::vector<std::uint32_t> Apportion(std::vector<GroupWish> const& wishes, std::uint32_t total)
{
    std::vector<std::uint32_t> result(wishes.size(), 0);
    std::uint64_t weightSum = 0;
    for (GroupWish const& w : wishes)
        weightSum += w.weight;
    if (!weightSum)
        return result;

    std::uint32_t used = 0;
    for (std::size_t i = 0; i < wishes.size(); ++i)
    {
        if (!wishes[i].weight || !wishes[i].minimum)
            continue;
        if (used + wishes[i].minimum > total)
            break;
        result[i] = wishes[i].minimum;
        used += wishes[i].minimum;
    }

    // Deficits are kept scaled by weightSum so that everything stays an integer.
    for (; used < total; ++used)
    {
        std::size_t best = wishes.size();
        std::int64_t bestDeficit = 0;
        for (std::size_t i = 0; i < wishes.size(); ++i)
        {
            if (!wishes[i].weight)
                continue;
            std::int64_t const deficit = static_cast<std::int64_t>(total) * wishes[i].weight -
                                         static_cast<std::int64_t>(result[i]) * static_cast<std::int64_t>(weightSum);
            if (best == wishes.size() || deficit > bestDeficit)
            {
                best = i;
                bestDeficit = deficit;
            }
        }
        ++result[best];
    }
    return result;
}

// ---------------------------------------------------------------------------------------------------------------------
// The per-match record idiom: one record per battleground instance, rebuilt from the battleground at most every
// REFRESH_MS by whichever bot asks first, behind one lock. A new plan state bumps the epoch; a bot adopts it after its
// tier lag. Records of matches that ended without the leave hook are dropped when a new one starts.
// ---------------------------------------------------------------------------------------------------------------------
constexpr std::uint32_t REFRESH_MS = 2000;
constexpr std::uint32_t RECORD_MAX_AGE_MS = 3u * 60u * 60u * 1000u;

struct RecordBase
{
    std::uint32_t instanceId = 0;
    std::uint32_t createdMs = 0;
    std::uint32_t lastTouchMs = 0;
    std::uint32_t lastRefreshMs = 0;
    std::uint32_t epoch = 1;
    std::uint32_t epochChangedMs = 0;
    bool started = false;
    std::uint32_t gatesOpenElapsedMs = 0;
    std::uint32_t playMs = 0;
    std::uint64_t signature = 0;
    bool baseline = false;
    std::uint32_t callSeq = 0;

    void BumpEpoch(std::uint32_t nowMs)
    {
        ++epoch;
        epochChangedMs = nowMs;
    }

    bool RefreshDue(std::uint32_t nowMs) const { return !lastRefreshMs || nowMs - lastRefreshMs >= REFRESH_MS; }
};

template <class R>
struct RecordTable
{
    std::mutex lock;
    std::unordered_map<std::uint32_t, R> records;

    // The record of an instance, created on first use. Called with the lock held. onDrop(record) is told about every old
    // record that is dropped (for the caller's own bookkeeping, for example pending call counts).
    template <class OnDrop>
    R& Of(std::uint32_t instanceId, std::uint32_t nowMs, OnDrop onDrop)
    {
        auto itr = records.find(instanceId);
        if (itr == records.end())
        {
            for (auto old = records.begin(); old != records.end();)
            {
                if (nowMs - old->second.lastTouchMs > RECORD_MAX_AGE_MS)
                {
                    onDrop(old->second);
                    old = records.erase(old);
                }
                else
                    ++old;
            }

            itr = records.emplace(instanceId, R()).first;
            itr->second.instanceId = instanceId;
            itr->second.createdMs = nowMs ? nowMs : 1;
            itr->second.epochChangedMs = nowMs;
        }

        itr->second.lastTouchMs = nowMs;
        return itr->second;
    }
};

}  // namespace BgPlan

#endif
