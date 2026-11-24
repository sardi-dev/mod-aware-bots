/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * BT-212: random bots earn interaction credits (talk, click, use a quest item, open an object) where the core gives a
 * creature objective's credit for that and not for a kill; the decisions are in Utils/QuestInteractPolicy.h.
 */

#include "NewRpgBaseAction.h"
#include "CheckMountStateAction.h"

#include <algorithm>
#include <list>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "BotCompat_S.h"
#include "BotScenario.h"
#include "ConditionMgr.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "DBCStores.h"
#include "DBCStructure.h"
#include "GameObject.h"
#include "GameObjectAI.h"
#include "GossipDef.h"
#include "Item.h"
#include "ItemPrototype.h"
#include "Map.h"
#include "NewRpgSpawnIndex.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "QuestHandInPolicy.h"
#include "QuestInteractPolicy.h"
#include "QuestRetryPolicy.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"
#include "SmartScriptMgr.h"
#include "Spell.h"
#include "SpellAuraDefines.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"

namespace
{
using QuestInteractPolicy::Skip;
using QuestInteractPolicy::Way;

// How each creature credit of a quest objective is given, read once from the core's SmartAI, spell click, item, spell,
// condition and gossip stores, and the open-world spawns of the targets.
namespace QuestInteractIndex
{
struct Interaction
{
    Way way = Way::Gossip;
    uint32 target = 0;     // creature or object entry
    bool object = false;   // the target is an object (OpenObject, or a quest item used on an object)
    uint32 spawnOnly = 0;  // a SmartAI row for one spawn (negative entryorguid): only that spawn
    uint32 itemId = 0;     // UseItem
    uint32 spellId = 0;    // UseItem: the item spell that gives the credit
    std::vector<QuestInteractPolicy::GossipStep> route;  // Gossip/OpenObject: options to pick, the credit's last
    bool chatThrough = false;  // a C++ gossip chain: the first chat option of each page
};

struct TargetSpawn
{
    uint32 spawnId = 0;
    uint32 map = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    bool phased = false;
    std::vector<int32> events;
};

// Credits that C++ scripts give: no world table says so. The target's script name must still match, so a changed
// script turns the row off instead of sending bots to the wrong place.
struct ScriptedCredit
{
    uint32 credit;
    Way way;
    uint32 target;
    char const* script;
};

ScriptedCredit const SCRIPTED_CREDITS[] = {
    // Dearest Natalia (8304), zone_silithus.cpp: Rutgar's chain first, then Frankal's.
    {15222, Way::Gossip, 15170, "npcs_rutgar_and_frankal"},
    {15221, Way::Gossip, 15171, "npcs_rutgar_and_frankal"},
    // Evacuate the Merchant Square (14098), gilneas.cpp: each door counts once.
    {35830, Way::OpenObject, 195327, "go_merchant_square_door"},
};

std::once_flag indexOnce;
// Written once under indexOnce, then read-only.
std::vector<Interaction> interactions;
std::unordered_map<uint32, std::vector<uint32>> byCredit;  // credit entry -> interaction indexes
std::unordered_set<uint32> creditSpawned;                   // credit entries with an unphased open-world spawn
std::unordered_map<uint32, std::vector<TargetSpawn>> creatureSpawns;
std::unordered_map<uint32, std::vector<TargetSpawn>> gameObjectSpawns;

// A lock that opens without a skill or a key (a quest object's "open" lock), or none.
bool OpensFreely(GameObjectTemplate const* tmpl)
{
    LockEntry const* lock = sLockStore.LookupEntry(tmpl->GetLockId());
    if (!lock)
        return true;
    for (uint8 i = 0; i < MAX_LOCK_CASE; ++i)
    {
        if (lock->Type[i] == LOCK_KEY_ITEM && lock->Index[i])
            return false;
        if (lock->Type[i] == LOCK_KEY_SKILL && SkillByLockType(LockType(lock->Index[i])))
            return false;
    }
    return true;
}

struct Builder
{
    std::unordered_set<uint32> credits;
    std::unordered_map<uint32, std::vector<uint32>> creditsOfSpell;  // spell -> credits it gives (or triggers)
    std::unordered_map<uint32, std::vector<uint32>> itemsOfSpell;    // on-use spell -> items
    std::unordered_set<uint32> skippedCredits[3];                     // by Skip
    uint32 unhandledEvents = 0;

    bool RideSpell(uint32 spellId) const
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo)
            return false;
        for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
            if (spellInfo->Effects[i].ApplyAuraName == SPELL_AURA_CONTROL_VEHICLE)
                return true;
        return false;
    }

    std::vector<uint32> const* CreditsOf(uint32 spellId) const
    {
        auto const found = creditsOfSpell.find(spellId);
        return found == creditsOfSpell.end() ? nullptr : &found->second;
    }

    void Add(uint32 credit, Interaction const& in, Skip skip)
    {
        // An object that needs a skill or a key would need the opening spell: not opened.
        if (in.way == Way::OpenObject)
        {
            GameObjectTemplate const* tmpl = sObjectMgr->GetGameObjectTemplate(in.target);
            if (!tmpl || !OpensFreely(tmpl))
                return;
        }
        if (skip != Skip::None)
        {
            skippedCredits[uint32(skip)].insert(credit);
            return;
        }
        std::vector<uint32>& list = byCredit[credit];
        for (uint32 idx : list)
        {
            Interaction const& other = interactions[idx];
            if (other.way == in.way && other.target == in.target && other.object == in.object &&
                other.itemId == in.itemId && other.spawnOnly == in.spawnOnly)
                return;
        }
        list.push_back(uint32(interactions.size()));
        interactions.push_back(in);
    }

