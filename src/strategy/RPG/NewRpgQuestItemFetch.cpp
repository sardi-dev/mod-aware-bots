/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * BT-211: random bots fetch quest items from their real sources; the decisions are in Utils/QuestItemSourcePolicy.h.
 */

#include "NewRpgBaseAction.h"

#include <algorithm>
#include <cmath>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "BotCompat_S.h"
#include "BotSalePolicy.h"
#include "BotScenario.h"
#include "Creature.h"
#include "DBCStores.h"
#include "DBCStructure.h"
#include "GameObject.h"
#include "GatherNodeMgr.h"
#include "ItemPrototype.h"
#include "LootMgr.h"
#include "LootObjectStack.h"
#include "LootValues.h"
#include "Map.h"
#include "NewRpgSpawnIndex.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "QuestHandInPolicy.h"
#include "QuestItemSourcePolicy.h"
#include "QuestRetryPolicy.h"
#include "SharedDefines.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"

namespace
{
using QuestItemSourcePolicy::Kind;

// The legitimate sources of every quest objective item (and of the reagents of recipes that make one), read once from
// the core's loot and vendor stores, and the open-world spawns of those sources.
namespace QuestItemSourceIndex
{
struct Source
{
    Kind kind = Kind::Creature;
    uint32 entry = 0;
    float chance = 100.0f;
};

struct SourceSpawn
{
    uint32 spawnId = 0;
    uint32 map = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    std::vector<int32> events;
};

std::once_flag indexOnce;
// Written once under indexOnce, then read-only.
std::unordered_map<uint32, std::vector<Source>> sourcesByItem;
std::unordered_map<uint32, std::vector<uint32>> recipesByItem;          // trade skill spells that make a quest item
std::unordered_map<uint32, std::vector<SourceSpawn>> creatureSpawns;    // mob or vendor entry -> spawns
std::unordered_map<uint32, std::vector<SourceSpawn>> gameObjectSpawns;  // chest entry -> spawns

void AddSource(uint32 itemId, Kind kind, uint32 entry, float chance)
{
    std::vector<Source>& sources = sourcesByItem[itemId];
    for (Source& source : sources)
        if (source.kind == kind && source.entry == entry)
        {
            source.chance = std::max(source.chance, chance);
            return;
        }
    Source source;
    source.kind = kind;
    source.entry = entry;
    source.chance = chance;
    sources.push_back(source);
}

// The ungrouped rows of a loot template, references followed one level, as (item, chance in percent). Grouped rows
// are left out: the core keeps them in a private group class that LootTemplateAccess does not mirror, and only 6 of
// the 6,909 quest drop rows in the live creature loot are grouped.
template <class Visit>
void VisitLoot(LootTemplate const* lootTemplate, Visit const& visit)
{
    LootTemplateAccess const* access = reinterpret_cast<LootTemplateAccess const*>(lootTemplate);
    if (!access)
        return;
    for (LootStoreItem const* row : access->Entries)
    {
        if (row->mincountOrRef > 0)
        {
            visit(row->itemid, row->chance);
            continue;
        }
        if (row->mincountOrRef == 0)
            continue;
        LootTemplateAccess const* ref =
            reinterpret_cast<LootTemplateAccess const*>(LootTemplates_Reference.GetLootFor(uint32(-row->mincountOrRef)));
        if (!ref)
            continue;
        for (LootStoreItem const* inner : ref->Entries)
            if (inner->mincountOrRef > 0)
                visit(inner->itemid, row->chance * inner->chance / 100.0f);
    }
}

template <class Data>
void AddSpawns(Data const& store, std::unordered_set<uint32> const& entries,
               std::unordered_map<uint32, std::vector<int32>> const& events,
               std::unordered_map<uint32, std::vector<SourceSpawn>>& out)
{
    for (auto const& itr : store)
    {
        auto const& data = itr.second;
        // Phase-id and phase-group spawns are not reliably visible to a roaming bot (as the quest ender index).
        if (!entries.count(data.id) || !data.dbData || data.phaseid || data.phaseGroup ||
            !RpgSpawnIndex::OpenWorld(data.mapid))
            continue;
        SourceSpawn spawn;
        spawn.spawnId = itr.first;
        spawn.map = data.mapid;
        spawn.x = data.posX;
        spawn.y = data.posY;
        spawn.z = data.posZ;
        auto const listed = events.find(itr.first);
        if (listed != events.end())
            spawn.events = listed->second;
        out[data.id].push_back(std::move(spawn));
    }
}

void Build()
{
    std::unordered_set<uint32> wanted;
    for (auto const& itr : sObjectMgr->GetQuestTemplates())
        for (QuestObjective const* objective : itr.second->m_questObjectives)
            if (objective->Type == QUEST_OBJECTIVE_TYPE_ITEM && objective->ObjectId)
                wanted.insert(objective->ObjectId);
    uint32 const questItems = uint32(wanted.size());

    std::unordered_set<uint32> reagents;
    for (uint32 spellId = 1; spellId < sSpellMgr->GetSpellInfoStoreSize(); ++spellId)
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo)
            continue;
        for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        {
            uint32 const itemId = spellInfo->Effects[i].ItemType;
            if (spellInfo->Effects[i].Effect != SPELL_EFFECT_CREATE_ITEM || !itemId || !wanted.count(itemId))
                continue;
            // A trade skill recipe always takes reagents; a spell without them is a quest script's or an item's.
            bool takesReagents = false;
            for (uint8 r = 0; r < MAX_SPELL_REAGENTS; ++r)
                if (spellInfo->Reagent[r] > 0 && spellInfo->ReagentCount[r])
                {
                    takesReagents = true;
                    reagents.insert(uint32(spellInfo->Reagent[r]));
                }
            if (takesReagents)
                recipesByItem[itemId].push_back(spellId);
            break;
        }
    }
    wanted.insert(reagents.begin(), reagents.end());

