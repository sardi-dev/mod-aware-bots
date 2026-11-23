#ifndef AWARE_BOTS_TANK_STANDOFF_POLICY_H
#define AWARE_BOTS_TANK_STANDOFF_POLICY_H

#include <cstdint>
#include <ctime>

// BT-194: Deadmines run 6 (2026-11-14): four mobs held the tank 3 min 20 s while none lost health (two Blood Wizards
// stop moving on aggro and cast only with line of sight) and no line said what the tank did. A tank in combat whose
// target loses no health for this long writes a [TANK] line, then again at most every LOG_GAP_SECONDS.
namespace TankStandoffPolicy
{
constexpr time_t STALL_SECONDS = 15;
constexpr time_t LOG_GAP_SECONDS = 15;
// The target is read at most this often (the engine ticks several times a second).
constexpr time_t OBSERVE_SECONDS = 1;
// A gap this long between two readings means the combat engine did not run (a fight ended): start over.
constexpr time_t READING_GAP_SECONDS = 10;

struct State
{
    uint64_t targetGuid = 0;
    uint64_t health = 0;
    time_t observedAt = 0;
    time_t changedAt = 0;
    time_t loggedAt = 0;
};

constexpr bool ObserveDue(State const& state, time_t now)
{
    return state.observedAt == 0 || now - state.observedAt >= OBSERVE_SECONDS;
}

// One reading of the tank's target (guid 0 = none or not fighting). Returns how long the target has lost no health.
// A new target starts the clock; health that rises (regeneration) does not reset it, a loss does.
constexpr time_t Observe(State& state, uint64_t guid, uint64_t health, time_t now)
{
    bool const gap = state.observedAt != 0 && now - state.observedAt > READING_GAP_SECONDS;
    state.observedAt = now;
    if (guid == 0)
    {
        state.targetGuid = 0;
        state.changedAt = 0;
        return 0;
    }
    if (guid != state.targetGuid || gap)
    {
        state.targetGuid = guid;
        state.health = health;
        state.changedAt = now;
        return 0;
    }
    if (health < state.health)
        state.changedAt = now;
    state.health = health;
    return now - state.changedAt;
}

constexpr bool ShouldLog(time_t stalled, time_t now, time_t lastLog)
{
    return stalled >= STALL_SECONDS && (lastLog == 0 || now - lastLog >= LOG_GAP_SECONDS);
}
}

#endif
