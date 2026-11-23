/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * World queries Legends of Azeroth's core offers and SkyFire does not: per-unit combat references (CombatManager),
 * creatures and game objects by spawn id, and the transport under a position. Built on SkyFire's attacker set,
 * threat lists and hostile references, and on the read-only spawn and transport accessors added to the core
 * (ObjectMgr::GetAllCreatureData, GetAllGameObjectData, Map::GetTransports; core patches in patches/).
 */

#ifndef MOD_AWARE_BOTS_COMPAT_BOTWORLD_H
#define MOD_AWARE_BOTS_COMPAT_BOTWORLD_H

#include "CellImpl.h"
#include "Creature.h"
#include "GridNotifiers.h"
#include "GameObject.h"
#include "GameObjectModel.h"
#include "HostileRefManager.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "ThreatManager.h"
#include "Transport.h"
#include "Unit.h"

#include <map>
#include <unordered_map>

// One combat pairing, like TrinityCore's CombatReference. Used through a pointer-like -> as the LoA code does.
struct BotCombatRef
{
    Unit* first = nullptr;
    Unit* second = nullptr;

    BotCombatRef const* operator->() const { return this; }
    Unit* GetOther(Unit const* who) const { return who == first ? second : first; }

    // PORT: SkyFire has no per-pair combat state; ending a pairing drops the creature side's threat on the other.
    void EndCombat() const
    {
        if (Creature* creature = second ? second->ToCreature() : nullptr)
            creature->getThreatManager().modifyThreatPercent(first, -100);
        if (Creature* creature = first ? first->ToCreature() : nullptr)
            creature->getThreatManager().modifyThreatPercent(second, -100);
    }
};

// TrinityCore's per-unit CombatManager, rebuilt from what SkyFire tracks: who attacks the unit, its victim, the
// creatures that have it on their threat list, and (for a creature) its own threat list. Player-controlled others go
// to the PvP list, the rest to the PvE list.
class BotCombatManager
{
public:
    explicit BotCombatManager(Unit const* unit) : _unit(const_cast<Unit*>(unit))
    {
        if (!_unit)
            return;

        for (Unit* attacker : _unit->getAttackers())
            Add(attacker);

        if (Unit* victim = _unit->GetVictim())
            Add(victim);

        for (HostileReference* ref = _unit->getHostileRefManager().getFirst(); ref; ref = ref->next())
            Add(ref->GetSource()->GetOwner());

        if (Creature* creature = _unit->ToCreature())
            for (HostileReference* ref : creature->getThreatManager().getThreatList())
                Add(ref->getTarget());
    }

    // By value: callers often hold the result of a temporary BotCombatManager.
    std::map<uint64, BotCombatRef> GetPvPCombatRefs() const { return _pvp; }
    std::map<uint64, BotCombatRef> GetPvECombatRefs() const { return _pve; }
    bool HasCombat() const { return !_pvp.empty() || !_pve.empty(); }

    bool IsInCombatWith(Unit const* who) const
    {
        return who && (_pvp.count(who->GetGUID()) || _pve.count(who->GetGUID()));
    }

private:
    void Add(Unit* other)
    {
        if (!other || other == _unit || !other->IsInWorld())
            return;
        BotCombatRef ref{ _unit, other };
        if (other->GetCharmerOrOwnerPlayerOrPlayerItself())
            _pvp[other->GetGUID()] = ref;
        else
            _pve[other->GetGUID()] = ref;
    }

    Unit* _unit;
    std::map<uint64, BotCombatRef> _pvp;
    std::map<uint64, BotCombatRef> _pve;
};

// Grid visitor that finds the creature with a given spawn id (Map::VisitAll calls Visit per object type).
struct BotSpawnIdVisitor
{
    uint32 spawnId;
    Creature* found = nullptr;

    void Visit(CreatureMapType& creatures)
    {
        for (CreatureMapType::iterator itr = creatures.begin(); itr != creatures.end() && !found; ++itr)
            if (itr->GetSource()->GetDBTableGUIDLow() == spawnId)
                found = itr->GetSource();
    }

    template<class SKIP> void Visit(GridRefManager<SKIP>&) { }
};