    std::unordered_set<uint32> creatureEntries;
    for (auto const& itr : *sObjectMgr->GetCreatureTemplates())
    {
        uint32 const entry = itr.first;
        CreatureTemplate const& tmpl = itr.second;
        if (tmpl.lootid)
            VisitLoot(LootTemplates_Creature.GetLootFor(tmpl.lootid), [&](uint32 itemId, float chance)
            {
                if (!wanted.count(itemId))
                    return;
                AddSource(itemId, Kind::Creature, entry, chance);
                creatureEntries.insert(entry);
            });
        if (!(tmpl.npcflag & UNIT_NPC_FLAG_VENDOR))
            continue;
        VendorItemData const* items = sObjectMgr->GetNpcVendorItemList(entry);
        if (!items)
            continue;
        for (VendorItem const* vendorItem : items->m_items)
        {
            // Plain gold only: tokens and currencies are earned elsewhere.
            if (!vendorItem || vendorItem->Type != ITEM_VENDOR_TYPE_ITEM || vendorItem->ExtendedCost ||
                !wanted.count(vendorItem->item))
                continue;
            AddSource(vendorItem->item, Kind::Vendor, entry, 100.0f);
            creatureEntries.insert(entry);
        }
    }

    std::unordered_set<uint32> gameObjectEntries;
    for (auto const& itr : *sObjectMgr->GetGameObjectTemplates())
    {
        uint32 const entry = itr.first;
        GameObjectTemplate const& tmpl = itr.second;
        if (tmpl.type != GAMEOBJECT_TYPE_CHEST || !tmpl.GetLootId())
            continue;
        VisitLoot(LootTemplates_Gameobject.GetLootFor(tmpl.GetLootId()), [&](uint32 itemId, float chance)
        {
            if (!wanted.count(itemId))
                return;
            AddSource(itemId, Kind::GameObject, entry, chance);
            gameObjectEntries.insert(entry);
        });
    }

    AddSpawns(sObjectMgr->GetAllCreatureData(), creatureEntries,
              RpgSpawnIndex::EventSpawns(sGameEventMgr->mGameEventCreatureGuids), creatureSpawns);
    AddSpawns(sObjectMgr->GetAllGameObjectData(), gameObjectEntries,
              RpgSpawnIndex::EventSpawns(sGameEventMgr->mGameEventGameobjectGuids), gameObjectSpawns);

    SF_LOG_INFO("playerbots",
                "[New RPG] quest item source index: %u of %u quest items have a source, %u recipes, %u mob or vendor "
                "and %u object entries spawned",
                uint32(sourcesByItem.size()), questItems, uint32(recipesByItem.size()), uint32(creatureSpawns.size()),
                uint32(gameObjectSpawns.size()));
}

std::vector<uint32> const* Recipes(uint32 itemId)
{
    std::call_once(indexOnce, Build);
    auto const found = recipesByItem.find(itemId);
    return found == recipesByItem.end() ? nullptr : &found->second;
}

