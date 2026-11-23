/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_BOTSKILLMATH_H
#define _PLAYERBOT_BOTSKILLMATH_H

// Which skill tier a bot has (BotSkill.h), free of game types so that tests/ can run it.

#include <cstdint>

namespace BotSkillMath
{
// How well a bot plays its class sheet. A bot keeps its tier for life: it follows from its GUID alone.
enum class Tier : std::uint8_t
{
    EXPERT,  // plays the whole sheet, reacts at once
    GOOD,    // plays the sheet without the expert lines, reacts a little late
    CASUAL   // plays the core lines only, reacts slowly
};

struct Shares
{
    std::uint32_t expertPercent;
    std::uint32_t goodPercent;  // the rest is casual
};

// A fixed, well spread number per GUID (the low GUIDs of bots are consecutive, so the GUID itself would put
// neighbours into the same tier).
inline std::uint32_t Mix(std::uint32_t guidLow)
{
    std::uint32_t value = guidLow * 2654435761u;
    value ^= value >> 15;
    value *= 2246822519u;
    value ^= value >> 13;
    return value;
}

inline Tier TierOf(std::uint32_t guidLow, Shares const& shares)
{
    std::uint32_t const roll = Mix(guidLow) % 100;
    if (roll < shares.expertPercent)
        return Tier::EXPERT;
    if (roll < shares.expertPercent + shares.goodPercent)
        return Tier::GOOD;
    return Tier::CASUAL;
}

// Whether a bot of this tier uses a priority line marked core, good or expert in its class sheet.
inline bool Uses(Tier bot, Tier line)
{
    return static_cast<std::uint8_t>(bot) <= static_cast<std::uint8_t>(line);
}
}  // namespace BotSkillMath

#endif
