/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "TrainerTriggers.h"

#include "BotTraining.h"
#include "Map.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"

namespace
{
    // A player trains between fights and outside dungeons; so does an alt, and never while its master fights.
    bool FreeToTrain(PlayerbotAI* botAI, Player* bot)
    {
        if (!BotTraining::EnabledFor(bot) || !bot->IsAlive() || bot->IsInCombat() || bot->GetMap()->Instanceable())
            return false;

        Player* master = botAI->GetMaster();
        return !master || master == bot || !master->IsInCombat();
    }
}

bool TrainerNearbyTrigger::IsActive()
{
    return FreeToTrain(botAI, bot) && BotTraining::FindNearbyTrainer(botAI, bot);
}

bool SpellsToTrainTrigger::IsActive()
{
    return FreeToTrain(botAI, bot) && BotTraining::CountLearnable(bot) && !BotTraining::FindNearbyTrainer(botAI, bot);
}