// A lock opens with any one of its cases: none set, a skill the bot has high enough (or a skill-less "open" lock), or
// a key in its bags.
bool CanOpen(Player* bot, GameObjectTemplate const* tmpl)
{
    LockEntry const* lock = sLockStore.LookupEntry(tmpl->GetLockId());
    if (!lock)
        return true;
    bool anyCase = false;
    for (uint8 i = 0; i < MAX_LOCK_CASE; ++i)
    {
        switch (lock->Type[i])
        {
            case LOCK_KEY_ITEM:
                anyCase = true;
                if (lock->Index[i] && bot->HasItemCount(lock->Index[i], 1))
                    return true;
                break;
            case LOCK_KEY_SKILL:
            {
                anyCase = true;
                uint32 const skill = SkillByLockType(LockType(lock->Index[i]));
                if (!skill)
                    return true;
                if (bot->HasSkill(skill) && bot->GetSkillValue(skill) >= std::max(1u, lock->Skill[i]))
                    return true;
                break;
            }
            default:
                break;
        }
    }
    return !anyCase;
}

uint32 FactionOf(Player* bot, CreatureTemplate const* tmpl)
{
    return bot->GetTeam() == HORDE ? tmpl->faction_H : tmpl->faction_A;
}

// What one source asks of this bot, for QuestItemSourcePolicy::Usable.
bool Facts(Player* bot, Source const& source, QuestItemSourcePolicy::SourceFacts& facts, ItemTemplate const* proto)
{
    facts.kind = source.kind;
    facts.chance = source.chance;
    FactionTemplateEntry const* botFaction = bot->GetFactionTemplateEntry();
    if (source.kind == Kind::GameObject)
    {
        GameObjectTemplate const* tmpl = sObjectMgr->GetGameObjectTemplate(source.entry);
        if (!tmpl)
            return false;
        facts.skillMissing = !CanOpen(bot, tmpl);
        return true;
    }

    CreatureTemplate const* tmpl = sObjectMgr->GetCreatureTemplate(source.entry);
    if (!tmpl)
        return false;
    FactionTemplateEntry const* faction = sFactionTemplateStore.LookupEntry(FactionOf(bot, tmpl));
    if (source.kind == Kind::Creature)
    {
        facts.minLevel = tmpl->minlevel;
        facts.elite = tmpl->rank > CREATURE_ELITE_NORMAL;
        facts.friendly = (tmpl->unit_flags & (UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE)) ||
                         (faction && botFaction && faction->IsFriendlyTo(*botFaction));
        return true;
    }

    facts.hostile = faction && botFaction && faction->IsHostileTo(*botFaction);
    if (!proto)
        return false;
    facts.lotPrice = proto->BuyPrice > 0 ? uint64(proto->BuyPrice) : 0;
    facts.lotSize = proto->BuyCount ? proto->BuyCount : 1;
    facts.otherCost = proto->RequiredReputationFaction &&
                      uint32(bot->GetReputationRank(proto->RequiredReputationFaction)) < proto->RequiredReputationRank;
    return true;
}

struct Resolved
{
    std::vector<QuestItemSourcePolicy::Spot> spots;
    std::vector<uint32> creatureEntries;
};

// Every spawn on the bot's map of a source of the item the bot can use now.
Resolved Resolve(PlayerbotAI* botAI, Player* bot, uint32 itemId, uint32 missing)
{
    std::call_once(indexOnce, Build);
    Resolved out;
    auto const found = sourcesByItem.find(itemId);
    if (found == sourcesByItem.end())
        return out;

    QuestItemSourcePolicy::BotFacts botFacts;
    botFacts.level = bot->getLevel();
    botFacts.canFightElite = botAI->GetAiObjectContext()->GetValue<bool>("can fight elite")->Get();
    botFacts.money = bot->GetMoney();
    botFacts.reserve = BotSalePolicy::PurchaseReserve(bot->getLevel());
    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
    GameEventMgr::ActiveEvents const active = QuestHubSearch::ActiveEventsSnapshot();
    uint32 const mapId = bot->GetMapId();

    for (Source const& source : found->second)
    {
        QuestItemSourcePolicy::SourceFacts facts;
        if (!Facts(bot, source, facts, proto) || !QuestItemSourcePolicy::Usable(facts, botFacts, missing))
            continue;
        auto const& index = source.kind == Kind::GameObject ? gameObjectSpawns : creatureSpawns;
        auto const spawns = index.find(source.entry);
        if (spawns == index.end())
            continue;
        if (source.kind == Kind::Creature)
            out.creatureEntries.push_back(source.entry);
        for (SourceSpawn const& spawn : spawns->second)
        {
            if (spawn.map != mapId || !QuestHubSearch::EventSpawnShown(spawn.events, active))
                continue;
            QuestItemSourcePolicy::Spot spot;
            spot.kind = source.kind;
            spot.entry = source.entry;
            spot.spawnId = spawn.spawnId;
            spot.map = spawn.map;
            spot.x = spawn.x;
            spot.y = spawn.y;
            spot.z = spawn.z;
            spot.chance = source.chance;
            out.spots.push_back(spot);
        }
    }
    return out;
}

