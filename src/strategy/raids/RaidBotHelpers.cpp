/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "RaidBotHelpers.h"

#include "Bag.h"
#include "CellImpl.h"
#include "Creature.h"
#include "GameObject.h"
#include "GridNotifiers.h"
#include "Group.h"
#include "Item.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerbotSpec.h"
#include "RaidGeometry.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Unit.h"
#include "Util.h"
#include <cmath>
#include <map>
#include <mutex>
#include <unordered_map>
#include <utility>

namespace
{
// AC Bot/PlayerbotAI.cpp IsAssistTankOfIndex and its siblings share this count: assistants first, then the others, in
// group order.
bool IsMemberOfIndex(Player* player, uint8 index, bool indexLivingOnly, BotGuid const& excluded,
    std::function<bool(Player*)> const& inRole)
{
    if (!inRole(player))
        return false;

    if (player->GetGUID() == excluded)
        return false;

    if (indexLivingOnly && !player->IsAlive())
        return false;

    Group* group = player->GetGroup();
    if (!group)
        return false;

    uint8 totalAssistants = 0;
    uint8 assistantsBeforePlayer = 0;
    uint8 nonAssistantsBeforePlayer = 0;
    bool playerFound = false;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || (indexLivingOnly && !member->IsAlive()) || !inRole(member) || member->GetGUID() == excluded)
            continue;

        bool const isAssistant = group->IsAssistant(member->GetGUID());
        if (isAssistant)
            ++totalAssistants;

        if (member == player)
            playerFound = true;
        else if (!playerFound)
        {
            if (isAssistant)
                ++assistantsBeforePlayer;
            else
                ++nonAssistantsBeforePlayer;
        }
    }

    if (!playerFound)
        return false;

    // An assistant's index is the number of assistants before them; a non-assistant's is shifted by all assistants.
    uint8 const playerIndex = group->IsAssistant(player->GetGUID()) ?
        assistantsBeforePlayer : uint8(totalAssistants + nonAssistantsBeforePlayer);

    return playerIndex == index;
}

struct GameObjectSpawnIdVisitor
{
    uint32 spawnId;
    GameObject* found = nullptr;

    void Visit(GameObjectMapType& objects)
    {
        for (GameObjectMapType::iterator itr = objects.begin(); itr != objects.end() && !found; ++itr)
            if (itr->GetSource()->GetDBTableGUIDLow() == spawnId)
                found = itr->GetSource();
    }

    template<class SKIP> void Visit(GridRefManager<SKIP>&) { }
};
}

BotGuid RaidBotHelpers::GetMainTankGuid(Group* group)
{
    if (!group)
        return BotGuid::Empty;

    Group::MemberSlotList const& slots = group->GetMemberSlots();
    for (Group::member_citerator itr = slots.begin(); itr != slots.end(); ++itr)
    {
        if (itr->flags & uint8(MEMBER_FLAG_MAINTANK))
            return itr->guid;
    }

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && PlayerBotSpec::IsTank(member) && member->IsAlive())
            return member->GetGUID();
    }

    return BotGuid::Empty;
}

bool RaidBotHelpers::IsAssistTankOfIndex(Player* player, uint8 index, bool indexLivingOnly)
{
    return IsMemberOfIndex(player, index, indexLivingOnly, GetMainTankGuid(player->GetGroup()),
        [](Player* member) { return PlayerBotSpec::IsTank(member); });
}

bool RaidBotHelpers::IsAssistHealOfIndex(Player* player, uint8 index, bool indexLivingOnly)
{
    return IsMemberOfIndex(player, index, indexLivingOnly, BotGuid::Empty,
        [](Player* member) { return PlayerBotSpec::IsHeal(member); });
}

bool RaidBotHelpers::IsAssistRangedDpsOfIndex(Player* player, uint8 index, bool indexLivingOnly)
{
    return IsMemberOfIndex(player, index, indexLivingOnly, BotGuid::Empty,
        [](Player* member) { return PlayerBotSpec::IsRangedDps(member); });
}

int32 RaidBotHelpers::GetAssistTankIndex(Player* player)
{
    Group* group = player->GetGroup();
    if (!group)
        return -1;

    int32 counter = 0;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member)
            continue;

        if (player == member)
            return counter;

        if (PlayerBotSpec::IsTank(member, true) && group->IsAssistant(member->GetGUID()))
            ++counter;
    }

    return 0;
}

bool RaidBotHelpers::StaggerTimer::Ready(uint64 key, uint32 minDelayMs, uint32 maxDelayMs)
{
    return _clock.Ready(key, getMSTime(), urand(minDelayMs, maxDelayMs));
}

