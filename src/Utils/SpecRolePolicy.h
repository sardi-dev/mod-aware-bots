#ifndef PLAYERBOT_SPEC_ROLE_POLICY_H
#define PLAYERBOT_SPEC_ROLE_POLICY_H

#include <cstdint>

namespace SpecRolePolicy
{
enum Role { Tank, Healer, Damage, Count };

// Shares are relative weights. A zero share disables preference for that role.
inline int Choose(uint32_t const counts[Count], uint32_t const shares[Count], bool const available[Count], uint32_t level)
{
    uint64_t sum = uint64_t(shares[0]) + shares[1] + shares[2];
    if (!sum)
        return -1;
    uint64_t total = uint64_t(counts[0]) + counts[1] + counts[2] + 1;
    int best = -1;
    int64_t bestScore = 0;
    bool bestMinimum = false;
    for (int role = 0; role < Count; ++role)
    {
        if (!available[role] || !shares[role])
            continue;
        bool const minimum = level >= 25 && level <= 54 && role != Damage && counts[role] < 4;
        int64_t const score = int64_t(total * shares[role]) - int64_t(uint64_t(counts[role]) * sum);
        if (best < 0 || (minimum && !bestMinimum) || (minimum == bestMinimum && score > bestScore))
        {
            best = role;
            bestScore = score;
            bestMinimum = minimum;
        }
    }
    return best;
}
}
#endif
