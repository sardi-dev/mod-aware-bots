/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "RaidInterrupts.h"

#include "HyjalHelpers.h"
#include "MagHelpers.h"
#include "Playerbots.h"
#include "SSCHelpers.h"

bool RaidInterrupts::ShouldInterruptCast(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (bot->GetMapId() == HyjalHelpers::HYJAL_MAP_ID && botAI->HasStrategy("hyjal", BOT_STATE_COMBAT))
        return HyjalHelpers::ShouldInterruptCast(bot);

    if (bot->GetMapId() == MagHelpers::MAG_MAP_ID && botAI->HasStrategy("magtheridon", BOT_STATE_COMBAT))
        return MagHelpers::ShouldInterruptCast(bot);

    if (bot->GetMapId() == SscHelpers::SSC_MAP_ID && botAI->HasStrategy("ssc", BOT_STATE_COMBAT))
        return SscHelpers::ShouldInterruptCast(bot);

    return false;
}
