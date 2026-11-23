#include "TestHarness.h"
#include "DpsSheetPolicy.h"

#include <set>
#include <string>

using namespace DpsSheetPolicy;
using BotSkillMath::Tier;

namespace
{
// The builder and spender every bot of the specialisation must keep: the first core lines of each sheet.
struct CoreSpells
{
    Spec spec;
    char const* builder;
    char const* spender;
};

constexpr CoreSpells Cores[] = {
    { WARRIOR_ARMS, "mortal strike", "execute" },
    { WARRIOR_FURY, "bloodthirst", "raging blow" },
    { PALADIN_RETRIBUTION, "crusader strike", "templar's verdict" },
    { HUNTER_BEAST_MASTERY, "kill command", "arcane shot" },
    { HUNTER_MARKSMANSHIP, "aimed shot", "steady shot" },
    { HUNTER_SURVIVAL, "explosive shot", "black arrow" },
    { ROGUE_ASSASSINATION, "mutilate", "envenom" },
    { ROGUE_COMBAT, "sinister strike", "eviscerate" },
    { ROGUE_SUBTLETY, "hemorrhage", "eviscerate" },
    { PRIEST_SHADOW, "mind blast", "devouring plague" },
    { DEATH_KNIGHT_FROST, "obliterate", "frost strike" },
    { DEATH_KNIGHT_UNHOLY, "scourge strike", "death coil" },
    { SHAMAN_ELEMENTAL, "lava burst", "lightning bolt" },
    { SHAMAN_ENHANCEMENT, "stormstrike", "lava lash" },
    { MAGE_ARCANE, "arcane blast", "arcane missiles" },
    { MAGE_FIRE, "fireball", "pyroblast" },
    { MAGE_FROST, "frostbolt", "ice lance" },
    { WARLOCK_AFFLICTION, "malefic grasp", "haunt" },
    { WARLOCK_DEMONOLOGY, "shadow bolt", "hand of gul'dan" },
    { WARLOCK_DESTRUCTION, "incinerate", "chaos bolt" },
    { MONK_WINDWALKER, "jab", "rising sun kick" },
    { DRUID_BALANCE, "wrath", "starfire" },
    { DRUID_FERAL, "mangle", "rip" },
};
}

TEST(dps_sheet_covers_the_23_damage_specs)
{
    CHECK(std::size(DamageSpecs) == 23u);
    CHECK(std::size(Cores) == 23u);
    std::set<std::uint32_t> seen;
    for (Spec spec : DamageSpecs)
    {
        CHECK(seen.insert(spec).second);
        CHECK(!Lines(spec).empty());
    }
    // Tank and healer specialisations belong to their own sheets and are never gated here.
    for (std::uint32_t other : { 65u, 66u, 73u, 104u, 105u, 250u, 256u, 257u, 264u, 268u, 270u, 0u })
    {
        CHECK(!IsDamageSpec(other));
        CHECK(Lines(other).empty());
        CHECK(Allows(Tier::CASUAL, other, "shield slam"));
    }
}

TEST(dps_sheet_every_tier_keeps_the_core_rotation)
{
    for (CoreSpells const& core : Cores)
        for (Tier tier : { Tier::EXPERT, Tier::GOOD, Tier::CASUAL })
        {
            CHECK(Allows(tier, core.spec, core.builder));
            CHECK(Allows(tier, core.spec, core.spender));
        }
}

TEST(dps_sheet_lines_are_unique_lower_case_and_not_core)
{
    for (Spec spec : DamageSpecs)
    {
        std::set<std::string> names;
        for (Line const& line : Lines(spec))
        {
            CHECK(line.tier != Tier::CASUAL);
            CHECK(names.insert(std::string(line.spell)).second);
            for (char c : line.spell)
                CHECK(!(c >= 'A' && c <= 'Z'));
            // A duty line in a table would be dead: duties are never gated.
            CHECK(!IsDuty(line.spell));
        }
    }
}

TEST(dps_sheet_tiers_drop_lines_in_order)
{
    // Arms: Slam is good (sheet line 6), Shattering Throw expert (line 26), Mortal Strike core (line 2).
    CHECK(Allows(Tier::EXPERT, WARRIOR_ARMS, "shattering throw"));
    CHECK(!Allows(Tier::GOOD, WARRIOR_ARMS, "shattering throw"));
    CHECK(!Allows(Tier::CASUAL, WARRIOR_ARMS, "shattering throw"));
    CHECK(Allows(Tier::GOOD, WARRIOR_ARMS, "slam"));
    CHECK(!Allows(Tier::CASUAL, WARRIOR_ARMS, "slam"));
    // Thunder Clap is core for Arms (Blood and Thunder, line 17) but good for Fury.
    CHECK(Allows(Tier::CASUAL, WARRIOR_ARMS, "thunder clap"));
    CHECK(!Allows(Tier::CASUAL, WARRIOR_FURY, "thunder clap"));
    // Casual bots skip the long damage cooldowns and the area spenders, good bots use them.
    CHECK(!Allows(Tier::CASUAL, MAGE_FIRE, "mirror image"));
    CHECK(Allows(Tier::GOOD, MAGE_FIRE, "mirror image"));
    CHECK(!Allows(Tier::CASUAL, HUNTER_MARKSMANSHIP, "multi-shot"));
    CHECK(!Allows(Tier::CASUAL, ROGUE_COMBAT, "adrenaline rush"));
    CHECK(!Allows(Tier::CASUAL, WARLOCK_DEMONOLOGY, "metamorphosis"));
    CHECK(Allows(Tier::GOOD, WARLOCK_DEMONOLOGY, "metamorphosis"));
    CHECK(!Allows(Tier::GOOD, WARLOCK_DEMONOLOGY, "imp swarm"));
    // Life Tap stays core for Affliction (derived): a bot without it stands out of mana.
    CHECK(Allows(Tier::CASUAL, WARLOCK_AFFLICTION, "life tap"));
}

TEST(dps_sheet_duties_stay_with_every_tier)
{
    // Interrupts follow the interrupt duty's own tier rule, crowd control follows the group's marks.
    for (Spec spec : DamageSpecs)
        for (char const* duty : { "kick", "pummel", "counterspell", "mind freeze", "wind shear", "rebuke",
                 "spear hand strike", "skull bash", "solar beam", "counter shot", "silence", "polymorph", "sap",
                 "blind", "freezing trap", "hex", "repentance", "fear", "banish", "paralysis", "disrupting shout" })
            CHECK(Allows(Tier::CASUAL, spec, duty));
}

TEST(dps_sheet_tier_shares_match_the_owner_mix)
{
    // 10 percent expert, 60 good, 30 casual (owner, 2026-11-10): over many GUIDs the casual share drops a good line.
    BotSkillMath::Shares const shares{ 10, 60 };
    unsigned slams = 0;
    unsigned const bots = 10000;
    for (std::uint32_t guid = 1; guid <= bots; ++guid)
        slams += Allows(BotSkillMath::TierOf(guid, shares), WARRIOR_ARMS, "slam") ? 1 : 0;
    CHECK(slams > bots * 66 / 100 && slams < bots * 74 / 100);
}
