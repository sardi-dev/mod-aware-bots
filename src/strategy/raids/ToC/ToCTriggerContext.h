/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef PLAYERBOTS_TOCTRIGGERCONTEXT_H
#define PLAYERBOTS_TOCTRIGGERCONTEXT_H

#include "NamedObjectContext.h"
#include "ToCTriggers.h"

class RaidToCTriggerContext : public NamedObjectContext<Trigger>
{
public:
    RaidToCTriggerContext()
    {
        creators["toc mark priority"] = &RaidToCTriggerContext::mark_priority;
        creators["toc attack priority"] = &RaidToCTriggerContext::attack_priority;
        creators["toc main tank target"] = &RaidToCTriggerContext::main_tank_target;
        creators["toc assist tank target"] = &RaidToCTriggerContext::assist_tank_target;
        creators["toc ground hazard"] = &RaidToCTriggerContext::ground_hazard;
        creators["toc icehowl charge"] = &RaidToCTriggerContext::icehowl_charge;
        creators["toc legion flame"] = &RaidToCTriggerContext::legion_flame;
        creators["toc incinerate flesh"] = &RaidToCTriggerContext::incinerate_flesh;
        creators["toc need essence"] = &RaidToCTriggerContext::need_essence;
        creators["toc twin orbs"] = &RaidToCTriggerContext::twin_orbs;
        creators["toc spike pursuit"] = &RaidToCTriggerContext::spike_pursuit;
        creators["toc burrower near boss"] = &RaidToCTriggerContext::burrower_near_boss;
    }

private:
    static Trigger* mark_priority(PlayerbotAI* botAI)
    {
        return new ToCTrigger(botAI, "toc mark priority", &ToCChecks::MarkPriority);
    }
    static Trigger* attack_priority(PlayerbotAI* botAI)
    {
        return new ToCTrigger(botAI, "toc attack priority", &ToCChecks::AttackPriority);
    }
    static Trigger* main_tank_target(PlayerbotAI* botAI)
    {
        return new ToCTrigger(botAI, "toc main tank target", &ToCChecks::MainTankTarget);
    }
    static Trigger* assist_tank_target(PlayerbotAI* botAI)
    {
        return new ToCTrigger(botAI, "toc assist tank target", &ToCChecks::AssistTankTarget);
    }
    static Trigger* ground_hazard(PlayerbotAI* botAI)
    {
        return new ToCTrigger(botAI, "toc ground hazard", &ToCChecks::GroundHazard);
    }
    static Trigger* icehowl_charge(PlayerbotAI* botAI)
    {
        return new ToCTrigger(botAI, "toc icehowl charge", &ToCChecks::IcehowlCharge);
    }
    static Trigger* legion_flame(PlayerbotAI* botAI)
    {
        return new ToCTrigger(botAI, "toc legion flame", &ToCChecks::LegionFlame);
    }
    static Trigger* incinerate_flesh(PlayerbotAI* botAI)
    {
        return new ToCTrigger(botAI, "toc incinerate flesh", &ToCChecks::IncinerateFlesh);
    }
    static Trigger* need_essence(PlayerbotAI* botAI)
    {
        return new ToCTrigger(botAI, "toc need essence", &ToCChecks::NeedEssence);
    }
    static Trigger* twin_orbs(PlayerbotAI* botAI)
    {
        return new ToCTrigger(botAI, "toc twin orbs", &ToCChecks::TwinOrbs);
    }
    static Trigger* spike_pursuit(PlayerbotAI* botAI)
    {
        return new ToCTrigger(botAI, "toc spike pursuit", &ToCChecks::SpikePursuit);
    }
    static Trigger* burrower_near_boss(PlayerbotAI* botAI)
    {
        return new ToCTrigger(botAI, "toc burrower near boss", &ToCChecks::BurrowerNearBoss);
    }
};

#endif