    // Spells that give a credit of a quest objective, and spells that trigger one of them (one level, as an item spell
    // that fires the credit spell on its target).
    void IndexSpells()
    {
        uint32 const size = sSpellMgr->GetSpellInfoStoreSize();
        for (uint32 spellId = 1; spellId < size; ++spellId)
            if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId))
                for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
                {
                    uint32 const effect = spellInfo->Effects[i].Effect;
                    uint32 const credit = uint32(spellInfo->Effects[i].MiscValue);
                    if ((effect == SPELL_EFFECT_KILL_CREDIT || effect == SPELL_EFFECT_KILL_CREDIT2) &&
                        credits.count(credit))
                        creditsOfSpell[spellId].push_back(credit);
                }
        std::unordered_map<uint32, std::vector<uint32>> triggered;
        for (uint32 spellId = 1; spellId < size; ++spellId)
            if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId))
                for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
                {
                    uint32 const effect = spellInfo->Effects[i].Effect;
                    if (effect != SPELL_EFFECT_TRIGGER_SPELL && effect != SPELL_EFFECT_TRIGGER_MISSILE &&
                        effect != SPELL_EFFECT_FORCE_CAST && effect != SPELL_EFFECT_TRIGGER_SPELL_WITH_VALUE)
                        continue;
                    if (std::vector<uint32> const* given = CreditsOf(spellInfo->Effects[i].TriggerSpell))
                        triggered[spellId].insert(triggered[spellId].end(), given->begin(), given->end());
                }
        for (auto& itr : triggered)
        {
            std::vector<uint32>& list = creditsOfSpell[itr.first];
            list.insert(list.end(), itr.second.begin(), itr.second.end());
        }
    }

    void IndexItems()
    {
        for (auto const& itr : *sObjectMgr->GetItemTemplateStore())
            for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
                if (itr.second.Spells[i].SpellId > 0 && itr.second.Spells[i].SpellTrigger == ITEM_SPELLTRIGGER_ON_USE)
                    itemsOfSpell[uint32(itr.second.Spells[i].SpellId)].push_back(itr.first);
    }

    // The database menus a creature's first menu opens, as far as a gossip route may reach.
    static std::vector<QuestInteractPolicy::MenuEdge> EdgesFrom(uint32 start)
    {
        std::vector<QuestInteractPolicy::MenuEdge> edges;
        std::vector<uint32> frontier = {start};
        std::unordered_set<uint32> seen = {start};
        for (std::size_t depth = 0; depth + 1 < QuestInteractPolicy::MAX_ROUTE_MENUS && !frontier.empty(); ++depth)
        {
            std::vector<uint32> next;
            for (uint32 menu : frontier)
            {
                GossipMenuItemsMapBounds bounds = sObjectMgr->GetGossipMenuItemsMapBounds(menu);
                for (auto itr = bounds.first; itr != bounds.second; ++itr)
                {
                    if (!itr->second.ActionMenuID)
                        continue;
                    edges.push_back({menu, itr->second.OptionID, itr->second.ActionMenuID});
                    if (seen.insert(itr->second.ActionMenuID).second)
                        next.push_back(itr->second.ActionMenuID);
                }
            }
            frontier = std::move(next);
        }
        return edges;
    }

    // The creature (3) or object (5) entries a spell's target conditions name.
    static void ConditionTargets(ConditionList const& conditions, std::vector<uint32>& creatures,
                                 std::vector<uint32>& objects)
    {
        for (Condition const* cond : conditions)
        {
            if (!cond || cond->ConditionType != CONDITION_OBJECT_ENTRY || cond->NegativeCondition ||
                !cond->ConditionValue2)
                continue;
            if (cond->ConditionValue1 == uint32(TypeID::TYPEID_UNIT))
                creatures.push_back(cond->ConditionValue2);
            else if (cond->ConditionValue1 == uint32(TypeID::TYPEID_GAMEOBJECT))
                objects.push_back(cond->ConditionValue2);
        }
    }

    // (c) A usable item whose spell gives the credit, used on the creature or object its conditions name. A spell
    // without a target condition is used on the credit creature itself when that one stands in the world.
    void IndexItemSpells(std::unordered_set<uint32> const& spawnedEntries)
    {
        for (auto const& itr : itemsOfSpell)
        {
            std::vector<uint32> const* given = CreditsOf(itr.first);
            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(itr.first);
            if (!given || !spellInfo)
                continue;
            std::vector<uint32> creatures;
            std::vector<uint32> objects;
            ConditionTargets(sConditionMgr->GetConditionsForNotGroupedEntry(CONDITION_SOURCE_TYPE_SPELL, itr.first),
                             creatures, objects);
            for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
                if (spellInfo->Effects[i].ImplicitTargetConditions)
                    ConditionTargets(*spellInfo->Effects[i].ImplicitTargetConditions, creatures, objects);
            for (uint32 credit : *given)
            {
                std::vector<uint32> targets = creatures;
                if (targets.empty() && objects.empty() && spawnedEntries.count(credit))
                    targets.push_back(credit);
                for (uint32 itemId : itr.second)
                {
                    Interaction in;
                    in.way = Way::UseItem;
                    in.itemId = itemId;
                    in.spellId = itr.first;
                    for (uint32 target : targets)
                    {
                        CreatureTemplate const* tmpl = sObjectMgr->GetCreatureTemplate(target);
                        in.target = target;
                        Add(credit, in,
                            QuestInteractPolicy::Classify(tmpl && tmpl->VehicleId, RideSpell(itr.first), false));
                    }
                    in.object = true;
                    for (uint32 target : objects)
                    {
                        in.target = target;
                        Add(credit, in, QuestInteractPolicy::Classify(false, RideSpell(itr.first), false));
                    }
                }
            }
        }
    }

    // The credits one SmartAI row gives: a kill credit, a cast of a credit spell, or a timed action list that does.
    std::vector<uint32> RowCredits(SmartScriptHolder const& row, bool followLists)
    {
        std::vector<uint32> out;
        uint32 const action = row.GetActionType();
        uint32 const param = row.action.raw.param1;
        if (action == QuestInteractPolicy::ACTION_KILLEDMONSTER && credits.count(param))
            out.push_back(param);
        else if (action == QuestInteractPolicy::ACTION_CAST || action == QuestInteractPolicy::ACTION_INVOKER_CAST ||
                 action == QuestInteractPolicy::ACTION_CROSS_CAST)
        {
            if (std::vector<uint32> const* given = CreditsOf(param))
                out = *given;
        }
        else if (followLists && (action == QuestInteractPolicy::ACTION_TIMED_ACTIONLIST ||
                                 action == QuestInteractPolicy::ACTION_RANDOM_TIMED_ACTIONLIST))
        {
            uint32 const lists[6] = {row.action.raw.param1, row.action.raw.param2, row.action.raw.param3,
                                     row.action.raw.param4, row.action.raw.param5, row.action.raw.param6};
            uint32 const count = action == QuestInteractPolicy::ACTION_TIMED_ACTIONLIST ? 1 : 6;
            for (uint32 i = 0; i < count; ++i)
                if (lists[i])
                    for (SmartScriptHolder const& inner :
                         sSmartScriptMgr->GetScript(int32(lists[i]), SMART_SCRIPT_TYPE_TIMED_ACTIONLIST))
                    {
                        std::vector<uint32> const more = RowCredits(inner, false);
                        out.insert(out.end(), more.begin(), more.end());
                    }
        }
        return out;
    }

    static bool StartsEscort(SmartAIEventList const& script)
    {
        for (SmartScriptHolder const& row : script)
            if (row.GetActionType() == QuestInteractPolicy::ACTION_WP_START)
                return true;
        return false;
    }

    // (a) The SmartAI script of a creature or object entry (or of one spawn): each row that gives a credit, traced back
    // through its links to the event a player starts.
    void IndexSmartScript(SmartAIEventList const& script, uint32 entry, uint32 spawnOnly, bool onObject,
                          uint32 gossipMenu)
    {
        if (script.empty())
            return;
        std::vector<QuestInteractPolicy::SmartRow> rows;
        rows.reserve(script.size());
        for (SmartScriptHolder const& row : script)
            rows.push_back({row.event_id, row.link, row.GetEventType()});

        CreatureTemplate const* tmpl = onObject ? nullptr : sObjectMgr->GetCreatureTemplate(entry);
        bool const vehicle = tmpl && tmpl->VehicleId;
        bool const escort = StartsEscort(script);
        for (std::size_t i = 0; i < script.size(); ++i)
        {
            std::vector<uint32> const given = RowCredits(script[i], true);
            if (given.empty())
                continue;
            int const root = QuestInteractPolicy::RootRow(rows, i);
            if (root < 0)
            {
                ++unhandledEvents;
                continue;
            }
            SmartScriptHolder const& start = script[root];
            Interaction in;
            in.target = entry;
            in.object = onObject;
            in.spawnOnly = spawnOnly;
            uint32 const eventType = start.GetEventType();
            if (eventType == QuestInteractPolicy::EVENT_SPELLHIT)
            {
                // A spell hit: the quest item that casts the spell, or the creature's own click spell.
                uint32 const spellId = start.event.spellHit.spell;
                auto const items = spellId ? itemsOfSpell.find(spellId) : itemsOfSpell.end();
                bool clickSpell = false;
                if (!onObject && spellId)
                {
                    SpellClickInfoMapBounds clicks = sObjectMgr->GetSpellClickInfoMapBounds(entry);
                    for (auto itr = clicks.first; itr != clicks.second; ++itr)
                        clickSpell = clickSpell || itr->second.spellId == spellId;
                }
                if (items == itemsOfSpell.end() && !clickSpell)
                {
                    ++unhandledEvents;
                    continue;
                }
                Skip const skip = QuestInteractPolicy::Classify(vehicle, RideSpell(spellId), escort);
                for (uint32 credit : given)
                {
                    if (clickSpell)
                    {
                        in.way = Way::Click;
                        in.itemId = 0;
                        in.spellId = 0;
                        Add(credit, in, skip);
                    }
                    if (items != itemsOfSpell.end())
                        for (uint32 itemId : items->second)
                        {
                            in.way = Way::UseItem;
                            in.itemId = itemId;
                            in.spellId = spellId;
                            Add(credit, in, skip);
                        }
                }
                continue;
            }

            Way way = Way::Gossip;
            if (!QuestInteractPolicy::WayForEvent(eventType, onObject, way))
            {
                ++unhandledEvents;
                continue;
            }
            in.way = way;
            if (eventType == QuestInteractPolicy::EVENT_GOSSIP_SELECT)
            {
                in.route = QuestInteractPolicy::GossipRoute(gossipMenu, start.event.gossip.sender,
                                                            start.event.gossip.action, EdgesFrom(gossipMenu));
                // Not reachable through the database menus: the script itself sends the menu (on its gossip hello),
                // so the option is picked when that menu shows.
                if (in.route.empty())
                    in.route.push_back({start.event.gossip.sender, start.event.gossip.action});
            }
            for (uint32 credit : given)
                Add(credit, in, QuestInteractPolicy::Classify(vehicle, false, escort));
        }
    }

    // (b) A click spell that gives the credit (rides are vehicles).
    void IndexClicks(uint32 entry, CreatureTemplate const& tmpl)
    {
        if (!(tmpl.npcflag & UNIT_NPC_FLAG_SPELLCLICK))
            return;
        SpellClickInfoMapBounds clicks = sObjectMgr->GetSpellClickInfoMapBounds(entry);
        for (auto itr = clicks.first; itr != clicks.second; ++itr)
            if (std::vector<uint32> const* given = CreditsOf(itr->second.spellId))
            {
                Interaction in;
                in.way = Way::Click;
                in.target = entry;
                for (uint32 credit : *given)
                    Add(credit, in,
                        QuestInteractPolicy::Classify(tmpl.VehicleId, RideSpell(itr->second.spellId), false));
            }
    }

    // (d) An object whose own spell gives the credit when used.
    void IndexObjectSpell(uint32 entry, GameObjectTemplate const& tmpl)
    {
        uint32 spellId = 0;
        if (tmpl.type == GAMEOBJECT_TYPE_GOOBER)
            spellId = tmpl.goober.spellId;
        else if (tmpl.type == GAMEOBJECT_TYPE_SPELLCASTER)
            spellId = tmpl.spellcaster.spellId;
        if (std::vector<uint32> const* given = spellId ? CreditsOf(spellId) : nullptr)
        {
            Interaction in;
            in.way = Way::OpenObject;
            in.target = entry;
            in.object = true;
            for (uint32 credit : *given)
                Add(credit, in, QuestInteractPolicy::Classify(false, RideSpell(spellId), false));
        }
    }

    void IndexScripted()
    {
        for (ScriptedCredit const& row : SCRIPTED_CREDITS)
        {
            if (!credits.count(row.credit))
                continue;
            uint32 scriptId = 0;
            if (row.way == Way::OpenObject)
            {
                GameObjectTemplate const* tmpl = sObjectMgr->GetGameObjectTemplate(row.target);
                scriptId = tmpl ? tmpl->ScriptId : 0;
            }
            else
            {
                CreatureTemplate const* tmpl = sObjectMgr->GetCreatureTemplate(row.target);
                scriptId = tmpl ? tmpl->ScriptID : 0;
            }
            if (!scriptId || std::string(sObjectMgr->GetScriptName(scriptId)) != row.script)
                continue;
            Interaction in;
            in.way = row.way;
            in.target = row.target;
            in.object = row.way == Way::OpenObject;
            in.chatThrough = row.way == Way::Gossip;
            Add(row.credit, in, Skip::None);
        }
    }
};