std::string SourceName(QuestItemSourcePolicy::Spot const& spot)
{
    if (spot.kind == Kind::GameObject)
    {
        GameObjectTemplate const* tmpl = sObjectMgr->GetGameObjectTemplate(spot.entry);
        return tmpl ? tmpl->name : std::to_string(spot.entry);
    }
    CreatureTemplate const* tmpl = sObjectMgr->GetCreatureTemplate(spot.entry);
    return tmpl ? tmpl->Name : std::to_string(spot.entry);
}

std::string ItemName(uint32 itemId)
{
    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
    return proto ? proto->Name1 : std::to_string(itemId);
}

// Items one cast of the recipe makes.
uint32 PerCast(SpellInfo const* spellInfo, uint32 itemId, Player* bot)
{
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        if (spellInfo->Effects[i].Effect == SPELL_EFFECT_CREATE_ITEM && spellInfo->Effects[i].ItemType == itemId)
            return uint32(std::max(1, spellInfo->Effects[i].CalcValue(bot)));
    return 1;
}
}  // namespace QuestItemSourceIndex

// One way to make progress on an open item objective: a spot to go to, or a recipe to cast right here.
struct FetchPlan
{
    uint32 itemId = 0;
    uint32 forItemId = 0;
    uint32 needed = 0;
    uint32 count = 0;
    uint32 recipe = 0;  // cast now (no spot)
    QuestItemSourcePolicy::Spot spot;
    std::vector<uint32> creatureEntries;
    float score = 0.0f;
};
}  // namespace

bool NewRpgBaseAction::ItemFetchOwns(WorldPosition const& dest)
{
    return QuestItemSourcePolicy::SameDest(botAI->GetItemFetch(), dest.GetMapId(), dest.GetPositionX(),
                                           dest.GetPositionY());
}

void NewRpgBaseAction::FailItemFetch(char const* why)
{
    QuestItemSourcePolicy::Fetch& fetch = botAI->GetItemFetch();
    if (!fetch.active)
        return;

    uint32 const questId = fetch.questId;
    uint32 const itemId = fetch.itemId;
    fetch = QuestItemSourcePolicy::Fetch();
    Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
    QuestRetryPolicy::Failure& failure = botAI->GetQuestRetries()[questId];
    bool const abandon =
        QuestRetryPolicy::RecordFailure(failure, getMSTime(), bot->GetZoneId(), bot->getLevel(), false,
                                        QuestRetryPolicy::IsStuckReason(why));
    uint32 const tries = failure.tries;
    SF_LOG_INFO("playerbots", "[QUEST] %s gives up fetching %s for %u %s: %s, failed tries %u", bot->GetName().c_str(),
                QuestItemSourceIndex::ItemName(itemId).c_str(), questId, quest ? quest->GetTitle().c_str() : "", why,
                tries);
    // The same three-strikes rule as an objective POI without progress (BT-208).
    if (abandon && bot->GetQuestStatus(questId) == QUEST_STATUS_INCOMPLETE && DropQuest(questId))
    {
        botAI->GetRpgStatistic().questAbandoned++;
        SF_LOG_INFO("playerbots", "[QUEST] %s abandons %u %s after %u failed tries", bot->GetName().c_str(), questId,
                    quest ? quest->GetTitle().c_str() : "", tries);
    }
    botAI->GetRpgInfo().ChangeToIdle();
}

bool NewRpgBaseAction::MoveFarOnQuestTrip(WorldPosition const& dest)
{
    // From far away, a route with a known flight path, boat, zeppelin or elevator leg goes first (NewRpgRoamTransport;
    // walking routes are left to MoveFarTo). Its stall teleport is off for quest trips (HandInTripOwns, ItemFetchOwns).
    if (bot->GetExactDist2d(dest.GetPositionX(), dest.GetPositionY()) >= QuestHandInPolicy::ROUTE_DIST)
    {
        int32 const routed = RoamTransportMove(dest, true, true);
        if (routed >= 0)
            return routed != 0;
    }

    // MoveFarTo fails the trip itself when the bot is stuck (no teleport); the rpg status may be idle afterwards.
    if (MoveFarTo(dest))
        return true;
    return MoveRandomNear(10.0f);
}

