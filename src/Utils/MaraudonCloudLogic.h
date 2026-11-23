/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef AWARE_BOTS_MARAUDON_CLOUD_LOGIC_H
#define AWARE_BOTS_MARAUDON_CLOUD_LOGIC_H

#include <cmath>
#include <cstdint>

namespace MaraudonCloud
{
constexpr uint32_t SPELL_NOXIOUS_CLOUD = 21070;

inline bool Relevant(uint32_t spell, int32_t duration, float radius, float heightDifference)
{
    return spell == SPELL_NOXIOUS_CLOUD && duration > 0 && radius > 0.0f && radius <= 30.0f &&
        std::fabs(heightDifference) <= radius;
}
}

#endif
