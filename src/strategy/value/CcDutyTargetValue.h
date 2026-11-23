/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_CCDUTYTARGETVALUE_H
#define _PLAYERBOT_CCDUTYTARGETVALUE_H

#include <string>

#include "TargetValue.h"

class Group;
class Player;
class PlayerbotAI;
class Unit;

// BT-165 part 2: the group's three control marks (triangle, moon, square) each have one controller, chosen by
// CrowdControlDutyPolicy. These helpers connect that policy to the live group.
namespace CcDuty
{
// The marked unit of a group icon when it is a possible target, else null.
Unit* MarkedUnit(PlayerbotAI* botAI, Group* group, unsigned icon);

// True when `spell` (a lower-case spell name) is the control spell the owner chose for this target: Hibernate only on
// beasts and dragonkin, Cyclone on the rest, Shackle Undead only on undead, Sap only on a mob that is not in combat.
// Spells outside the owner's list never fit.
bool SpellFitsTarget(std::string const& spell, Unit* target);

// True when `creature` carries a control mark whose controller is another bot than this one.
bool IsOtherControllersMark(PlayerbotAI* botAI, Unit* creature);

// True when `creature` carries a control mark that some group bot can control (its owner exists).
bool IsClaimedMark(PlayerbotAI* botAI, Unit* creature);

// True for a square or triangle marked unit that currently carries a control that damage breaks: damage dealers and
// the tank leave it alone until the control breaks.
bool IsControlledMark(Group* group, Unit* unit);

// BT-165: the bot is inside the cast range (a little inside the maximum, with line of sight) of the control spell it
// would cast on `target`. True for a spell without a range (a trap is laid where the hunter stands).
bool InControlRange(Player* bot, Unit* target);

// How close the controller walks to its duty target before the pull: melee reach for Sap, else a few yards inside the
// spell's maximum range.
float ApproachDistance(Player* bot, Unit* target);

// True while this bot's duty target is out of the cast range of its control spell and nothing has been pulled yet.
bool NeedsApproach(PlayerbotAI* botAI);

struct Required
{
    Unit* target = nullptr;
    bool duty = false; // The target is this bot's duty target, so only the owner's spells fit it.
};

// The target a cc trigger must work on: this bot's duty target in a group, else the old rti cc mark, which a group
// bot only keeps while no other bot can control it. Alone it is the rti cc mark as before.
Required RequiredTarget(PlayerbotAI* botAI);
}

class CcDutyTargetValue : public UnitCalculatedValue
{
public:
    CcDutyTargetValue(PlayerbotAI* botAI) : UnitCalculatedValue(botAI, "cc duty target", 1) {}

    // The guid is cached for CcDutyRefreshMs so the several cc triggers of one tick share a single group scan; the
    // unit is looked up on every call so a despawned mob is never returned.
    Unit* Get() override;
    Unit* Calculate() override;

private:
    uint64 cachedGuid = 0;
    uint32 cachedAtMs = 0;
};

#endif