uint32 RaidBotHelpers::StaggerTimer::RemainingMs() const
{
    return _clock.RemainingMs(getMSTime());
}

bool RaidBotHelpers::HasItemInInventory(Player* bot, uint32 itemId)
{
    for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
        if (Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            if (item->GetEntry() == itemId)
                return true;

    for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        Bag* bag = bot->GetBagByPos(i);
        if (!bag)
            continue;

        for (uint32 j = 0; j < bag->GetBagSize(); ++j)
            if (Item* item = bag->GetItemByPos(uint8(j)))
                if (item->GetEntry() == itemId)
                    return true;
    }

    return false;
}

namespace
{
bool UseItemWithTargets(Player* bot, Item* item, SpellCastTargets& targets)
{
    if (!item || bot->CanUseItem(item) != EQUIP_ERR_OK || bot->IsNonMeleeSpellCasted(false))
        return false;

    bool hasSpell = false;
    for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
    {
        if (item->GetTemplate()->Spells[i].SpellId > 0)
        {
            hasSpell = true;
            break;
        }
    }

    if (!hasSpell)
        return false;

    if (!sScriptMgr->OnItemUse(bot, item, targets))
        bot->CastItemUseSpell(item, targets, 1, 0);

    return true;
}
}

bool RaidBotHelpers::UseItemOnUnit(Player* bot, Item* item, Unit* target)
{
    if (!target)
        return false;

    SpellCastTargets targets;
    targets.SetUnitTarget(target);
    return UseItemWithTargets(bot, item, targets);
}

bool RaidBotHelpers::UseItemOnGameObject(Player* bot, Item* item, GameObject* target)
{
    if (!target)
        return false;

    SpellCastTargets targets;
    targets.SetGOTarget(target);
    return UseItemWithTargets(bot, item, targets);
}

GameObject* RaidBotHelpers::FindGameObjectBySpawnId(Map* map, uint32 spawnId)
{
    GameObjectData const* data = sObjectMgr->GetGOData(spawnId);
    if (!map || !data || data->mapid != map->GetId())
        return nullptr;

    if (!map->Instanceable())
        return map->GetGameObject(MAKE_NEW_GUID(spawnId, data->id, HIGHGUID_GAMEOBJECT));

    GameObjectSpawnIdVisitor visitor{ spawnId };
    map->VisitAll(data->posX, data->posY, 50.0f, visitor);
    return visitor.found;
}

namespace
{
// (instance id, spawn id) to the GUID found; maps of different instances may update on different threads. Entries of
// instances nobody asks about any more are pruned.
struct CachedSpawnGuid
{
    uint64 guid = 0;
    uint32 lastTouchMs = 0;
};

using SpawnGuidCache = std::map<std::pair<uint32, uint32>, CachedSpawnGuid>;

std::mutex spawnGuidCacheLock;
SpawnGuidCache creatureSpawnGuids;
SpawnGuidCache gameObjectSpawnGuids;
uint32 creatureCacheLastPruneMs = 0;
uint32 gameObjectCacheLastPruneMs = 0;

uint64 CachedGuid(SpawnGuidCache& cache, std::pair<uint32, uint32> const& key)
{
    std::lock_guard<std::mutex> lock(spawnGuidCacheLock);
    auto it = cache.find(key);
    if (it == cache.end())
        return 0;

    it->second.lastTouchMs = getMSTime();
    return it->second.guid;
}

void StoreGuid(SpawnGuidCache& cache, std::pair<uint32, uint32> const& key, uint64 guid)
{
    std::lock_guard<std::mutex> lock(spawnGuidCacheLock);
    uint32 const now = getMSTime();
    RaidBotHelpers::PruneIdleEntries(creatureSpawnGuids, creatureCacheLastPruneMs, now);
    RaidBotHelpers::PruneIdleEntries(gameObjectSpawnGuids, gameObjectCacheLastPruneMs, now);
    cache[key] = CachedSpawnGuid{ guid, now };
}
}

Creature* RaidBotHelpers::FindCreatureBySpawnIdCached(Map* map, uint32 spawnId)
{
    if (!map)
        return nullptr;

    std::pair<uint32, uint32> const key(map->GetInstanceId(), spawnId);
    if (uint64 guid = CachedGuid(creatureSpawnGuids, key))
        if (Creature* creature = map->GetCreature(guid))
            return creature;

    Creature* creature = BotFindCreatureBySpawnId(map, spawnId);
    if (creature)
        StoreGuid(creatureSpawnGuids, key, creature->GetGUID());

    return creature;
}

