/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * Alt bots craft their gear upgrades like a player (BotCrafting.h).
 */

#include "BotCrafting.h"

#include "Config.h"
#include "Event.h"
#include "ItemVisitors.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"

#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>

namespace
{
    // A bot's recipe list changes rarely; between checks its bags and position may change, so the answer is kept
    // only this long.
    constexpr uint32 RECIPE_CACHE_MS = 30000;

    struct CachedRecipe
    {
        uint32 time;
        uint32 spellId;
    };

    // Bots update on the map threads, so the cache is shared and locked.
    std::mutex sLock;
    std::unordered_map<uint64, CachedRecipe> sCache;

    // The gear item a recipe makes, or 0.
    uint32 CraftedGear(SpellInfo const* spellInfo)
    {
        for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        {
            if (spellInfo->Effects[i].Effect != SPELL_EFFECT_CREATE_ITEM || !spellInfo->Effects[i].ItemType)
                continue;

            ItemTemplate const* proto = sObjectMgr->GetItemTemplate(spellInfo->Effects[i].ItemType);
            if (proto && (proto->Class == ITEM_CLASS_WEAPON || proto->Class == ITEM_CLASS_ARMOR))
                return proto->ItemId;
        }
        return 0;
    }

    bool HasReagents(Player* bot, SpellInfo const* spellInfo)
    {
        bool needsReagent = false;
        for (uint8 i = 0; i < MAX_SPELL_REAGENTS; ++i)
        {
            if (spellInfo->Reagent[i] <= 0 || !spellInfo->ReagentCount[i])
                continue;
            needsReagent = true;
            if (!bot->HasItemCount(uint32(spellInfo->Reagent[i]), spellInfo->ReagentCount[i]))
                return false;
        }
        // A trade skill recipe always takes reagents; anything else is not crafting.
        return needsReagent;
    }

    uint32 SearchRecipes(PlayerbotAI* botAI, Player* bot)
    {
        for (auto const& entry : bot->GetSpellMap())
        {
            if (entry.second->state == PLAYERSPELL_REMOVED || !entry.second->active || entry.second->disabled)
                continue;

            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(entry.first);
            if (!spellInfo)
                continue;

            uint32 const itemId = CraftedGear(spellInfo);
            if (!itemId || !HasReagents(bot, spellInfo))
                continue;

            ItemUsage const usage =
                botAI->GetAiObjectContext()->GetValue<ItemUsage>("item usage", std::to_string(itemId))->Get();
            if (usage != ITEM_USAGE_EQUIP && usage != ITEM_USAGE_REPLACE)
                continue;

            // The core's own check: skill, anvil or forge nearby, tools, cooldown.
            if (botAI->CanCastSpell(entry.first, bot, true))
                return entry.first;
        }
        return 0;
    }
}

bool BotCrafting::Enabled()
{
    // Read once on first use, like the other local settings (BotTraining.cpp), to keep PlayerbotAIConfig.h unchanged.
    static bool const enabled = sConfigMgr->GetBoolDefault("AiPlayerbot.AltBotsCraftGear", true);
    return enabled;
}

uint32 BotCrafting::FindCraftableUpgrade(PlayerbotAI* botAI, Player* bot)
{
    uint32 const now = getMSTime();
    {
        std::lock_guard<std::mutex> guard(sLock);
        auto const itr = sCache.find(bot->GetGUID());
        if (itr != sCache.end() && getMSTimeDiff(itr->second.time, now) < RECIPE_CACHE_MS)
            return itr->second.spellId;
    }

    uint32 const spellId = SearchRecipes(botAI, bot);

    std::lock_guard<std::mutex> guard(sLock);
    sCache[bot->GetGUID()] = { now, spellId };
    return spellId;
}

void BotCrafting::Forget(Player* bot)
{
    std::lock_guard<std::mutex> guard(sLock);
    sCache.erase(bot->GetGUID());
}

bool CraftUpgradeAction::Execute(Event /*event*/)
{
    uint32 const spellId = BotCrafting::FindCraftableUpgrade(botAI, bot);
    BotCrafting::Forget(bot);
    if (!spellId || !botAI->CastSpell(spellId, bot))
        return false;

    // The item push equips the result ("equip upgrades packet action"), as with loot.
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (ItemTemplate const* proto = spellInfo ? sObjectMgr->GetItemTemplate(CraftedGear(spellInfo)) : nullptr)
    {
        std::ostringstream out;
        out << "I crafted " << proto->Name1 << ".";
        botAI->TellMaster(out.str());
    }
    return true;
}