template <class Data>
void AddSpawns(Data const& store, std::unordered_set<uint32> const& entries,
               std::unordered_map<uint32, std::vector<int32>> const& events,
               std::unordered_map<uint32, std::vector<TargetSpawn>>& out)
{
    for (auto const& itr : store)
    {
        auto const& data = itr.second;
        if (!entries.count(data.id) || !data.dbData || !RpgSpawnIndex::OpenWorld(data.mapid))
            continue;
        TargetSpawn spawn;
        spawn.spawnId = itr.first;
        spawn.map = data.mapid;
        spawn.x = data.posX;
        spawn.y = data.posY;
        spawn.z = data.posZ;
        // Kept, unlike the hand-in and item indexes: a quest's own target often stands in the phase the quest gives.
        spawn.phased = data.phaseid || data.phaseGroup;
        auto const listed = events.find(itr.first);
        if (listed != events.end())
            spawn.events = listed->second;
        out[data.id].push_back(std::move(spawn));
    }
}

void Build()
{
    Builder b;
    for (auto const& itr : sObjectMgr->GetQuestTemplates())
        for (QuestObjective const* objective : itr.second->m_questObjectives)
            if (objective->Type == QUEST_OBJECTIVE_TYPE_NPC && objective->ObjectId)
                b.credits.insert(objective->ObjectId);

    std::unordered_set<uint32> spawnedEntries;
    for (auto const& itr : sObjectMgr->GetAllCreatureData())
        if (itr.second.dbData && !itr.second.phaseid && !itr.second.phaseGroup &&
            RpgSpawnIndex::OpenWorld(itr.second.mapid))
            spawnedEntries.insert(itr.second.id);
    for (uint32 credit : b.credits)
        if (spawnedEntries.count(credit))
            creditSpawned.insert(credit);

    b.IndexSpells();
    b.IndexItems();
    b.IndexItemSpells(spawnedEntries);

    std::unordered_set<uint32> smartCreatures;
    for (auto const& itr : *sObjectMgr->GetCreatureTemplates())
    {
        b.IndexClicks(itr.first, itr.second);
        if (itr.second.AIName == "SmartAI")
        {
            smartCreatures.insert(itr.first);
            b.IndexSmartScript(sSmartScriptMgr->GetScript(int32(itr.first), SMART_SCRIPT_TYPE_CREATURE), itr.first, 0,
                               false, itr.second.GossipMenuId);
        }
    }
    // Scripts of single spawns (negative entryorguid).
    for (auto const& itr : sObjectMgr->GetAllCreatureData())
        if (smartCreatures.count(itr.second.id))
            if (CreatureTemplate const* tmpl = sObjectMgr->GetCreatureTemplate(itr.second.id))
                b.IndexSmartScript(sSmartScriptMgr->GetScript(-int32(itr.first), SMART_SCRIPT_TYPE_CREATURE),
                                   itr.second.id, itr.first, false, tmpl->GossipMenuId);

    for (auto const& itr : *sObjectMgr->GetGameObjectTemplates())
    {
        b.IndexObjectSpell(itr.first, itr.second);
        if (itr.second.AIName == "SmartGameObjectAI")
            b.IndexSmartScript(sSmartScriptMgr->GetScript(int32(itr.first), SMART_SCRIPT_TYPE_GAMEOBJECT), itr.first,
                               0, true, itr.second.GetGossipMenuId());
    }
    b.IndexScripted();

    std::unordered_set<uint32> creatureTargets;
    std::unordered_set<uint32> objectTargets;
    for (Interaction const& in : interactions)
        (in.object ? objectTargets : creatureTargets).insert(in.target);
    AddSpawns(sObjectMgr->GetAllCreatureData(), creatureTargets,
              RpgSpawnIndex::EventSpawns(sGameEventMgr->mGameEventCreatureGuids), creatureSpawns);
    AddSpawns(sObjectMgr->GetAllGameObjectData(), objectTargets,
              RpgSpawnIndex::EventSpawns(sGameEventMgr->mGameEventGameobjectGuids), gameObjectSpawns);

    // The quests left to the POI walk because their only known way is a vehicle or an escort.
    uint32 skippedQuests[3] = {0, 0, 0};
    std::string skippedIds[3];
    for (auto const& itr : sObjectMgr->GetQuestTemplates())
    {
        bool counted[3] = {false, false, false};
        for (QuestObjective const* objective : itr.second->m_questObjectives)
        {
            if (objective->Type != QUEST_OBJECTIVE_TYPE_NPC || byCredit.count(objective->ObjectId))
                continue;
            for (uint32 s = uint32(Skip::Vehicle); s <= uint32(Skip::Escort); ++s)
                if (!counted[s] && b.skippedCredits[s].count(objective->ObjectId))
                {
                    counted[s] = true;
                    ++skippedQuests[s];
                    skippedIds[s] += " " + std::to_string(itr.first);
                }
        }
    }

    uint32 byWay[4] = {0, 0, 0, 0};
    for (Interaction const& in : interactions)
        ++byWay[uint32(in.way)];
    SF_LOG_INFO("playerbots",
                "[New RPG] quest interaction index: %u of %u creature credits have a way (%u gossip, %u click, "
                "%u item, %u object), %u credit rows without a player event, quests skipped: %u vehicle, %u escort",
                uint32(byCredit.size()), uint32(b.credits.size()), byWay[0], byWay[1], byWay[2], byWay[3],
                b.unhandledEvents, skippedQuests[uint32(Skip::Vehicle)], skippedQuests[uint32(Skip::Escort)]);
    SF_LOG_DEBUG("playerbots", "[New RPG] quest interaction index: vehicle quests:%s",
                 skippedIds[uint32(Skip::Vehicle)].c_str());
    SF_LOG_DEBUG("playerbots", "[New RPG] quest interaction index: escort quests:%s",
                 skippedIds[uint32(Skip::Escort)].c_str());
}

