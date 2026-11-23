/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef PLAYERBOTS_TOCACTIONCONTEXT_H
#define PLAYERBOTS_TOCACTIONCONTEXT_H

#include "Action.h"
#include "NamedObjectContext.h"
#include "ToCActions.h"

class RaidToCActionContext : public NamedObjectContext<Action>
{
public:
    RaidToCActionContext()
    {
        creators["toc mark priority"] = &RaidToCActionContext::mark_priority;
        creators["toc attack priority"] = &RaidToCActionContext::attack_priority;
        creators["toc main tank attack"] = &RaidToCActionContext::main_tank_attack;
        creators["toc assist tank attack"] = &RaidToCActionContext::assist_tank_attack;
        creators["toc avoid hazard"] = &RaidToCActionContext::avoid_hazard;
        creators["toc dodge charge"] = &RaidToCActionContext::dodge_charge;
        creators["toc run from raid"] = &RaidToCActionContext::run_from_raid;
        creators["toc heal incinerated"] = &RaidToCActionContext::heal_incinerated;
        creators["toc take essence"] = &RaidToCActionContext::take_essence;
        creators["toc handle orbs"] = &RaidToCActionContext::handle_orbs;
        creators["toc kite spike"] = &RaidToCActionContext::kite_spike;
        creators["toc pull burrower away"] = &RaidToCActionContext::pull_burrower_away;
    }

private:
    static Action* mark_priority(PlayerbotAI* botAI) { return new ToCMarkPriorityAction(botAI); }
    static Action* attack_priority(PlayerbotAI* botAI) { return new ToCAttackPriorityAction(botAI); }
    static Action* main_tank_attack(PlayerbotAI* botAI)
    {
        return new ToCTankTargetAction(botAI, "toc main tank attack", true);
    }
    static Action* assist_tank_attack(PlayerbotAI* botAI)
    {
        return new ToCTankTargetAction(botAI, "toc assist tank attack", false);
    }
    static Action* avoid_hazard(PlayerbotAI* botAI) { return new ToCAvoidHazardAction(botAI); }
    static Action* dodge_charge(PlayerbotAI* botAI) { return new ToCDodgeChargeAction(botAI); }
    static Action* run_from_raid(PlayerbotAI* botAI) { return new ToCRunFromRaidAction(botAI); }
    static Action* heal_incinerated(PlayerbotAI* botAI) { return new ToCHealIncineratedAction(botAI); }
    static Action* take_essence(PlayerbotAI* botAI) { return new ToCTakeEssenceAction(botAI); }
    static Action* handle_orbs(PlayerbotAI* botAI) { return new ToCHandleOrbsAction(botAI); }
    static Action* kite_spike(PlayerbotAI* botAI) { return new ToCKiteSpikeAction(botAI); }
    static Action* pull_burrower_away(PlayerbotAI* botAI) { return new ToCPullBurrowerAwayAction(botAI); }
};

#endif
