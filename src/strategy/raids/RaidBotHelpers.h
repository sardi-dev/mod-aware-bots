/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// The PlayerbotAI and core helpers the raid strategies of AC's mod-playerbots call that this module's PlayerbotAI or
// SkyFire lack. Role indexes are AC Bot/PlayerbotAI.cpp IsAssistTankOfIndex, IsAssistHealOfIndex,
// IsAssistRangedDpsOfIndex and GetAssistTankIndex with PlayerBotSpec for the role checks: this module's
// PlayerBotSpec::IsAssistTankOfIndex has an older shape (no living-only index) that other code relies on.

#ifndef PLAYERBOTS_RAIDBOTHELPERS_H
#define PLAYERBOTS_RAIDBOTHELPERS_H

#include "BotGuid.h"
#include "Common.h"
#include "StaggerClock.h"
#include "Timer.h"
#include <functional>
#include <mutex>
#include <unordered_map>

class Creature;
class GameObject;
class Group;
class Item;
class Map;
class Player;
class Unit;
struct Position;

namespace RaidBotHelpers
{
    // AC RtiTargetValue's raid icon indexes (star to skull, the order of RtiTargetValue.cpp); this module's
    // RtiTargetValue has GetRtiIndex only.
    inline constexpr int8 starIndex = 0;
    inline constexpr int8 circleIndex = 1;
    inline constexpr int8 diamondIndex = 2;
    inline constexpr int8 triangleIndex = 3;
    inline constexpr int8 moonIndex = 4;
    inline constexpr int8 squareIndex = 5;
    inline constexpr int8 crossIndex = 6;
    inline constexpr int8 skullIndex = 7;

    // AC GetMainTankGuid: the group's marked main tank, else its first living tank.
    BotGuid GetMainTankGuid(Group* group);

    // With indexLivingOnly, dead members leave the index and the living ones move up; without it, roles stay stable
    // across deaths (AC).
    bool IsAssistTankOfIndex(Player* player, uint8 index, bool indexLivingOnly = false);
    bool IsAssistHealOfIndex(Player* player, uint8 index, bool indexLivingOnly = false);
    bool IsAssistRangedDpsOfIndex(Player* player, uint8 index, bool indexLivingOnly = false);
    int32 GetAssistTankIndex(Player* player);

    // AC PlayerbotAI::HasItemInInventory (bags only, as AC).
    bool HasItemInInventory(Player* bot, uint32 itemId);

    // AC PlayerbotAI::ImbueItem on a unit or a game object: what WorldSession::HandleUseItemOpcode does after it parsed
    // the packet (CMSG_USE_ITEM is bit-packed on 5.4.8, see UseItemAction::UseItem).
    bool UseItemOnUnit(Player* bot, Item* item, Unit* target);
    bool UseItemOnGameObject(Player* bot, Item* item, GameObject* target);

    // AC Map::GetGameObjectBySpawnIdStore for one spawn. SkyFire gives instance spawns a generated GUID, so the
    // object is found near its spawn point by its table guid (BotFindCreatureBySpawnId does the same for creatures).
    GameObject* FindGameObjectBySpawnId(Map* map, uint32 spawnId);

    // The same lookups for strategies that ask every tick: the grid search runs once per instance and spawn, then the
    // GUID it found is looked up directly.
    Creature* FindCreatureBySpawnIdCached(Map* map, uint32 spawnId);
    GameObject* FindGameObjectBySpawnIdCached(Map* map, uint32 spawnId);

    // What WorldSession::HandlePetCastSpellOpcode does for the bar of a player's charmed or possessed creature: the
    // spell must be one of the creature's own, and the cast runs every normal check (cooldown, global cooldown, power,
    // range, facing) and starts the creature's cooldown. Returns true when the cast started.
    bool CastCharmSpell(Player* bot, uint32 spellId, Unit* target);

    // The direction a unit is moving in, as a player sees it: read from how its position changes between two looks
    // (any bot of the instance may look). False while it stands still or before it moved far enough to tell.
    bool GetObservedHeading(Unit* unit, float& dx, float& dy, float& dz);

    // True while the unit is seen moving towards the point, its heading within acos(minCosine) of the direction to it.
    bool IsSeenMovingTowards(Unit* unit, Position const& point, float minCosine);

    // AC Unit::GetMeleeRange: the distance IsWithinMeleeRange tests (MELEE_RANGE plus both melee reaches).
    float MeleeRange(Unit const* attacker, Unit const* target);

    // How long a per-instance or per-GUID entry may sit untouched before it is erased: far longer than any pause in
    // a fight, and every bot that reads an entry touches it, so no bot can still hold a reference this old.
    inline constexpr uint32 IDLE_ENTRY_PRUNE_MS = 30 * MINUTE * IN_MILLISECONDS;
    inline constexpr uint32 PRUNE_INTERVAL_MS = MINUTE * IN_MILLISECONDS;

    // Erases the entries whose value has a lastTouchMs older than idleMs, at most once per interval, so caches keyed
    // by instance or GUID do not grow with every instance the server ever ran. The caller holds the map's lock.
    template <typename MapType>
    void PruneIdleEntries(MapType& entries, uint32& lastPruneMs, uint32 now, uint32 idleMs = IDLE_ENTRY_PRUNE_MS)
    {
        if (getMSTimeDiff(lastPruneMs, now) < PRUNE_INTERVAL_MS)
            return;

        lastPruneMs = now;
        for (auto it = entries.begin(); it != entries.end();)
        {
            if (getMSTimeDiff(it->second.lastTouchMs, now) > idleMs)
                it = entries.erase(it);
            else
                ++it;
        }
    }

    // The encounter state the bots of one instance share. A reset clears the state in place instead of erasing it,
    // so a reference a bot took earlier in its update stays valid; instances nobody reads any more are pruned.
    template <typename State>
    class InstanceStateStore
    {
    public:
        State& Get(uint32 instanceId)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            uint32 const now = getMSTime();
            PruneIdleEntries(_entries, _lastPruneMs, now);
            Entry& entry = _entries[instanceId];
            entry.lastTouchMs = now;
            return entry.state;
        }

        // Clears an instance's state; returns what wasSet says about the state before the reset (false without one).
        template <typename Predicate>
        bool Reset(uint32 instanceId, Predicate wasSet)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            auto it = _entries.find(instanceId);
            if (it == _entries.end())
                return false;

            bool const result = wasSet(it->second.state);
            it->second.state = State{};
            return result;
        }

        void Reset(uint32 instanceId)
        {
            Reset(instanceId, [](State const&) { return true; });
        }

    private:
        struct Entry
        {
            State state;
            uint32 lastTouchMs = 0;
        };

        std::mutex _mutex;
        std::unordered_map<uint32, Entry> _entries;
        uint32 _lastPruneMs = 0;
    };

    // Delays one step of an action without a delayed callback (a callback holding the action could outlive it): the
    // first call arms a random delay for a key (the target's GUID), and Ready turns true once it passed (StaggerClock).
    class StaggerTimer
    {
    public:
        bool Ready(uint64 key, uint32 minDelayMs, uint32 maxDelayMs);
        uint32 RemainingMs() const;

    private:
        StaggerClock _clock;
    };
}

#endif
