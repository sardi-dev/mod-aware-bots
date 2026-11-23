/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "GroupTravelTriggers.h"

#include "Playerbots.h"
#include "TravelToGroupAction.h"

bool FarFromGroupLeaderTrigger::IsActive() { return GroupTravel::WantsTravel(botAI, bot); }
