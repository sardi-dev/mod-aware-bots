/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_PVPSPELLCATALOG_H
#define _PLAYERBOT_PVPSPELLCATALOG_H

// BT-520: the spells the PvP rules reason about (design decision 0032, docs/pvp/awareness-r1-r2.md): interrupts and silences with
// their lockouts and cooldowns, the PvP trinket and the cheaper control breakers, the big defensives by kind, and the
// crowd control spells with their diminishing returns category. Pure data and lookups, free of game types, so that tests/
// can run it. Every id was checked against the 5.4.8 Spell.dbc names; cooldowns come from SpellCooldowns.dbc, lockouts
// from the interrupt table of the fundamentals sheet (a lockout is how long the stopped school stays closed).
//
// The diminishing returns categories are the 5.4.8 ones (DRList for MoP), not the core's older table: the bots plan with
// the real rule and neither exploit nor depend on a gap of the core (owner, 2026-11-18).

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace PvpSpells
{
// Class ids as the core numbers them (the same as OneVsOne::CLS_*).
constexpr std::uint8_t CLS_ANY = 0;
constexpr std::uint8_t CLS_WARRIOR = 1;
constexpr std::uint8_t CLS_PALADIN = 2;
constexpr std::uint8_t CLS_HUNTER = 3;
constexpr std::uint8_t CLS_ROGUE = 4;
constexpr std::uint8_t CLS_PRIEST = 5;
constexpr std::uint8_t CLS_DEATH_KNIGHT = 6;
constexpr std::uint8_t CLS_SHAMAN = 7;
constexpr std::uint8_t CLS_MAGE = 8;
constexpr std::uint8_t CLS_WARLOCK = 9;
constexpr std::uint8_t CLS_MONK = 10;
constexpr std::uint8_t CLS_DRUID = 11;

// School masks as the core numbers them (SpellSchoolMask).
constexpr std::uint32_t SCHOOL_NORMAL = 0x01;
constexpr std::uint32_t SCHOOL_HOLY = 0x02;
constexpr std::uint32_t SCHOOL_FIRE = 0x04;
constexpr std::uint32_t SCHOOL_NATURE = 0x08;
constexpr std::uint32_t SCHOOL_FROST = 0x10;
constexpr std::uint32_t SCHOOL_SHADOW = 0x20;
constexpr std::uint32_t SCHOOL_ARCANE = 0x40;

// The kinds of loss of control and impairment a break can remove, and the kinds the bot can be under (a bit mask).
enum CcKind : std::uint32_t
{
    CC_NONE = 0,
    CC_STUN = 1u << 0,
    CC_INCAPACITATE = 1u << 1,  // polymorph, sap, hex, gouge, trap, paralysis
    CC_DISORIENT = 1u << 2,
    CC_FEAR = 1u << 3,
    CC_HORROR = 1u << 4,
    CC_ROOT = 1u << 5,
    CC_SNARE = 1u << 6,
    CC_CHARM = 1u << 7,
    CC_SLEEP = 1u << 8,
    CC_SILENCE = 1u << 9,
    CC_DISARM = 1u << 10
};

// What takes control of the bot (a stun, a fear, a polymorph ...) as against what only slows or silences it.
constexpr std::uint32_t CC_HARD = CC_STUN | CC_INCAPACITATE | CC_DISORIENT | CC_FEAR | CC_HORROR | CC_CHARM | CC_SLEEP;
// Everything the medallion removes: loss of control and movement impairment (silence and disarm stay).
constexpr std::uint32_t CC_MEDALLION = CC_HARD | CC_ROOT | CC_SNARE;

// The 5.4.8 diminishing returns categories. Each diminishes only with itself: full, half, quarter, immune.
enum class DrCategory : std::uint8_t
{
    NONE,
    STUN,
    INCAPACITATE,
    DISORIENT,
    FEAR,
    HORROR,
    SILENCE,
    ROOT,
    DISARM,
    MIND_CONTROL,
    CYCLONE,
    COUNT
};

enum class Kind : std::uint8_t
{
    INTERRUPT,           // stops a cast and locks its school
    SILENCE,             // a silence that also stops a cast (Strangulate, Silence)
    TRINKET,             // the PvP medallion's item spell
    BREAKER,             // a racial or class spell that frees the bot from control
    DEFENSIVE_TEMPO,     // a smaller defensive used early in an enemy go
    DEFENSIVE_IMMUNITY,  // an immunity: last
    EXTERNAL,            // cast on a teammate
    CROWD_CONTROL
};

// Which damage a defensive answers.
enum class Fit : std::uint8_t
{
    ALL,
    PHYSICAL,  // Evasion
    MAGIC      // Cloak of Shadows, Anti-Magic Shell
};

struct Entry
{
    std::uint32_t spellId;
    Kind kind;
    std::uint8_t cls;             // class that casts it (CLS_ANY for racials and the trinket)
    std::uint32_t cooldownMs;     // 0 when the spell has none or the table does not know
    std::uint32_t lockoutMs;      // INTERRUPT and SILENCE: how long the stopped school (or every spell) stays closed
    DrCategory dr;                // CROWD_CONTROL and SILENCE: the category it diminishes in
    std::uint32_t breaks;         // BREAKER and TRINKET: the CcKind mask it removes
    std::uint8_t cost;            // BREAKER and TRINKET: 0 cheapest ... 3 the medallion (the order of use);
                                  // INTERRUPT and SILENCE: 1 for a stopper the class may not have (a talent or one spec)
    Fit fit;                      // DEFENSIVE_*: which damage it answers
    bool silenceBlocked;          // DEFENSIVE_*: cannot be pressed while silenced (Divine Protection), so press it before
};

constexpr std::uint32_t TRINKET_SPELL = 42292;          // PvP Trinket: Immune Movement Impairment and Loss of Control
constexpr std::uint32_t EVERY_MAN_FOR_HIMSELF = 59752;  // shares its cooldown with the medallion
constexpr std::uint32_t TRINKET_COOLDOWN_MS = 120000;

// Cost order of the breaks: a narrow class spell first, then a racial or a spell with its own cooldown, then the medallion
// and the racial that shares its cooldown.
constexpr std::uint8_t BREAK_COST_CLASS = 0;
constexpr std::uint8_t BREAK_COST_OWN = 1;
constexpr std::uint8_t BREAK_COST_SHARED = 2;
constexpr std::uint8_t BREAK_COST_MEDALLION = 3;

constexpr DrCategory NO_DR = DrCategory::NONE;

// clang-format off
inline std::vector<Entry> const& Catalog()
{
    static std::vector<Entry> const rows = {
        // --- Interrupts (lockout and cooldown per the fundamentals sheet, 4.1) ---
        { 1766, Kind::INTERRUPT, CLS_ROGUE, 15000, 5000, NO_DR, 0, 0, Fit::ALL, false },          // Kick
        { 2139, Kind::INTERRUPT, CLS_MAGE, 24000, 6000, NO_DR, 0, 0, Fit::ALL, false },           // Counterspell
        { 57994, Kind::INTERRUPT, CLS_SHAMAN, 12000, 3000, NO_DR, 0, 0, Fit::ALL, false },        // Wind Shear
        { 6552, Kind::INTERRUPT, CLS_WARRIOR, 15000, 4000, NO_DR, 0, 0, Fit::ALL, false },        // Pummel
        { 102060, Kind::INTERRUPT, CLS_WARRIOR, 40000, 4000, NO_DR, 0, 1, Fit::ALL, false },      // Disrupting Shout
        { 47528, Kind::INTERRUPT, CLS_DEATH_KNIGHT, 15000, 4000, NO_DR, 0, 0, Fit::ALL, false },  // Mind Freeze
        { 96231, Kind::INTERRUPT, CLS_PALADIN, 15000, 4000, NO_DR, 0, 0, Fit::ALL, false },       // Rebuke
        { 106839, Kind::INTERRUPT, CLS_DRUID, 15000, 4000, NO_DR, 0, 0, Fit::ALL, false },        // Skull Bash
        { 116705, Kind::INTERRUPT, CLS_MONK, 15000, 4000, NO_DR, 0, 0, Fit::ALL, false },         // Spear Hand Strike
        { 147362, Kind::INTERRUPT, CLS_HUNTER, 24000, 3000, NO_DR, 0, 0, Fit::ALL, false },       // Counter Shot
        { 19647, Kind::INTERRUPT, CLS_WARLOCK, 24000, 6000, NO_DR, 0, 0, Fit::ALL, false },       // Spell Lock
        { 115781, Kind::INTERRUPT, CLS_WARLOCK, 24000, 6000, NO_DR, 0, 1, Fit::ALL, false },      // Optical Blast
        // --- Silences that also stop a cast ---
        { 34490, Kind::SILENCE, CLS_HUNTER, 24000, 3000, DrCategory::SILENCE, 0, 1, Fit::ALL, false },        // Silencing Shot
        { 31935, Kind::SILENCE, CLS_PALADIN, 15000, 3000, DrCategory::SILENCE, 0, 1, Fit::ALL, false },       // Avenger's Shield
        { 47476, Kind::SILENCE, CLS_DEATH_KNIGHT, 60000, 5000, DrCategory::SILENCE, 0, 1, Fit::ALL, false },  // Strangulate
        { 15487, Kind::SILENCE, CLS_PRIEST, 45000, 5000, DrCategory::SILENCE, 0, 1, Fit::ALL, false },        // Silence
        { 78675, Kind::SILENCE, CLS_DRUID, 60000, 8000, DrCategory::SILENCE, 0, 1, Fit::ALL, false },         // Solar Beam
        // --- The medallion and the control breakers, cheapest first ---
        { 42292, Kind::TRINKET, CLS_ANY, 120000, 0, NO_DR, CC_MEDALLION, BREAK_COST_MEDALLION, Fit::ALL, false },       // PvP Trinket
        { 59752, Kind::BREAKER, CLS_ANY, 120000, 0, NO_DR, CC_MEDALLION, BREAK_COST_SHARED, Fit::ALL, false },          // Every Man for Himself
        { 7744, Kind::BREAKER, CLS_ANY, 120000, 0, NO_DR, CC_FEAR | CC_CHARM | CC_SLEEP, BREAK_COST_OWN, Fit::ALL, false },  // Will of the Forsaken
        { 20589, Kind::BREAKER, CLS_ANY, 90000, 0, NO_DR, CC_ROOT | CC_SNARE, BREAK_COST_OWN, Fit::ALL, false },       // Escape Artist
        { 18499, Kind::BREAKER, CLS_WARRIOR, 30000, 0, NO_DR, CC_FEAR | CC_INCAPACITATE, BREAK_COST_CLASS, Fit::ALL, false },  // Berserker Rage
        { 49039, Kind::BREAKER, CLS_DEATH_KNIGHT, 120000, 0, NO_DR, CC_FEAR | CC_CHARM | CC_SLEEP, BREAK_COST_CLASS, Fit::ALL, false },  // Lichborne
        // BT-662: Nimble Brew (level 30, every monk) clears stuns, fears, roots and horrors and keeps them off for 6 s: the cheap break
        // that a monk uses before the medallion (the Mistweaver guide, rule 7). Cooldown from SpellCooldowns.dbc.
        { 137562, Kind::BREAKER, CLS_MONK, 120000, 0, NO_DR, CC_STUN | CC_FEAR | CC_ROOT | CC_HORROR, BREAK_COST_CLASS, Fit::ALL, false },  // Nimble Brew
        { 108482, Kind::BREAKER, CLS_WARLOCK, 60000, 0, NO_DR, CC_MEDALLION, BREAK_COST_OWN, Fit::ALL, false },        // Unbound Will
        // --- Defensives: tempo first, immunities last ---
        { 22812, Kind::DEFENSIVE_TEMPO, CLS_DRUID, 60000, 0, NO_DR, 0, 0, Fit::ALL, false },        // Barkskin
        { 61336, Kind::DEFENSIVE_TEMPO, CLS_DRUID, 180000, 0, NO_DR, 0, 0, Fit::ALL, false },       // Survival Instincts
        { 498, Kind::DEFENSIVE_TEMPO, CLS_PALADIN, 60000, 0, NO_DR, 0, 0, Fit::ALL, true },         // Divine Protection
        { 871, Kind::DEFENSIVE_TEMPO, CLS_WARRIOR, 180000, 0, NO_DR, 0, 0, Fit::ALL, false },       // Shield Wall
        { 118038, Kind::DEFENSIVE_TEMPO, CLS_WARRIOR, 120000, 0, NO_DR, 0, 0, Fit::PHYSICAL, false },  // Die by the Sword
        { 55694, Kind::DEFENSIVE_TEMPO, CLS_WARRIOR, 60000, 0, NO_DR, 0, 0, Fit::ALL, false },      // Enraged Regeneration
        { 5277, Kind::DEFENSIVE_TEMPO, CLS_ROGUE, 120000, 0, NO_DR, 0, 0, Fit::PHYSICAL, false },   // Evasion
        { 31224, Kind::DEFENSIVE_TEMPO, CLS_ROGUE, 60000, 0, NO_DR, 0, 0, Fit::MAGIC, false },      // Cloak of Shadows
        { 47585, Kind::DEFENSIVE_TEMPO, CLS_PRIEST, 120000, 0, NO_DR, 0, 0, Fit::ALL, false },      // Dispersion
        { 19236, Kind::DEFENSIVE_TEMPO, CLS_PRIEST, 120000, 0, NO_DR, 0, 0, Fit::ALL, false },      // Desperate Prayer
        { 48792, Kind::DEFENSIVE_TEMPO, CLS_DEATH_KNIGHT, 180000, 0, NO_DR, 0, 0, Fit::ALL, false },  // Icebound Fortitude
        { 48707, Kind::DEFENSIVE_TEMPO, CLS_DEATH_KNIGHT, 45000, 0, NO_DR, 0, 0, Fit::MAGIC, false },  // Anti-Magic Shell
        { 55233, Kind::DEFENSIVE_TEMPO, CLS_DEATH_KNIGHT, 60000, 0, NO_DR, 0, 0, Fit::ALL, false },  // Vampiric Blood
        { 108271, Kind::DEFENSIVE_TEMPO, CLS_SHAMAN, 90000, 0, NO_DR, 0, 0, Fit::ALL, false },      // Astral Shift
        { 11426, Kind::DEFENSIVE_TEMPO, CLS_MAGE, 25000, 0, NO_DR, 0, 0, Fit::ALL, false },         // Ice Barrier
        { 104773, Kind::DEFENSIVE_TEMPO, CLS_WARLOCK, 180000, 0, NO_DR, 0, 0, Fit::ALL, false },    // Unending Resolve
        { 108359, Kind::DEFENSIVE_TEMPO, CLS_WARLOCK, 120000, 0, NO_DR, 0, 0, Fit::ALL, false },    // Dark Regeneration
        { 108416, Kind::DEFENSIVE_TEMPO, CLS_WARLOCK, 60000, 0, NO_DR, 0, 0, Fit::ALL, false },     // Sacrificial Pact
        { 115203, Kind::DEFENSIVE_TEMPO, CLS_MONK, 180000, 0, NO_DR, 0, 0, Fit::ALL, false },       // Fortifying Brew
        { 122278, Kind::DEFENSIVE_TEMPO, CLS_MONK, 90000, 0, NO_DR, 0, 0, Fit::ALL, false },        // Dampen Harm
        { 45438, Kind::DEFENSIVE_IMMUNITY, CLS_MAGE, 300000, 0, NO_DR, 0, 0, Fit::ALL, false },     // Ice Block
        { 642, Kind::DEFENSIVE_IMMUNITY, CLS_PALADIN, 300000, 0, NO_DR, 0, 0, Fit::ALL, false },    // Divine Shield
        { 19263, Kind::DEFENSIVE_IMMUNITY, CLS_HUNTER, 180000, 0, NO_DR, 0, 0, Fit::ALL, false },   // Deterrence
        // --- Externals on a teammate ---
        { 33206, Kind::EXTERNAL, CLS_PRIEST, 180000, 0, NO_DR, 0, 0, Fit::ALL, false },     // Pain Suppression
        { 47788, Kind::EXTERNAL, CLS_PRIEST, 180000, 0, NO_DR, 0, 0, Fit::ALL, false },     // Guardian Spirit
        { 116849, Kind::EXTERNAL, CLS_MONK, 120000, 0, NO_DR, 0, 0, Fit::ALL, false },      // Life Cocoon
        { 102342, Kind::EXTERNAL, CLS_DRUID, 60000, 0, NO_DR, 0, 0, Fit::ALL, false },      // Ironbark
        { 1022, Kind::EXTERNAL, CLS_PALADIN, 300000, 0, NO_DR, 0, 0, Fit::ALL, false },     // Hand of Protection
        { 6940, Kind::EXTERNAL, CLS_PALADIN, 120000, 0, NO_DR, 0, 0, Fit::ALL, false },     // Hand of Sacrifice
        // --- Stuns ---
        { 408, Kind::CROWD_CONTROL, CLS_ROGUE, 20000, 0, DrCategory::STUN, 0, 0, Fit::ALL, false },        // Kidney Shot
        { 1833, Kind::CROWD_CONTROL, CLS_ROGUE, 0, 0, DrCategory::STUN, 0, 0, Fit::ALL, false },           // Cheap Shot
        { 853, Kind::CROWD_CONTROL, CLS_PALADIN, 60000, 0, DrCategory::STUN, 0, 0, Fit::ALL, false },      // Hammer of Justice
        { 105593, Kind::CROWD_CONTROL, CLS_PALADIN, 30000, 0, DrCategory::STUN, 0, 0, Fit::ALL, false },   // Fist of Justice
        { 44572, Kind::CROWD_CONTROL, CLS_MAGE, 30000, 0, DrCategory::STUN, 0, 0, Fit::ALL, false },       // Deep Freeze
        { 30283, Kind::CROWD_CONTROL, CLS_WARLOCK, 30000, 0, DrCategory::STUN, 0, 0, Fit::ALL, false },    // Shadowfury
        { 107570, Kind::CROWD_CONTROL, CLS_WARRIOR, 30000, 0, DrCategory::STUN, 0, 0, Fit::ALL, false },   // Storm Bolt
        { 46968, Kind::CROWD_CONTROL, CLS_WARRIOR, 40000, 0, DrCategory::STUN, 0, 0, Fit::ALL, false },    // Shockwave
        { 119381, Kind::CROWD_CONTROL, CLS_MONK, 45000, 0, DrCategory::STUN, 0, 0, Fit::ALL, false },      // Leg Sweep
        { 108194, Kind::CROWD_CONTROL, CLS_DEATH_KNIGHT, 30000, 0, DrCategory::STUN, 0, 0, Fit::ALL, false },  // Asphyxiate
        { 91800, Kind::CROWD_CONTROL, CLS_DEATH_KNIGHT, 60000, 0, DrCategory::STUN, 0, 0, Fit::ALL, false },   // Gnaw
        { 5211, Kind::CROWD_CONTROL, CLS_DRUID, 50000, 0, DrCategory::STUN, 0, 0, Fit::ALL, false },       // Mighty Bash
        { 22570, Kind::CROWD_CONTROL, CLS_DRUID, 10000, 0, DrCategory::STUN, 0, 0, Fit::ALL, false },      // Maim
        { 9005, Kind::CROWD_CONTROL, CLS_DRUID, 0, 0, DrCategory::STUN, 0, 0, Fit::ALL, false },           // Pounce
        { 19577, Kind::CROWD_CONTROL, CLS_HUNTER, 60000, 0, DrCategory::STUN, 0, 0, Fit::ALL, false },     // Intimidation
        { 20549, Kind::CROWD_CONTROL, CLS_ANY, 120000, 0, DrCategory::STUN, 0, 0, Fit::ALL, false },       // War Stomp
        // --- Incapacitates ---
        { 118, Kind::CROWD_CONTROL, CLS_MAGE, 0, 0, DrCategory::INCAPACITATE, 0, 0, Fit::ALL, false },       // Polymorph
        { 28272, Kind::CROWD_CONTROL, CLS_MAGE, 0, 0, DrCategory::INCAPACITATE, 0, 0, Fit::ALL, false },     // Polymorph (pig)
        { 28271, Kind::CROWD_CONTROL, CLS_MAGE, 0, 0, DrCategory::INCAPACITATE, 0, 0, Fit::ALL, false },     // Polymorph (turtle)
        { 61305, Kind::CROWD_CONTROL, CLS_MAGE, 0, 0, DrCategory::INCAPACITATE, 0, 0, Fit::ALL, false },     // Polymorph (cat)
        { 61721, Kind::CROWD_CONTROL, CLS_MAGE, 0, 0, DrCategory::INCAPACITATE, 0, 0, Fit::ALL, false },     // Polymorph (rabbit)
        { 61780, Kind::CROWD_CONTROL, CLS_MAGE, 0, 0, DrCategory::INCAPACITATE, 0, 0, Fit::ALL, false },     // Polymorph (turkey)
        { 126819, Kind::CROWD_CONTROL, CLS_MAGE, 0, 0, DrCategory::INCAPACITATE, 0, 0, Fit::ALL, false },    // Polymorph (porcupine)
        { 6770, Kind::CROWD_CONTROL, CLS_ROGUE, 0, 0, DrCategory::INCAPACITATE, 0, 0, Fit::ALL, false },     // Sap
        { 1776, Kind::CROWD_CONTROL, CLS_ROGUE, 10000, 0, DrCategory::INCAPACITATE, 0, 0, Fit::ALL, false }, // Gouge
        { 51514, Kind::CROWD_CONTROL, CLS_SHAMAN, 45000, 0, DrCategory::INCAPACITATE, 0, 0, Fit::ALL, false },   // Hex
        { 20066, Kind::CROWD_CONTROL, CLS_PALADIN, 15000, 0, DrCategory::INCAPACITATE, 0, 0, Fit::ALL, false },  // Repentance
        { 3355, Kind::CROWD_CONTROL, CLS_HUNTER, 0, 0, DrCategory::INCAPACITATE, 0, 0, Fit::ALL, false },        // Freezing Trap
        { 19386, Kind::CROWD_CONTROL, CLS_HUNTER, 45000, 0, DrCategory::INCAPACITATE, 0, 0, Fit::ALL, false },  // Wyvern Sting
        { 115078, Kind::CROWD_CONTROL, CLS_MONK, 15000, 0, DrCategory::INCAPACITATE, 0, 0, Fit::ALL, false },   // Paralysis
        { 82691, Kind::CROWD_CONTROL, CLS_MAGE, 0, 0, DrCategory::INCAPACITATE, 0, 0, Fit::ALL, false },        // Ring of Frost
        { 9484, Kind::CROWD_CONTROL, CLS_PRIEST, 0, 0, DrCategory::INCAPACITATE, 0, 0, Fit::ALL, false },       // Shackle Undead
        { 2637, Kind::CROWD_CONTROL, CLS_DRUID, 0, 0, DrCategory::INCAPACITATE, 0, 0, Fit::ALL, false },        // Hibernate
        { 710, Kind::CROWD_CONTROL, CLS_WARLOCK, 0, 0, DrCategory::INCAPACITATE, 0, 0, Fit::ALL, false },       // Banish
        // --- Disorients ---
        { 31661, Kind::CROWD_CONTROL, CLS_MAGE, 20000, 0, DrCategory::DISORIENT, 0, 0, Fit::ALL, false },       // Dragon's Breath
        { 19503, Kind::CROWD_CONTROL, CLS_HUNTER, 30000, 0, DrCategory::DISORIENT, 0, 0, Fit::ALL, false },     // Scatter Shot
        { 115750, Kind::CROWD_CONTROL, CLS_PALADIN, 120000, 0, DrCategory::DISORIENT, 0, 0, Fit::ALL, false },  // Blinding Light
        { 88625, Kind::CROWD_CONTROL, CLS_PRIEST, 30000, 0, DrCategory::DISORIENT, 0, 0, Fit::ALL, false },     // Holy Word: Chastise
        { 99, Kind::CROWD_CONTROL, CLS_DRUID, 30000, 0, DrCategory::DISORIENT, 0, 0, Fit::ALL, false },         // Disorienting Roar
        // --- Fears ---
        { 5782, Kind::CROWD_CONTROL, CLS_WARLOCK, 0, 0, DrCategory::FEAR, 0, 0, Fit::ALL, false },           // Fear
        { 5484, Kind::CROWD_CONTROL, CLS_WARLOCK, 40000, 0, DrCategory::FEAR, 0, 0, Fit::ALL, false },       // Howl of Terror
        { 8122, Kind::CROWD_CONTROL, CLS_PRIEST, 30000, 0, DrCategory::FEAR, 0, 0, Fit::ALL, false },        // Psychic Scream
        { 108921, Kind::CROWD_CONTROL, CLS_PRIEST, 45000, 0, DrCategory::FEAR, 0, 0, Fit::ALL, false },      // Psyfiend
        { 2094, Kind::CROWD_CONTROL, CLS_ROGUE, 120000, 0, DrCategory::FEAR, 0, 0, Fit::ALL, false },        // Blind
        { 5246, Kind::CROWD_CONTROL, CLS_WARRIOR, 90000, 0, DrCategory::FEAR, 0, 0, Fit::ALL, false },       // Intimidating Shout
        { 6358, Kind::CROWD_CONTROL, CLS_WARLOCK, 0, 0, DrCategory::FEAR, 0, 0, Fit::ALL, false },           // Seduction
        { 10326, Kind::CROWD_CONTROL, CLS_PALADIN, 15000, 0, DrCategory::FEAR, 0, 0, Fit::ALL, false },      // Turn Evil
        // --- Horrors ---
        { 64044, Kind::CROWD_CONTROL, CLS_PRIEST, 45000, 0, DrCategory::HORROR, 0, 0, Fit::ALL, false },     // Psychic Horror
        { 6789, Kind::CROWD_CONTROL, CLS_WARLOCK, 45000, 0, DrCategory::HORROR, 0, 0, Fit::ALL, false },     // Mortal Coil
        // --- Roots ---
        { 122, Kind::CROWD_CONTROL, CLS_MAGE, 25000, 0, DrCategory::ROOT, 0, 0, Fit::ALL, false },           // Frost Nova
        { 33395, Kind::CROWD_CONTROL, CLS_MAGE, 25000, 0, DrCategory::ROOT, 0, 0, Fit::ALL, false },         // Freeze
        { 339, Kind::CROWD_CONTROL, CLS_DRUID, 0, 0, DrCategory::ROOT, 0, 0, Fit::ALL, false },              // Entangling Roots
        { 102359, Kind::CROWD_CONTROL, CLS_DRUID, 30000, 0, DrCategory::ROOT, 0, 0, Fit::ALL, false },       // Mass Entanglement
        { 108920, Kind::CROWD_CONTROL, CLS_PRIEST, 30000, 0, DrCategory::ROOT, 0, 0, Fit::ALL, false },      // Void Tendrils
        // --- Disarms, mind control, cyclone ---
        { 676, Kind::CROWD_CONTROL, CLS_WARRIOR, 60000, 0, DrCategory::DISARM, 0, 0, Fit::ALL, false },      // Disarm
        { 51722, Kind::CROWD_CONTROL, CLS_ROGUE, 60000, 0, DrCategory::DISARM, 0, 0, Fit::ALL, false },      // Dismantle
        { 605, Kind::CROWD_CONTROL, CLS_PRIEST, 0, 0, DrCategory::MIND_CONTROL, 0, 0, Fit::ALL, false },     // Dominate Mind
        { 33786, Kind::CROWD_CONTROL, CLS_DRUID, 0, 0, DrCategory::CYCLONE, 0, 0, Fit::ALL, false },         // Cyclone
    };
    return rows;
}
// clang-format on

// The core reports every spell of every player to the awareness hook, so the lookup is by an index built once.
inline Entry const* Find(std::uint32_t spellId)
{
    static std::unordered_map<std::uint32_t, Entry const*> const index = []
    {
        std::unordered_map<std::uint32_t, Entry const*> map;
        for (Entry const& e : Catalog())
            map.emplace(e.spellId, &e);
        return map;
    }();
    auto const it = index.find(spellId);
    return it == index.end() ? nullptr : it->second;
}

inline bool IsKind(std::uint32_t spellId, Kind kind)
{
    Entry const* e = Find(spellId);
    return e && e->kind == kind;
}

// An interrupt or a silence that also stops a cast: the spells a kick-aware bot watches the enemy for.
inline bool StopsCast(std::uint32_t spellId)
{
    Entry const* e = Find(spellId);
    return e && (e->kind == Kind::INTERRUPT || e->kind == Kind::SILENCE);
}

// The medallion and the racial that shares its cooldown: using either starts the same two minutes.
inline bool SharesTrinketCooldown(std::uint32_t spellId)
{
    return spellId == TRINKET_SPELL || spellId == EVERY_MAN_FOR_HIMSELF;
}

// The rows of one class and kind, in table order (CLS_ANY rows are every class's).
inline std::vector<Entry> RowsFor(std::uint8_t cls, Kind kind)
{
    std::vector<Entry> out;
    for (Entry const& e : Catalog())
        if (e.kind == kind && (e.cls == cls || e.cls == CLS_ANY))
            out.push_back(e);
    return out;
}

// The cast-stopping spells a class can have (a player sees the class on the frame, so this is knowledge a player has:
// the enemy has some way to stop a cast, whether or not it was seen yet).
inline std::vector<Entry> InterruptsOf(std::uint8_t cls)
{
    std::vector<Entry> out;
    for (Entry const& e : Catalog())
        if ((e.kind == Kind::INTERRUPT || e.kind == Kind::SILENCE) && e.cls == cls)
            out.push_back(e);
    return out;
}

inline DrCategory DrCategoryOf(std::uint32_t spellId)
{
    Entry const* e = Find(spellId);
    return e ? e->dr : DrCategory::NONE;
}

// The core's mechanic numbers (SharedDefines.h, enum Mechanics); the glue asserts they match.
constexpr std::uint32_t MECH_CHARM = 1;
constexpr std::uint32_t MECH_DISORIENTED = 2;
constexpr std::uint32_t MECH_DISARM = 3;
constexpr std::uint32_t MECH_FEAR = 5;
constexpr std::uint32_t MECH_ROOT = 7;
constexpr std::uint32_t MECH_SILENCE = 9;
constexpr std::uint32_t MECH_SLEEP = 10;
constexpr std::uint32_t MECH_SNARE = 11;
constexpr std::uint32_t MECH_STUN = 12;
constexpr std::uint32_t MECH_FREEZE = 13;
constexpr std::uint32_t MECH_KNOCKOUT = 14;
constexpr std::uint32_t MECH_POLYMORPH = 17;
constexpr std::uint32_t MECH_BANISH = 18;
constexpr std::uint32_t MECH_SHACKLE = 20;
constexpr std::uint32_t MECH_HORROR = 24;
constexpr std::uint32_t MECH_SAPPED = 30;

constexpr std::uint32_t MechBit(std::uint32_t mechanic) { return 1u << mechanic; }

// The kinds of control a mask of mechanics puts on a unit (what a bot reads off the auras it carries).
inline std::uint32_t CcKindsOfMechanics(std::uint32_t mechanicMask)
{
    std::uint32_t kinds = CC_NONE;
    if (mechanicMask & (MechBit(MECH_STUN) | MechBit(MECH_KNOCKOUT)))
        kinds |= CC_STUN;
    if (mechanicMask & (MechBit(MECH_POLYMORPH) | MechBit(MECH_SAPPED) | MechBit(MECH_BANISH) | MechBit(MECH_SHACKLE) |
                        MechBit(MECH_FREEZE)))
        kinds |= CC_INCAPACITATE;
    if (mechanicMask & MechBit(MECH_DISORIENTED))
        kinds |= CC_DISORIENT;
    if (mechanicMask & MechBit(MECH_FEAR))
        kinds |= CC_FEAR;
    if (mechanicMask & MechBit(MECH_HORROR))
        kinds |= CC_HORROR;
    if (mechanicMask & MechBit(MECH_ROOT))
        kinds |= CC_ROOT;
    if (mechanicMask & MechBit(MECH_SNARE))
        kinds |= CC_SNARE;
    if (mechanicMask & MechBit(MECH_CHARM))
        kinds |= CC_CHARM;
    if (mechanicMask & MechBit(MECH_SLEEP))
        kinds |= CC_SLEEP;
    if (mechanicMask & MechBit(MECH_SILENCE))
        kinds |= CC_SILENCE;
    if (mechanicMask & MechBit(MECH_DISARM))
        kinds |= CC_DISARM;
    return kinds;
}

// The diminishing returns category of a spell the catalog does not list, from the mechanics of its effects. Roots win
// over a snare of the same spell (Frost Nova's root), a stun over everything.
inline DrCategory DrCategoryOfMechanics(std::uint32_t mechanicMask)
{
    if (mechanicMask & (MechBit(MECH_STUN) | MechBit(MECH_KNOCKOUT)))
        return DrCategory::STUN;
    if (mechanicMask & MechBit(MECH_HORROR))
        return DrCategory::HORROR;
    if (mechanicMask & MechBit(MECH_FEAR))
        return DrCategory::FEAR;
    if (mechanicMask & MechBit(MECH_DISORIENTED))
        return DrCategory::DISORIENT;
    if (mechanicMask & (MechBit(MECH_POLYMORPH) | MechBit(MECH_SAPPED) | MechBit(MECH_BANISH) | MechBit(MECH_SHACKLE) |
                        MechBit(MECH_FREEZE)))
        return DrCategory::INCAPACITATE;
    if (mechanicMask & MechBit(MECH_ROOT))
        return DrCategory::ROOT;
    if (mechanicMask & MechBit(MECH_SILENCE))
        return DrCategory::SILENCE;
    if (mechanicMask & MechBit(MECH_DISARM))
        return DrCategory::DISARM;
    if (mechanicMask & MechBit(MECH_CHARM))
        return DrCategory::MIND_CONTROL;
    return DrCategory::NONE;
}

// The DR category of a spell: the catalog first, the mechanics when the spell is not listed.
inline DrCategory DrCategoryFor(std::uint32_t spellId, std::uint32_t mechanicMask)
{
    DrCategory const listed = DrCategoryOf(spellId);
    return listed != DrCategory::NONE ? listed : DrCategoryOfMechanics(mechanicMask);
}

// The kind of control a diminishing returns category puts on its target.
inline std::uint32_t CcKindOfCategory(DrCategory dr)
{
    switch (dr)
    {
        case DrCategory::STUN: return CC_STUN;
        case DrCategory::INCAPACITATE: return CC_INCAPACITATE;
        case DrCategory::DISORIENT: return CC_DISORIENT;
        case DrCategory::FEAR: return CC_FEAR;
        case DrCategory::HORROR: return CC_HORROR;
        case DrCategory::SILENCE: return CC_SILENCE;
        case DrCategory::ROOT: return CC_ROOT;
        case DrCategory::DISARM: return CC_DISARM;
        case DrCategory::MIND_CONTROL: return CC_CHARM;
        case DrCategory::CYCLONE: return CC_INCAPACITATE;
        default: return CC_NONE;
    }
}

inline char const* DrCategoryName(DrCategory dr)
{
    switch (dr)
    {
        case DrCategory::STUN: return "stun";
        case DrCategory::INCAPACITATE: return "incapacitate";
        case DrCategory::DISORIENT: return "disorient";
        case DrCategory::FEAR: return "fear";
        case DrCategory::HORROR: return "horror";
        case DrCategory::SILENCE: return "silence";
        case DrCategory::ROOT: return "root";
        case DrCategory::DISARM: return "disarm";
        case DrCategory::MIND_CONTROL: return "mind-control";
        case DrCategory::CYCLONE: return "cyclone";
        default: return "none";
    }
}
}  // namespace PvpSpells

#endif
