/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_OSTRIGGERS_H
#define PLAYERBOTS_OSTRIGGERS_H

#include "Trigger.h"

// AC Ai/Raid/OS/OSTriggers.h enum ObsidianSanctumIDs, the ids the strategy reads. Checked for 5.4.8: the creatures in
// creature_template, the portal in gameobject_template, the spells in Spell.dbc and the core's boss_sartharion.cpp.
enum ObsidianSanctumIDs
{
    NPC_SARTHARION                              = 28860,
    NPC_SHADRON                                 = 30451,
    NPC_TENEBRON                                = 30452,
    NPC_VESPERON                                = 30449,

    // Sartharion with drakes: each drake alive buffs him and the raid with its power until it dies
    SPELL_POWER_OF_TENEBRON                     = 61248,
    SPELL_POWER_OF_VESPERON                     = 61251,
    SPELL_POWER_OF_SHADRON                      = 58105,
    // On Sartharion while the Acolyte of Shadron lives (boss_sartharion.cpp SPELL_GIFT_OF_TWILIGTH_SAR)
    SPELL_GIFT_OF_TWILIGHT_FIRE                 = 58766,

    NPC_FLAME_TSUNAMI                           = 30616,
    NPC_TWILIGHT_FISSURE                        = 30641,
    GO_TWILIGHT_PORTAL                          = 193988,
    SPELL_TWILIGHT_SHIFT                        = 57874,
};

uint32 constexpr OS_MAP_ID = 615;

// AC Ai/Raid/OS/OSTriggers.h class SartharionTankTrigger
class SartharionTankTrigger : public Trigger
{
public:
    SartharionTankTrigger(PlayerbotAI* botAI) : Trigger(botAI, "sartharion tank") {}
    bool IsActive() override;
};

// AC Ai/Raid/OS/OSTriggers.h class FlameTsunamiTrigger
class FlameTsunamiTrigger : public Trigger
{
public:
    FlameTsunamiTrigger(PlayerbotAI* botAI) : Trigger(botAI, "flame tsunami") {}
    bool IsActive() override;
};

// AC Ai/Raid/OS/OSTriggers.h class TwilightFissureTrigger
class TwilightFissureTrigger : public Trigger
{
public:
    TwilightFissureTrigger(PlayerbotAI* botAI) : Trigger(botAI, "twilight fissure") {}
    bool IsActive() override;
};

// AC Ai/Raid/OS/OSTriggers.h class SartharionDpsTrigger
class SartharionDpsTrigger : public Trigger
{
public:
    SartharionDpsTrigger(PlayerbotAI* botAI) : Trigger(botAI, "sartharion dps") {}
    bool IsActive() override;
};

// AC Ai/Raid/OS/OSTriggers.h class SartharionMeleePositioningTrigger
class SartharionMeleePositioningTrigger : public Trigger
{
public:
    SartharionMeleePositioningTrigger(PlayerbotAI* botAI) : Trigger(botAI, "sartharion melee positioning") {}
    bool IsActive() override;
};

// AC Ai/Raid/OS/OSTriggers.h class TwilightPortalEnterTrigger
class TwilightPortalEnterTrigger : public Trigger
{
public:
    TwilightPortalEnterTrigger(PlayerbotAI* botAI) : Trigger(botAI, "twilight portal enter") {}
    bool IsActive() override;
};

#endif
