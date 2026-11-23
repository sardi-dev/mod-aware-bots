/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef PLAYERBOTS_SUNWELLTRIGGERCONTEXT_H
#define PLAYERBOTS_SUNWELLTRIGGERCONTEXT_H

#include "NamedObjectContext.h"
#include "SunwellTriggers.h"

class RaidSunwellTriggerContext : public NamedObjectContext<Trigger>
{
public:
    RaidSunwellTriggerContext()
    {
        creators["sunwell standing in hazard"] = &RaidSunwellTriggerContext::standing_in_hazard;
        creators["sunwell in kalecgos cone"] = &RaidSunwellTriggerContext::in_kalecgos_cone;
        creators["sunwell in spectral realm"] = &RaidSunwellTriggerContext::in_spectral_realm;
        creators["sunwell priority add"] = &RaidSunwellTriggerContext::priority_add;
        creators["sunwell interrupt"] = &RaidSunwellTriggerContext::interrupt;
        creators["sunwell pyrogenics"] = &RaidSunwellTriggerContext::pyrogenics;
        creators["sunwell twins tank split"] = &RaidSunwellTriggerContext::twins_tank_split;
        creators["sunwell ranged spread"] = &RaidSunwellTriggerContext::ranged_spread;
        creators["sunwell meteor slash position"] = &RaidSunwellTriggerContext::meteor_slash_position;
        creators["sunwell brutallus take over"] = &RaidSunwellTriggerContext::brutallus_take_over;
        creators["sunwell chased"] = &RaidSunwellTriggerContext::chased;
        creators["sunwell felmyst regroup"] = &RaidSunwellTriggerContext::felmyst_regroup;
    }

private:
    static Trigger* standing_in_hazard(PlayerbotAI* botAI) { return new SunwellStandingInHazardTrigger(botAI); }
    static Trigger* in_kalecgos_cone(PlayerbotAI* botAI) { return new SunwellInKalecgosConeTrigger(botAI); }
    static Trigger* in_spectral_realm(PlayerbotAI* botAI) { return new SunwellInSpectralRealmTrigger(botAI); }
    static Trigger* priority_add(PlayerbotAI* botAI) { return new SunwellPriorityAddTrigger(botAI); }
    static Trigger* interrupt(PlayerbotAI* botAI) { return new SunwellInterruptTrigger(botAI); }
    static Trigger* pyrogenics(PlayerbotAI* botAI) { return new SunwellPyrogenicsTrigger(botAI); }
    static Trigger* twins_tank_split(PlayerbotAI* botAI) { return new SunwellTwinsTankSplitTrigger(botAI); }
    static Trigger* ranged_spread(PlayerbotAI* botAI) { return new SunwellRangedSpreadTrigger(botAI); }
    static Trigger* meteor_slash_position(PlayerbotAI* botAI) { return new SunwellMeteorSlashPositionTrigger(botAI); }
    static Trigger* brutallus_take_over(PlayerbotAI* botAI) { return new SunwellBrutallusTakeOverTrigger(botAI); }
    static Trigger* chased(PlayerbotAI* botAI) { return new SunwellChasedTrigger(botAI); }
    static Trigger* felmyst_regroup(PlayerbotAI* botAI) { return new SunwellFelmystRegroupTrigger(botAI); }
};

#endif
