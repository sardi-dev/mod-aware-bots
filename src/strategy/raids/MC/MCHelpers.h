/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// AC Ai/Raid/MC/MCHelpers.h. Checked against the core's MoltenCore scripts and the client data on 2026-11-16: Inferno
// 19695, Living Bomb 20475, Golemagg's Trust 20553 and Magma Splash 13880 exist, Core Rager 11672, Core Hound 11671
// and Majordomo 12018 are the core's entries. What changed: the boss resistance triggers are dropped (5.4.8 has no
// resistance auras), the lava test reads the map's liquid status (SkyFire has no cached liquid data on the unit), and
// target exclusions apply to the tank target only (this module has no area-of-effect exclusion type).

#ifndef PLAYERBOTS_MCHELPERS_H
#define PLAYERBOTS_MCHELPERS_H

#include "Common.h"

class Player;

namespace MoltenCoreHelpers
{
enum MoltenCoreNPCs
{
    // Golemagg
    NPC_CORE_RAGER = 11672,

    // Majordomo Executus
    NPC_MAJORDOMO_EXECUTUS = 12018,

    // Core Hound (trash)
    NPC_CORE_HOUND = 11671,
};
enum MoltenCoreSpells
{
    // Baron Geddon
    SPELL_INFERNO = 19695,
    SPELL_LIVING_BOMB = 20475,

    // Golemagg
    SPELL_GOLEMAGGS_TRUST = 20553,
    SPELL_MAGMA_SPLASH = 13880,
};

constexpr uint32 MAGMA_SPLASH_BACK_OFF_STACKS = 20;
constexpr float MAGMA_SPLASH_BACK_OFF_DISTANCE = 12.0f;

// Shazzrah's Arcane Explosion radius
constexpr float ARCANE_EXPLOSION_DISTANCE = 26.0f;

// AC reads Unit::GetLiquidData: whether the unit is over magma, and whether it stands in it.
bool IsOverMagma(Player* player);
bool IsInMagma(Player* player);
}

#endif