std::vector<uint32> const* WaysFor(uint32 credit)
{
    std::call_once(indexOnce, Build);
    auto const found = byCredit.find(credit);
    return found == byCredit.end() ? nullptr : &found->second;
}

std::string TargetName(Interaction const& in)
{
    if (in.object)
    {
        GameObjectTemplate const* tmpl = sObjectMgr->GetGameObjectTemplate(in.target);
        return tmpl ? tmpl->name : std::to_string(in.target);
    }
    CreatureTemplate const* tmpl = sObjectMgr->GetCreatureTemplate(in.target);
    return tmpl ? tmpl->Name : std::to_string(in.target);
}

FactionTemplateEntry const* FactionFor(Player* bot, CreatureTemplate const* tmpl)
{
    return sFactionTemplateStore.LookupEntry(bot->GetTeam() == HORDE ? tmpl->faction_H : tmpl->faction_A);
}

// Whether the grind could earn this credit by killing it (QuestInteractPolicy::KillEarnsCredit).
QuestInteractPolicy::CreditFacts CreditFactsFor(Player* bot, uint32 credit)
{
    QuestInteractPolicy::CreditFacts facts;
    facts.spawned = creditSpawned.count(credit) != 0;
    CreatureTemplate const* tmpl = sObjectMgr->GetCreatureTemplate(credit);
    if (!tmpl)
        return facts;
    FactionTemplateEntry const* faction = FactionFor(bot, tmpl);
    FactionTemplateEntry const* botFaction = bot->GetFactionTemplateEntry();
    facts.attackable = !(tmpl->unit_flags & (UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE)) &&
                       !(faction && botFaction && faction->IsFriendlyTo(*botFaction));
    facts.trigger = (tmpl->flags_extra & CREATURE_FLAG_EXTRA_TRIGGER) || (tmpl->unit_flags & UNIT_FLAG_NOT_SELECTABLE);
    return facts;
}

