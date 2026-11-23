/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_CLASSSKILLLINES_H
#define _PLAYERBOT_CLASSSKILLLINES_H

// Which skill lines a new bot gets as its class skills. SkyFire does not load SkillRaceClassInfo, so the factory takes every
// skill line with a class-restricted SkillLineAbility entry. The pets' skill lines (Imp, Felhunter, Ghoul, the hunter pet and
// the other families in CreatureFamily.dbc) have such entries too, and learning them taught the bot its pets' spells. Their
// passives (the Infernal's Immolation, the Tamed Pet Passives) then ran on the bot itself, and every login re-learned them
// from the stored skill rows. Pet skill lines are therefore never class skills for the bot.
// Pure logic, free of game types (tests/test_class_skill_lines.cpp). The glue is Factory/BotFactory.cpp InitClassSpells.

#include <cstdint>
#include <set>
#include <utility>
#include <vector>

namespace ClassSkillLines
{
// abilities: (skill line, class mask) of every SkillLineAbility row; petLines: the skill lines of the pet families.
inline std::set<uint32_t> For(std::vector<std::pair<uint32_t, uint32_t>> const& abilities, uint32_t classMask,
                              std::set<uint32_t> const& petLines)
{
    std::set<uint32_t> skills;
    for (auto const& [skill, mask] : abilities)
        if ((mask & classMask) && !petLines.count(skill))
            skills.insert(skill);
    return skills;
}
}  // namespace ClassSkillLines

#endif
