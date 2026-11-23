/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ONYTRIGGERS_H
#define PLAYERBOTS_ONYTRIGGERS_H

#include "PlayerbotAI.h"
#include "Trigger.h"

// Ids of the core's boss_onyxia script (src/server/scripts/Kalimdor/OnyxiasLair/boss_onyxia.cpp), the same as AC's.
enum OnyxiaIds : uint32
{
    NPC_ONYXIAN_WHELP = 11262,

    SPELL_ONYXIA_FIREBALL = 18392,

    // Deep Breath, named by the direction she flies
    SPELL_ONYXIA_BREATH_NORTH_TO_SOUTH = 17086,
    SPELL_ONYXIA_BREATH_SOUTH_TO_NORTH = 18351,
    SPELL_ONYXIA_BREATH_EAST_TO_WEST = 18576,
    SPELL_ONYXIA_BREATH_WEST_TO_EAST = 18609,
    SPELL_ONYXIA_BREATH_SOUTHEAST_TO_NORTHWEST = 18564,
    SPELL_ONYXIA_BREATH_NORTHWEST_TO_SOUTHEAST = 18584,
    SPELL_ONYXIA_BREATH_SOUTHWEST_TO_NORTHEAST = 18596,
    SPELL_ONYXIA_BREATH_NORTHEAST_TO_SOUTHWEST = 18617
};

// 5.4.8: AC asks Unit::IsFlying(). In this core SetCanFly(true), which the boss script calls for phase 2, sets only
// MOVEMENTFLAG_CAN_FLY, and Creature::UpdateMovementFlags sets the flying flags from the height above the map floor,
// so IsFlying() is no reliable phase marker. The script's own heights are: floor -87.7 (spawn and landing point of
// phase 3), hover about -58 (phase 2 point 11) and -55 (breath points).
float constexpr ONYXIA_AIRBORNE_MIN_Z = -75.0f;

inline bool IsOnyxiaAirborne(Unit const* boss) { return boss->GetPositionZ() > ONYXIA_AIRBORNE_MIN_Z; }

// Mechanics
// AC Ai/Raid/Ony/OnyTriggers.h class OnyxiaDeepBreathTrigger
class OnyxiaDeepBreathTrigger : public Trigger
{
public:
    OnyxiaDeepBreathTrigger(PlayerbotAI* botAI);
    bool IsActive() override;
};

// AC Ai/Raid/Ony/OnyTriggers.h class OnyxiaNearTailTrigger
class OnyxiaNearTailTrigger : public Trigger
{
public:
    OnyxiaNearTailTrigger(PlayerbotAI* botAI);
    bool IsActive() override;
};

// AC Ai/Raid/Ony/OnyTriggers.h class RaidOnyxiaFireballSplashTrigger
class RaidOnyxiaFireballSplashTrigger : public Trigger
{
public:
    RaidOnyxiaFireballSplashTrigger(PlayerbotAI* botAI);
    bool IsActive() override;
};

// AC Ai/Raid/Ony/OnyTriggers.h class RaidOnyxiaWhelpsSpawnTrigger
class RaidOnyxiaWhelpsSpawnTrigger : public Trigger
{
public:
    RaidOnyxiaWhelpsSpawnTrigger(PlayerbotAI* botAI);
    bool IsActive() override;
};

// AC Ai/Raid/Ony/OnyTriggers.h class OnyxiaAvoidEggsTrigger
class OnyxiaAvoidEggsTrigger : public Trigger
{
public:
    OnyxiaAvoidEggsTrigger(PlayerbotAI* botAI);
    bool IsActive() override;
};

#endif