// The bot can do it now: it carries the item, and a creature to talk to or click will deal with it.
bool Usable(Player* bot, Interaction const& in)
{
    if (in.way == Way::UseItem)
        return bot->HasItemCount(in.itemId, 1);
    if (in.object)
        return true;
    CreatureTemplate const* tmpl = sObjectMgr->GetCreatureTemplate(in.target);
    if (!tmpl)
        return false;
    FactionTemplateEntry const* faction = FactionFor(bot, tmpl);
    FactionTemplateEntry const* botFaction = bot->GetFactionTemplateEntry();
    return !(faction && botFaction && faction->IsHostileTo(*botFaction));
}

// Every spawn on the bot's map of a target of the interactions it can do now.
std::vector<QuestInteractPolicy::Spot> Resolve(Player* bot, std::vector<uint32> const& ways)
{
    std::vector<QuestInteractPolicy::Spot> spots;
    GameEventMgr::ActiveEvents const active = QuestHubSearch::ActiveEventsSnapshot();
    for (uint32 idx : ways)
    {
        Interaction const& in = interactions[idx];
        if (!Usable(bot, in))
            continue;
        auto const& index = in.object ? gameObjectSpawns : creatureSpawns;
        auto const spawns = index.find(in.target);
        if (spawns == index.end())
            continue;
        for (TargetSpawn const& spawn : spawns->second)
        {
            if (spawn.map != bot->GetMapId() || (in.spawnOnly && spawn.spawnId != in.spawnOnly) ||
                !QuestHubSearch::EventSpawnShown(spawn.events, active))
                continue;
            QuestInteractPolicy::Spot spot;
            spot.interaction = idx;
            spot.entry = in.target;
            spot.spawnId = spawn.spawnId;
            spot.map = spawn.map;
            spot.x = spawn.x;
            spot.y = spawn.y;
            spot.z = spawn.z;
            spot.phased = spawn.phased;
            spots.push_back(spot);
        }
    }
    return spots;
}

