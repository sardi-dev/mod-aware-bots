/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_PVPOBSERVATIONMATH_H
#define _PLAYERBOT_PVPOBSERVATIONMATH_H

// BT-480: the pure part of the shared PvP match state (src/PvP/PvpMatchState.h, design decision 0032). What a side knows about the match
// is a list of OBSERVATIONS, each with its provenance (who said it) and its expiry, never the core's unrestricted state:
// bots share what they see only through visible battleground call-outs (owner 2026-11-18), so every observation here is
// one of those lines, from a human or from a bot. The store is bounded, deduplicates, rate limits a sender and sweeps what
// expired. The match table's safety-net sweep (which matches to drop) is here too.
//
// Free of game types so that tests/ can run it (tests/test_pvp_observation.cpp).

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace PvpObs
{
constexpr std::uint32_t SIDE_COUNT = 2;
// The place byte of an observation that names no place ("help" alone). The same number as BgPlan::WORD_NONE.
constexpr std::uint8_t PLACE_UNKNOWN = 0xFF;

// ---------------------------------------------------------------------------------------------------------------------
// Time. The module's clock is the 32 bit millisecond clock (getMSTime), which wraps after about 49.7 days: deadlines are
// compared as signed differences. 0 is "unset" for the stamps of the plans, so a real stamp is never 0.
// ---------------------------------------------------------------------------------------------------------------------
inline bool Reached(std::uint32_t nowMs, std::uint32_t deadlineMs)
{
    return static_cast<std::int32_t>(nowMs - deadlineMs) >= 0;
}

inline std::uint32_t Stamp(std::uint32_t ms)
{
    return ms ? ms : 1;
}

// ---------------------------------------------------------------------------------------------------------------------
// Match identity: the battleground's instance id plus a generation assigned locally when a state is created. A message or
// an answer that carries another generation belongs to an earlier match with the same instance id and is dropped.
// ---------------------------------------------------------------------------------------------------------------------
struct MatchKey
{
    std::uint32_t instanceId = 0;
    std::uint32_t generation = 0;

    bool operator==(MatchKey const& other) const
    {
        return instanceId == other.instanceId && generation == other.generation;
    }
};

// The instance clock only runs forward: a reading well below the last one means the instance was reused for a new match.
constexpr std::uint32_t RESTART_SLACK_MS = 5000;

inline bool IsRestart(std::uint32_t lastElapsedMs, std::uint32_t elapsedMs)
{
    return elapsedMs + RESTART_SLACK_MS < lastElapsedMs;
}

// ---------------------------------------------------------------------------------------------------------------------
// Observations.
// ---------------------------------------------------------------------------------------------------------------------
enum class Kind : std::uint8_t
{
    INCOMING,        // "inc bs 3": enemies come to the place (count when said)
    HELP,            // "help fr": somebody needs help at the place
    ENEMY_CARRIER,   // "efc mid": the enemy carries our flag, there
    OWN_CARRIER,     // "fc", "flag": our carrier of their flag (the sender when no place is said)
    CLAIM,           // "take lm": the sender says it takes the node (information about a friend, never an order)
    LOST,            // "st lost": the node is lost
    CLEAR            // "gm clear": the place has no enemies any more
};

enum class Provenance : std::uint8_t
{
    HUMAN_CALL,      // a human's line in the battleground chat
    BOT_CALL         // a bot's line, said from what its plan read
};

struct Observation
{
    Kind kind = Kind::INCOMING;
    Provenance provenance = Provenance::HUMAN_CALL;
    std::uint8_t place = PLACE_UNKNOWN;   // the map's own place or node id, PLACE_UNKNOWN when none was said
    std::uint8_t count = 0;               // 0 when no number was said
    std::uint32_t senderLow = 0;
    std::uint32_t receivedMs = 0;
    std::uint32_t expiresMs = 0;
};

// How an observation is weighed: a line of a human may be wrong or old, a bot says what its plan read from the match.
constexpr std::uint32_t HUMAN_WEIGHT_PERCENT = 70;
constexpr std::uint32_t BOT_WEIGHT_PERCENT = 90;
// A call without a number ("help fr") counts as this many enemies.
constexpr std::uint32_t UNNUMBERED_ENEMIES = 2;
// A team has at most this many players (rated battlegrounds of 15), so a larger number is not a count.
constexpr std::uint32_t MAX_REPORTED_COUNT = 15;

struct Limits
{
    std::uint32_t ttlMs = 15000;          // AiPlayerbot.Pvp.CalloutReader.TtlSeconds
    std::size_t maxPerSide = 32;          // .MaxPerSide
    std::uint32_t senderGapMs = 2000;     // .SenderGapMs
};

enum class AddResult : std::uint8_t
{
    ADDED,
    DUPLICATE,       // the same sender said the same thing and the first line is still fresh
    RATE_LIMITED     // the sender spoke less than senderGapMs ago
};

inline bool IsFresh(Observation const& o, std::uint32_t nowMs)
{
    return !Reached(nowMs, o.expiresMs);
}

// 100 when received, 0 at the expiry, times the weight of the provenance.
inline std::uint32_t ConfidencePercent(Observation const& o, std::uint32_t nowMs)
{
    if (!IsFresh(o, nowMs))
        return 0;

    std::uint32_t const span = o.expiresMs - o.receivedMs;
    if (!span)
        return 0;

    std::uint32_t const left = o.expiresMs - nowMs;
    std::uint32_t const weight = o.provenance == Provenance::HUMAN_CALL ? HUMAN_WEIGHT_PERCENT : BOT_WEIGHT_PERCENT;
    return static_cast<std::uint32_t>(static_cast<std::uint64_t>(left) * weight / span);
}

// One side's observations: bounded, deduplicated, rate limited per sender, newest last.
struct SideStore
{
    std::vector<Observation> items;

    AddResult Add(Observation o, std::uint32_t nowMs, Limits const& limits)
    {
        Sweep(nowMs);

        for (Observation const& old : items)
        {
            if (old.senderLow != o.senderLow || old.provenance != o.provenance)
                continue;
            if (nowMs - old.receivedMs < limits.senderGapMs)
                return AddResult::RATE_LIMITED;
            if (old.kind == o.kind && old.place == o.place)
                return AddResult::DUPLICATE;
        }

        o.receivedMs = Stamp(nowMs);
        o.expiresMs = o.receivedMs + limits.ttlMs;
        if (limits.maxPerSide && items.size() >= limits.maxPerSide)
            items.erase(items.begin());   // the oldest goes: the list is in the order of arrival
        items.push_back(o);
        return AddResult::ADDED;
    }

    void Sweep(std::uint32_t nowMs)
    {
        items.erase(std::remove_if(items.begin(), items.end(),
                                   [nowMs](Observation const& o) { return !IsFresh(o, nowMs); }),
                    items.end());
    }

    std::size_t Fresh(std::uint32_t nowMs) const
    {
        return static_cast<std::size_t>(std::count_if(items.begin(), items.end(),
                                                       [nowMs](Observation const& o) { return IsFresh(o, nowMs); }));
    }
};

// ---------------------------------------------------------------------------------------------------------------------
// Weighing reports of pressure. A report is uncertain evidence: it is worth its count times its confidence, the most
// confident line wins (ten players repeating "inc fr 3" are one report, not thirty enemies), and a "clear" received after
// it cancels it. What the plan sees itself comes first: reported enemies above the visible ones are added at their
// confidence, so a report never lowers what the plan already counts.
// ---------------------------------------------------------------------------------------------------------------------
inline bool IsPressureKind(Kind kind)
{
    return kind == Kind::INCOMING || kind == Kind::HELP;
}

// The enemies reported at a place, weighted by confidence (rounded half up).
inline std::uint32_t ReportedEnemies(std::vector<Observation> const& items, std::uint8_t place, std::uint32_t nowMs)
{
    std::uint32_t best = 0;
    for (Observation const& o : items)
    {
        if (!IsPressureKind(o.kind) || o.place != place || !IsFresh(o, nowMs))
            continue;

        bool cleared = false;
        for (Observation const& c : items)
            if (c.kind == Kind::CLEAR && c.place == place && IsFresh(c, nowMs) &&
                static_cast<std::int32_t>(c.receivedMs - o.receivedMs) > 0)
                cleared = true;
        if (cleared)
            continue;

        std::uint32_t const count = o.count ? o.count : UNNUMBERED_ENEMIES;
        std::uint32_t const weighted = (count * ConfidencePercent(o, nowMs) + 50) / 100;
        best = std::max(best, weighted);
    }
    return best;
}

inline std::uint32_t ReconciledEnemies(std::uint32_t visible, std::uint32_t reported)
{
    // A report adds what is above what the plan sees; the visible count is never lowered by it.
    return reported > visible ? reported : visible;
}

// ---------------------------------------------------------------------------------------------------------------------
// A bounded queue of messages that session threads post and the map thread drains. The queue itself is plain data: the
// caller (PvpMatchState) holds the short lock around it.
// ---------------------------------------------------------------------------------------------------------------------
constexpr std::size_t INBOX_MAX = 64;

struct Message
{
    std::uint32_t generation = 0;   // of the match the sender stood in when it posted
    std::uint8_t side = 0;
    Observation obs;
};

struct Inbox
{
    std::vector<Message> queue;

    // False when the queue is full: the message is dropped (a flood loses its newest lines, not the plan's memory).
    bool Push(Message const& m, std::size_t max = INBOX_MAX)
    {
        if (queue.size() >= max)
            return false;
        queue.push_back(m);
        return true;
    }

    std::vector<Message> Drain()
    {
        std::vector<Message> out;
        out.swap(queue);
        return out;
    }
};

// A drained message is used only when it belongs to this generation and a real side.
inline bool MessageValid(Message const& m, std::uint32_t generation)
{
    return m.generation == generation && m.side < SIDE_COUNT;
}

// ---------------------------------------------------------------------------------------------------------------------
// The match table's safety net: a match whose end hook never ran (a crash of the leave path, a map that was destroyed) must
// not stay forever. A state is dropped when it ended and its grace passed, when nobody touched it for IDLE_DROP_MS, or
// (oldest first) when the table is over its size.
// ---------------------------------------------------------------------------------------------------------------------
constexpr std::size_t MAX_MATCHES = 64;
constexpr std::uint32_t IDLE_DROP_MS = 10u * 60u * 1000u;
constexpr std::uint32_t END_GRACE_MS = 2u * 60u * 1000u;
constexpr std::uint32_t SWEEP_INTERVAL_MS = 60u * 1000u;

struct MatchStamp
{
    std::uint32_t lastTouchMs = 0;
    bool ended = false;
    std::uint32_t endedMs = 0;
};

// The indices of the stamps to drop.
inline std::vector<std::size_t> SweepSelect(std::vector<MatchStamp> const& stamps, std::uint32_t nowMs,
                                            std::size_t maxMatches = MAX_MATCHES)
{
    std::vector<std::size_t> drop;
    std::vector<std::size_t> keep;
    for (std::size_t i = 0; i < stamps.size(); ++i)
    {
        MatchStamp const& s = stamps[i];
        bool const endedLong = s.ended && Reached(nowMs, s.endedMs + END_GRACE_MS);
        bool const idle = Reached(nowMs, s.lastTouchMs + IDLE_DROP_MS);
        if (endedLong || idle)
            drop.push_back(i);
        else
            keep.push_back(i);
    }

    // Over the size: the least recently touched go first.
    std::stable_sort(keep.begin(), keep.end(), [&](std::size_t a, std::size_t b)
    {
        return static_cast<std::int32_t>(stamps[a].lastTouchMs - stamps[b].lastTouchMs) < 0;
    });
    for (std::size_t i = 0; keep.size() - i > maxMatches; ++i)
        drop.push_back(keep[i]);

    std::sort(drop.begin(), drop.end());
    return drop;
}
}  // namespace PvpObs

#endif
