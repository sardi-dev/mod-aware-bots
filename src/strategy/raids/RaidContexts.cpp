/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "RaidContexts.h"

#include "Action.h"
#include "Strategy.h"
#include "Trigger.h"
#include "Value.h"

#include "Aq20ActionContext.h"
#include "Aq20TriggerContext.h"
#include "EoEActionContext.h"
#include "EoETriggerContext.h"
#include "BTActionContext.h"
#include "BTTriggerContext.h"
#include "BWLActionContext.h"
#include "BWLTriggerContext.h"
#include "GruulActionContext.h"
#include "GruulTriggerContext.h"
#include "GruulValueContext.h"
#include "HyjalActionContext.h"
#include "HyjalTriggerContext.h"
#include "HyjalValueContext.h"
#include "ICCActionContext.h"
#include "ICCTriggerContext.h"
#include "NaxxActionContext.h"
#include "NaxxTriggerContext.h"
#include "OSActionContext.h"
#include "OSTriggerContext.h"
#include "KaraActionContext.h"
#include "KaraTriggerContext.h"
#include "MCActionContext.h"
#include "MCTriggerContext.h"
#include "MagActionContext.h"
#include "MagTriggerContext.h"
#include "MagValueContext.h"
#include "OnyActionContext.h"
#include "OnyTriggerContext.h"
#include "RSActionContext.h"
#include "RSTriggerContext.h"
#include "RaidStrategyContext.h"
#include "ToCActionContext.h"
#include "ToCTriggerContext.h"
#include "UldActionContext.h"
#include "UldTriggerContext.h"
#include "VoAActionContext.h"
#include "VoATriggerContext.h"
#include "SSCActionContext.h"
#include "SSCTriggerContext.h"
#include "SSCValueContext.h"
#include "SunwellActionContext.h"
#include "SunwellTriggerContext.h"
#include "TKActionContext.h"
#include "TKTriggerContext.h"
#include "TKValueContext.h"

// AC Bot/Engine/BuildSharedStrategyContexts.cpp:19
void RaidContexts::AddStrategyContexts(NamedObjectContextList<Strategy>& strategyContexts)
{
    strategyContexts.Add(new RaidStrategyContext());
}

// AC Bot/Engine/BuildSharedActionContexts.cpp:39-57 (raid entries, AC order; ported ones only)
void RaidContexts::AddActionContexts(NamedObjectContextList<Action>& actionContexts)
{
    actionContexts.Add(new RaidAq20ActionContext());
    actionContexts.Add(new RaidMcActionContext());
    actionContexts.Add(new RaidBwlActionContext());
    actionContexts.Add(new RaidKarazhanActionContext());
    actionContexts.Add(new RaidGruulsLairActionContext());
    actionContexts.Add(new RaidMagtheridonActionContext());
    actionContexts.Add(new RaidSscActionContext());
    actionContexts.Add(new RaidTempestKeepActionContext());
    actionContexts.Add(new RaidHyjalActionContext());
    actionContexts.Add(new RaidNaxxActionContext());
    actionContexts.Add(new RaidOsActionContext());
    actionContexts.Add(new RaidEoEActionContext());
    actionContexts.Add(new RaidVoAActionContext());
    actionContexts.Add(new RaidUlduarActionContext());
    actionContexts.Add(new RaidToCActionContext());
    actionContexts.Add(new RaidIccActionContext());
    actionContexts.Add(new RaidRsActionContext());
    actionContexts.Add(new RaidBlackTempleActionContext());
    actionContexts.Add(new RaidOnyxiaActionContext());
    actionContexts.Add(new RaidSunwellActionContext());
}

// AC Bot/Engine/BuildSharedTriggerContexts.cpp:39-57 (raid entries, AC order; ported ones only)
void RaidContexts::AddTriggerContexts(NamedObjectContextList<Trigger>& triggerContexts)
{
    triggerContexts.Add(new RaidAq20TriggerContext());
    triggerContexts.Add(new RaidMcTriggerContext());
    triggerContexts.Add(new RaidBwlTriggerContext());
    triggerContexts.Add(new RaidKarazhanTriggerContext());
    triggerContexts.Add(new RaidGruulsLairTriggerContext());
    triggerContexts.Add(new RaidMagtheridonTriggerContext());
    triggerContexts.Add(new RaidSscTriggerContext());
    triggerContexts.Add(new RaidTempestKeepTriggerContext());
    triggerContexts.Add(new RaidHyjalTriggerContext());
    triggerContexts.Add(new RaidNaxxTriggerContext());
    triggerContexts.Add(new RaidOsTriggerContext());
    triggerContexts.Add(new RaidEoETriggerContext());
    triggerContexts.Add(new RaidVoATriggerContext());
    triggerContexts.Add(new RaidUlduarTriggerContext());
    triggerContexts.Add(new RaidToCTriggerContext());
    triggerContexts.Add(new RaidIccTriggerContext());
    triggerContexts.Add(new RaidRsTriggerContext());
    triggerContexts.Add(new RaidBlackTempleTriggerContext());
    triggerContexts.Add(new RaidOnyxiaTriggerContext());
    triggerContexts.Add(new RaidSunwellTriggerContext());
}

// AC Bot/Engine/BuildSharedValueContexts.cpp:24-29 (raid entries, AC order; ported ones only)
void RaidContexts::AddValueContexts(NamedObjectContextList<UntypedValue>& valueContexts)
{
    valueContexts.Add(new RaidGruulsLairValueContext());
    valueContexts.Add(new RaidHyjalValueContext());
    valueContexts.Add(new RaidMagtheridonValueContext());
    valueContexts.Add(new RaidSscValueContext());
    valueContexts.Add(new RaidTempestKeepValueContext());
}