std::vector<QuestInteractPolicy::ShownOption> ShownOptions(Player* bot)
{
    std::vector<QuestInteractPolicy::ShownOption> shown;
    for (auto const& itr : bot->PlayerTalkClass->GetGossipMenu().GetMenuItems())
    {
        QuestInteractPolicy::ShownOption option;
        option.key = itr.first;
        option.optionType = itr.second.OptionType;
        option.boxMoney = itr.second.BoxMoney;
        option.coded = itr.second.IsCoded;
        shown.push_back(option);
    }
    return shown;
}

// The calls the world session makes for a gossip click and the selects that follow (MiscHandler.cpp
// HandleGossipSelectOptionOpcode): the creature's or object's AI first, then the script manager, then the database
// menu. The object's hello is its use (GameObject::Use runs the script's and the AI's gossip hello). True when the
// hello was all the event asked for or at least one option was picked.
bool Talk(Player* bot, Creature* creature, GameObject* go, Interaction const& in)
{
    bot->PlayerTalkClass->ClearMenus();
    if (creature)
    {
        if (!sScriptMgr->OnGossipHello(bot, creature) && !creature->AI()->OnGossipHello(bot))
        {
            bot->TalkedToCreature(creature->GetEntry(), creature->GetGUID());
            bot->PrepareGossipMenu(creature, creature->GetCreatureTemplate()->GossipMenuId, true);
        }
    }
    else
        go->Use(bot);

    if (in.route.empty() && !in.chatThrough)
        return true;

    bool answered = false;
    uint32 lastSender = 0, lastAction = 0;
    for (uint32 page = 0; page < QuestInteractPolicy::MAX_GOSSIP_PAGES; ++page)
    {
        GossipMenu& menu = bot->PlayerTalkClass->GetGossipMenu();
        uint32 const menuId = menu.GetMenuId();
        std::vector<QuestInteractPolicy::ShownOption> const shown = ShownOptions(bot);
        int64 const pick = in.chatThrough ? QuestInteractPolicy::PickChatOption(shown)
                                          : QuestInteractPolicy::PickRouteOption(in.route, menuId, shown);
        if (pick < 0)
            break;
        uint32 const option = uint32(pick);
        uint32 const sender = bot->PlayerTalkClass->GetGossipOptionSender(option);
        uint32 const action = bot->PlayerTalkClass->GetGossipOptionAction(option);
        // A script that closes its menu without clearing it leaves the same item: not answered twice.
        if (answered && sender == lastSender && action == lastAction)
            break;
        lastSender = sender;
        lastAction = action;
        if (creature)
        {
            creature->AI()->sGossipSelect(bot, menuId, option);
            if (!sScriptMgr->OnGossipSelect(bot, creature, sender, action))
                bot->OnGossipSelect(creature, option, menuId);
        }
        else
        {
            go->AI()->GossipSelect(bot, menuId, option);
            if (!sScriptMgr->OnGossipSelect(bot, go, sender, action))
                bot->OnGossipSelect(go, option, menuId);
        }
        answered = true;
        if (!in.chatThrough && menuId == in.route.back().menu && option == in.route.back().option)
            break;
    }
    bot->PlayerTalkClass->ClearMenus();
    return answered;
}

// What HandleUseItemOpcode does after reading the packet (the module's UseItemAction does the same for itself), with
// the creature or object as the spell's target. The item stays a real item: charges and cooldowns apply.
bool UseItemOn(Player* bot, Interaction const& in, Unit* unit, GameObject* go)
{
    Item* item = bot->GetItemByEntry(in.itemId);
    if (!item || bot->CanUseItem(item) != EQUIP_ERR_OK)
        return false;
    if (item->GetTemplate()->Bonding == BIND_WHEN_USE && !item->IsSoulBound())
    {
        item->SetState(ITEM_CHANGED, bot);
        item->SetBinding(true);
    }
    SpellCastTargets targets;
    if (unit)
        targets.SetUnitTarget(unit);
    else
        targets.SetGOTarget(go);
    if (!sScriptMgr->OnItemUse(bot, item, targets))
        bot->CastItemUseSpell(item, targets, 1, 0);
    return true;
}
}  // namespace QuestInteractIndex
}  // namespace

bool NewRpgBaseAction::InteractTripOwns(WorldPosition const& dest)
{
    return QuestInteractPolicy::SameDest(botAI->GetInteractTrip(), dest.GetMapId(), dest.GetPositionX(),
                                         dest.GetPositionY());
}

void NewRpgBaseAction::FailInteractTrip(char const* why)
{
    QuestInteractPolicy::Trip& trip = botAI->GetInteractTrip();
    if (!trip.active)
        return;

    uint32 const questId = trip.questId;
    uint32 const interaction = trip.spot.interaction;
    trip = QuestInteractPolicy::Trip();
    Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
    QuestRetryPolicy::Failure& failure = botAI->GetQuestRetries()[questId];
    bool const abandon =
        QuestRetryPolicy::RecordFailure(failure, getMSTime(), bot->GetZoneId(), bot->getLevel(), false,
                                        QuestRetryPolicy::IsStuckReason(why));
    uint32 const tries = failure.tries;
    std::string const target = interaction < QuestInteractIndex::interactions.size()
                                   ? QuestInteractIndex::TargetName(QuestInteractIndex::interactions[interaction])
                                   : std::string();
    SF_LOG_INFO("playerbots", "[QUEST] %s gives up on %s for %u %s: %s, failed tries %u", bot->GetName().c_str(),
                target.c_str(), questId, quest ? quest->GetTitle().c_str() : "", why, tries);
    // The same three-strikes rule as an objective POI without progress (BT-208).
    if (abandon && bot->GetQuestStatus(questId) == QUEST_STATUS_INCOMPLETE && DropQuest(questId))
    {
        botAI->GetRpgStatistic().questAbandoned++;
        SF_LOG_INFO("playerbots", "[QUEST] %s abandons %u %s after %u failed tries", bot->GetName().c_str(), questId,
                    quest ? quest->GetTitle().c_str() : "", tries);
    }
    botAI->GetRpgInfo().ChangeToIdle();
}

