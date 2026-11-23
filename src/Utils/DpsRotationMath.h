/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_DPSROTATIONMATH_H
#define _PLAYERBOT_DPSROTATIONMATH_H

// Decision rules of the damage class sheets (docs/class-sheets/), free of game types so that tests/ can run them. The
// class triggers read the game state and ask these functions; the numbers come from the sheets' priority lists.

#include <cstdint>

namespace DpsRotationMath
{
// Arms warrior, sheet gap item 10 (expert lines from level 81, when Colossus Smash is learned).
namespace Warrior
{
constexpr std::uint8_t COLOSSUS_SMASH_LEVEL = 81;
// Heroic Strike before the Colossus Smash level: the levelling dump (sheet line 5).
constexpr std::uint32_t LEVELLING_DUMP_RAGE = 60;
// From 81, outside the window: pool rage for the next window and dump only near the cap (level 90 list).
constexpr std::uint32_t POOL_DUMP_RAGE = 100;
// Inside the window Heroic Strike goes freely (both guides); keep one Slam's rage for the global cooldown (derived).
constexpr std::uint32_t SLAM_RAGE = 25;
constexpr std::uint32_t WINDOW_DUMP_RAGE = SLAM_RAGE + 30;  // Heroic Strike costs 30

// The window rules are expert lines of the sheet; other bots keep the levelling dump.
inline bool WindowRules(std::uint8_t level, bool expert)
{
    return expert && level >= COLOSSUS_SMASH_LEVEL;
}

inline std::uint32_t HeroicStrikeRage(std::uint8_t level, bool expert, bool windowUp)
{
    if (!WindowRules(level, expert))
        return LEVELLING_DUMP_RAGE;
    return windowUp ? WINDOW_DUMP_RAGE : POOL_DUMP_RAGE;
}

// Inside the window Slam beats Overpower (wowtbc, Arms level 90 list); outside it Overpower stays first.
inline bool SlamBeforeOverpower(std::uint8_t level, bool expert, bool windowUp, std::uint32_t rage)
{
    return WindowRules(level, expert) && windowUp && rage >= SLAM_RAGE;
}

// Execute (5308) costs 30 rage. The level 90 list casts it freely inside the window and from 100 rage outside it, so the
// rage spent on a plain Execute does not starve the next window's Slams and Mortal Strike (sheet gap item 10).
constexpr std::uint32_t EXECUTE_RAGE = 30;
constexpr std::uint32_t EXECUTE_POOL_RAGE = 100;

inline std::uint32_t ExecuteRage(std::uint8_t level, bool expert, bool windowUp)
{
    if (!WindowRules(level, expert) || windowUp)
        return EXECUTE_RAGE;
    return EXECUTE_POOL_RAGE;
}
}  // namespace Warrior
// Demonology and Destruction warlock, sheet gap items 14 and 15 (expert lines).
namespace Warlock
{
// Soul Fire at 9 or more Molten Core stacks (wowtbc); Method saves the procs for Metamorphosis.
constexpr std::uint32_t MOLTEN_CORE_SPEND_STACKS = 9;
// Ember Tap below 45 percent health while Sacrificial Pact and Unending Resolve are down (Destruction sheet, derived).
constexpr float EMBER_TAP_HEALTH_PCT = 45.0f;
// The core stores Burning Embers in tenths: 10 power is one ember, the cost of Ember Tap.
constexpr std::uint32_t EMBER_POWER = 10;

inline bool SoulFireOnMoltenCore(bool expert, std::uint32_t stacks, bool inMetamorphosis)
{
    if (!stacks)
        return false;
    return !expert || inMetamorphosis || stacks >= MOLTEN_CORE_SPEND_STACKS;
}

inline bool EmberTapUseful(bool expert, float healthPct, std::uint32_t emberPower, bool defensiveUp)
{
    return expert && healthPct < EMBER_TAP_HEALTH_PCT && emberPower >= EMBER_POWER && !defensiveUp;
}

// Affliction sheet line 14 and Destruction's Immolate (expert): recast Corruption, Agony, Unstable Affliction, Doom and
// Immolate with about one tick left, so the DoT never drops between two casts. Method's 40 percent left would throw
// away more than a third of the duration on 5.4.8, where a recast restarts the full duration (no pandemic rule before
// 6.0; inference, to confirm on Wowhead), so the window is the one the Shadow priest uses.
constexpr std::uint32_t DOT_REFRESH_MS = 3000;

inline std::uint32_t DotRefreshMs(bool expert)
{
    return expert ? DOT_REFRESH_MS : 0;
}
}  // namespace Warlock
// Windwalker monk, sheet gap item 9: Legacy of the White Tiger is one of the group's critical strike buffs, which do
// not stack; the monk's own buff comes first, as the "party member without aura" value expects.
namespace Monk
{
constexpr char const* CRITICAL_STRIKE_BUFFS =
    "legacy of the white tiger,leader of the pack,arcane brilliance,dalaran brilliance,furious howl,terrifying roar,"
    "fearless roar,still water";
}  // namespace Monk
// Fire mage, sheet line 5 (good): a hard-cast Pyroblast opens a fight the mage starts alone (both guides pre-cast it).
// In a group the tank opens, so the mage never pulls with it.
namespace Mage
{
inline bool PyroblastOpener(bool inGroup, bool targetInCombat, bool targetIsPlayer, bool knowsPyroblast)
{
    return knowsPyroblast && !inGroup && !targetInCombat && !targetIsPlayer;
}

// Fire sheet rotation item 5 and the level 87 line (both guides): Presence of Mind (12043, tier 1 talent) turns the next
// Pyroblast into an instant on a pack of elites or a boss; Alter Time (108978, level 87) is pressed while a free
// Pyroblast is up, so the proc comes back when the mage returns in time. Against a player both are burst too.
constexpr std::uint8_t ALTER_TIME_LEVEL = 87;

inline bool PresenceOfMindNow(bool knowsTalent, bool bigFight, bool presenceUp)
{
    return knowsTalent && bigFight && !presenceUp;
}

inline bool AlterTimeNow(std::uint8_t level, bool goodTier, bool bigFight, bool freePyroblastUp, bool alterTimeUp)
{
    return level >= ALTER_TIME_LEVEL && goodTier && bigFight && freePyroblastUp && !alterTimeUp;
}
}  // namespace Mage
// Hunter sheets, traps (BM line 12 and 18, SV gap item 10): Trap Launcher (77769, level 48) is a toggle that throws the
// traps to a point within 40 yd; without it a trap lands at the hunter's feet, where a pack never runs. Good and expert
// bots keep it on; an expert also throws Explosive Trap into a pack of three around its target (both guides: on cooldown
// at two or more targets, three keeps it off a pair a single target spell kills as fast).
namespace Hunter
{
constexpr std::uint8_t TRAP_LAUNCHER_LEVEL = 48;
constexpr unsigned TRAP_PACK = 3;
constexpr float TRAP_LAUNCH_RANGE_YD = 40.0f;

inline bool KeepTrapLauncher(std::uint8_t level, bool goodTier, bool launcherUp)
{
    return goodTier && level >= TRAP_LAUNCHER_LEVEL && !launcherUp;
}

inline bool ExplosiveTrapAtPack(bool expert, bool launcherUp, unsigned packAroundTarget, float targetDistance)
{
    return expert && launcherUp && packAroundTarget >= TRAP_PACK && targetDistance > 0.0f &&
        targetDistance <= TRAP_LAUNCH_RANGE_YD;
}
}  // namespace Hunter
// Rogue sheets, level 80 burst windows (Combat line 80, Subtlety gap item 10; wowtbc): Killing Spree at low energy with
// Adrenaline Rush and Shadow Blades down (the 3 s spree wastes no regeneration then); Shadow Dance from 80 at 75 energy
// or more outside Find Weakness, so its Ambushes renew the armor debuff; Subtlety's Shadow Blades only while Find
// Weakness is down, so the two windows do not overlap. Good and expert lines; casual bots keep the plain buff checks.
namespace Rogue
{
constexpr std::uint32_t KILLING_SPREE_ENERGY = 40;
constexpr std::uint8_t SHADOW_DANCE_LEVEL = 80;
constexpr std::uint32_t SHADOW_DANCE_ENERGY = 75;

inline bool KillingSpreeNow(bool goodTier, std::uint32_t energy, bool adrenalineRushUp, bool shadowBladesUp)
{
    if (adrenalineRushUp || shadowBladesUp)
        return false;
    return !goodTier || energy < KILLING_SPREE_ENERGY;
}

inline bool ShadowDanceNow(std::uint8_t level, bool goodTier, std::uint32_t energy, bool findWeaknessUp, bool stealthed)
{
    return goodTier && level >= SHADOW_DANCE_LEVEL && energy >= SHADOW_DANCE_ENERGY && !findWeaknessUp && !stealthed;
}

inline bool SubtletyShadowBladesNow(bool goodTier, bool findWeaknessUp)
{
    return !goodTier || !findWeaknessUp;
}
}  // namespace Rogue
// Frost and Unholy death knight sheets (level 80 line, Method): Army of the Dead (42650, a 4 s channel) only before the
// pull, on a boss the group is ready to pull (expert line). In a fight the existing boost edge stays.
namespace DeathKnight
{
constexpr std::uint8_t ARMY_LEVEL = 80;
constexpr float ARMY_PREPULL_RANGE_YD = 40.0f;

inline bool PrePullArmy(std::uint8_t level, bool expert, bool damageSpec, bool bossMark, bool groupReady,
    float markDistance)
{
    return expert && damageSpec && level >= ARMY_LEVEL && bossMark && groupReady && markDistance > 0.0f &&
        markDistance <= ARMY_PREPULL_RANGE_YD;
}
}  // namespace DeathKnight
// Shadow priest, sheet line 15 and gap item 12 (expert): refresh Shadow Word: Pain and Vampiric Touch with about one
// Mind Flay channel left (wowtbc: before the final tick), so the DoT never drops between two casts.
namespace Priest
{
constexpr std::int32_t DOT_REFRESH_MS = 3000;

inline bool RefreshDot(bool expert, std::int32_t remainingMs)
{
    return expert && remainingMs > 0 && remainingMs <= DOT_REFRESH_MS;
}

// Sheet line 14 (expert): a Mind Flay channel of three ticks is stopped after its second tick when Mind Blast is ready,
// since the first two ticks carry two thirds of its damage and Mind Blast makes an orb (derived).
constexpr std::int32_t MIND_FLAY_TICKS = 3;

inline bool ClipMindFlay(bool expert, bool mindBlastReady, std::int32_t remainingMs, std::int32_t channelMs)
{
    return expert && mindBlastReady && channelMs > 0 && remainingMs > 0 &&
        remainingMs * MIND_FLAY_TICKS <= channelMs;
}

// Sheet gap item 7: Mind Sear hits every enemy within 10 yd of its target, so it waits while that area holds a hostile
// creature that is not in the fight (the next pack) or one under a crowd control that damage breaks.
inline bool MindSearSafe(unsigned idleHostiles, unsigned controlled)
{
    return idleHostiles == 0 && controlled == 0;
}
}  // namespace Priest
}  // namespace DpsRotationMath

#endif