// A loaded creature by its spawn id (the creature table guid). In the open world SkyFire gives DB spawns their spawn
// id as the low GUID; inside instances it generates one, so there the creature is found near its spawn point.
inline Creature* BotFindCreatureBySpawnId(Map* map, uint32 spawnId)
{
    CreatureData const* data = sObjectMgr->GetCreatureData(spawnId);
    if (!map || !data || data->mapid != map->GetId())
        return nullptr;

    if (Creature* creature = map->GetCreature(MAKE_NEW_GUID(spawnId, data->id, HIGHGUID_UNIT)))
        return creature;

    if (!map->Instanceable())
        return nullptr;

    // Instances: search the grid cells around the spawn point for the creature with that spawn id.
    BotSpawnIdVisitor visitor{ spawnId };
    map->VisitAll(data->posX, data->posY, 150.0f, visitor);
    return visitor.found;
}

// A spawn store built on demand. LoA's map keeps its store alive; this one is a value, so iterators into a temporary
// would dangle: equal_range and find exist only on a named store, while a range-for over a temporary is fine.
template<class T>
class BotSpawnStore
{
public:
    using Container = std::unordered_multimap<uint32, T*>;

    void emplace(uint32 spawnId, T* object) { _store.emplace(spawnId, object); }
    typename Container::const_iterator begin() const { return _store.begin(); }
    typename Container::const_iterator end() const { return _store.end(); }
    bool empty() const { return _store.empty(); }
    std::size_t size() const { return _store.size(); }

    auto equal_range(uint32 spawnId) const& { return _store.equal_range(spawnId); }
    auto equal_range(uint32 spawnId) const&& = delete;
    auto find(uint32 spawnId) const& { return _store.find(spawnId); }
    auto find(uint32 spawnId) const&& = delete;

private:
    Container _store;
};

// LoA's Map::GetCreatureBySpawnIdStore: the loaded creatures of this map keyed by spawn id. Built on demand (debug
// harness and rare lookups only), from the spawn table. For one spawn, use BotFindCreatureBySpawnId.
inline BotSpawnStore<Creature> BotCreatureBySpawnIdStore(Map* map)
{
    BotSpawnStore<Creature> store;
    if (!map)
        return store;
    for (auto const& [spawnId, data] : sObjectMgr->GetAllCreatureData())
        if (data.mapid == map->GetId())
            if (Creature* creature = BotFindCreatureBySpawnId(map, spawnId))
                store.emplace(spawnId, creature);
    return store;
}

// LoA's Map::GetGameObjectBySpawnIdStore, the same way for game objects.
inline BotSpawnStore<GameObject> BotGameObjectBySpawnIdStore(Map* map)
{
    BotSpawnStore<GameObject> store;
    if (!map)
        return store;
    for (auto const& [spawnId, data] : sObjectMgr->GetAllGameObjectData())
        if (data.mapid == map->GetId())
            if (GameObject* go = map->GetGameObject(MAKE_NEW_GUID(spawnId, data.id, HIGHGUID_GAMEOBJECT)))
                store.emplace(spawnId, go);
    return store;
}

// LoA's Map::GetTransportForPos (Map.cpp): a moving transport whose model is under the position, else a static
// (type 11) transport nearby whose deck is above the ground there.
inline Transport* BotGetTransportForPos(Map* map, uint32 phaseMask, float x, float y, float z, WorldObject* worldObject)
{
    if (!map)
        return nullptr;

    G3D::Vector3 const v(x, y, z + 2.0f);
    G3D::Ray const ray(v, G3D::Vector3(0.0f, 0.0f, -1.0f));

    for (Transport* transport : map->GetTransports())
    {
        if (transport->IsInWorld() && transport->GetExactDist(x, y, z) < 75.0f && transport->m_model)
        {
            float dist = 30.0f;
            if (transport->m_model->intersectRay(ray, dist, false, phaseMask))
                return transport;
        }
    }

    if (worldObject)
        if (GameObject* staticTransport = worldObject->FindNearestGameObjectOfType(GAMEOBJECT_TYPE_TRANSPORT, 75.0f))
            if (staticTransport->m_model)
            {
                float dist = 10.0f;
                if (staticTransport->m_model->intersectRay(ray, dist, false, phaseMask))
                    if (map->GetHeight(phaseMask, x, y, z, true, 30.0f) < (v.z - dist + 1.0f))
                        return staticTransport->ToTransport();
            }

    return nullptr;
}

#endif