bool NewRpgBaseAction::StartQuestItemFetch(Quest const* quest)
{
    // Off in a deterministic .botscenario (pick=nearest) so those legs keep their own picks (as the hand-in trip).
    if (BotScenario::PickNearest(bot) || bot->InBattleground() || !bot->GetMap() || bot->GetMap()->Instanceable() ||
        bot->IsInFlight() || bot->GetTransport() || bot->IsBeingTeleported() ||
        !(bot->GetPhaseMask() & PHASEMASK_NORMAL))
        return false;
    // Loot and purchases would not fit (the loot pipeline stores nothing above this, as the gather status knows).
    if (AI_VALUE(uint8, "bag space") > 80)
        return false;

    uint32 const questId = quest->GetQuestId();
    uint32 const mapId = bot->GetMapId();
    float const x = bot->GetPositionX();
    float const y = bot->GetPositionY();

    auto planFor = [&](uint32 itemId, uint32 forItemId, uint32 needed, uint32 count, FetchPlan& plan) -> bool
    {
        QuestItemSourceIndex::Resolved resolved = QuestItemSourceIndex::Resolve(botAI, bot, itemId, needed - count);
        float score = 0.0f;
        int const idx = QuestItemSourcePolicy::PickSpot(resolved.spots, mapId, x, y, {}, &score);
        if (idx < 0)
            return false;
        plan.itemId = itemId;
        plan.forItemId = forItemId;
        plan.needed = needed;
        plan.count = count;
        plan.spot = resolved.spots[idx];
        plan.creatureEntries = std::move(resolved.creatureEntries);
        plan.score = score;
        return true;
    };

    std::vector<FetchPlan> plans;
    for (QuestObjective const& objective : BotQuestObjectives(quest))
    {
        if (objective.Type != QUEST_OBJECTIVE_TYPE_ITEM || !objective.ObjectId || objective.Amount <= 0 ||
            BotIsQuestObjectiveComplete(bot, quest, objective))
            continue;
        uint32 const itemId = objective.ObjectId;
        uint32 const needed = uint32(objective.Amount);
        uint32 const count = bot->GetItemCount(itemId, false);
        if (count >= needed)
            continue;

        FetchPlan plan;
        if (planFor(itemId, 0, needed, count, plan))
        {
            plans.push_back(std::move(plan));
            continue;
        }

        // No source of its own: a recipe the bot knows, with reagents in the bags or fetchable the same way.
        std::vector<uint32> const* recipes = QuestItemSourceIndex::Recipes(itemId);
        if (!recipes)
            continue;
        for (uint32 spellId : *recipes)
        {
            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
            if (!spellInfo || !bot->HasSpell(spellId))
                continue;
            uint32 const casts =
                QuestItemSourcePolicy::CastsFor(needed - count, QuestItemSourceIndex::PerCast(spellInfo, itemId, bot));
            std::vector<QuestItemSourcePolicy::Reagent> reagents;
            std::vector<FetchPlan> reagentPlans;
            for (uint8 r = 0; r < MAX_SPELL_REAGENTS; ++r)
            {
                if (spellInfo->Reagent[r] <= 0 || !spellInfo->ReagentCount[r])
                    continue;
                QuestItemSourcePolicy::Reagent reagent;
                reagent.item = uint32(spellInfo->Reagent[r]);
                reagent.need = spellInfo->ReagentCount[r] * casts;
                reagent.have = bot->GetItemCount(reagent.item, false);
                FetchPlan reagentPlan;
                reagent.obtainable =
                    reagent.have < reagent.need && planFor(reagent.item, itemId, reagent.need, reagent.have, reagentPlan);
                reagents.push_back(reagent);
                reagentPlans.push_back(std::move(reagentPlan));
            }
            std::size_t fetchIdx = 0;
            QuestItemSourcePolicy::CraftStep const step = QuestItemSourcePolicy::PlanCraft(reagents, fetchIdx);
            if (step == QuestItemSourcePolicy::CraftStep::FetchReagent)
            {
                plans.push_back(std::move(reagentPlans[fetchIdx]));
                break;
            }
            // The core's own check: skill, fire or anvil nearby, tools, cooldown. A recipe that needs a place the bot is
            // not at is skipped; the quest retry book handles the objective.
            if (step == QuestItemSourcePolicy::CraftStep::Cast && botAI->CanCastSpell(spellId, bot, true))
            {
                FetchPlan castPlan;
                castPlan.itemId = itemId;
                castPlan.needed = needed;
                castPlan.count = count;
                castPlan.recipe = spellId;
                plans.push_back(std::move(castPlan));
                break;
            }
        }
    }

    // A recipe ready to cast goes first; otherwise the best scored spot.
    for (FetchPlan const& plan : plans)
    {
        if (!plan.recipe || !botAI->CastSpell(plan.recipe, bot))
            continue;
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(plan.recipe);
        SF_LOG_INFO("playerbots", "[QUEST] %s fetches %s for %u %s from %s (craft)", bot->GetName().c_str(),
                    QuestItemSourceIndex::ItemName(plan.itemId).c_str(), questId, quest->GetTitle().c_str(),
                    spellInfo && spellInfo->SpellName ? spellInfo->SpellName : "a recipe");
        return true;
    }

    std::vector<QuestItemSourcePolicy::Candidate> candidates;
    std::vector<std::size_t> planOf;
    for (std::size_t i = 0; i < plans.size(); ++i)
    {
        if (plans[i].recipe)
            continue;
        QuestItemSourcePolicy::Candidate candidate;
        candidate.itemId = plans[i].itemId;
        candidate.score = plans[i].score;
        candidates.push_back(candidate);
        planOf.push_back(i);
    }
    int const pick = QuestItemSourcePolicy::PickCandidate(candidates);
    if (pick < 0)
        return false;

    FetchPlan& chosen = plans[planOf[pick]];
    float const dist = QuestItemSourcePolicy::Dist2d(chosen.spot, x, y);
    QuestItemSourcePolicy::Fetch& fetch = botAI->GetItemFetch();
    fetch = QuestItemSourcePolicy::Start(questId, chosen.itemId, chosen.forItemId, chosen.needed, chosen.count,
                                         chosen.spot, dist, botAI->GetQuestClockMs());
    fetch.creatureEntries = std::move(chosen.creatureEntries);
    SF_LOG_INFO("playerbots", "[QUEST] %s fetches %s for %u %s from %s (%s)", bot->GetName().c_str(),
                QuestItemSourceIndex::ItemName(chosen.itemId).c_str(), questId, quest->GetTitle().c_str(),
                QuestItemSourceIndex::SourceName(chosen.spot).c_str(),
                QuestItemSourcePolicy::KindName(chosen.spot.kind));
    return true;
}

