/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 *
 * Sunwell Plateau (map 580). mod-playerbots has no strategy for it, so this one is written for this module from the
 * encounter mechanics of the core's scripts (scripts/EasternKingdoms/SunwellPlateau) and the client data, in the
 * layout of the ported raids. It keeps to the essentials per boss:
 * - Kalecgos: non-tanks stand at his sides (Frost Breath in front, Tail Lash behind, both 30 yd); bots in the
 *   Spectral Realm fight Sathrovarr and bots above fight Kalecgos (target exclusions); Sathrovarr's 1 s Shadow Bolt
 *   Volley is interrupted. Curse of Boundless Agony is left to the class curse removers.
 * - Brutallus: Meteor Slash (90 degree cone, shared damage, stacking debuff) is soaked by the tank and one of two
 *   damage dealer teams; the team rotates by the stacks it carries and the second tank taunts at four stacks, everyone
 *   else stays out of the cone. A bot with Burn keeps away from the raid and the raid from it (Burn jumps 2 yd) and
 *   does not soak.
 * - Felmyst: everyone leaves the Encapsulate target (20 yd pulses), the Demonic Vapor, its trail and the Fog of
 *   Corruption (20 yd charm); the player a vapor chases kites it away from the raid; damage dealers kill the
 *   Unyielding Dead. In the air nobody chases her: the raid gathers where she took off (she lands on her top threat
 *   target) and leaves the lane she faces before each breath.
 * - Eredar Twins: the main tank takes Lady Sacrolash and the first assist tank Grand Warlock Alythess; ranged spread
 *   for Conflagration and Shadow Nova (8 and 10 yd); Shadow Images die first; Pyrogenics is stolen or purged.
 * - M'uru and Entropius: damage dealers kill Dark Fiends, Void Sentinels, Fury Mages, Berserkers and Void Spawns in
 *   that order; Fel Fireball and Void Blast are interrupted; everyone leaves Darkness (15 yd) and Singularities.
 * - Kil'jaeden: ranged damage dealers kill Shield Orbs (they circle 17 yd up), Volatile Felfire Fiends and
 *   Sinister Reflections, melee the reflections (those on healers first); the fiend's target kites it and the others
 *   keep 10 yd from it (Felfire Fission); ranged spread 11 yd for Legion Lightning (10 yd jumps); Fire Bloom carriers
 *   spread (10 yd); everyone leaves Armageddon and Shadow Spike targets.
 * Not covered: the Orb of the Blue Flight at Kil'jaeden (the world has no orb on this map and the Power of the Blue
 * Flight has no spells, so there is no Shield of the Blue; Darkness of a Thousand Souls and Flame Dart hit the whole
 * room, so stepping away does nothing) and the twins' tank spots.
 */

#ifndef PLAYERBOTS_SUNWELLHELPERS_H
#define PLAYERBOTS_SUNWELLHELPERS_H

#include "Common.h"
#include "RaidGeometry.h"
#include "SunwellLogic.h"
#include <vector>

class Player;
class PlayerbotAI;
class Unit;

namespace SunwellHelpers
{
inline constexpr uint32 SWP_MAP_ID = 580;

enum SunwellNpcs : uint32
{
    // Kalecgos
    NPC_KALECGOS                = 24850,
    NPC_SATHROVARR              = 24892,

    // Brutallus
    NPC_BRUTALLUS               = 24882,

    // Felmyst
    NPC_FELMYST                 = 25038,
    NPC_DEMONIC_VAPOR           = 25265,
    NPC_DEMONIC_VAPOR_TRAIL     = 25267,
    NPC_UNYIELDING_DEAD         = 25268,

    // Eredar Twins
    NPC_LADY_SACROLASH          = 25165,
    NPC_GRAND_WARLOCK_ALYTHESS  = 25166,
    NPC_SHADOW_IMAGE            = 25214,

    // M'uru and Entropius
    NPC_MURU                    = 25741,
    NPC_ENTROPIUS               = 25840,
    NPC_DARK_FIEND              = 25744,
    NPC_VOID_SENTINEL           = 25772,
    NPC_SHADOWSWORD_BERSERKER   = 25798,
    NPC_SHADOWSWORD_FURY_MAGE   = 25799,
    NPC_VOID_SPAWN              = 25824,
    NPC_SINGULARITY             = 25855,

