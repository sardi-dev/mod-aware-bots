/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_ARENACHATMATH_H
#define _PLAYERBOT_ARENACHATMATH_H

// BT-154: when the bots talk about the ladder. Which moment a result is worth a line, and how often the ladder as a
// whole, one team and one day may speak. Free of game types so that tests/ can run it; the game side (ArenaChat in
// ArenaLadder.cpp) sends the line through the bot voice.

#include <cstdint>
#include <deque>
#include <map>

namespace ArenaChatMath
{
enum class Moment : std::uint8_t
{
    NONE,
    FORMED,        // a new team is founded
    STREAK3,       // a win streak reaches 3
    STREAK5,       // a win streak reaches 5
    STREAK_BREAK,  // a streak of 3 or more ends
    CLIMB,         // the team takes rank 1 or enters the top 3
    RIVAL          // a rematch of a close rivalry
};

constexpr std::int32_t STREAK_FIRST = 3;
constexpr std::int32_t STREAK_SECOND = 5;
constexpr std::uint32_t TOP_RANKS = 3;
constexpr std::uint32_t RIVAL_MIN_MEETINGS = 4;
constexpr std::uint32_t RIVAL_WINDOW_DAYS = 14;
constexpr std::uint32_t RIVAL_MAX_WIN_GAP = 1;

struct ResultFacts
{
    std::int32_t oldStreak = 0;
    std::int32_t newStreak = 0;
    std::uint32_t oldRank = 0;  // 1 based, 0: unranked
    std::uint32_t newRank = 0;
    std::uint32_t meetings = 0;      // matches of these two teams in the window, this one included
    std::uint32_t winsOfTeam = 0;    // of those, won by the team that is speaking
    std::uint32_t winsOfOpponent = 0;
};

// Rivalry: the same two teams met at least four times in 14 days and neither leads by more than one win.
inline bool IsRivalry(ResultFacts const& facts)
{
    if (facts.meetings < RIVAL_MIN_MEETINGS)
        return false;
    std::uint32_t const gap = facts.winsOfTeam > facts.winsOfOpponent ? facts.winsOfTeam - facts.winsOfOpponent
                                                                      : facts.winsOfOpponent - facts.winsOfTeam;
    return gap <= RIVAL_MAX_WIN_GAP;
}

// The one moment a winning team's result is worth, most notable first. A loss never gets a line from its own team;
// the winner talks, and a broken streak is told by the team that broke it.
inline Moment ChooseMoment(ResultFacts const& facts, bool teamWon)
{
    if (!teamWon)
        return Moment::NONE;

    bool const enteredTop = facts.newRank != 0 && facts.newRank <= TOP_RANKS && (facts.oldRank == 0 || facts.oldRank > TOP_RANKS);
    bool const tookFirst = facts.newRank == 1 && facts.oldRank != 1;
    if (tookFirst || enteredTop)
        return Moment::CLIMB;
    if (facts.newStreak == STREAK_SECOND)
        return Moment::STREAK5;
    if (facts.newStreak == STREAK_FIRST)
        return Moment::STREAK3;
    return IsRivalry(facts) ? Moment::RIVAL : Moment::NONE;
}

// Did the loser lose a streak that was worth a line? (the winner then says STREAK_BREAK)
inline bool BrokeAStreak(std::int32_t loserOldStreak)
{
    return loserOldStreak >= STREAK_FIRST;
}

// Rate limits: one line for the whole ladder per `ladderGap`, one per team per `teamGap`, at most `perDay` lines in any
// 24 hours, and none in quiet hours (owner time). All times are UTC epoch seconds, passed in by the caller.
struct ChatLimits
{
    std::int64_t ladderGapSeconds = 15 * 60;
    std::int64_t teamGapSeconds = 45 * 60;
    std::uint32_t perDay = 12;
};

class ChatLimiter
{
public:
    explicit ChatLimiter(ChatLimits limits = ChatLimits()) : _limits(limits) {}

    bool MayTake(std::int64_t now, bool quietHours, std::uint32_t teamId)
    {
        if (quietHours)
            return false;
        while (!_today.empty() && now - _today.front() >= DAY_SECONDS)
            _today.pop_front();
        if (_today.size() >= _limits.perDay)
            return false;
        if (_hasLast && now - _lastLadder < _limits.ladderGapSeconds)
            return false;
        auto const team = _lastTeam.find(teamId);
        return team == _lastTeam.end() || now - team->second >= _limits.teamGapSeconds;
    }

    // Takes a slot when allowed; false (and nothing recorded) otherwise.
    bool TryTake(std::int64_t now, bool quietHours, std::uint32_t teamId)
    {
        if (!MayTake(now, quietHours, teamId))
            return false;
        _hasLast = true;
        _lastLadder = now;
        _lastTeam[teamId] = now;
        _today.push_back(now);
        return true;
    }

private:
    static constexpr std::int64_t DAY_SECONDS = 24 * 3600;

    ChatLimits _limits;
    bool _hasLast = false;
    std::int64_t _lastLadder = 0;
    std::map<std::uint32_t, std::int64_t> _lastTeam;
    std::deque<std::int64_t> _today;
};
}  // namespace ArenaChatMath

#endif
