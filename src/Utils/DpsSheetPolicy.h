/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_DPSSHEETPOLICY_H
#define _PLAYERBOT_DPSSHEETPOLICY_H

// Which lines of a damage class sheet (docs/class-sheets/) a bot of each skill tier plays, free of game types so that
// tests/ can run it. The sheets mark every priority line core, good or expert; a spell takes the lowest tier of the
// lines that use it, so a spell with one core line is never dropped. Unlisted spells are core.

#include "BotSkillMath.h"

#include <cstdint>
#include <span>
#include <string_view>

namespace DpsSheetPolicy
{
using BotSkillMath::Tier;

struct Line
{
    std::string_view spell;  // the spell name the bots cast (CastSpellAction::getSpell), lower case
    Tier tier;               // GOOD: good and expert bots; EXPERT: expert bots only
};

// Specialisation ids of Specializations (SharedDefines.h), repeated here so that the tests need no game header.
enum Spec : std::uint32_t
{
    MAGE_ARCANE = 62,
    MAGE_FIRE = 63,
    MAGE_FROST = 64,
    PALADIN_RETRIBUTION = 70,
    WARRIOR_ARMS = 71,
    WARRIOR_FURY = 72,
    DRUID_BALANCE = 102,
    DRUID_FERAL = 103,
    DEATH_KNIGHT_FROST = 251,
    DEATH_KNIGHT_UNHOLY = 252,
    HUNTER_BEAST_MASTERY = 253,
    HUNTER_MARKSMANSHIP = 254,
    HUNTER_SURVIVAL = 255,
    PRIEST_SHADOW = 258,
    ROGUE_ASSASSINATION = 259,
    ROGUE_COMBAT = 260,
    ROGUE_SUBTLETY = 261,
    SHAMAN_ELEMENTAL = 262,
    SHAMAN_ENHANCEMENT = 263,
    WARLOCK_AFFLICTION = 265,
    WARLOCK_DEMONOLOGY = 266,
    WARLOCK_DESTRUCTION = 267,
    MONK_WINDWALKER = 269
};

constexpr Spec DamageSpecs[] = { MAGE_ARCANE, MAGE_FIRE, MAGE_FROST, PALADIN_RETRIBUTION, WARRIOR_ARMS, WARRIOR_FURY,
    DRUID_BALANCE, DRUID_FERAL, DEATH_KNIGHT_FROST, DEATH_KNIGHT_UNHOLY, HUNTER_BEAST_MASTERY, HUNTER_MARKSMANSHIP,
    HUNTER_SURVIVAL, PRIEST_SHADOW, ROGUE_ASSASSINATION, ROGUE_COMBAT, ROGUE_SUBTLETY, SHAMAN_ELEMENTAL,
    SHAMAN_ENHANCEMENT, WARLOCK_AFFLICTION, WARLOCK_DEMONOLOGY, WARLOCK_DESTRUCTION, MONK_WINDWALKER };

// Lines that stay with every tier whatever the sheet says: interrupts already follow the tier through the interrupt
// duty (InterruptPolicy.h, a casual bot misses one cast in three), and crowd control is a duty the group's marks
// assign (CrowdControlDutyPolicy.h), so a casual bot that skipped it would leave a marked enemy free. Heals and
// battle resurrection are never gated here (the caller skips healing spells).
inline bool IsDuty(std::string_view spell)
{
    for (std::string_view duty : { "kick", "pummel", "rebuke", "wind shear", "counterspell", "mind freeze",
             "strangulate", "skull bash", "silence", "counter shot", "silencing shot", "spear hand strike",
             "solar beam", "spell lock", "disrupting shout", "polymorph", "sap", "blind", "freezing trap", "hex",
             "repentance", "shackle undead", "fear", "banish", "paralysis", "hibernate", "entangling roots",
             "cyclone", "scare beast", "turn evil" })
        if (spell == duty)
            return true;
    return false;
}

// One table per damage specialisation, from the "Tier" column of its sheet's priority lists and cooldown table.
inline std::span<Line const> Lines(std::uint32_t spec)
{
    using enum BotSkillMath::Tier;
    // Warrior sheets: the area lines, cooldowns and defensives are good; Thunder Clap is core for Arms (Blood and
    // Thunder keeps Deep Wounds up from 46) and Enraged Regeneration stays with every tier at critical health.
    static constexpr Line arms[] = { { "slam", GOOD }, { "whirlwind", GOOD }, { "sunder armor", GOOD },
        { "heroic throw", EXPERT }, { "hamstring", GOOD }, { "piercing howl", GOOD }, { "cleave", GOOD },
        { "berserker rage", GOOD }, { "die by the sword", GOOD }, { "intimidating shout", GOOD },
        { "bladestorm", GOOD }, { "dragon roar", GOOD }, { "recklessness", GOOD }, { "commanding shout", EXPERT },
        { "shattering throw", EXPERT }, { "rallying cry", GOOD }, { "skull banner", GOOD },
        { "demoralizing banner", GOOD }, { "heroic leap", GOOD } };
    static constexpr Line fury[] = { { "whirlwind", GOOD }, { "thunder clap", GOOD }, { "heroic throw", EXPERT },
        { "cleave", GOOD }, { "hamstring", GOOD }, { "piercing howl", GOOD }, { "berserker rage", GOOD },
        { "die by the sword", GOOD }, { "intimidating shout", GOOD }, { "dragon roar", GOOD }, { "bladestorm", GOOD },
        { "commanding shout", EXPERT }, { "shattering throw", EXPERT }, { "rallying cry", GOOD },
        { "skull banner", GOOD }, { "heroic leap", GOOD } };
    // Retribution: Hammer of the Righteous and Divine Storm are the area lines; Seal of Righteousness is the pack swap.
    static constexpr Line retribution[] = { { "hammer of the righteous", GOOD }, { "divine shield", GOOD },
        { "divine protection", GOOD }, { "divine storm", GOOD }, { "seal of righteousness", GOOD },
        { "hand of protection", GOOD }, { "hand of freedom", GOOD }, { "devotion aura", GOOD },
        { "hand of salvation", EXPERT } };
    static constexpr Line beastMastery[] = { { "concussive shot", GOOD }, { "multi-shot", GOOD },
        { "focus fire", GOOD }, { "tranquilizing shot", GOOD }, { "feign death", GOOD }, { "explosive trap", GOOD },
        { "disengage", GOOD } };
    static constexpr Line marksmanship[] = { { "concussive shot", GOOD }, { "multi-shot", GOOD },
        { "tranquilizing shot", GOOD }, { "feign death", GOOD }, { "explosive trap", GOOD }, { "disengage", GOOD } };
    static constexpr Line survival[] = { { "concussive shot", GOOD }, { "multi-shot", GOOD },
        { "tranquilizing shot", GOOD }, { "feign death", GOOD }, { "explosive trap", GOOD }, { "disengage", GOOD } };
    // Rogue sheets: the openers other than the spec's main one, utility finishers and defensives are good.
    static constexpr Line assassination[] = { { "ambush", GOOD }, { "evasion", GOOD }, { "gouge", EXPERT },
        { "cheap shot", GOOD }, { "expose armor", GOOD }, { "vanish", GOOD }, { "kidney shot", GOOD },
        { "feint", GOOD }, { "garrote", GOOD }, { "cloak of shadows", GOOD }, { "shadowstep", GOOD },
        { "fan of knives", GOOD }, { "preparation", GOOD } };
    static constexpr Line combat[] = { { "ambush", GOOD }, { "evasion", GOOD }, { "gouge", EXPERT },
        { "cheap shot", GOOD }, { "expose armor", GOOD }, { "adrenaline rush", GOOD }, { "vanish", GOOD },
        { "feint", GOOD }, { "garrote", GOOD }, { "cloak of shadows", GOOD }, { "shadowstep", GOOD },
        { "fan of knives", GOOD }, { "preparation", GOOD } };
    static constexpr Line subtlety[] = { { "evasion", GOOD }, { "gouge", EXPERT }, { "cheap shot", GOOD },
        { "expose armor", GOOD }, { "vanish", GOOD }, { "feint", GOOD }, { "kidney shot", GOOD }, { "garrote", GOOD },
        { "cloak of shadows", GOOD }, { "shadowstep", GOOD }, { "fan of knives", GOOD }, { "preparation", GOOD } };
    static constexpr Line shadow[] = { { "fade", GOOD }, { "desperate prayer", GOOD }, { "hymn of hope", GOOD } };
    static constexpr Line dkFrost[] = { { "pestilence", GOOD }, { "blood boil", GOOD }, { "raise dead", GOOD },
        { "chains of ice", EXPERT }, { "horn of winter", GOOD }, { "anti-magic shell", GOOD } };
    static constexpr Line dkUnholy[] = { { "pestilence", GOOD }, { "blood boil", GOOD },
        { "icebound fortitude", GOOD }, { "unholy frenzy", GOOD }, { "anti-magic shell", GOOD } };
    static constexpr Line elemental[] = { { "searing totem", GOOD }, { "earth shock", GOOD },
        { "thunderstorm", GOOD }, { "purge", GOOD }, { "chain lightning", GOOD }, { "totemic recall", GOOD },
        { "astral shift", GOOD }, { "tremor totem", EXPERT },
        { "earthquake", GOOD }, { "shamanistic rage", GOOD }, { "fire elemental totem", GOOD } };
    static constexpr Line enhancement[] = { { "earth shock", GOOD }, { "searing totem", GOOD }, { "purge", GOOD },
        { "spirit walk", GOOD }, { "totemic recall", GOOD }, { "magma totem", GOOD },
        { "fire nova", GOOD }, { "chain lightning", GOOD }, { "tremor totem", EXPERT }, { "shamanistic rage", GOOD },
        { "fire elemental totem", GOOD }, { "elemental mastery", GOOD }, { "astral shift", GOOD } };
    // Mage sheets: Frost Nova and Blink are the kiting pair, Ice Block the solo duel answer.
    static constexpr Line arcane[] = { { "frost nova", GOOD }, { "blink", GOOD }, { "arcane explosion", GOOD },
        { "cone of cold", GOOD }, { "ice block", EXPERT }, { "slow", GOOD }, { "flamestrike", GOOD },
        { "mirror image", GOOD } };
    static constexpr Line fire[] = { { "frost nova", GOOD }, { "blink", GOOD }, { "arcane explosion", GOOD },
        { "cone of cold", GOOD }, { "ice block", EXPERT }, { "flamestrike", GOOD }, { "mirror image", GOOD },
        { "dragon's breath", GOOD } };
    static constexpr Line frost[] = { { "frost nova", GOOD }, { "blink", GOOD }, { "arcane explosion", GOOD },
        { "cone of cold", GOOD }, { "ice block", EXPERT }, { "flamestrike", GOOD }, { "mirror image", GOOD } };
    // Affliction: Life Tap stays core (a bot that skipped it would stand out of mana), derived.
    static constexpr Line affliction[] = { { "rain of fire", GOOD }, { "mortal coil", GOOD },
        { "shadowfury", GOOD }, { "curse of the elements", GOOD }, { "summon doomguard", GOOD },
        { "soulshatter", GOOD }, { "seed of corruption", GOOD }, { "unending resolve", GOOD } };
    static constexpr Line demonology[] = { { "metamorphosis", GOOD }, { "hellfire", GOOD }, { "imp swarm", EXPERT } };
    static constexpr Line destruction[] = { { "rain of fire", GOOD }, { "havoc", GOOD },
        { "fire and brimstone", GOOD } };
    static constexpr Line windwalker[] = { { "touch of death", GOOD }, { "fortifying brew", GOOD },
        { "touch of karma", GOOD }, { "roll", EXPERT }, { "disable", EXPERT }, { "chi wave", GOOD },
        { "energizing brew", GOOD }, { "chi brew", GOOD },
        { "crackling jade lightning", GOOD } };
    static constexpr Line balance[] = { { "barkskin", GOOD }, { "nature's grasp", GOOD },
        { "hurricane", GOOD }, { "force of nature", GOOD } };
    static constexpr Line feral[] = { { "swipe", GOOD }, { "ravage", EXPERT }, { "barkskin", GOOD },
        { "force of nature", GOOD } };

    switch (spec)
    {
        case WARRIOR_ARMS: return arms;
        case WARRIOR_FURY: return fury;
        case PALADIN_RETRIBUTION: return retribution;
        case HUNTER_BEAST_MASTERY: return beastMastery;
        case HUNTER_MARKSMANSHIP: return marksmanship;
        case HUNTER_SURVIVAL: return survival;
        case ROGUE_ASSASSINATION: return assassination;
        case ROGUE_COMBAT: return combat;
        case ROGUE_SUBTLETY: return subtlety;
        case PRIEST_SHADOW: return shadow;
        case DEATH_KNIGHT_FROST: return dkFrost;
        case DEATH_KNIGHT_UNHOLY: return dkUnholy;
        case SHAMAN_ELEMENTAL: return elemental;
        case SHAMAN_ENHANCEMENT: return enhancement;
        case MAGE_ARCANE: return arcane;
        case MAGE_FIRE: return fire;
        case MAGE_FROST: return frost;
        case WARLOCK_AFFLICTION: return affliction;
        case WARLOCK_DEMONOLOGY: return demonology;
        case WARLOCK_DESTRUCTION: return destruction;
        case MONK_WINDWALKER: return windwalker;
        case DRUID_BALANCE: return balance;
        case DRUID_FERAL: return feral;
        default: return {};
    }
}

inline bool IsDamageSpec(std::uint32_t spec)
{
    for (Spec damage : DamageSpecs)
        if (damage == spec)
            return true;
    return false;
}

// The tier of the sheet line a spell belongs to; CASUAL means a core line every bot plays.
inline Tier LineTier(std::uint32_t spec, std::string_view spell)
{
    if (IsDuty(spell))
        return Tier::CASUAL;
    for (Line const& line : Lines(spec))
        if (line.spell == spell)
            return line.tier;
    return Tier::CASUAL;
}

// Whether a bot of this tier casts the spell in combat. Tank and healer specialisations are never gated here.
inline bool Allows(Tier bot, std::uint32_t spec, std::string_view spell)
{
    return !IsDamageSpec(spec) || BotSkillMath::Uses(bot, LineTier(spec, spell));
}
}  // namespace DpsSheetPolicy

#endif
