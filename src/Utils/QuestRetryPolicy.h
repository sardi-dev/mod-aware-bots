#ifndef QUEST_RETRY_POLICY_H
#define QUEST_RETRY_POLICY_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace QuestRetryPolicy
{
struct Failure
{
    uint32_t tries = 0;
    uint32_t at = 0;
    uint32_t zone = 0;
    uint32_t level = 0;
    // BT-381: how often the work on this quest failed because the bot could not get anywhere (stuck on the way, stood
    // still). Cleared with the entry when the quest advances or is handed in.
    uint32_t stuck = 0;
};
using Book = std::unordered_map<uint32_t, Failure>;

enum class QuestEvent { Accepted, Dropped, Progress, Rewarded };

inline void OnQuestEvent(Book& book, uint32_t questId, QuestEvent event)
{
    if (event == QuestEvent::Progress || event == QuestEvent::Rewarded)
        book.erase(questId);
}

inline bool Advanced(std::vector<uint32_t> const& before, std::vector<uint32_t> const& after)
{
    for (std::size_t i = 0; i < before.size() && i < after.size(); ++i)
        if (after[i] > before[i])
            return true;
    return false;
}

inline uint32_t Delay(uint32_t tries)
{
    return tries == 1 ? 10 * 60 * 1000u : tries == 2 ? 30 * 60 * 1000u : 2 * 60 * 60 * 1000u;
}

// BT-381: the reasons that mean the bot could not reach the place (the texts the trip failures log).
constexpr char const* STUCK_ON_THE_WAY = "stuck on the way";
constexpr char const* STOOD_STILL = "stood still";
constexpr char const* TRANSPORT_WALK_STALLED = "the walk to the transport stalled";

inline bool IsStuckReason(char const* why)
{
    return why && (!std::strcmp(why, STUCK_ON_THE_WAY) || !std::strcmp(why, STOOD_STILL) ||
                   !std::strcmp(why, TRANSPORT_WALK_STALLED));
}

inline bool IsTransportWalkStall(char const* why)
{
    return why && (!std::strcmp(why, "walk-stall") || !std::strcmp(why, "taxi-walk-stall") ||
                   !std::strcmp(why, "portal-walk-stall") || !std::strcmp(why, "dock-walk-stall"));
}

// BT-381: a second failure of that kind on the same quest is no accident of the path (an island, a ledge, a closed
// road: Clynesa walked from Darnassus to Silithus 17 times in a day, Ocassa stood at one ledge ten times in four
// hours). The quest then waits 3 hours, and a walk into another zone does not release it, only a level does.
constexpr uint32_t STUCK_LONG_AFTER = 2;
constexpr uint32_t STUCK_DELAY_MS = 3 * 60 * 60 * 1000u;

inline bool Unreachable(Failure const& failure)
{
    return failure.stuck >= STUCK_LONG_AFTER;
}

inline uint32_t Delay(Failure const& failure)
{
    uint32_t const delay = Delay(failure.tries);
    return Unreachable(failure) && delay < STUCK_DELAY_MS ? STUCK_DELAY_MS : delay;
}

inline bool Wait(Failure const& failure, uint32_t now, uint32_t zone, uint32_t level)
{
    return failure.tries && (zone == failure.zone || Unreachable(failure)) && level == failure.level &&
        uint32_t(now - failure.at) < Delay(failure);
}

inline bool RecordFailure(Failure& failure, uint32_t now, uint32_t zone, uint32_t level, bool complete,
                          bool stuck = false)
{
    if (stuck)
        ++failure.stuck;
    if (failure.tries < 3)
        ++failure.tries;
    failure.at = now;
    failure.zone = zone;
    failure.level = level;
    return !complete && failure.tries >= 3;
}
}
#endif