    // Kil'jaeden
    NPC_KILJAEDEN               = 25315,
    NPC_HAND_OF_THE_DECEIVER    = 25588,
    NPC_VOLATILE_FELFIRE_FIEND  = 25598,
    NPC_FELFIRE_PORTAL          = 25603,
    NPC_SHIELD_ORB              = 25502,
    NPC_SINISTER_REFLECTION     = 25708,
    NPC_ARMAGEDDON_TARGET       = 25735,
    NPC_SPIKE_TARGET            = 30598,
    NPC_SPIKE_TARGET_2          = 30614,
};

enum SunwellSpells : uint32
{
    // Kalecgos: the core puts this aura on every player it sends to the Spectral Realm.
    SPELL_SPECTRAL_REALM        = 46021,
    SPELL_SATHROVARR_VOLLEY     = 45031,

    // Brutallus: 60 s, jumps to players within 2 yd every second (45151). Meteor Slash's debuff stacks on everyone in
    // the cone (40 s, refreshed by each hit).
    SPELL_BURN                  = 46394,
    SPELL_METEOR_SLASH          = 45150,

    // Felmyst: Encapsulate pulses 45662 (20 yd) around its target; the fog creature carries 45582 (charms within
    // 20 yd), the Demonic Vapor trail 45399 (3 yd).
    SPELL_ENCAPSULATE           = 45665,
    SPELL_FOG_OF_CORRUPTION     = 45582,
    SPELL_VAPOR_TRAIL           = 45399,
    // Felmyst's own speed buff while she crosses the room breathing (the script's SPELL_FOG_BREATH).
    SPELL_SPEED_BURST           = 45495,

    // Eredar Twins: Alythess's self buff (spell damage, 15 s), magic.
    SPELL_PYROGENICS            = 45230,

    // M'uru and Entropius: the Darkness void zone (persistent area, 15 yd); Fury Mage and Void Sentinel casts.
    SPELL_DARKNESS              = 45996,
    SPELL_FEL_FIREBALL          = 46101,
    SPELL_VOID_BLAST            = 46161,

