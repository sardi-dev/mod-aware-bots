#pragma once

#include <algorithm>

// BT-151: fill only while a human is waiting and stop at the per-team queue quota.
// BT-430 (owner 2026-11-18): the same rule for every battleground queue a human joins, not only Warsong Gulch and the
// random queue; the team sizes come from the map's template, the random queue has its own cap.
namespace BgFillPolicy
{
// The largest team any fill ever asks for: the 40 a side of Alterac Valley and Isle of Conquest.
inline constexpr unsigned MAX_FILL_PER_TEAM = 40;

// The random battleground picks its map only when the match is made, so its template (5 to 40 a side) says nothing about
// the team size. Fill as many as the biggest of the small maps (Arathi Basin, Eye of the Storm, Strand of the Ancients
// take 15): an uninvited bot beyond the real team leaves when the human enters (ShouldLeaveQueue).
inline constexpr unsigned RANDOM_QUEUE_FILL_PER_TEAM = 15;

// How many bots (and humans) one team of a queue may hold: the map's own team size, never less than the number that starts
// the match, never more than the biggest battleground. A template that says 0 (no battleground) fills nothing.
inline unsigned TeamQuota(unsigned minPerTeam, unsigned maxPerTeam, bool randomQueue)
{
    if (randomQueue)
        return RANDOM_QUEUE_FILL_PER_TEAM;
    if (maxPerTeam == 0)
        return 0;
    return std::min(std::max(maxPerTeam, minPerTeam), MAX_FILL_PER_TEAM);
}

inline bool Wanted(unsigned humansWaiting, unsigned teamPlayers, unsigned teamBots, unsigned quota)
{
    return humansWaiting != 0 && teamPlayers < quota && teamBots < quota - teamPlayers;
}

// BT-430: the census is rebuilt every BgCheckInterval seconds, so every bot that looks in that window sees the same
// count and all of them queue (live 2026-11-17: 12 Alliance and 17 Horde bots for a Warsong quota of 10, 29 queued in
// one window). A bot books its place the moment it queues: the check and the count change are one step, so the next bot
// sees it. The census overwrites the count from the real queue on its next pass.
inline bool TryBook(unsigned humansWaiting, unsigned teamPlayers, unsigned& teamBots, unsigned quota)
{
    if (!Wanted(humansWaiting, teamPlayers, teamBots, quota))
        return false;
    ++teamBots;
    return true;
}

// BT-387: a bot that waits in a queue no human waits in (and that no match has invited yet) leaves it.
inline bool ShouldLeaveQueue(unsigned humansWaiting, bool invited)
{
    return humansWaiting == 0 && !invited;
}
}
