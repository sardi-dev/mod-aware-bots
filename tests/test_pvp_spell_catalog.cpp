#include "TestHarness.h"
#include "PvpSpellCatalog.h"

#include <set>

// BT-520: the catalog of spells the PvP rules reason about. The ids themselves were checked against Spell.dbc names when the
// table was written; these tests pin the shape (no duplicate id, every row has the fields its kind needs) and the lookups.

using namespace PvpSpells;

TEST(pvp_catalog_spell_ids_are_unique)
{
    std::set<std::uint32_t> seen;
    for (Entry const& e : Catalog())
        CHECK(seen.insert(e.spellId).second);
    CHECK(Catalog().size() > 100);
}

TEST(pvp_catalog_interrupts_have_a_cooldown_and_a_lockout)
{
    int interrupts = 0;
    for (Entry const& e : Catalog())
    {
        if (e.kind != Kind::INTERRUPT && e.kind != Kind::SILENCE)
            continue;
        ++interrupts;
        CHECK(e.cooldownMs > 0);
        CHECK(e.lockoutMs > 0);
        CHECK(e.cls != CLS_ANY);
    }
    CHECK(interrupts == 17);
}

TEST(pvp_catalog_lockouts_match_the_sheet)
{
    CHECK(Find(57994)->lockoutMs == 3000);    // Wind Shear
    CHECK(Find(2139)->lockoutMs == 6000);     // Counterspell
    CHECK(Find(1766)->lockoutMs == 5000);     // Kick
    CHECK(Find(6552)->lockoutMs == 4000);     // Pummel
    CHECK(Find(57994)->cooldownMs == 12000);
    CHECK(Find(2139)->cooldownMs == 24000);
    CHECK(Find(47476)->kind == Kind::SILENCE);
}

TEST(pvp_catalog_every_class_has_a_stopper_but_not_always_a_sure_one)
{
    for (std::uint8_t cls : { CLS_WARRIOR, CLS_PALADIN, CLS_HUNTER, CLS_ROGUE, CLS_DEATH_KNIGHT, CLS_SHAMAN, CLS_MAGE,
                              CLS_WARLOCK, CLS_MONK, CLS_DRUID })
        CHECK(!InterruptsOf(cls).empty());
    // A priest only has Silence (shadow), a talent-free kit has none of the sure ones.
    CHECK(InterruptsOf(CLS_PRIEST).size() == 1);
    CHECK(InterruptsOf(CLS_PRIEST)[0].cost == 1);
    CHECK(InterruptsOf(CLS_MAGE).size() == 1);
    CHECK(InterruptsOf(CLS_MAGE)[0].spellId == 2139);
}

TEST(pvp_catalog_trinket_and_the_racial_that_shares_its_cooldown)
{
    CHECK(SharesTrinketCooldown(TRINKET_SPELL));
    CHECK(SharesTrinketCooldown(EVERY_MAN_FOR_HIMSELF));
    CHECK(!SharesTrinketCooldown(7744));  // Will of the Forsaken has its own two minutes in the data
    CHECK(Find(TRINKET_SPELL)->kind == Kind::TRINKET);
    CHECK(Find(TRINKET_SPELL)->cost == BREAK_COST_MEDALLION);
    CHECK(Find(EVERY_MAN_FOR_HIMSELF)->cost == BREAK_COST_SHARED);
    CHECK(Find(TRINKET_SPELL)->cooldownMs == TRINKET_COOLDOWN_MS);
}

TEST(pvp_catalog_breakers_are_ordered_cheapest_first)
{
    // A warrior: Berserker Rage (class) before the racials before the medallion.
    std::vector<Entry> const rows = RowsFor(CLS_WARRIOR, Kind::BREAKER);
    bool rage = false;
    for (Entry const& e : rows)
        if (e.spellId == 18499)
        {
            rage = true;
            CHECK(e.cost == BREAK_COST_CLASS);
            CHECK((e.breaks & CC_FEAR) != 0);
            CHECK((e.breaks & CC_STUN) == 0);
        }
    CHECK(rage);
    // Lichborne is for death knights only.
    CHECK(RowsFor(CLS_MAGE, Kind::BREAKER).size() == 3);  // the three racials
    CHECK(RowsFor(CLS_DEATH_KNIGHT, Kind::BREAKER).size() == 4);
    // The medallion removes stun, fear, root and snare, not silence or disarm.
    CHECK((CC_MEDALLION & CC_STUN) && (CC_MEDALLION & CC_FEAR) && (CC_MEDALLION & CC_ROOT) && (CC_MEDALLION & CC_SNARE));
    CHECK(!(CC_MEDALLION & CC_SILENCE) && !(CC_MEDALLION & CC_DISARM));
}

