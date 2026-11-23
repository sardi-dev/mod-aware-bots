/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_EOETRIGGERS_H
#define PLAYERBOTS_EOETRIGGERS_H

#include "Trigger.h"

class Player;
class Unit;

// AC Ai/Raid/EoE/EoETriggers.h enum EyeOfEternityIDs, the ids the strategy reads. Checked for 5.4.8: creatures in
// creature_template (the Wyrmrest Skytalon's spell1-4 are these four drake spells) and the core's eye_of_eternity.h,
// spells in Spell.dbc.
enum EyeOfEternityIDs
{
    NPC_MALYGOS                         = 28859,
    NPC_POWER_SPARK                     = 30084,
    NPC_NEXUS_LORD                      = 30245,
    NPC_SCION_OF_ETERNITY               = 30249,
    NPC_WYRMREST_SKYTALON               = 30161,

    // Drake abilities: damage
    SPELL_FLAME_SPIKE                   = 56091,
    SPELL_ENGULF_IN_FLAMES              = 56092,
    // Drake abilities: healing
    SPELL_REVIVIFY                      = 57090,
    SPELL_LIFE_BURST                    = 57143,
};

uint32 constexpr EOE_MAP_ID = 616;

// AC Ai/Raid/EoE/EoETriggers.h class MalygosTrigger
class MalygosTrigger : public Trigger
{
public:
    MalygosTrigger(PlayerbotAI* botAI) : Trigger(botAI, "malygos") {}
    bool IsActive() override;
    static uint8 getPhase(Player* bot, Unit* boss);
};

// AC Ai/Raid/EoE/EoETriggers.h class PowerSparkTrigger (registered, used by no strategy, as in AC)
class PowerSparkTrigger : public Trigger
{
public:
    PowerSparkTrigger(PlayerbotAI* botAI) : Trigger(botAI, "power spark") {}
    bool IsActive() override;
};

#endif
