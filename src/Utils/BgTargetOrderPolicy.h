/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_BGTARGETORDERPOLICY_H
#define _PLAYERBOT_BGTARGETORDERPOLICY_H

// BT-553: the order in which a bot picks its target in a fight at a node (owner decision 2026-11-18, the Skill-Capped Battle for
// Gilneas guide and the Arathi Basin one): expert, the healer first when the team holds a numbers edge, otherwise the squishy
// damage dealers first (their defensive cooldowns and the healers' mana go first, the healer dies after); good, the healer first;
// casual, the lowest health. Pure, free of game types, so that tests/ can run it (tests/test_bg_target_order.cpp).

#include "BotSkillMath.h"

#include <cstdint>
#include <vector>

namespace BgTargetOrder
{
using Tier = BotSkillMath::Tier;

// An enemy player in reach: what the order needs to know.
struct Foe
{
    std::uint64_t guid = 0;
    bool healer = false;
    bool squishy = false;       // cloth and leather damage dealers, death knights and warlocks take the most damage (guide)
    bool carrier = false;       // carries the flag (Eye of the Storm, BT-585): good and expert bots hit it first
    float healthPercent = 100.0f;
    float distance = 0.0f;
};

// The numbers edge of the fight: our players near it against the enemies seen near it, by at least this many.
constexpr int NUMBERS_EDGE = 1;

constexpr float REACH_YARDS = 30.0f;          // only enemies this close to the bot are weighed
constexpr std::uint32_t MIN_SWITCH_MS = 3000; // a bot changes its mind about a target at most this often

inline bool HasEdge(unsigned friends, unsigned enemies)
{
    return int(friends) - int(enemies) >= NUMBERS_EDGE;
}

enum class Class : std::uint8_t
{
    HEALER,
    SQUISHY,
    OTHER
};

inline int Rank(Tier tier, bool edge, Foe const& foe)
{
    // Lower rank is hit first. Within a rank the lowest health goes first, then the nearest, then the lowest GUID.
    if (foe.carrier && tier != Tier::CASUAL)
        return -1;
    switch (tier)
    {
        case Tier::EXPERT:
            if (edge)
                return foe.healer ? 0 : 1;
            return foe.squishy && !foe.healer ? 0 : foe.healer ? 1 : 2;
        case Tier::GOOD:
            return foe.healer ? 0 : 1;
        case Tier::CASUAL:
            break;
    }
    return 0;
}

// The target the order picks among the foes in reach, or 0 when there is none.
inline std::uint64_t Pick(Tier tier, bool edge, std::vector<Foe> const& foes)
{
    Foe const* best = nullptr;
    int bestRank = 0;
    for (Foe const& foe : foes)
    {
        if (foe.distance > REACH_YARDS)
            continue;
        int const rank = Rank(tier, edge, foe);
        bool const better = !best || rank < bestRank ||
                            (rank == bestRank && foe.healthPercent < best->healthPercent) ||
                            (rank == bestRank && foe.healthPercent == best->healthPercent && foe.distance < best->distance) ||
                            (rank == bestRank && foe.healthPercent == best->healthPercent && foe.distance == best->distance &&
                             foe.guid < best->guid);
        if (better)
        {
            best = &foe;
            bestRank = rank;
        }
    }
    return best ? best->guid : 0;
}

// A bot already on a target that the order ranks as high as its own pick keeps it (no flapping between two near-equal foes).
inline bool ShouldSwitch(Tier tier, bool edge, std::vector<Foe> const& foes, std::uint64_t current, std::uint64_t pick,
                         std::uint32_t sinceLastSwitchMs)
{
    if (!pick || pick == current || sinceLastSwitchMs < MIN_SWITCH_MS)
        return false;
    Foe const* now = nullptr;
    Foe const* wanted = nullptr;
    for (Foe const& foe : foes)
    {
        if (foe.guid == current)
            now = &foe;
        if (foe.guid == pick)
            wanted = &foe;
    }
    if (!wanted)
        return false;
    if (!now)
        return true;   // the current target is out of reach or not an enemy player: take the pick
    return Rank(tier, edge, *wanted) < Rank(tier, edge, *now);
}
}  // namespace BgTargetOrder

#endif