GameObject* RaidBotHelpers::FindGameObjectBySpawnIdCached(Map* map, uint32 spawnId)
{
    if (!map)
        return nullptr;

    std::pair<uint32, uint32> const key(map->GetInstanceId(), spawnId);
    if (uint64 guid = CachedGuid(gameObjectSpawnGuids, key))
        if (GameObject* go = map->GetGameObject(guid))
            return go;

    GameObject* go = FindGameObjectBySpawnId(map, spawnId);
    if (go)
        StoreGuid(gameObjectSpawnGuids, key, go->GetGUID());

    return go;
}

bool RaidBotHelpers::CastCharmSpell(Player* bot, uint32 spellId, Unit* target)
{
    Unit* charm = bot->GetCharm();
    if (!charm || !charm->IsInWorld() || !charm->IsAlive())
        return false;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo || spellInfo->IsPassive() || !charm->HasSpell(spellId))
        return false;

    charm->ClearUnitState(UNIT_STATE_FOLLOW);

    Spell* spell = new Spell(charm, spellInfo, TRIGGERED_NONE);
    if (target)
        spell->m_targets.SetUnitTarget(target);

    if (spell->CheckPetCast(target) != SpellCastResult::SPELL_CAST_OK)
    {
        spell->finish(false);
        delete spell;
        return false;
    }

    spell->prepare(&spell->m_targets);
    if (Creature* creature = charm->ToCreature())
        creature->AddCreatureSpellCooldown(spellId);

    return true;
}

namespace
{
struct HeadingSample
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float dx = 0.0f;
    float dy = 0.0f;
    float dz = 0.0f;
    bool hasPosition = false;
    bool hasHeading = false;
    uint32 lastStepMs = 0;
    uint32 lastTouchMs = 0;
};

// A step shorter than this is the same look or jitter; a longer one than the far limit is a respawn or a reset.
constexpr float HEADING_MIN_STEP = 0.5f;
constexpr float HEADING_MAX_STEP = 60.0f;
// A unit that has not moved for this long stands still, and its old heading no longer counts.
constexpr uint32 HEADING_STALE_MS = 1000;

std::mutex headingSamplesLock;
std::unordered_map<uint64, HeadingSample> headingSamples;
uint32 headingSamplesLastPruneMs = 0;
}

bool RaidBotHelpers::GetObservedHeading(Unit* unit, float& dx, float& dy, float& dz)
{
    if (!unit || !unit->IsInWorld())
        return false;

    std::lock_guard<std::mutex> lock(headingSamplesLock);
    uint32 const now = getMSTime();
    PruneIdleEntries(headingSamples, headingSamplesLastPruneMs, now);

    HeadingSample& sample = headingSamples[unit->GetGUID()];
    sample.lastTouchMs = now;

    float const x = unit->GetPositionX();
    float const y = unit->GetPositionY();
    float const z = unit->GetPositionZ();
    if (!sample.hasPosition)
    {
        sample.x = x;
        sample.y = y;
        sample.z = z;
        sample.hasPosition = true;
        sample.lastStepMs = now;
        return false;
    }

    float const mx = x - sample.x;
    float const my = y - sample.y;
    float const mz = z - sample.z;
    float const step = std::sqrt(mx * mx + my * my + mz * mz);
    if (step >= HEADING_MIN_STEP)
    {
        sample.hasHeading = step <= HEADING_MAX_STEP;
        if (sample.hasHeading)
        {
            sample.dx = mx / step;
            sample.dy = my / step;
            sample.dz = mz / step;
        }

        sample.x = x;
        sample.y = y;
        sample.z = z;
        sample.lastStepMs = now;
    }
    else if (getMSTimeDiff(sample.lastStepMs, now) > HEADING_STALE_MS)
        sample.hasHeading = false;

    if (!sample.hasHeading)
        return false;

    dx = sample.dx;
    dy = sample.dy;
    dz = sample.dz;
    return true;
}

bool RaidBotHelpers::IsSeenMovingTowards(Unit* unit, Position const& point, float minCosine)
{
    float hx = 0.0f, hy = 0.0f, hz = 0.0f;
    if (!GetObservedHeading(unit, hx, hy, hz))
        return false;

    RaidGeometry::Point3 const from{ unit->GetPositionX(), unit->GetPositionY(), unit->GetPositionZ() };
    RaidGeometry::Point3 const to{ point.GetPositionX(), point.GetPositionY(), point.GetPositionZ() };
    return RaidGeometry::HeadingCosine(from, { hx, hy, hz }, to) >= minCosine;
}

float RaidBotHelpers::MeleeRange(Unit const* attacker, Unit const* target)
{
    return MELEE_RANGE + attacker->GetMeleeReach() + target->GetMeleeReach();
}
