/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "SummonTriggers.h"

#include "AcceptSummonAction.h"
#include "Group.h"
#include "Playerbots.h"

namespace
{
    // A group mate this far away, or on another map, is not with the group.
    float const MATE_FAR_DISTANCE = 150.0f;
}

bool SummonPendingTrigger::IsActive() { return AcceptSummon::HasPending(bot); }

bool GroupMateFarTrigger::IsActive()
{
    if (!AcceptSummon::StoneSummonsEnabled())
        return false;

    Group* group = bot->GetGroup();
    if (!group || !botAI->HasRealPlayerMaster() || !bot->IsAlive() || bot->IsInCombat())
        return false;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        // Only a bot mate is summoned by a bot; a player decides for themselves.
        PlayerbotAI* memberAI = member ? GET_PLAYERBOT_AI(member) : nullptr;
        if (!member || member == bot || !memberAI || memberAI->IsRealPlayer())
            continue;

        if (member->GetMapId() != bot->GetMapId() || bot->GetExactDist2d(member) > MATE_FAR_DISTANCE)
            return true;
    }

    return false;
}
