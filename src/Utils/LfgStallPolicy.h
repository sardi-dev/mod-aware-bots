#ifndef AWARE_BOTS_LFG_STALL_POLICY_H
#define AWARE_BOTS_LFG_STALL_POLICY_H

#include <cstdint>

// When a bot-led dungeon run is given up (LfgCrawlAction.cpp) and how long the dungeon is left alone afterwards
// (LfgFill.cpp). A leader that cannot walk to a boss skips it after StuckSeconds without progress and tries the next; in
// a dungeon whose route is broken it skipped every boss within a minute, waited for the skips to expire and did it all
// again until the run's time limit, five bots tied up for 40 minutes. A run in which every boss still to do is skipped,
// with no pack pulled since the first of those skips, ends at once (the group wraps up as at the time limit), and the
// dungeon is not queued for a bot-only run again for a while.
namespace LfgStallPolicy
{
// A pull at or after the first of the skips means the leader still gets somewhere (a pack ahead was fought), whatever its
// timestamp: times are the wrapping millisecond clock, so the difference is read as signed. 0 means never pulled.
constexpr bool PulledSince(uint32_t pulledAt, uint32_t since)
{
    return pulledAt != 0 && int32_t(pulledAt - since) >= 0;
}

// bossesLeft: bosses not dead that the leader can walk to (they have a spawn); bossesSkipped: how many of them are on
// the skip list right now; pulledSinceFirstSkip: see PulledSince.
constexpr bool RunStalled(uint32_t bossesLeft, uint32_t bossesSkipped, bool pulledSinceFirstSkip)
{
    return bossesLeft > 0 && bossesSkipped >= bossesLeft && !pulledSinceFirstSkip;
}

constexpr int64_t SECONDS_PER_HOUR = 3600;

// The end of a dungeon's cooldown after a failed run; 0 hours means no cooldown.
constexpr int64_t CooldownEnd(int64_t now, uint32_t hours)
{
    return hours ? now + int64_t(hours) * SECONDS_PER_HOUR : 0;
}

constexpr bool CoolingDown(int64_t now, int64_t until)
{
    return until != 0 && now < until;
}
}

#endif
