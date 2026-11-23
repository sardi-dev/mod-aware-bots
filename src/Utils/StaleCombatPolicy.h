#ifndef AWARE_BOTS_STALE_COMBAT_POLICY_H
#define AWARE_BOTS_STALE_COMBAT_POLICY_H

#include <cstdint>
#include <ctime>

// BT-168: a member can stay flagged in combat although nothing attacks it. Live 2026-11-13 (Deadmines run 3): a
// stationary Mine Bunny that never came kept the tank and the healer flagged for five minutes, the group's ready test
// said "everyone must be out of combat", two 120 s waits ran out and the tank (a bot in the combat engine) never reached
// its marked pull. A flag with no creature attacking the member for this long is not a fight.
namespace StaleCombatPolicy
{
constexpr time_t STALE_COMBAT_SECONDS = 15;

// quietSince: when the member was first seen flagged with nothing attacking it (0 = not quiet now).
constexpr bool IsStale(bool inCombat, time_t quietSince, time_t now)
{
    return inCombat && quietSince != 0 && now - quietSince >= STALE_COMBAT_SECONDS;
}

// BT-187: the stale target is chosen without the sight test, but the "invalid target" check dropped it again at once
// for being out of sight, so the bot never walked to it (Deadmines run 5: a Defias Envoker held the tank 125 s, a
// watcher 4.5 min). The bot keeps that one target without sight for this long after choosing it, enough to walk a
// ramp or round a corner; then the usual sight rule applies again.
constexpr time_t STALE_TARGET_NO_SIGHT_SECONDS = 20;

// chosenAt: when the bot took this target as its stale target (0 = it is not the stale target).
constexpr bool KeepWithoutSight(time_t chosenAt, time_t now)
{
    return chosenAt != 0 && now - chosenAt < STALE_TARGET_NO_SIGHT_SECONDS;
}

// A creature a player cannot select or attack (UNIT_FLAG_NOT_SELECTABLE, UNIT_FLAG_NON_ATTACKABLE) is not a fight the
// group can end. Live 2026-11-17 (Shadowfang Keep proof run, Springvale Spawn Stalker 50611: hostile faction, not
// selectable, no AI): it stood at the group's spot, swung at a member every 2 s and held three members flagged in
// combat for four minutes, because the sight test counted it as an attacker while the bots (rightly) never target it.
// The tank waited on "everyone caught up" until the run was stopped. Only a creature the bot could fight stops the clock.
// The values are Unit.h's UNIT_FLAG_NON_ATTACKABLE and UNIT_FLAG_NOT_SELECTABLE; this header stays free of the core.
constexpr uint32_t UNIT_FLAG_NON_ATTACKABLE_BIT = 0x00000002;
constexpr uint32_t UNIT_FLAG_NOT_SELECTABLE_BIT = 0x02000000;

constexpr bool HoldsInFight(uint32_t unitFlags)
{
    return (unitFlags & (UNIT_FLAG_NON_ATTACKABLE_BIT | UNIT_FLAG_NOT_SELECTABLE_BIT)) == 0;
}

// The new quietSince for one reading: kept while it stays quiet, cleared when something attacks or the flag drops.
constexpr time_t NextQuietSince(bool inCombat, bool somethingAttacks, time_t quietSince, time_t now)
{
    if (!inCombat || somethingAttacks)
        return 0;
    return quietSince ? quietSince : now;
}
}

#endif
