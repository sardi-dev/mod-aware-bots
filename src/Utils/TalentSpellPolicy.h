/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * BT-322: when a bot casts the active talents that no class strategy named (the PvP sets pick them, and several earn
 * their place in a dungeon too). Free of game types so tests/ can run it; TalentSpells.cpp reads the facts.
 *
 * Talents that replace a baseline spell need no rule here: PlayerbotAI::GetSpellInfo swaps the cast through the talent's
 * override (Asphyxiate for Strangulate, Fist of Justice for Hammer of Justice, Mindbender for Shadowfiend, Earthgrab
 * Totem for Earthbind Totem), so the existing actions cast them. Soul Link (108415) is a passive spell in the 5.4.8 data
 * and needs no cast. Safeguard (level 30) is the exception: the swap needs
 * Intervene, learned at 72, and only the tank strategy has the edge, so Safeguard is cast by its own name here.
 */

#ifndef _PLAYERBOT_TALENTSPELLPOLICY_H
#define _PLAYERBOT_TALENTSPELLPOLICY_H

#include <array>
#include <cstdint>

namespace TalentSpellPolicy
{
enum class Talent : std::uint8_t
{
    STAGGERING_SHOUT,
    SAFEGUARD,
    SPEED_OF_LIGHT,
    HOLY_PRISM_HEAL,
    HOLY_PRISM_DAMAGE,
    BURST_OF_SPEED,
    PSYFIEND,
    ANGELIC_FEATHER,
    SPECTRAL_GUISE,
    DIVINE_STAR,
    LICHBORNE,
    DESECRATED_GROUND,
    WINDWALK_TOTEM,
    RING_OF_FROST,
    RING_OF_PEACE,
    LEG_SWEEP,
    DISPLACER_BEAST,
    CENARION_WARD,
    DISORIENTING_ROAR,
    HEART_OF_THE_WILD,
    COUNT
};

// Player class ids of the 5.4.8 ChrClasses table (SharedDefines.h Classes).
constexpr std::uint8_t WARRIOR = 1, PALADIN = 2, ROGUE = 4, PRIEST = 5, DEATH_KNIGHT = 6, SHAMAN = 7, MAGE = 8,
                       MONK = 10, DRUID = 11;

struct Entry
{
    Talent talent;
    char const* name;     // trigger and action name
    char const* spell;    // the spell name the action casts (lower case, as the spell id value reads it)
    std::uint32_t spellId;  // Talent.dbc SpellId, checked against the 5.4.8 client data
    std::uint8_t playerClass;
};

constexpr std::array<Entry, static_cast<std::size_t>(Talent::COUNT)> TABLE = { {
    { Talent::STAGGERING_SHOUT, "talent staggering shout", "staggering shout", 107566, WARRIOR },
    { Talent::SAFEGUARD, "talent safeguard", "safeguard", 114029, WARRIOR },
    { Talent::SPEED_OF_LIGHT, "talent speed of light", "speed of light", 85499, PALADIN },
    { Talent::HOLY_PRISM_HEAL, "talent holy prism heal", "holy prism", 114165, PALADIN },
    { Talent::HOLY_PRISM_DAMAGE, "talent holy prism", "holy prism", 114165, PALADIN },
    { Talent::BURST_OF_SPEED, "talent burst of speed", "burst of speed", 108212, ROGUE },
    { Talent::PSYFIEND, "talent psyfiend", "psyfiend", 108921, PRIEST },
    { Talent::ANGELIC_FEATHER, "talent angelic feather", "angelic feather", 121536, PRIEST },
    { Talent::SPECTRAL_GUISE, "talent spectral guise", "spectral guise", 112833, PRIEST },
    { Talent::DIVINE_STAR, "talent divine star", "divine star", 110744, PRIEST },
    { Talent::LICHBORNE, "talent lichborne", "lichborne", 49039, DEATH_KNIGHT },
    { Talent::DESECRATED_GROUND, "talent desecrated ground", "desecrated ground", 108201, DEATH_KNIGHT },
    { Talent::WINDWALK_TOTEM, "talent windwalk totem", "windwalk totem", 108273, SHAMAN },
    { Talent::RING_OF_FROST, "talent ring of frost", "ring of frost", 113724, MAGE },
    { Talent::RING_OF_PEACE, "talent ring of peace", "ring of peace", 116844, MONK },
    { Talent::LEG_SWEEP, "talent leg sweep", "leg sweep", 119381, MONK },
    { Talent::DISPLACER_BEAST, "talent displacer beast", "displacer beast", 102280, DRUID },
    { Talent::CENARION_WARD, "talent cenarion ward", "cenarion ward", 102351, DRUID },
    { Talent::DISORIENTING_ROAR, "talent disorienting roar", "disorienting roar", 99, DRUID },
    { Talent::HEART_OF_THE_WILD, "talent heart of the wild", "heart of the wild", 108288, DRUID },
} };

constexpr Entry const& Of(Talent talent) { return TABLE[static_cast<std::size_t>(talent)]; }

// The facts a rule reads. The trigger fills only the ones its talent needs (each costs a search).
struct Facts
{
    bool pvp = false;                // battleground, arena, duel, or the current target is a player
    bool enemyPlayerClose = false;   // an attackable enemy player within melee reach of the bot
    unsigned attackersOnBot = 0;     // hostile units whose victim is the bot, within 8 yd
    unsigned enemiesInRadius = 0;    // attackers within the spell's own radius (Leg Sweep 5 yd)
    bool casterInRadius = false;     // an interruptible enemy cast within that radius (group pve or pvp)
    float healthPct = 100.0f;
    bool tank = false;
    bool healer = false;
    bool melee = false;
    bool grouped = false;
    bool impaired = false;           // rooted or slowed
    bool rooted = false;
    float targetDistance = 0.0f;     // to the current target, 0 without one
    bool targetIsPlayer = false;
    bool targetSnared = false;       // the current target moves slower than normal
    bool facingTarget = false;
    bool fearCasterNear = false;     // an enemy priest, warlock or warrior player within 20 yd
    float healTargetPct = 100.0f;    // the group member the healer would heal next
    bool hasAura = false;            // the talent's own aura is on its target already
    bool catForm = false;
    bool mainTankInCombat = false;   // the main tank is fighting and a ward on it would be used
};

// Health under which a damage dealer or a healer drops its threat with a stealth (Spectral Guise): low enough that the
// tank's taunt or the healer's own heals came too late, and high enough that the next hit is not the last.
constexpr float ESCAPE_HEALTH_PCT = 35.0f;
// Health under which a self peel is worth its cooldown when two mobs are on a non-tank (Disorienting Roar, Ring of
// Peace, Psyfiend alone), or one enemy player is.
constexpr float PEEL_HEALTH_PCT = 60.0f;
// Leg Sweep stuns everything within its 5 yd radius and the stun does not break on damage: on a pack of three it is a
// tank's area mitigation, which is how players use it in a dungeon.
constexpr unsigned AREA_STUN_PACK = 3;
// Attackers on a non-tank that make a self peel worthwhile.
constexpr unsigned PEEL_ATTACKERS = 2;
// Displacer Beast leaps 20 yd forward; Speed of Light is a 70 percent sprint. A chase starts out of melee reach and
// ends where a leap or a sprint would overshoot the fight.
constexpr float CHASE_MIN_YD = 8.0f;
constexpr float LEAP_MAX_YD = 20.0f;
constexpr float SPRINT_MAX_YD = 40.0f;
// Divine Star travels 24 yd ahead of the priest; Staggering Shout roots snared enemies within 10 yd.
constexpr float STAR_RANGE_YD = 24.0f;
constexpr float SHOUT_RADIUS_YD = 10.0f;
// Holy Prism on a member heals that member and damages five enemies near it: a holy paladin's spot heal below 80.
constexpr float PRISM_HEAL_PCT = 80.0f;
// Cenarion Ward on itself: a non-healer druid under attack below 80 percent.
constexpr float SELF_WARD_PCT = 80.0f;

// Psyfiend fears whoever attacks the priest. In a group its fears send mobs into the next pack, so it is a solo and
// PvP tool only.
constexpr bool WantsPsyfiend(Facts const& f)
{
    return f.enemyPlayerClose || (!f.grouped && f.attackersOnBot >= PEEL_ATTACKERS && f.healthPct < PEEL_HEALTH_PCT);
}

// Spectral Guise: a stealth with a decoy, so the mobs (or the player) on the priest hit the decoy while the tank or the
// heals catch up.
constexpr bool WantsEscape(Facts const& f)
{
    return !f.tank && f.healthPct < ESCAPE_HEALTH_PCT && (f.attackersOnBot > 0 || f.enemyPlayerClose);
}

// Angelic Feather at the priest's feet: run from a melee player. A mob is not kited (the bots tank and heal PvE packs in
// place), so it is PvP only.
constexpr bool WantsKiteSpeed(Facts const& f) { return f.enemyPlayerClose && !f.melee; }

// Speed of Light: a holy paladin kites a melee player like the feather; a melee paladin runs down a player out of reach.
constexpr bool WantsSpeedOfLight(Facts const& f)
{
    if (!f.melee)
        return WantsKiteSpeed(f);
    return f.targetIsPlayer && f.targetDistance > CHASE_MIN_YD && f.targetDistance <= SPRINT_MAX_YD && !f.rooted;
}

// Burst of Speed and Windwalk Totem end slows and roots: when the bot is held in place, or slowed while it chases or
// plays against players.
constexpr bool WantsFreedom(Facts const& f)
{
    if (!f.impaired)
        return false;
    bool const chasing = f.melee && f.targetDistance > CHASE_MIN_YD;
    return f.rooted || f.pvp || chasing;
}

// Ring of Frost under the attackers of a mage: two mobs on it in any fight, or a player in its face.
constexpr bool WantsRingOfFrost(Facts const& f)
{
    return !f.tank && (f.enemyPlayerClose || f.attackersOnBot >= PEEL_ATTACKERS);
}

// Leg Sweep: interrupt a caster in reach, stun a pack on the tank (or on anyone), or break a player's melee.
constexpr bool WantsLegSweep(Facts const& f)
{
    if (f.casterInRadius)
        return true;
    if (f.enemiesInRadius >= AREA_STUN_PACK)
        return true;
    if (!f.tank && f.attackersOnBot >= PEEL_ATTACKERS && f.enemiesInRadius >= PEEL_ATTACKERS)
        return true;
    return f.enemyPlayerClose && f.healthPct < PEEL_HEALTH_PCT + 10.0f;
}

// Disorienting Roar and Ring of Peace on itself: a non-tank with two mobs on it and losing health, or a player in melee.
// Both break or end on damage, so they buy seconds for a heal or a taunt, not a kill.
constexpr bool WantsSelfPeel(Facts const& f)
{
    if (f.tank)
        return false;
    if (f.enemyPlayerClose && f.healthPct < PEEL_HEALTH_PCT)
        return true;
    return f.attackersOnBot >= PEEL_ATTACKERS && f.healthPct < PEEL_HEALTH_PCT;
}

// Displacer Beast leaps forward into cat form: a gap closer for a cat that faces its target 8 to 20 yd away.
constexpr bool WantsLeap(Facts const& f)
{
    return f.catForm && f.facingTarget && !f.rooted && f.targetDistance > CHASE_MIN_YD &&
        f.targetDistance <= LEAP_MAX_YD;
}

// Divine Star: the priest faces an enemy within the star's path. A healer throws it when a member needs healing (the
// star heals everyone on its way out and back); a shadow priest throws it on cooldown.
constexpr bool WantsDivineStar(Facts const& f)
{
    if (!f.facingTarget || f.targetDistance <= 0.0f || f.targetDistance > STAR_RANGE_YD)
        return false;
    return !f.healer || f.healTargetPct < PRISM_HEAL_PCT + 5.0f;
}

constexpr bool WantsPrismHeal(Facts const& f) { return f.healer && f.healTargetPct < PRISM_HEAL_PCT; }

// Holy Prism on the enemy: a retribution or protection paladin's filler (the prism heals five members near the target).
constexpr bool WantsPrismDamage(Facts const& f) { return !f.healer && f.targetDistance > 0.0f; }

// Staggering Shout roots the snared enemies around the warrior: a player that the warrior slowed and that runs.
constexpr bool WantsStaggeringShout(Facts const& f)
{
    return f.targetIsPlayer && f.targetSnared && f.targetDistance > 0.0f && f.targetDistance <= SHOUT_RADIUS_YD;
}

// Lichborne makes the death knight immune to fear, charm and sleep; it cannot be cast once feared, so it goes up when a
// player who fears is near.
constexpr bool WantsLichborne(Facts const& f) { return f.pvp && f.fearCasterNear; }

// Desecrated Ground: immune to loss of control while standing in it. Against players it goes down when one is in melee
// and the death knight is losing.
constexpr bool WantsDesecratedGround(Facts const& f) { return f.enemyPlayerClose && f.healthPct < PEEL_HEALTH_PCT; }

// Cenarion Ward: a restoration druid keeps it on the fighting main tank; any other druid puts it on itself when hit.
constexpr bool WantsCenarionWard(Facts const& f)
{
    if (f.hasAura)
        return false;
    if (f.healer)
        return f.mainTankInCombat;
    return f.attackersOnBot > 0 && f.healthPct < SELF_WARD_PCT;
}
}  // namespace TalentSpellPolicy

#endif
