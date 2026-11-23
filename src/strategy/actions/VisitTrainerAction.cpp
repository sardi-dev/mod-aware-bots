/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "VisitTrainerAction.h"

#include "BotTraining.h"
#include "Creature.h"
#include "Event.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"

#include <mutex>
#include <sstream>
#include <unordered_map>

namespace
{
    // Bots update on the map threads, so the per-bot memory below is shared and locked.
    std::mutex sLock;

    // A visit that could not pay for anything; the bot retries once its level or money grows.
    struct FailedVisit
    {
        uint8 level;
        uint64 money;
    };
    std::unordered_map<uint64, FailedVisit> sFailedVisits;

    // The level at which the bot last told its master it had spells to train.
    std::unordered_map<uint64, uint8> sAnnouncedLevel;

    constexpr float TRAINER_TALK_DISTANCE = 4.0f;
}

bool VisitTrainerAction::isUseful()
{
    std::lock_guard<std::mutex> guard(sLock);
    auto const itr = sFailedVisits.find(bot->GetGUID());
    return itr == sFailedVisits.end() || bot->getLevel() > itr->second.level || bot->GetMoney() > itr->second.money;
}

bool VisitTrainerAction::Execute(Event /*event*/)
{
    Creature* trainer = BotTraining::FindNearbyTrainer(botAI, bot);
    if (!trainer)
        return false;

    if (bot->GetDistance(trainer) > TRAINER_TALK_DISTANCE)
        return MoveTo(trainer, TRAINER_TALK_DISTANCE - 1.0f);

    bot->SetFacingToObject(trainer);
    uint32 const learned = BotTraining::LearnAffordable(botAI, bot, trainer);
    BotTraining::Forget(bot);
    uint32 const unaffordable = BotTraining::CountLearnableAt(bot, trainer);

    std::ostringstream out;
    if (learned)
        out << "I learned " << learned << (learned == 1 ? " new spell" : " new spells") << " from " << trainer->GetName();
    if (learned && unaffordable)
        out << "; I can't afford the other " << unaffordable << " yet.";
    else if (learned)
        out << ".";
    else
        out << "I can't afford my new spells yet.";

    {
        std::lock_guard<std::mutex> guard(sLock);
        if (unaffordable)
            sFailedVisits[bot->GetGUID()] = { bot->getLevel(), bot->GetMoney() };
        else
            sFailedVisits.erase(bot->GetGUID());
    }
    if (!botAI->SayToParty(out.str()))   // a groupmate says it in party chat; alone, it whispers its master
        botAI->TellMaster(out);
    return true;
}

bool AnnounceTrainingAction::isUseful()
{
    std::lock_guard<std::mutex> guard(sLock);
    auto const itr = sAnnouncedLevel.find(bot->GetGUID());
    return itr == sAnnouncedLevel.end() || bot->getLevel() > itr->second;
}

bool AnnounceTrainingAction::Execute(Event /*event*/)
{
    uint32 const count = BotTraining::CountLearnable(bot);
    if (!count)
        return false;

    std::ostringstream out;
    out << "I have " << count << (count == 1 ? " new spell" : " new spells") << " to learn at my class trainer.";
    if (!botAI->SayToParty(out.str()))
        botAI->TellMaster(out);

    std::lock_guard<std::mutex> guard(sLock);
    sAnnouncedLevel[bot->GetGUID()] = bot->getLevel();
    return true;
}
