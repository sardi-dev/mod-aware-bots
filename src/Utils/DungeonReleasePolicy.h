#ifndef _PLAYERBOT_DUNGEONRELEASEPOLICY_H
#define _PLAYERBOT_DUNGEONRELEASEPOLICY_H

#include <cstdint>

namespace DungeonReleasePolicy
{
enum class Decision { Wait, NoResurrecter, CombatWaitExpired, DeathWaitExpired };

struct Facts
{
    std::uint64_t secondsSinceDeath;
    std::uint64_t secondsSinceCombatEnded;
    std::uint32_t waitSeconds;
    std::uint32_t maxWaitSeconds;
    bool inCombat;
    bool pendingRequest;
    bool selfResurrection;
    bool livingResurrecter;
};

inline Decision Decide(Facts const& facts)
{
    if (facts.secondsSinceDeath >= facts.maxWaitSeconds)
        return Decision::DeathWaitExpired;
    // Give the existing self-resurrection action priority within the death limit.
    if (facts.selfResurrection)
        return Decision::Wait;
    if (!facts.inCombat && facts.secondsSinceCombatEnded >= facts.waitSeconds)
        return Decision::CombatWaitExpired;
    if (facts.pendingRequest || facts.livingResurrecter)
        return Decision::Wait;
    return Decision::NoResurrecter;
}
}
#endif
