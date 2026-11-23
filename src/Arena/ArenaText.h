/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_ARENATEXT_H
#define _PLAYERBOT_ARENATEXT_H

// BT-154: the plain text lines of the ladder (the gossip pages of the ladder NPC, the console listing and the chat
// placeholders), free of game types so that tests/ can check the shapes. A gossip line has to stay under 80 characters.

#include "ArenaComp.h"
#include "BotSkillMath.h"

#include <cstdint>
#include <string>

namespace ArenaText
{
constexpr std::size_t GOSSIP_LINE_MAX = 80;

inline char const* TierName(BotSkillMath::Tier tier)
{
    switch (tier)
    {
        case BotSkillMath::Tier::EXPERT:
            return "expert";
        case BotSkillMath::Tier::GOOD:
            return "good";
        case BotSkillMath::Tier::CASUAL:
            return "casual";
    }
    return "good";
}

inline char const* ClassName(std::uint8_t classId)
{
    switch (classId)
    {
        case ArenaComp::CLASS_ID_WARRIOR: return "Warrior";
        case ArenaComp::CLASS_ID_PALADIN: return "Paladin";
        case ArenaComp::CLASS_ID_HUNTER: return "Hunter";
        case ArenaComp::CLASS_ID_ROGUE: return "Rogue";
        case ArenaComp::CLASS_ID_PRIEST: return "Priest";
        case ArenaComp::CLASS_ID_DEATH_KNIGHT: return "Death Knight";
        case ArenaComp::CLASS_ID_SHAMAN: return "Shaman";
        case ArenaComp::CLASS_ID_MAGE: return "Mage";
        case ArenaComp::CLASS_ID_WARLOCK: return "Warlock";
        case ArenaComp::CLASS_ID_MONK: return "Monk";
        case ArenaComp::CLASS_ID_DRUID: return "Druid";
    }
    return "Adventurer";
}

// The specialisation word of a ChrSpecialization id ("Subtlety"), empty for an unknown id.
inline char const* SpecName(std::uint16_t specId)
{
    switch (specId)
    {
        case 62: return "Arcane";
        case 63: return "Fire";
        case 64: return "Frost";
        case 65: return "Holy";
        case 66: return "Protection";
        case 70: return "Retribution";
        case 71: return "Arms";
        case 72: return "Fury";
        case 73: return "Protection";
        case 102: return "Balance";
        case 103: return "Feral";
        case 104: return "Guardian";
        case 105: return "Restoration";
        case 250: return "Blood";
        case 251: return "Frost";
        case 252: return "Unholy";
        case 253: return "Beast Mastery";
        case 254: return "Marksmanship";
        case 255: return "Survival";
        case 256: return "Discipline";
        case 257: return "Holy";
        case 258: return "Shadow";
        case 259: return "Assassination";
        case 260: return "Combat";
        case 261: return "Subtlety";
        case 262: return "Elemental";
        case 263: return "Enhancement";
        case 264: return "Restoration";
        case 265: return "Affliction";
        case 266: return "Demonology";
        case 267: return "Destruction";
        case 268: return "Brewmaster";
        case 269: return "Windwalker";
        case 270: return "Mistweaver";
    }
    return "";
}

// "Subtlety Rogue", "Restoration Druid"; the class alone when the spec is unknown.
inline std::string SpecAndClass(std::uint8_t classId, std::uint16_t specId)
{
    std::string const spec = SpecName(specId);
    return spec.empty() ? std::string(ClassName(classId)) : spec + " " + ClassName(classId);
}

inline std::string StreakText(std::int32_t streak)
{
    if (streak >= 2)
        return std::to_string(streak) + " in a row";
    if (streak <= -2)
        return std::to_string(-streak) + " losses in a row";
    return std::string();
}

inline std::string RecordText(std::uint32_t wins, std::uint32_t losses, std::uint32_t draws)
{
    std::string text = std::to_string(wins) + " W / " + std::to_string(losses) + " L";
    if (draws)
        text += " / " + std::to_string(draws) + " D";
    return text;
}

// "1. Gilded Lanterns - 1684 (31 W / 12 L) 4 in a row [expert]"
inline std::string LadderLine(std::uint32_t rank, std::string const& name, std::uint32_t rating, std::uint32_t wins,
    std::uint32_t losses, std::uint32_t draws, std::int32_t streak, BotSkillMath::Tier tier)
{
    std::string line = std::to_string(rank) + ". " + name + " - " + std::to_string(rating) + " (" +
        RecordText(wins, losses, draws) + ")";
    std::string const run = StreakText(streak);
    if (!run.empty())
        line += " " + run;
    line += std::string(" [") + TierName(tier) + "]";
    return line;
}

// "Rogue, damage" / "Restoration Druid, healer" lines of a roster.
inline std::string RosterLine(std::string const& name, std::uint8_t classId, std::uint16_t specId, bool healer)
{
    return name + ": " + SpecAndClass(classId, specId) + (healer ? ", healer" : ", damage");
}

// Cuts a gossip line to the limit at a word boundary and marks the cut with "..." (no special dash characters).
inline std::string Fit(std::string line, std::size_t limit = GOSSIP_LINE_MAX)
{
    if (line.size() <= limit)
        return line;
    std::string cut = line.substr(0, limit - 3);
    std::size_t const space = cut.find_last_of(' ');
    if (space != std::string::npos && space > limit / 2)
        cut.resize(space);
    return cut + "...";
}
}  // namespace ArenaText

#endif
