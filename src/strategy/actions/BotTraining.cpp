/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * Alt bots learn at a class trainer like a player (BotTraining.h).
 */

#include "BotTraining.h"

#include "BotFactory.h"
#include "BotGuid.h"
#include "BudgetValues.h"
#include "Config.h"
#include "Creature.h"
#include "DBCStores.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RandomPlayerbotMgr.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"

#include <cmath>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
    bool IsClassTrainerFor(CreatureTemplate const& info, uint8 classId)
    {
        return info.trainer_type == TRAINER_TYPE_CLASS && info.trainer_class == classId;
    }

    // Every class trainer template, by the class it trains.
    std::unordered_map<uint8, std::vector<CreatureTemplate const*>> BuildClassTrainers()
    {
        std::unordered_map<uint8, std::vector<CreatureTemplate const*>> trainers;
        for (auto const& [entry, info] : *sObjectMgr->GetCreatureTemplates())
            if (info.trainer_type == TRAINER_TYPE_CLASS && info.trainer_class && sObjectMgr->GetNpcTrainerSpells(entry))
                trainers[uint8(info.trainer_class)].push_back(&info);
        return trainers;
    }

    std::vector<CreatureTemplate const*> const& ClassTrainers(uint8 classId)
    {
        // Built on first use, after the world has loaded its templates; static initialisation is thread-safe.
        static std::unordered_map<uint8, std::vector<CreatureTemplate const*>> const trainers = BuildClassTrainers();
        static std::vector<CreatureTemplate const*> const none;
        auto const itr = trainers.find(classId);
        return itr != trainers.end() ? itr->second : none;
    }

    // A profession trainer lists hundreds of recipes, and the trigger asks every tick, so the answer is kept for a few
    // seconds per bot (as BotShopping does for vendors). Bots update on the map threads, so the cache is locked.
    constexpr uint32 TRAINER_CACHE_MS = 5000;

    struct CachedTrainer
    {
        uint32 time;
        uint64 trainer;
    };

    std::mutex sCacheLock;
    std::unordered_map<uint64, CachedTrainer> sTrainerCache;

    // BT-197: a random bot also learns its professions at a trade skill trainer, paying like a player; alt bots keep to
    // their class trainer.
    bool TrainsBotAt(CreatureTemplate const& info, Player* bot)
    {
        if (IsClassTrainerFor(info, bot->getClass()))
            return true;
        if (!sRandomPlayerbotMgr->IsRandomBot(bot))
            return false;
        // BT-218: riding, flight licences and Cold Weather Flying come from riding trainers (a racial one teaches its
        // own race only), paid like any trainer spell.
        if (info.trainer_type == TRAINER_TYPE_MOUNTS)
            return !info.trainer_race || info.trainer_race == bot->getRace();
        return info.trainer_type == TRAINER_TYPE_TRADESKILLS;
    }

    // Learnable now (level, prerequisites, known spells, free profession slots; money is not checked) and, at a trade
    // skill trainer, one of the bot's own professions (BotFactory::MayLearnTrainerSpell).
    bool Teachable(Player* bot, TrainerSpellData const* spells, TrainerSpell const& trainerSpell)
    {
        return bot->GetTrainerSpellState(&trainerSpell) == TRAINER_SPELL_GREEN &&
               BotFactory::MayLearnTrainerSpell(bot, spells, &trainerSpell);
    }

    uint32 CountGreen(Player* bot, TrainerSpellData const* spells)
    {
        uint32 count = 0;
        for (auto const& [spellId, trainerSpell] : spells->spellList)
            if (Teachable(bot, spells, trainerSpell))
                ++count;
        return count;
    }
}

// Read once on first use, like the other trainer settings (TrainerAction.cpp), to keep PlayerbotAIConfig.h unchanged.
bool BotTraining::Enabled()
{
    static bool const enabled = sConfigMgr->GetBoolDefault("AiPlayerbot.AltBotsVisitTrainers", true);
    return enabled;
}

bool BotTraining::RandomBotsEnabled()
{
    static bool const enabled = sConfigMgr->GetBoolDefault("AiPlayerbot.RandomBotsVisitTrainers", true);
    return enabled;
}

bool BotTraining::EnabledFor(Player* bot)
{
    return sRandomPlayerbotMgr->IsRandomBot(bot) ? RandomBotsEnabled() : Enabled();
}

float BotTraining::Range()
{
    static float const range = sConfigMgr->GetFloatDefault("AiPlayerbot.AltBotTrainerRange", 100.0f);
    return range;
}