TEST(pvp_catalog_defensives_split_into_tempo_and_immunity)
{
    CHECK(Find(45438)->kind == Kind::DEFENSIVE_IMMUNITY);  // Ice Block
    CHECK(Find(642)->kind == Kind::DEFENSIVE_IMMUNITY);    // Divine Shield
    CHECK(Find(19263)->kind == Kind::DEFENSIVE_IMMUNITY);  // Deterrence
    CHECK(Find(22812)->kind == Kind::DEFENSIVE_TEMPO);     // Barkskin
    CHECK(Find(498)->silenceBlocked);                      // Divine Protection cannot be pressed while silenced
    CHECK(!Find(47585)->silenceBlocked);                   // Dispersion can
    CHECK(Find(5277)->fit == Fit::PHYSICAL);               // Evasion
    CHECK(Find(31224)->fit == Fit::MAGIC);                 // Cloak of Shadows
    CHECK(Find(48707)->fit == Fit::MAGIC);                 // Anti-Magic Shell
    // The classes that matter have a tempo defensive to use before an immunity.
    CHECK(!RowsFor(CLS_PALADIN, Kind::DEFENSIVE_TEMPO).empty());
    CHECK(!RowsFor(CLS_MAGE, Kind::DEFENSIVE_TEMPO).empty());
    CHECK(RowsFor(CLS_HUNTER, Kind::DEFENSIVE_TEMPO).empty());
    CHECK(!RowsFor(CLS_HUNTER, Kind::DEFENSIVE_IMMUNITY).empty());
}

TEST(pvp_catalog_dr_categories_follow_5_4_8_not_the_core_table)
{
    CHECK(DrCategoryOf(3355) == DrCategory::INCAPACITATE);   // Freezing Trap: the core gives it no category at all
    CHECK(DrCategoryOf(82691) == DrCategory::INCAPACITATE);  // Ring of Frost
    CHECK(DrCategoryOf(88625) == DrCategory::DISORIENT);     // Chastise
    CHECK(DrCategoryOf(115750) == DrCategory::DISORIENT);    // Blinding Light
    CHECK(DrCategoryOf(1833) == DrCategory::STUN);           // Cheap Shot shares with Kidney Shot
    CHECK(DrCategoryOf(408) == DrCategory::STUN);
    CHECK(DrCategoryOf(64044) == DrCategory::HORROR);        // Psychic Horror
    CHECK(DrCategoryOf(5782) == DrCategory::FEAR);
    CHECK(DrCategoryOf(2094) == DrCategory::FEAR);           // Blind
    CHECK(DrCategoryOf(33786) == DrCategory::CYCLONE);
    CHECK(DrCategoryOf(1) == DrCategory::NONE);
    CHECK(DrCategoryOf(1766) == DrCategory::NONE);           // a kick has no category of its own
}

TEST(pvp_catalog_mechanic_numbers_match_the_core)
{
    // The core's enum Mechanics (SharedDefines.h): the glue asserts the same numbers against the real enum.
    CHECK(MECH_CHARM == 1 && MECH_DISORIENTED == 2 && MECH_DISARM == 3 && MECH_FEAR == 5 && MECH_ROOT == 7);
    CHECK(MECH_SILENCE == 9 && MECH_SLEEP == 10 && MECH_SNARE == 11 && MECH_STUN == 12 && MECH_FREEZE == 13);
    CHECK(MECH_KNOCKOUT == 14 && MECH_POLYMORPH == 17 && MECH_BANISH == 18 && MECH_SHACKLE == 20);
    CHECK(MECH_HORROR == 24 && MECH_SAPPED == 30);
}

TEST(pvp_catalog_mechanics_give_kinds_and_a_fallback_category)
{
    CHECK(CcKindsOfMechanics(MechBit(MECH_STUN)) == CC_STUN);
    CHECK(CcKindsOfMechanics(MechBit(MECH_FEAR) | MechBit(MECH_ROOT)) == (CC_FEAR | CC_ROOT));
    CHECK(CcKindsOfMechanics(MechBit(MECH_SAPPED)) == CC_INCAPACITATE);
    CHECK(CcKindsOfMechanics(0) == CC_NONE);
    // A spell the catalog does not list falls back on the mechanics of its effects: a stun over a snare.
    CHECK(DrCategoryFor(999999, MechBit(MECH_STUN) | MechBit(MECH_SNARE)) == DrCategory::STUN);
    CHECK(DrCategoryFor(999999, MechBit(MECH_SNARE)) == DrCategory::NONE);
    CHECK(DrCategoryFor(999999, MechBit(MECH_POLYMORPH)) == DrCategory::INCAPACITATE);
    // A listed spell keeps its catalog category whatever the mechanics say.
    CHECK(DrCategoryFor(408, MechBit(MECH_ROOT)) == DrCategory::STUN);
}

TEST(pvp_catalog_category_kinds_round_trip)
{
    CHECK(CcKindOfCategory(DrCategory::STUN) == CC_STUN);
    CHECK(CcKindOfCategory(DrCategory::HORROR) == CC_HORROR);
    CHECK(CcKindOfCategory(DrCategory::NONE) == CC_NONE);
    CHECK((CC_HARD & CC_ROOT) == 0);
    CHECK((CC_HARD & CC_STUN) != 0);
}
