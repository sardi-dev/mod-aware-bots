/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ONYACTIONS_H
#define PLAYERBOTS_ONYACTIONS_H

#include <vector>

#include "AttackActions.h"  // 5.4.8: AC "AttackAction.h" -> fork strategy/actions/AttackActions.h
#include "GenericSpellActions.h"
#include "MovementActions.h"
#include "OnyTriggers.h"

class PlayerbotAI;

// AC Ai/Raid/Ony/OnyActions.h class RaidOnyxiaMoveToSideAction
class RaidOnyxiaMoveToSideAction : public MovementAction
{
public:
    RaidOnyxiaMoveToSideAction(PlayerbotAI* botAI, std::string const name = "ony move to side")
        : MovementAction(botAI, name)
    {
    }
    bool Execute(Event event) override;
};

// AC Ai/Raid/Ony/OnyActions.h class RaidOnyxiaSpreadOutAction
class RaidOnyxiaSpreadOutAction : public MovementAction
{
public:
    RaidOnyxiaSpreadOutAction(PlayerbotAI* botAI, std::string const name = "ony spread out")
        : MovementAction(botAI, name)
    {
    }
    bool Execute(Event event) override;
};

// AC Ai/Raid/Ony/OnyActions.h struct SafeZone
struct OnyxiaSafeZone
{
    Position pos;
    float radius;
};

// AC Ai/Raid/Ony/OnyActions.h class RaidOnyxiaMoveToSafeZoneAction
class RaidOnyxiaMoveToSafeZoneAction : public MovementAction
{
public:
    RaidOnyxiaMoveToSafeZoneAction(PlayerbotAI* botAI, std::string const name = "ony move to safe zone")
        : MovementAction(botAI, name)
    {
    }
    bool Execute(Event event) override;

private:
    // Safe zone coordinates of Onyxia's lair map (249), one pair per breath direction
    static std::vector<OnyxiaSafeZone> GetSafeZonesForBreath(uint32 spellId);
};

// AC Ai/Raid/Ony/OnyActions.h class RaidOnyxiaKillWhelpsAction
class RaidOnyxiaKillWhelpsAction : public AttackAction
{
public:
    RaidOnyxiaKillWhelpsAction(PlayerbotAI* botAI, std::string const name = "ony kill whelps")
        : AttackAction(botAI, name)
    {
    }

    bool Execute(Event event) override;
};

// AC Ai/Raid/Ony/OnyActions.h class OnyxiaAvoidEggsAction
class OnyxiaAvoidEggsAction : public MovementAction
{
public:
    OnyxiaAvoidEggsAction(PlayerbotAI* botAI) : MovementAction(botAI, "ony avoid eggs move") {}

    bool Execute(Event event) override;
};

#endif
