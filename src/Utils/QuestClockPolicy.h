#ifndef AWARE_BOTS_QUEST_CLOCK_POLICY_H
#define AWARE_BOTS_QUEST_CLOCK_POLICY_H

#include <cstdint>

// The clock the quest trips (hand-in, item fetch, interaction) measure their budgets and waits on (BT-230). A random
// bot away from every real player runs its full AI only in its share of the activity rotation (BotActiveAlone, 10 %
// of the 30 s windows on this realm); the rest of the time it ticks minimally and does not walk. Budgets measured on
// the wall clock therefore ran out after a tenth of the planned walking: in the 5 h log of 2026-11-16, 494 of the 667
// "out of time" hand-in trips ended within 10 % of their starting distance. This clock only runs while the bot is
// allowed full activity, so a budget means minutes of play, as for a player.
namespace QuestClock
{
// One world tick is far shorter; a longer one is a server stall, which should not burn a trip's budget either.
constexpr uint32_t MAX_TICK_MS = 2000;

// The clock never reads 0, since the trips keep 0 as "not yet".
inline uint32_t Advance(uint32_t clock, uint32_t elapsedMs, bool active)
{
    if (!active)
        return clock ? clock : 1;
    uint32_t const step = elapsedMs > MAX_TICK_MS ? MAX_TICK_MS : elapsedMs;
    uint32_t const next = clock + step;
    return next ? next : 1;
}

// BT-371: how long the current rpg status has run in active time. The status durations (30 minutes on a quest, 5 of
// wandering) ran on the wall clock, so a rotating bot got a tenth of them: three minutes of play per quest pick, too
// short to walk to a POI and search it for the 5 minutes the retry book waits for, and the next draw often picked
// another quest or POI. A status change bumps a serial with no bot at hand (NewRpgInfo::ChangeTo...); the first look
// after it reads the quest clock as the start.
struct StatusWatch
{
    uint32_t serial = 0;
    uint32_t startMs = 0;

    uint32_t Elapsed(uint32_t statusSerial, uint32_t clock)
    {
        if (statusSerial != serial || !startMs)
        {
            serial = statusSerial;
            startMs = clock ? clock : 1;
        }
        return clock - startMs;
    }
};
}  // namespace QuestClock

#endif
