/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "StaleCombat.h"

#include "Creature.h"
#include "Player.h"
#include "StaleCombatPolicy.h"
#include "Unit.h"

#include <ctime>
#include <unordered_map>

namespace
{
// Since when each member of this thread's groups is flagged in combat with nothing attacking it (0 = not quiet).
thread_local std::unordered_map<uint64, time_t> quietSince;
}

bool StaleCombat::AttackedInSight(Player* member)
{
    // The sight test (a map query) runs last, only for a creature that really has the member as its victim.
    for (Unit* attacker : member->getAttackers())
        if (attacker && attacker->ToCreature() && attacker->IsAlive() && attacker->GetVictim() == member &&
            StaleCombatPolicy::HoldsInFight(attacker->GetUInt32Value(UNIT_FIELD_FLAGS)) &&
            member->IsWithinLOSInMap(attacker))
            return true;
    return false;
}

bool StaleCombat::IsStale(Player* member)
{
    time_t const now = time(nullptr);
    time_t& since = quietSince[member->GetGUID()];
    since = StaleCombatPolicy::NextQuietSince(member->IsInCombat(), AttackedInSight(member), since, now);
    return StaleCombatPolicy::IsStale(member->IsInCombat(), since, now);
}
