/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef PLAYERBOTS_SUNWELLTRIGGERS_H
#define PLAYERBOTS_SUNWELLTRIGGERS_H

#include "Trigger.h"

class SunwellStandingInHazardTrigger : public Trigger
{
public:
    SunwellStandingInHazardTrigger(PlayerbotAI* botAI) : Trigger(botAI, "sunwell standing in hazard") {}
    bool IsActive() override;
};

class SunwellInKalecgosConeTrigger : public Trigger
{
public:
    SunwellInKalecgosConeTrigger(PlayerbotAI* botAI) : Trigger(botAI, "sunwell in kalecgos cone") {}
    bool IsActive() override;
};

class SunwellInSpectralRealmTrigger : public Trigger
{
public:
    SunwellInSpectralRealmTrigger(PlayerbotAI* botAI) : Trigger(botAI, "sunwell in spectral realm") {}
    bool IsActive() override;
};

class SunwellPriorityAddTrigger : public Trigger
{
public:
    SunwellPriorityAddTrigger(PlayerbotAI* botAI) : Trigger(botAI, "sunwell priority add") {}
    bool IsActive() override;
};

class SunwellInterruptTrigger : public Trigger
{
public:
    SunwellInterruptTrigger(PlayerbotAI* botAI) : Trigger(botAI, "sunwell interrupt") {}
    bool IsActive() override;
};

class SunwellPyrogenicsTrigger : public Trigger
{
public:
    SunwellPyrogenicsTrigger(PlayerbotAI* botAI) : Trigger(botAI, "sunwell pyrogenics") {}
    bool IsActive() override;
};

class SunwellTwinsTankSplitTrigger : public Trigger
{
public:
    SunwellTwinsTankSplitTrigger(PlayerbotAI* botAI) : Trigger(botAI, "sunwell twins tank split") {}
    bool IsActive() override;
};

class SunwellRangedSpreadTrigger : public Trigger
{
public:
    SunwellRangedSpreadTrigger(PlayerbotAI* botAI) : Trigger(botAI, "sunwell ranged spread") {}
    bool IsActive() override;
};

class SunwellMeteorSlashPositionTrigger : public Trigger
{
public:
    SunwellMeteorSlashPositionTrigger(PlayerbotAI* botAI) : Trigger(botAI, "sunwell meteor slash position") {}
    bool IsActive() override;
};

class SunwellBrutallusTakeOverTrigger : public Trigger
{
public:
    SunwellBrutallusTakeOverTrigger(PlayerbotAI* botAI) : Trigger(botAI, "sunwell brutallus take over") {}
    bool IsActive() override;
};

class SunwellChasedTrigger : public Trigger
{
public:
    SunwellChasedTrigger(PlayerbotAI* botAI) : Trigger(botAI, "sunwell chased") {}
    bool IsActive() override;
};

class SunwellFelmystRegroupTrigger : public Trigger
{
public:
    SunwellFelmystRegroupTrigger(PlayerbotAI* botAI) : Trigger(botAI, "sunwell felmyst regroup") {}
    bool IsActive() override;
};

#endif