bool NewRpgBaseAction::StartQuestInteraction(Quest const* quest, std::vector<uint32> const& deferred)
{
    // Off in a deterministic .botscenario and wherever a trip would not be a plain walk (as the item fetch).
    if (BotScenario::PickNearest(bot) || bot->InBattleground() || !bot->GetMap() || bot->GetMap()->Instanceable() ||
        bot->IsInFlight() || bot->GetTransport() || bot->GetVehicle() || bot->IsBeingTeleported())
        return false;

    uint32 const mapId = bot->GetMapId();
    float const x = bot->GetPositionX();
    float const y = bot->GetPositionY();

    bool found = false;
    float bestScore = 0.0f;
    QuestInteractPolicy::Spot bestSpot;
    QuestObjective const* bestObjective = nullptr;
    uint32 bestCount = 0;
    for (QuestObjective const& objective : BotQuestObjectives(quest))
    {
        if (objective.Type != QUEST_OBJECTIVE_TYPE_NPC || !objective.ObjectId || objective.Amount <= 0 ||
            BotIsQuestObjectiveComplete(bot, quest, objective))
            continue;
        uint32 const count = bot->GetQuestObjectiveCounter(objective.Id);
        if (count >= uint32(objective.Amount) || QuestInteractPolicy::Deferred(deferred, objective.ObjectId))
            continue;
        std::vector<uint32> const* ways = QuestInteractIndex::WaysFor(objective.ObjectId);
        if (!QuestInteractPolicy::NeedsInteraction(QuestInteractIndex::CreditFactsFor(bot, objective.ObjectId),
                                                   ways != nullptr))
            continue;
        std::vector<QuestInteractPolicy::Spot> const spots = QuestInteractIndex::Resolve(bot, *ways);
        float score = 0.0f;
        int const idx = QuestInteractPolicy::PickSpot(spots, mapId, x, y, {}, &score);
        if (idx < 0 || (found && score >= bestScore))
            continue;
        found = true;
        bestScore = score;
        bestSpot = spots[idx];
        bestObjective = &objective;
        bestCount = count;
    }
    if (!found)
        return false;

    QuestInteractPolicy::Trip& trip = botAI->GetInteractTrip();
    trip = QuestInteractPolicy::Start(quest->GetQuestId(), bestObjective->Id, bestObjective->ObjectId,
                                      uint32(bestObjective->Amount), bestCount, bestSpot,
                                      QuestInteractPolicy::Dist2d(bestSpot, x, y), botAI->GetQuestClockMs());
    trip.deferred = deferred;
    QuestInteractIndex::Interaction const& in = QuestInteractIndex::interactions[bestSpot.interaction];
    SF_LOG_INFO("playerbots", "[QUEST] %s travels to %s (%u yd) for %u %s (%s)", bot->GetName().c_str(),
                QuestInteractIndex::TargetName(in).c_str(), uint32(QuestInteractPolicy::Dist2d(bestSpot, x, y)),
                quest->GetQuestId(), quest->GetTitle().c_str(), QuestInteractPolicy::Verb(in.way));
    return true;
}

bool NewRpgBaseAction::DoQuestInteraction(Quest const* quest, bool mayStart, bool& handled)
{
    handled = false;
    QuestInteractPolicy::Trip& trip = botAI->GetInteractTrip();
    // A ride or a taxi on the route is still under way (as the hand-in trip).
    if (trip.active && trip.questId == quest->GetQuestId() && trip.spot.map != bot->GetMapId() &&
        (bot->IsInFlight() || bot->GetTransport() || bot->IsBeingTeleported()))
    {
        handled = true;
        return false;
    }
    // Another quest's trip or one left behind on another continent: dropped quietly, a later pick starts it again.
    if (trip.active && (trip.questId != quest->GetQuestId() || trip.spot.map != bot->GetMapId()))
        trip = QuestInteractPolicy::Trip();

    if (!trip.active)
    {
        if (!mayStart || !StartQuestInteraction(quest, {}))
            return false;
        handled = true;
        return true;
    }

    handled = true;
    uint32 const count = bot->GetQuestObjectiveCounter(trip.objectiveId);
    WorldPosition const dest(trip.spot.map, trip.spot.x, trip.spot.y, trip.spot.z);
    float const dist = bot->GetExactDist(&dest);
    QuestInteractPolicy::Step const step = QuestInteractPolicy::Judge(trip, botAI->GetQuestClockMs(), dist, count);
    switch (step)
    {
        case QuestInteractPolicy::Step::Done:
            SF_LOG_INFO("playerbots", "[QUEST] %s has %u/%u credits for %u %s", bot->GetName().c_str(), count,
                        trip.needed, quest->GetQuestId(), quest->GetTitle().c_str());
            QuestRetryPolicy::OnQuestEvent(botAI->GetQuestRetries(), quest->GetQuestId(), QuestRetryPolicy::QuestEvent::Progress);
            trip = QuestInteractPolicy::Trip();
            return true;
        case QuestInteractPolicy::Step::Expired:
            FailInteractTrip(QuestInteractPolicy::StepName(step));
            return true;
        case QuestInteractPolicy::Step::NextSpot:
            return NextInteractSpot();
        case QuestInteractPolicy::Step::Travel:
            return MoveFarOnQuestTrip(dest);
        case QuestInteractPolicy::Step::Wait:
            return false;
        case QuestInteractPolicy::Step::Act:
            break;
    }
    return WorkInteraction(quest);
}

