#ifndef AWARE_BOTS_LOOT_WALK_POLICY_H
#define AWARE_BOTS_LOOT_WALK_POLICY_H

#include <ctime>

// BT-182: Deadmines run 4 (2026-11-13) sent the leader 286 "All paths not in LOS" lines, 279 of them out of combat:
// the walk to a corpse tried only spots the bot could see in a straight line, so a corpse around a corner, behind a
// ship rail or on another deck was never walked to, and the bot kept the loot target instead of following (the healer
// stood 33 yd from the leader for 25 s). The walk now takes the navmesh path to the corpse; a corpse the bot cannot get
// closer to is given up after a few tries and not taken up again.
namespace LootWalkPolicy
{
constexpr int MAX_FAILED_TRIES = 3;
// A new walk that ends less than this much closer than the last one made no progress (the end of an incomplete path).
constexpr float PROGRESS_YARDS = 1.0f;
// A corpse given up is not taken up again for this long (corpses despawn within minutes).
constexpr time_t SKIP_SECONDS = 600;
// The shared "All paths not in LOS" error reaches the leader at most this often per bot and action.
constexpr time_t LOS_ERROR_GAP_SECONDS = 30;

// One try at a loot target failed: the walk was refused while the bot stands still (no path, a shut door), or a new
// walk was launched from no closer than the last one. lastLaunchDistance <= 0: no walk launched yet for this target.
// BT-195: Deadmines run 6 still gave a corpse up at 6.7 yd in sight; a walk that is still running is never a failed
// try, only a bot that stopped (the end of an incomplete path, a refused move) is judged.
constexpr bool TryFailed(bool launched, bool standingStill, float distance, float lastLaunchDistance)
{
    if (!standingStill)
        return false;
    if (!launched)
        return true;
    return lastLaunchDistance > 0.0f && distance > lastLaunchDistance - PROGRESS_YARDS;
}

constexpr bool GiveUp(int failedTries) { return failedTries >= MAX_FAILED_TRIES; }

// BT-188: tries were judged on every tick, so three quick ticks of one walk in progress gave the corpse up: Deadmines
// run 5 had all 7 give-ups at 7 to 14 yd with the corpse in sight, two of them 3 s apart. A try is judged at most once
// in this window, and the progress of a walk is measured across it.
constexpr unsigned PROGRESS_WINDOW_MS = 2000;
// BT-195: a walk that keeps being relaunched reads as moving on every tick; without a yard of progress for this long it
// is a failed try all the same, so an unreachable corpse is still given up.
constexpr unsigned NO_PROGRESS_MS = 10000;

// The walk to one loot target, read on every tick.
struct Walk
{
    int failedTries = 0;
    float referenceDistance = 0.0f;  // distance at the last judged launch (0: none yet)
    bool judged = false;
    unsigned judgedAtMs = 0;
    float closestDistance = 0.0f;   // closest distance reached so far (0: none yet)
    unsigned closerAtMs = 0;        // when the walk last got PROGRESS_YARDS closer than that

    // nowMs is the server's millisecond clock; the unsigned difference survives its wrap.
    constexpr void Observe(bool launched, bool standingStill, float distance, unsigned nowMs)
    {
        if (judged && nowMs - judgedAtMs < PROGRESS_WINDOW_MS)
            return;
        judged = true;
        judgedAtMs = nowMs;
        bool stalled = false;
        if (closestDistance <= 0.0f || distance <= closestDistance - PROGRESS_YARDS)
        {
            closestDistance = distance;
            closerAtMs = nowMs;
        }
        else if (nowMs - closerAtMs >= NO_PROGRESS_MS)
        {
            stalled = true;
            closerAtMs = nowMs;
        }
        if (stalled || TryFailed(launched, standingStill, distance, referenceDistance))
            ++failedTries;
        if (launched)
            referenceDistance = distance;
    }

    constexpr bool GaveUp() const { return GiveUp(failedTries); }
};

constexpr bool StillSkipped(time_t skippedAt, time_t now) { return skippedAt != 0 && now - skippedAt < SKIP_SECONDS; }

constexpr bool LosErrorDue(time_t lastError, time_t now)
{
    return lastError == 0 || now - lastError >= LOS_ERROR_GAP_SECONDS;
}
}

#endif
