#ifndef AWARE_BOTS_BUFF_RECAST_POLICY_H
#define AWARE_BOTS_BUFF_RECAST_POLICY_H

#include <ctime>

// BT-176: the Deadmines run of 2026-11-13 shows the protection paladin casting Blessing of Might (19740) 212 times in 34
// minutes, one cast every 4.3 s, the paladin's own party buff check, with mana going from 95 to 100 percent between the
// casts. Whatever keeps the buff from being found on its target, the same blessing on the same member is not cast again
// before this many seconds.
namespace BuffRecastPolicy
{
constexpr time_t BLESSING_RECAST_SECONDS = 30;

constexpr bool RecastAllowed(bool sameTargetAsLast, time_t secondsSinceLast)
{
    return !sameTargetAsLast || secondsSinceLast >= BLESSING_RECAST_SECONDS;
}
}

#endif