bool NewRpgBaseAction::DoQuestItemFetch(Quest const* quest, bool mayStart, bool& handled)
{
    handled = false;
    QuestItemSourcePolicy::Fetch& fetch = botAI->GetItemFetch();
    // A ride or a taxi on the route is still under way (as the hand-in trip).
    if (fetch.active && fetch.questId == quest->GetQuestId() && fetch.spot.map != bot->GetMapId() &&
        (bot->IsInFlight() || bot->GetTransport() || bot->IsBeingTeleported()))
    {
        handled = true;
        return false;
    }
    // Another quest's fetch (that quest was interrupted) or one left behind on another continent: dropped quietly, a
    // later pick of that quest starts it again.
    if (fetch.active && (fetch.questId != quest->GetQuestId() || fetch.spot.map != bot->GetMapId()))
        fetch = QuestItemSourcePolicy::Fetch();

    if (!fetch.active)
    {
        if (!mayStart)
            return false;
        // A cast under way may be a recipe for the quest: plan again once it ends, before the POI walk takes over.
        if (bot->IsNonMeleeSpellCasted(false))
        {
            handled = true;
            return false;
        }
        if (!StartQuestItemFetch(quest))
            return false;
        handled = true;
        return true;
    }

    handled = true;
    uint32 const count = bot->GetItemCount(fetch.itemId, false);
    WorldPosition const dest(fetch.spot.map, fetch.spot.x, fetch.spot.y, fetch.spot.z);
    float const dist = bot->GetExactDist(&dest);
    QuestItemSourcePolicy::Step const step = QuestItemSourcePolicy::Judge(fetch, botAI->GetQuestClockMs(), dist, count);
    switch (step)
    {
        case QuestItemSourcePolicy::Step::Done:
            SF_LOG_INFO("playerbots", "[QUEST] %s has %u/%u %s for %u %s", bot->GetName().c_str(), count, fetch.needed,
                        QuestItemSourceIndex::ItemName(fetch.itemId).c_str(), quest->GetQuestId(),
                        quest->GetTitle().c_str());
            QuestRetryPolicy::OnQuestEvent(botAI->GetQuestRetries(), quest->GetQuestId(), QuestRetryPolicy::QuestEvent::Progress);
            fetch = QuestItemSourcePolicy::Fetch();
            return true;
        case QuestItemSourcePolicy::Step::Expired:
            FailItemFetch(QuestItemSourcePolicy::StepName(step));
            return true;
        case QuestItemSourcePolicy::Step::NextSpot:
            return NextItemFetchSpot(count);
        case QuestItemSourcePolicy::Step::Travel:
            return MoveFarOnQuestTrip(dest);
        case QuestItemSourcePolicy::Step::Work:
            break;
    }

    switch (fetch.spot.kind)
    {
        case QuestItemSourcePolicy::Kind::Creature:
            return WorkFetchCreatures(dest, dist, count);
        case QuestItemSourcePolicy::Kind::GameObject:
            return WorkFetchObject(count);
        case QuestItemSourcePolicy::Kind::Vendor:
            return WorkFetchVendor(count);
    }
    return false;
}