    // Kil'jaeden: Fire Bloom hits everyone within 10 yd of its carrier every 2 s (45642).
    SPELL_FIRE_BLOOM            = 45641,
};

// The core's Spectral Realm lies under the floor; a player below this height is in it (DEMON_REALM_Z + 5).
inline constexpr float SPECTRAL_REALM_MAX_Z = -69.558f;

// Hazard radii from the client data, plus 2 yd so a bot leaving one does not step back in on the next tick.
inline constexpr float HAZARD_PADDING = 2.0f;
inline constexpr float BURN_SPREAD_DISTANCE = 5.0f;
inline constexpr float ENCAPSULATE_RADIUS = 20.0f;
inline constexpr float FOG_RADIUS = 20.0f;
inline constexpr float VAPOR_TRAIL_RADIUS = 3.0f;
inline constexpr float DEMONIC_VAPOR_RADIUS = 6.0f;
inline constexpr float DARKNESS_RADIUS = 15.0f;
inline constexpr float SINGULARITY_RADIUS = 8.0f;
inline constexpr float EXPLODING_ADD_RADIUS = 5.0f;
// Felfire Fission (45779) when a Volatile Felfire Fiend reaches its target or dies.
inline constexpr float FELFIRE_FISSION_RADIUS = 10.0f;
inline constexpr float FIRE_BLOOM_RADIUS = 10.0f;
inline constexpr float ARMAGEDDON_RADIUS = 9.0f;
// Shadow Spike's impact (45885).
inline constexpr float SHADOW_SPIKE_RADIUS = 8.0f;
inline constexpr float HAZARD_SCAN_RADIUS = 50.0f;

// Kalecgos: Frost Breath and Tail Lash are 30 yd cones; 90 degrees each is the core's default cone width.
inline constexpr float KALECGOS_CONE_RANGE = 30.0f;
inline constexpr float KALECGOS_CONE_WIDTH = RaidGeometry::PI / 2.0f;

// Eredar Twins: Shadow Nova (10 yd) and Conflagration (8 yd) splash around their target.
inline constexpr float TWINS_RANGED_SPREAD = 10.0f;
// Kil'jaeden: Legion Lightning (45664) chains to five players; a magic chain jumps 10 yd in the core.
inline constexpr float KILJAEDEN_RANGED_SPREAD = 11.0f;
// A Demonic Vapor or Volatile Felfire Fiend after the bot: kite it in 8 yd steps while it is within 20 yd.
inline constexpr float CHASER_SCAN_RADIUS = 40.0f;
inline constexpr float KITE_STEP = 8.0f;
inline constexpr float KITE_UNTIL = 20.0f;

// The instance's encounter data (sunwell_plateau.h order). The strategy reads it first, so bots between the bosses
// skip every grid search below.
enum class SunwellEncounter : uint8
{
    None,
    Kalecgos,
    Brutallus,
    Felmyst,
    EredarTwins,
    Muru,
    Kiljaeden
};

SunwellEncounter GetActiveEncounter(Player* bot);

// The boss of `entry` near the bot (alive), or nullptr.
Unit* FindBoss(Player* bot, uint32 entry);
bool InSpectralRealm(Player* bot);

// Every circle the bot must stay out of right now, including the spread circles of debuffed raid members. A bot that
// carries a spreading debuff gets circles around the other raid members instead.
std::vector<RaidGeometry::Circle> GetHazards(Player* bot);
bool IsInHazard(Player* bot, std::vector<RaidGeometry::Circle> const& hazards);

// Kalecgos is near and the bot (not his victim) stands in his breath or tail cone.
Unit* GetKalecgosConeThreat(Player* bot);

// The add the damage dealers kill first in the current encounter, or nullptr.
Unit* GetPriorityAdd(Player* bot);

// A nearby enemy casting a spell the raid interrupts, or nullptr.
Unit* GetInterruptTarget(Player* bot);

// The ranged spread the current encounter needs, 0 when none, and the circles of the raid members closer than it.
float GetRangedSpreadDistance(Player* bot);
std::vector<RaidGeometry::Circle> GetSpreadCircles(Player* bot, float distance);

// Eredar Twins: the main tank takes Lady Sacrolash, the first living assist tank Grand Warlock Alythess.
Unit* GetAssignedTwin(Player* bot);

// Brutallus: what the bot does about the next Meteor Slash.
struct MeteorSlashPlan
{
    enum class Role
    {
        None,      // nothing to do (the tank he hits, or a bot outside the cone that does not soak)
        Soak,      // stand at `spot` in front of him
        Avoid,     // stay out of the cone
        TakeOver   // the second tank: step in front at `spot` and taunt
    };

    Role role = Role::None;
    Unit* boss = nullptr;
    RaidGeometry::Point spot{ 0.0f, 0.0f };
};

MeteorSlashPlan GetMeteorSlashPlan(Player* bot);
// The bot is not where its plan puts it: a soaker away from its spot or outside the cone, a bystander inside the
// cone (with margin), the second tank not yet in front of him in melee range.
bool MeteorPlanNeedsMove(Player* bot, MeteorSlashPlan const& plan);

// Felmyst's phase as the raid sees it (Ground before she was seen on the ground in this fight).
SunwellLogic::FelmystPhase GetFelmystPhase(Player* bot);
// Where she last stood on the ground; false before she was seen there.
bool GetFelmystRally(Player* bot, RaidGeometry::Point& rally);

// The Demonic Vapor or Volatile Felfire Fiend that chases the bot, or nullptr.
Unit* GetChaser(Player* bot);
// The middle of the other living raid members near the bot (the bot's own position without any).
RaidGeometry::Point GetRaidCenter(Player* bot);

// Grand Warlock Alythess while Pyrogenics is on her and the bot has a way to take it off (Spellsteal, Purge, Dispel
// Magic, Tranquilizing Shot), or nullptr.
Unit* GetPyrogenicsTarget(Player* bot);
}

#endif