bool NewRpgBaseAction::NextInteractSpot()
{
    QuestInteractPolicy::Trip& trip = botAI->GetInteractTrip();
    if (QuestInteractPolicy::LeaveSpot(trip))
    {
        FailInteractTrip("no credit at the last spots");
        return true;
    }
    std::vector<uint32> const* ways = QuestInteractIndex::WaysFor(trip.credit);
    std::vector<QuestInteractPolicy::Spot> const spots =
        ways ? QuestInteractIndex::Resolve(bot, *ways) : std::vector<QuestInteractPolicy::Spot>();
    int const idx = QuestInteractPolicy::PickSpot(spots, bot->GetMapId(), bot->GetPositionX(), bot->GetPositionY(),
                                                  trip.visited);
    if (idx < 0)
    {
        FailInteractTrip("no other spot");
        return true;
    }
    QuestInteractPolicy::GoToSpot(trip, spots[idx]);
    SF_LOG_DEBUG("playerbots", "[QUEST] %s tries the next spot of %s, %u spots without a credit",
                 bot->GetName().c_str(),
                 QuestInteractIndex::TargetName(QuestInteractIndex::interactions[spots[idx].interaction]).c_str(),
                 trip.spotsWithoutCredit);
    return true;
}

bool NewRpgBaseAction::WorkInteraction(Quest const* quest)
{
    QuestInteractPolicy::Trip& trip = botAI->GetInteractTrip();
    QuestInteractIndex::Interaction const& in = QuestInteractIndex::interactions[trip.spot.interaction];
    // A cast under way (the item's spell, an object's opening) would be cut by a move.
    if (bot->IsNonMeleeSpellCasted(false))
        return false;
    if (in.way == Way::UseItem && !bot->HasItemCount(in.itemId, 1))
    {
        FailInteractTrip("the quest item is gone");
        return true;
    }

    // The nearest target in sight the bot has not acted on yet (the spawn the trip went to, or another of the entry).
    WorldObject* target = nullptr;
    float targetDist = 0.0f;
    if (in.object)
    {
        std::list<GameObject*> objects;
        bot->GetGameObjectListWithEntryInGrid(objects, in.target, QuestInteractPolicy::TARGET_SIGHT);
        for (GameObject* go : objects)
        {
            if (!go->isSpawned() || go->GetGoState() != GO_STATE_READY ||
                QuestInteractPolicy::Used(trip, go->GetGUID()) ||
                (in.spawnOnly && go->GetDBTableGUIDLow() != in.spawnOnly))
                continue;
            float const d = bot->GetDistance(go);
            if (!target || d < targetDist)
            {
                target = go;
                targetDist = d;
            }
        }
    }
    else
    {
        std::list<Creature*> creatures;
        bot->GetCreatureListWithEntryInGrid(creatures, in.target, QuestInteractPolicy::TARGET_SIGHT);
        for (Creature* creature : creatures)
        {
            if (!creature->IsAlive() || creature->IsInCombat() ||
                QuestInteractPolicy::Used(trip, creature->GetGUID()) ||
                (in.spawnOnly && creature->GetDBTableGUIDLow() != in.spawnOnly))
                continue;
            float const d = bot->GetDistance(creature);
            if (!target || d < targetDist)
            {
                target = creature;
                targetDist = d;
            }
        }
    }
    if (!target)
        return NextInteractSpot();

    if (!IsWithinInteractionDist(target))
        return MoveWorldObjectTo(target->GetGUID());

    Creature* creature = target->ToCreature();
    GameObject* go = target->ToGameObject();
    bool acted = false;
    switch (in.way)
    {
        case Way::Gossip:
            // The world session's own check for a gossip hello: alive, in reach, not hostile.
            acted = bot->GetNPCIfCanInteractWith(creature->GetGUID(), UNIT_NPC_FLAG_NONE) &&
                    QuestInteractIndex::Talk(bot, creature, nullptr, in);
            break;
        case Way::Click:
            // A client click dismounts first (HandleSpellClick's tryAutoDismount).
            if (bot->IsMounted())
                CheckMountStateAction::DismountLikeClient(bot);  // BT-235: not a bare Unit::Dismount()
        {
            // False when the entry's click spells do not fit the bot (their conditions). An entry with no click spell
            // at all is clicked for its script (SmartAI's on spell click), which runs either way.
            SpellClickInfoMapBounds const clicks = sObjectMgr->GetSpellClickInfoMapBounds(creature->GetEntry());
            bool const clicked = creature->HandleSpellClick(bot);
            acted = clicked || clicks.first == clicks.second;
            break;
        }
        case Way::UseItem:
            if (bot->isMoving())
                bot->StopMoving();
            bot->SetFacingToObject(target);
            acted = QuestInteractIndex::UseItemOn(bot, in, creature, go);
            break;
        case Way::OpenObject:
            if (!bot->GetGameObjectIfCanInteractWith(go->GetGUID(), GameobjectTypes(go->GetGoType())))
                break;
            acted = QuestInteractIndex::Talk(bot, nullptr, go, in);
            break;
    }

    if (!acted)
    {
        // Nothing offered for this credit now (an earlier step of the quest comes first, or the core refused): the
        // quest's other interaction credits get their turn before this trip counts as failed.
        SF_LOG_DEBUG("playerbots", "[QUEST] %s: %s offers nothing for %u %s now", bot->GetName().c_str(),
                     target->GetName().c_str(), quest->GetQuestId(), quest->GetTitle().c_str());
        QuestInteractPolicy::Trip const current = trip;
        if (StartQuestInteraction(quest, QuestInteractPolicy::Defer(current)))
            return true;
        // No other credit to go for: this target stays the one to try, and the act counts toward giving up.
        trip = current;
    }
    QuestInteractPolicy::NoteAct(trip, botAI->GetQuestClockMs(), target->GetGUID(), in.way);
    if (acted)
        SF_LOG_INFO("playerbots", "[QUEST] %s %s %s for %u %s", bot->GetName().c_str(),
                    QuestInteractPolicy::Verb(in.way), target->GetName().c_str(), quest->GetQuestId(),
                    quest->GetTitle().c_str());
    return true;
}