bool NewRpgBaseAction::NextItemFetchSpot(uint32 count)
{
    QuestItemSourcePolicy::Fetch& fetch = botAI->GetItemFetch();
    if (QuestItemSourcePolicy::LeaveSpot(fetch))
    {
        FailItemFetch("no item at the last spots");
        return true;
    }

    uint32 const missing = fetch.needed > count ? fetch.needed - count : 0;
    QuestItemSourceIndex::Resolved resolved = QuestItemSourceIndex::Resolve(botAI, bot, fetch.itemId, missing);
    int const idx = QuestItemSourcePolicy::PickSpot(resolved.spots, bot->GetMapId(), bot->GetPositionX(),
                                                    bot->GetPositionY(), fetch.visited);
    if (idx < 0)
    {
        FailItemFetch("no other spot");
        return true;
    }
    QuestItemSourcePolicy::GoToSpot(fetch, resolved.spots[idx]);
    fetch.creatureEntries = std::move(resolved.creatureEntries);
    SF_LOG_DEBUG("playerbots", "[QUEST] %s fetches %s at the next spot: %s (%s), %u spots without an item",
                 bot->GetName().c_str(), QuestItemSourceIndex::ItemName(fetch.itemId).c_str(),
                 QuestItemSourceIndex::SourceName(fetch.spot).c_str(), QuestItemSourcePolicy::KindName(fetch.spot.kind),
                 fetch.spotsWithoutItem);
    return true;
}

bool NewRpgBaseAction::WorkFetchCreatures(WorldPosition const& dest, float dist, uint32 count)
{
    // A corpse with loot first: the loot pipeline picks the item up (StoreLootAction keeps quest items).
    if (AI_VALUE(bool, "has available loot"))
        return false;

    QuestItemSourcePolicy::Fetch& fetch = botAI->GetItemFetch();
    Creature* nearest = nullptr;
    float nearestDist = 0.0f;
    for (uint32 entry : fetch.creatureEntries)
    {
        Creature* creature = bot->FindNearestCreature(entry, QuestItemSourcePolicy::SOURCE_SIGHT, true);
        if (!creature)
            continue;
        float const d = bot->GetDistance(creature);
        if (!nearest || d < nearestDist)
        {
            nearest = creature;
            nearestDist = d;
        }
    }

    if (!nearest)
    {
        // Back from a chase first: the spot is where the mobs were.
        if (dist > QuestItemSourcePolicy::LEASH_DIST)
            return MoveFarOnQuestTrip(dest);
        return NextItemFetchSpot(count);
    }

    // The grind pick prefers these mobs and takes them even when gray (GrindTargetValue); a walk up brings one into its
    // view, and the fight starts the way it does for any player who walks up to a mob.
    if (nearestDist > QuestItemSourcePolicy::APPROACH_DIST)
        return MoveWorldObjectTo(nearest->GetGUID(), QuestItemSourcePolicy::APPROACH_STOP);
    return MoveRandomNear(8.0f);
}

