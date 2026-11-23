#ifndef AWARE_BOTS_GROUP_REJOIN_POLICY_H
#define AWARE_BOTS_GROUP_REJOIN_POLICY_H

#include "StuckMemberPolicy.h"

#include <cstdint>
#include <ctime>

// BT-202: Shadowfang Keep run 7 (2026-11-15): the healer stood 20 minutes at one spot, flagged in combat, 40 to 148 yd
// from every member, with three Tormented Officers "attacking" her that never reached her. Nothing owned that state:
// the heal code skips a tank beyond sightDistance ("the follow code's job", HealerValues.cpp), the follow code never
// runs while the bot is flagged, the stale-combat clock (StaleCombatPolicy.h) never starts while a creature in sight
// still has the bot as its victim, and "combat stuck" waits five minutes and is off for a human master.
//
// Owner decision (2026-11-15): a bot that has had no member within FAR_YARDS for FAR_HOLD_SECONDS (and either is beyond
// sightDistance or has not moved for STILL_SECONDS) first walks to the tank by the full path. When that walk fails
// twice it drops a combat state that is stale (nothing hurt it, nothing stands at it) and follows. Never a teleport.
namespace GroupRejoinPolicy
{
constexpr time_t FAR_HOLD_SECONDS = 30;
// A walk is judged over this window. It failed only when the bot itself moved under WALK_MOVED_YARDS AND the distance
// to the tank did not close by WALK_PROGRESS_YARDS: a bot chasing a tank that walks on moves, so it is not stuck.
constexpr time_t WALK_WINDOW_SECONDS = 10;
constexpr float WALK_PROGRESS_YARDS = 5.0f;
constexpr float WALK_MOVED_YARDS = 5.0f;
constexpr uint32_t WALK_FAILS_BEFORE_DROP = 2;
// The bot took no damage (core damage hook) for this long, and has been far this long.
constexpr time_t HURT_QUIET_SECONDS = 30;
// A creature that has the bot as its victim and stands this close is a fight even if it has missed so far. One that
// has not come within reach in 30 s is not (the Officers stood 17 yd away for 20 minutes).
constexpr float ATTACKER_REACH_YARDS = 6.0f;
// After a drop the bot keeps walking to the tank this long (or until a member is within FAR_YARDS), in either engine,
// so a creature that still has it on its threat list cannot pull it back into the standing state.
constexpr time_t FOLLOW_AFTER_DROP_SECONDS = 45;
constexpr time_t LOG_GAP_SECONDS = 20;
// The member readings are fresh when the last one is this young (they come every CHECK_SECONDS).
constexpr time_t FRESH_SECONDS = 10;
// BT-698: the walk order is repeated at a backoff while the bot gains no ground. Jihma (Shadowfang run, 2026-11-19) logged
// "walks to Sonwyn at 5890 yd" 306 times in two hours without moving: a path request on every 2 s trigger tick. A gap
// starts at MOVE_GAP_MIN_SECONDS, doubles up to MOVE_GAP_MAX_SECONDS while the distance does not shrink, and starts over
// as soon as it does (a bot that walks is ordered as often as before). Beyond FAR_WALK_YARDS the gap is the longest at
// once: a path that long is not walked in a minute, and the stale-combat drop (NextStep) still judges every call.
constexpr time_t MOVE_GAP_MIN_SECONDS = 2;
constexpr time_t MOVE_GAP_MAX_SECONDS = 30;
constexpr float MOVE_PROGRESS_YARDS = 1.0f;
constexpr float FAR_WALK_YARDS = 500.0f;
// An order older than this belongs to an earlier episode (the walk was not wanted in between): start over.
constexpr time_t MOVE_EPISODE_SECONDS = 3 * MOVE_GAP_MAX_SECONDS;

struct State
{
    // The walk window: since when, the distance to the tank at its start, and how many windows closed no yards.
    time_t walkSince = 0;
    float walkStartDistance = 0.0f;
    float walkStartX = 0.0f;
    float walkStartY = 0.0f;
    float walkStartZ = 0.0f;
    uint32_t walkFails = 0;
    // After a drop: when it happened, and walk to the tank until followUntil (0 = no post-drop walk).
    time_t droppedAt = 0;
    time_t followUntil = 0;
    time_t loggedAt = 0;
    // BT-698: the last walk order (when, the distance then, the gap until the next one).
    time_t moveAt = 0;
    float moveDistance = 0.0f;
    time_t moveGap = 0;
};

enum class Step
{
    Walk,
    Drop
};

constexpr bool Fresh(StuckMemberPolicy::State const& member, time_t now)
{
    return member.checkAt != 0 && now - member.checkAt <= FRESH_SECONDS;
}

constexpr bool QuietSince(StuckMemberPolicy::State const& member, time_t now, time_t seconds)
{
    return member.hurtAt == 0 || now - member.hurtAt >= seconds;
}

// Whether the bot should be walking to the tank now. sightDistance is the config value beyond which the heal and
// follow code leave a far member alone.
constexpr bool Wants(StuckMemberPolicy::State const& member, State const& state, float sightDistance, time_t now)
{
    if (!Fresh(member, now) || member.nearestMember <= StuckMemberPolicy::FAR_YARDS)
        return false;
    if (now < state.followUntil)
        return true;
    return member.farSince != 0 && now - member.farSince >= FAR_HOLD_SECONDS &&
        (member.nearestMember > sightDistance ||
         (member.stillSince != 0 && now - member.stillSince >= StuckMemberPolicy::STILL_SECONDS));
}

// Damage after the drop ends the post-drop walk for good: a new fight is the combat code's, not a bot left behind.
constexpr void EndFollowOnDamage(State& state, time_t lastDamageAt)
{
    if (state.followUntil != 0 && lastDamageAt > state.droppedAt)
        state.followUntil = 0;
}

// A walk that stops being wanted starts over next time.
constexpr void Idle(State& state)
{
    state.walkSince = 0;
    state.walkStartDistance = 0.0f;
    state.walkFails = 0;
}

constexpr bool MovedInWindow(State const& state, float x, float y, float z)
{
    float const dx = x - state.walkStartX;
    float const dy = y - state.walkStartY;
    float const dz = z - state.walkStartZ;
    return dx * dx + dy * dy + dz * dz >= WALK_MOVED_YARDS * WALK_MOVED_YARDS;
}

// One call per walk order. `distance` is the distance to the tank now and (x, y, z) the bot's own spot. Returns Drop
// when WALK_FAILS_BEFORE_DROP windows in a row failed (see WALK_MOVED_YARDS) and the bot is flagged in combat; the caller
// starts over (Idle) when the drop is refused. A window in which the bot moved breaks the chain.
constexpr Step NextStep(State& state, float distance, float x, float y, float z, bool inCombat, time_t now)
{
    if (state.walkSince == 0 || now - state.walkSince >= WALK_WINDOW_SECONDS)
    {
        bool const judged = state.walkSince != 0;
        bool const progress = state.walkStartDistance - distance >= WALK_PROGRESS_YARDS;
        bool const moved = MovedInWindow(state, x, y, z);
        state.walkSince = now;
        state.walkStartDistance = distance;
        state.walkStartX = x;
        state.walkStartY = y;
        state.walkStartZ = z;
        if (!judged)
            return Step::Walk;
        if (progress || moved)
        {
            state.walkFails = 0;
            return Step::Walk;
        }
        ++state.walkFails;
        return state.walkFails >= WALK_FAILS_BEFORE_DROP && inCombat ? Step::Drop : Step::Walk;
    }
    return Step::Walk;
}

// BT-698: whether the walk order is sent now (true) or skipped for the backoff (false). `distance` is the distance to
// the tank now. Sending records the order.
constexpr bool MoveDue(State& state, float distance, time_t now)
{
    bool const fresh = state.moveAt != 0 && now - state.moveAt <= MOVE_EPISODE_SECONDS;
    bool progress = false;
    time_t gap = MOVE_GAP_MIN_SECONDS;
    if (fresh)
    {
        progress = state.moveDistance - distance >= MOVE_PROGRESS_YARDS;
        if (!progress && now - state.moveAt < state.moveGap)
            return false;
        if (!progress)
            gap = state.moveGap * 2 > MOVE_GAP_MAX_SECONDS ? MOVE_GAP_MAX_SECONDS : state.moveGap * 2;
    }
    if (distance > FAR_WALK_YARDS && !progress && fresh)
        gap = MOVE_GAP_MAX_SECONDS;
    state.moveAt = now;
    state.moveDistance = distance;
    state.moveGap = gap;
    return true;
}

// The flag is stale when the bot has been far for HURT_QUIET_SECONDS, took no damage for that long, no creature that has
// it as its victim stands within ATTACKER_REACH_YARDS, and no player or player pet attacks it.
constexpr bool FlagIsStale(StuckMemberPolicy::State const& member, bool attackerAtHand, bool playerAttacker, time_t now)
{
    return member.farSince != 0 && !attackerAtHand && !playerAttacker && now - member.farSince >= HURT_QUIET_SECONDS &&
        QuietSince(member, now, HURT_QUIET_SECONDS);
}

constexpr bool ShouldLog(State const& state, time_t now)
{
    return state.loggedAt == 0 || now - state.loggedAt >= LOG_GAP_SECONDS;
}
}

#endif
