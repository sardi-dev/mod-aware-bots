/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Raid half of AC Bot/Engine/BuildShared{Strategy,Action,Trigger,Value}Contexts.cpp (Ai/Raid in AC's mod-playerbots).
// Same shape as DungeonContexts.h: this fork builds the context lists per AiObjectContext
// (strategy/AiObjectContext.cpp), so the raid contexts are added there through these helpers. Keeping the per-raid
// includes in RaidContexts.cpp means a new raid touches only this folder.

#ifndef PLAYERBOTS_RAIDCONTEXTS_H
#define PLAYERBOTS_RAIDCONTEXTS_H

#include "NamedObjectContext.h"

class Action;
class Strategy;
class Trigger;
class UntypedValue;

namespace RaidContexts
{
    void AddStrategyContexts(NamedObjectContextList<Strategy>& strategyContexts);
    void AddActionContexts(NamedObjectContextList<Action>& actionContexts);
    void AddTriggerContexts(NamedObjectContextList<Trigger>& triggerContexts);
    void AddValueContexts(NamedObjectContextList<UntypedValue>& valueContexts);
}

#endif