bool NewRpgBaseAction::WorkFetchObject(uint32 count)
{
    QuestItemSourcePolicy::Fetch& fetch = botAI->GetItemFetch();
    GameObject* go = GatherNodeMgr::FindLiveNode(bot->GetMap(), fetch.spot.entry, fetch.spot.spawnId);
    if (!go)
    {
        // Pooled chests show at one of several spawn points: another one in sight will do.
        go = bot->FindNearestGameObject(fetch.spot.entry, QuestItemSourcePolicy::SOURCE_SIGHT);
        if (go && (!go->isSpawned() || go->GetGoState() != GO_STATE_READY))
            go = nullptr;
    }
    if (!go)
        return NextItemFetchSpot(count);

    // An opening cast is under way: movement would cancel it.
    if (bot->GetCurrentSpell(CURRENT_GENERIC_SPELL) || bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
        return false;

    LootObject lootObj(bot, go->GetGUID());
    if (lootObj.IsEmpty())
        return NextItemFetchSpot(count);

    // The gather status's handover to the loot pipeline (NewRpgDoGather.cpp): point it at this chest unless it is busy
    // with a valid target in loot range, walk up ourselves when its own approach cannot, and let it open and loot.
    LootObject currentTarget = AI_VALUE(LootObject, "loot target");
    bool const keepCurrent = !currentTarget.IsEmpty() && currentTarget.IsLootPossible(bot) &&
                             AI_VALUE2(float, "distance", "loot target") <= sPlayerbotAIConfig->lootDistance;
    if (currentTarget.guid != go->GetGUID() && !keepCurrent)
        context->GetValue<LootObject>("loot target")->Set(LootObject(bot, go->GetGUID()));

    if (!lootObj.IsLootPossible(bot) || bot->GetDistance(go) > INTERACTION_DISTANCE - 2.0f)
    {
        if (bot->isMoving())
            return false;
        return MoveTo(bot->GetMapId(), go->GetPositionX(), go->GetPositionY(), go->GetPositionZ(), false, false, false,
                      false);
    }
    return AI_VALUE(LootObjectStack*, "available loot")->Add(go->GetGUID());
}

bool NewRpgBaseAction::WorkFetchVendor(uint32 count)
{
    QuestItemSourcePolicy::Fetch& fetch = botAI->GetItemFetch();
    Creature* vendor =
        bot->GetMap()->GetCreature(BotGuid::Create<BotHighGuid::Unit>(fetch.spot.entry, fetch.spot.spawnId));
    if (!vendor || !vendor->IsAlive())
        vendor = bot->FindNearestCreature(fetch.spot.entry, QuestItemSourcePolicy::SOURCE_SIGHT, true);
    if (!vendor)
        return NextItemFetchSpot(count);

    if (!IsWithinInteractionDist(vendor))
        return MoveWorldObjectTo(vendor->GetGUID());

    VendorItemData const* items = vendor->GetVendorItems();
    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(fetch.itemId);
    if (!items || !proto)
        return NextItemFetchSpot(count);

    for (uint32 slot = 0; slot < items->GetItemCount(); ++slot)
    {
        VendorItem const* vendorItem = items->GetItem(slot);
        if (!vendorItem || vendorItem->item != fetch.itemId || vendorItem->Type != ITEM_VENDOR_TYPE_ITEM ||
            vendorItem->ExtendedCost)
            continue;

        uint32 const lotSize = proto->BuyCount ? proto->BuyCount : 1;
        uint64 const lotPrice =
            proto->BuyPrice > 0 ? uint64(std::floor(proto->BuyPrice * bot->GetReputationPriceDiscount(vendor))) : 0;
        uint32 lots = QuestItemSourcePolicy::LotsToBuy(fetch.needed > count ? fetch.needed - count : 0, lotSize,
                                                       bot->GetMoney(), BotSalePolicy::PurchaseReserve(bot->getLevel()),
                                                       lotPrice);
        if (vendorItem->maxcount)
            lots = std::min(lots, vendor->GetVendorItemCurrentCount(vendorItem) / lotSize);

        uint32 bought = 0;
        for (uint32 lot = 0; lot < lots; ++lot)
        {
            uint32 const before = bot->GetItemCount(fetch.itemId, false);
            // The core takes the bot's own money and runs every vendor check (reputation, stock, bag space).
            bot->BuyItemFromVendorSlot(vendor->GetGUID(), slot, fetch.itemId, 1, NULL_BAG, NULL_SLOT);
            uint32 const after = bot->GetItemCount(fetch.itemId, false);
            if (after <= before)
                break;
            bought += after - before;
        }
        if (!bought)
        {
            FailItemFetch(lots ? "the vendor refused the purchase" : "not enough money or stock");
            return true;
        }
        SF_LOG_INFO("playerbots", "[QUEST] %s buys %u %s from %s", bot->GetName().c_str(), bought, proto->Name1.c_str(),
                    vendor->GetName().c_str());
        return true;
    }
    return NextItemFetchSpot(count);
}