uint32 BotTraining::CountLearnable(Player* bot)
{
    // Only trainers of the bot's own side: the other faction's portal spells are listed too, and it cannot reach them.
    FactionTemplateEntry const* botFaction = bot->GetFactionTemplateEntry();
    if (!botFaction)
        return 0;

    std::unordered_set<uint32> learnable;
    for (CreatureTemplate const* info : ClassTrainers(bot->getClass()))
    {
        FactionTemplateEntry const* trainerFaction = sFactionTemplateStore.LookupEntry(info->faction_A);
        if (!trainerFaction || !trainerFaction->IsFriendlyTo(*botFaction))
            continue;

        for (auto const& [spellId, trainerSpell] : sObjectMgr->GetNpcTrainerSpells(info->Entry)->spellList)
            if (bot->GetTrainerSpellState(&trainerSpell) == TRAINER_SPELL_GREEN)
                learnable.insert(spellId);
    }
    return uint32(learnable.size());
}

Creature* BotTraining::FindNearbyTrainer(PlayerbotAI* botAI, Player* bot)
{
    uint32 const now = getMSTime();
    {
        std::lock_guard<std::mutex> guard(sCacheLock);
        auto const itr = sTrainerCache.find(bot->GetGUID());
        if (itr != sTrainerCache.end() && getMSTimeDiff(itr->second.time, now) < TRAINER_CACHE_MS)
        {
            Creature* trainer = itr->second.trainer ? botAI->GetCreature(BotGuid(itr->second.trainer)) : nullptr;
            return trainer && trainer->IsAlive() ? trainer : nullptr;
        }
    }

    Creature* nearest = nullptr;
    float nearestDist = Range();
    for (BotGuid const guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest npcs")->Get())
    {
        Creature* creature = botAI->GetCreature(guid);
        if (!creature || !creature->IsAlive() || !creature->IsTrainer() || bot->IsHostileTo(creature))
            continue;

        CreatureTemplate const* info = creature->GetCreatureTemplate();
        if (!info || !TrainsBotAt(*info, bot))
            continue;

        float const dist = bot->GetDistance(creature);
        if (dist > nearestDist)
            continue;

        TrainerSpellData const* spells = sObjectMgr->GetNpcTrainerSpells(creature->GetEntry());
        if (!spells || !CountGreen(bot, spells))
            continue;

        nearest = creature;
        nearestDist = dist;
    }

    std::lock_guard<std::mutex> guard(sCacheLock);
    sTrainerCache[bot->GetGUID()] = { now, nearest ? uint64(nearest->GetGUID()) : 0 };
    return nearest;
}

void BotTraining::Forget(Player* bot)
{
    std::lock_guard<std::mutex> guard(sCacheLock);
    sTrainerCache.erase(bot->GetGUID());
}

uint32 BotTraining::CountLearnableAt(Player* bot, Creature* trainer)
{
    TrainerSpellData const* spells = sObjectMgr->GetNpcTrainerSpells(trainer->GetEntry());
    return spells ? CountGreen(bot, spells) : 0;
}

uint32 BotTraining::LearnAffordable(PlayerbotAI* botAI, Player* bot, Creature* trainer)
{
    TrainerSpellData const* spells = sObjectMgr->GetNpcTrainerSpells(trainer->GetEntry());
    if (!spells)
        return 0;

    float const discount = bot->GetReputationPriceDiscount(trainer);
    uint32 learned = 0;
    for (auto const& [spellId, trainerSpell] : spells->spellList)
    {
        if (!Teachable(bot, spells, trainerSpell) || !sSpellMgr->GetSpellInfo(trainerSpell.spell))
            continue;

        // The money a player would keep for repairs and travel stays untouched, as in TrainerAction::Learn.
        uint32 const cost = uint32(std::floor(trainerSpell.spellCost * discount));
        uint32 const freeMoney = botAI->GetAiObjectContext()->GetValue<uint32>("free money for",
            int32(NeedMoneyFor::spells))->Get();
        if (freeMoney < cost)
            continue;

        bot->ModifyMoney(-int64(cost));
        // The branch the core's own trainer handler takes (Handlers/NPCHandler.cpp).
        if (trainerSpell.IsCastable())
            bot->CastSpell(bot, trainerSpell.spell, true);
        else
            bot->learnSpell(trainerSpell.spell, false);
        ++learned;
    }
    return learned;
}
