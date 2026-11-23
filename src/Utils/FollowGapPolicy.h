#ifndef AWARE_BOTS_FOLLOW_GAP_POLICY_H
#define AWARE_BOTS_FOLLOW_GAP_POLICY_H

#include <ctime>

// BT-183: Deadmines run 4 (2026-11-13) lost 4.2 min to six group waits where one to three bots stood at the last fight
// spot 31 to 35 yd from the leader, out of combat and at full health, until the leader walked on. No line showed what
// the follow decided (the [DEFEND] sample runs only while a member fights). A bot out of combat that stays this far from
// the leader this long writes a [FOLLOW] line, then again at most every LOG_GAP_SECONDS while it stays far.
namespace FollowGapPolicy
{
constexpr float FAR_YARDS = 30.0f;
constexpr time_t HOLD_SECONDS = 20;
constexpr time_t LOG_GAP_SECONDS = 20;
// The distance is read at most this often (the engine ticks several times a second).
constexpr time_t CHECK_SECONDS = 2;

// A gap this long between two readings means the out of combat engine did not run (a fight, death): start over.
constexpr time_t READING_GAP_SECONDS = 10;

// Since when the bot is far from the leader (0 = not far now). lastCheck: the previous reading (0 = none).
// BT-189: resting (sitting to eat or drink now) explains a long gap: the drink holds the engine up to 18 s, so the clock
// restarted at every reading and a drinking bot never got a line (Deadmines run 5: the healer's two waits at 36 and
// 30 yd while her mana went from 89 to 99 percent).
constexpr time_t NextFarSince(bool far, time_t farSince, time_t lastCheck, time_t now, bool resting = false)
{
    if (!far)
        return 0;
    if (farSince && lastCheck && (resting || now - lastCheck <= READING_GAP_SECONDS))
        return farSince;
    return now;
}

constexpr bool ShouldLog(time_t farSince, time_t now, time_t lastLog)
{
    return farSince != 0 && now - farSince >= HOLD_SECONDS && (lastLog == 0 || now - lastLog >= LOG_GAP_SECONDS);
}

// BT-193: the clock lives once per bot (PlayerbotAI), not in each of its engines. Deadmines run 6: a bot that swaps
// between its combat and non-combat engines made each engine see a reading gap, so the far clock restarted and one
// of six "members not moving" waits (37.6 yd, 93 s) had no line.
struct State
{
    time_t checkAt = 0;
    time_t farSince = 0;
    time_t loggedAt = 0;
};

// A reading is due when the last one (by any engine of the bot) is CHECK_SECONDS old.
constexpr bool Due(State const& state, time_t now)
{
    return state.checkAt == 0 || now - state.checkAt >= CHECK_SECONDS;
}

// Takes one reading (call only when Due) and returns whether a [FOLLOW] line is due now; the caller sets
// state.loggedAt after writing it.
constexpr bool Read(State& state, bool far, time_t now, bool resting)
{
    state.farSince = NextFarSince(far, state.farSince, state.checkAt, now, resting);
    state.checkAt = now;
    return ShouldLog(state.farSince, now, state.loggedAt);
}
}

#endif
