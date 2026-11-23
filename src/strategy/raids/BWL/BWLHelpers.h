/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// AC Ai/Raid/BWL/BWLHelpers.h. Checked against the core's BlackwingLair scripts and the client data on 2026-11-16: the
// spell, object and creature ids below exist, except the core's own 42013 (see Razorgore). What changed:
// - The Onyxia Scale Cloak aura AC hands every bot is a cheat and is gone (the id stays for reference).
// - The boss resistance triggers are dropped (no resistance auras on 5.4.8).
// - Only rogues disarm Suppression Devices (AC's raid cheat let anyone).
// - Hourglass Sand is used from the bags (AC cast its spell without the item).
// - Burning Adrenaline is the core's 23620.
// - The boss rear flank action lives in this folder, and the Wyrmguard checks walk the bot's hostile references
//   (SkyFire has no threatened-by-me list).

#ifndef PLAYERBOTS_BWLHELPERS_H
#define PLAYERBOTS_BWLHELPERS_H

#include "Player.h"
#include "PlayerbotAI.h"

namespace BlackwingLairHelpers
{
    enum class BlackwingLairSpells : uint32
    {
        // General
        SPELL_ONYXIA_SCALE_CLOAK = 22683,

        // Razorgore the Untamed. The core's Orb of Domination casts 42013, which the 5.4.8 client data lacks, so the
        // orb cannot take control here; both ids are checked in case the core moves to the client's 19832.
        SPELL_MINDCONTROL = 19832,
        SPELL_MINDCONTROL_CORE = 42013,

        // Vaelastrasz the Corrupt: the core casts 23620 (AC's 18173 has the same effects).
        SPELL_BURNING_ADRENALINE = 23620,

        // Chromaggus
        SPELL_BROOD_AFFLICTION_BRONZE = 23170,
        SPELL_HOURGLASS_SAND = 23645,

        // Nefarian
        SPELL_WILD_MAGIC = 23410
    };

    enum class BlackwingLairItems : uint32
    {
        // Chromaggus: the item behind SPELL_HOURGLASS_SAND
        ITEM_HOURGLASS_SAND = 19183
    };

    enum class BlackwingLairGameObjects : uint32
    {
        // General
        GO_SUPPRESSION_DEVICE = 179784,

        // Razorgore the Untamed
        GO_BLACK_DRAGON_EGG = 177807
    };

    enum class BlackwingLairNPCs : uint32
    {
        // Broodlord Lashlayer
        NPC_CORRUPTED_RED_WHELP = 14022,
        NPC_CORRUPTED_GREEN_WHELP = 14023,
        NPC_CORRUPTED_BLUE_WHELP = 14024,
        NPC_CORRUPTED_BRONZE_WHELP = 14025,

        // Trash
        NPC_DEATH_TALON_WYRMGUARD = 12460
    };

    constexpr float BROODLORD_SAFE_DISTANCE = 18.0f;

    bool IsActiveSuppressionDeviceInRange(GameObject const* go, Player const* bot);
    bool AreRazorgoreEggsAlive(PlayerbotAI* botAI);
    bool IsRazorgoreOffTank(Player* bot);
    bool IsNonBABotNearPosition(Player const* bot, Position const& position, float distance);
    bool IsCorruptedWhelp(Unit const* unit);
}

#endif
