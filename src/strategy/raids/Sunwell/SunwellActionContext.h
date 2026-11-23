/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef PLAYERBOTS_SUNWELLACTIONCONTEXT_H
#define PLAYERBOTS_SUNWELLACTIONCONTEXT_H

#include "NamedObjectContext.h"
#include "SunwellActions.h"

class RaidSunwellActionContext : public NamedObjectContext<Action>
{
public:
    RaidSunwellActionContext()
    {
        creators["sunwell leave hazard"] = &RaidSunwellActionContext::leave_hazard;
        creators["sunwell leave kalecgos cone"] = &RaidSunwellActionContext::leave_kalecgos_cone;
        creators["sunwell attack sathrovarr"] = &RaidSunwellActionContext::attack_sathrovarr;
        creators["sunwell attack priority add"] = &RaidSunwellActionContext::attack_priority_add;
        creators["sunwell interrupt"] = &RaidSunwellActionContext::interrupt;
        creators["sunwell remove pyrogenics"] = &RaidSunwellActionContext::remove_pyrogenics;
        creators["sunwell twins tank split"] = &RaidSunwellActionContext::twins_tank_split;
        creators["sunwell spread ranged"] = &RaidSunwellActionContext::spread_ranged;
        creators["sunwell meteor slash position"] = &RaidSunwellActionContext::meteor_slash_position;
        creators["sunwell brutallus take over"] = &RaidSunwellActionContext::brutallus_take_over;
        creators["sunwell kite chaser"] = &RaidSunwellActionContext::kite_chaser;
        creators["sunwell felmyst regroup"] = &RaidSunwellActionContext::felmyst_regroup;
    }

private:
    static Action* leave_hazard(PlayerbotAI* botAI) { return new SunwellLeaveHazardAction(botAI); }
    static Action* leave_kalecgos_cone(PlayerbotAI* botAI) { return new SunwellLeaveKalecgosConeAction(botAI); }
    static Action* attack_sathrovarr(PlayerbotAI* botAI) { return new SunwellAttackSathrovarrAction(botAI); }
    static Action* attack_priority_add(PlayerbotAI* botAI) { return new SunwellAttackPriorityAddAction(botAI); }
    static Action* interrupt(PlayerbotAI* botAI) { return new SunwellInterruptAction(botAI); }
    static Action* remove_pyrogenics(PlayerbotAI* botAI) { return new SunwellRemovePyrogenicsAction(botAI); }
    static Action* twins_tank_split(PlayerbotAI* botAI) { return new SunwellTwinsTankSplitAction(botAI); }
    static Action* spread_ranged(PlayerbotAI* botAI) { return new SunwellSpreadRangedAction(botAI); }
    static Action* meteor_slash_position(PlayerbotAI* botAI) { return new SunwellMeteorSlashPositionAction(botAI); }
    static Action* brutallus_take_over(PlayerbotAI* botAI) { return new SunwellBrutallusTakeOverAction(botAI); }
    static Action* kite_chaser(PlayerbotAI* botAI) { return new SunwellKiteChaserAction(botAI); }
    static Action* felmyst_regroup(PlayerbotAI* botAI) { return new SunwellFelmystRegroupAction(botAI); }
};

#endif
