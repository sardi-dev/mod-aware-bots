#ifndef AWARE_BOTS_STUCK_MEMBER_POLICY_H
#define AWARE_BOTS_STUCK_MEMBER_POLICY_H

#include <cstdint>
#include <ctime>

// BT-201: Shadowfang Keep run 7 (2026-11-15): the healer stood still 20 minutes at one spot, flagged in combat, more
// than 40 yd from every member, and no line named her. The [DEFEND] sample runs only for a bot out of combat and the
// [FOLLOW] sample only for a bot out of combat too, so a bot flagged in combat was invisible to both. A bot that is
// flagged in combat, over FAR_YARDS from every member and has not moved for STILL_SECONDS now writes both lines (tagged
// stuck=1), then again at most every LOG_GAP_SECONDS. Log only: the decision of what to do is BT-202.
namespace StuckMemberPolicy
{
constexpr float FAR_YARDS = 40.0f;
// A bot that moved less than this since its reference spot has not moved (a crawl or a turn on the spot).
constexpr float STILL_YARDS = 2.0f;
constexpr time_t STILL_SECONDS = 20;
constexpr time_t LOG_GAP_SECONDS = 20;
// The group is read at most this often (the engine ticks several times a second).
constexpr time_t CHECK_SECONDS = 2;
// A gap this long between two readings means the bot was not in combat (or its engine did not run): start over.
constexpr time_t READING_GAP_SECONDS = 10;

// One reading of the bot. nearestMember is the distance to the nearest living member of the bot's group on its map, or
// a negative number when there is none (a bot alone is not stuck from anybody).
struct Sample
{
    bool inCombat = false;
    float nearestMember = -1.0f;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    // When the bot last took damage, from the core's damage hook (BT-202; 0 = never seen). Net health between two
    // readings misses a hit that was healed back before the next reading.
    time_t lastDamageAt = 0;
};

struct State
{
    time_t checkAt = 0;
    // Since when the bot is flagged in combat with no member within FAR_YARDS (0 = not now).
    time_t farSince = 0;
    // Since when the bot has stayed within STILL_YARDS of (refX, refY, refZ) while far (0 = not far).
    time_t stillSince = 0;
    float refX = 0.0f;
    float refY = 0.0f;
    float refZ = 0.0f;
    float nearestMember = -1.0f;
    time_t loggedAt = 0;
    // BT-202: when the bot last took damage (copied from the sample).
    time_t hurtAt = 0;
};

constexpr bool Due(State const& state, time_t now)
{
    return state.checkAt == 0 || now - state.checkAt >= CHECK_SECONDS;
}

constexpr bool Moved(State const& state, Sample const& sample)
{
    float const dx = sample.x - state.refX;
    float const dy = sample.y - state.refY;
    float const dz = sample.z - state.refZ;
    return dx * dx + dy * dy + dz * dz > STILL_YARDS * STILL_YARDS;
}

constexpr bool IsFar(Sample const& sample)
{
    return sample.inCombat && sample.nearestMember > FAR_YARDS;
}

constexpr bool ShouldLog(State const& state, time_t now)
{
    return state.farSince != 0 && state.stillSince != 0 && now - state.stillSince >= STILL_SECONDS &&
        (state.loggedAt == 0 || now - state.loggedAt >= LOG_GAP_SECONDS);
}

// Takes one reading (call only when Due) and returns whether the [DEFEND] and [FOLLOW] lines are due now; the caller
// sets state.loggedAt after writing them.
constexpr bool Read(State& state, Sample const& sample, time_t now)
{
    bool const gap = state.checkAt != 0 && now - state.checkAt > READING_GAP_SECONDS;
    bool const moved = Moved(state, sample);
    state.checkAt = now;
    state.nearestMember = sample.nearestMember;
    state.hurtAt = sample.lastDamageAt;
    if (!IsFar(sample))
    {
        state.farSince = 0;
        state.stillSince = 0;
        return false;
    }
    if (!state.farSince || gap)
        state.farSince = now;
    if (!state.stillSince || gap || moved)
    {
        state.stillSince = now;
        state.refX = sample.x;
        state.refY = sample.y;
        state.refZ = sample.z;
    }
    return ShouldLog(state, now);
}
}

#endif
