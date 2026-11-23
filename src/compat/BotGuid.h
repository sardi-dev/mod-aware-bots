/*
 * This file is part of mod-aware-bots-548, a playerbots module for SkyFire 5.4.8.
 * Licensed under the GNU General Public License v2; see LICENSE.
 *
 * The playerbots code was written against a core with TrinityCore's ObjectGuid class. SkyFire keeps GUIDs as uint64
 * and its own ObjectGuid is a small packet helper, so the module uses BotGuid: the same interface, built on
 * SkyFire's GUID layout (ObjectDefines.h). It converts to and from uint64 implicitly, so SkyFire APIs accept it.
 */

#ifndef MOD_AWARE_BOTS_BOTGUID_H
#define MOD_AWARE_BOTS_BOTGUID_H

#include "Define.h"
#include "ObjectDefines.h"
#include "ByteBuffer.h"
#include "Errors.h"
#include "Object.h"

#include <deque>
#include <functional>
#include <list>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

// SkyFire's HighGuid is a plain enum whose values (HIGHGUID_*) are the ones in its GUIDs; this scoped copy gives the
// module the TrinityCore names without clashing with class names such as Player or Unit.
enum class BotHighGuid : uint32
{
    None          = 0,
    Item          = HIGHGUID_ITEM,
    Container     = HIGHGUID_CONTAINER,
    Player        = HIGHGUID_PLAYER,
    GameObject    = HIGHGUID_GAMEOBJECT,
    Transport     = HIGHGUID_TRANSPORT,
    Unit          = HIGHGUID_UNIT,
    Pet           = HIGHGUID_PET,
    Vehicle       = HIGHGUID_VEHICLE,
    DynamicObject = HIGHGUID_DYNAMICOBJECT,
    Corpse        = HIGHGUID_CORPSE,
    AreaTrigger   = HIGHGUID_AREATRIGGER,
    Loot          = HIGHGUID_LOOT,
    BattleGround  = HIGHGUID_BATTLEGROUND,
    Mo_Transport  = HIGHGUID_MO_TRANSPORT,
    Group         = HIGHGUID_GROUP,
    Guild         = HIGHGUID_GUILD
};

class BotGuid
{
public:
    typedef uint32 LowType;

    static BotGuid const Empty;

    // Global GUIDs (players, items, groups) carry no entry; map-specific ones (creatures, objects) do.
    template<BotHighGuid type>
    static BotGuid Create(LowType counter) { return BotGuid(MAKE_NEW_GUID(counter, 0, uint32(type))); }

    template<BotHighGuid type>
    static BotGuid Create(uint32 entry, LowType counter) { return BotGuid(MAKE_NEW_GUID(counter, entry, uint32(type))); }

    BotGuid() : _guid(0) { }
    BotGuid(uint64 guid) : _guid(guid) { }
    BotGuid(ObjectGuid guid) : _guid(uint64(guid)) { }

    // No comparison operators on purpose: comparisons go through this conversion, so mixing BotGuid with uint64 or
    // 0 is never ambiguous.
    operator uint64() const { return _guid; }

    // Byte access for packet code that reads and writes GUIDs byte by byte, as SkyFire's ObjectGuid allows.
    uint8& operator[](uint32 index)
    {
        ASSERT(index < sizeof(uint64));
        return reinterpret_cast<uint8*>(&_guid)[index];
    }

    uint8 const& operator[](uint32 index) const
    {
        ASSERT(index < sizeof(uint64));
        return reinterpret_cast<uint8 const*>(&_guid)[index];
    }

    uint64 GetRawValue() const { return _guid; }
    void Set(uint64 guid) { _guid = guid; }
    void Clear() { _guid = 0; }

    BotHighGuid GetHigh() const { return BotHighGuid(GUID_HIPART(_guid)); }
    uint32 GetEntry() const { return IsGuidHaveEnPart(_guid) ? GUID_ENPART(_guid) : 0; }
    LowType GetCounter() const { return GUID_LOPART(_guid); }
    static LowType GetMaxCounter(BotHighGuid /*high*/) { return LowType(0xFFFFFFFF); }

    bool IsEmpty() const { return _guid == 0; }
    bool IsCreature() const { return IS_CREATURE_GUID(_guid); }
    bool IsPet() const { return IS_PET_GUID(_guid); }
    bool IsVehicle() const { return IS_VEHICLE_GUID(_guid); }
    bool IsCreatureOrPet() const { return IsCreature() || IsPet(); }
    bool IsCreatureOrVehicle() const { return IS_CRE_OR_VEH_GUID(_guid); }
    bool IsAnyTypeCreature() const { return IS_CRE_OR_VEH_OR_PET_GUID(_guid); }
    bool IsPlayer() const { return IS_PLAYER_GUID(_guid); }
    bool IsUnit() const { return IS_UNIT_GUID(_guid); }
    bool IsItem() const { return IS_ITEM_GUID(_guid); }
    bool IsGameObject() const { return IS_GAMEOBJECT_GUID(_guid); }
    bool IsDynamicObject() const { return IS_DYNAMICOBJECT_GUID(_guid); }
    bool IsCorpse() const { return IS_CORPSE_GUID(_guid); }
    bool IsAreaTrigger() const { return IS_AREATRIGGER_GUID(_guid); }
    bool IsTransport() const { return IS_TRANSPORT_GUID(_guid); }
    bool IsMOTransport() const { return IS_MO_TRANSPORT_GUID(_guid); }
    bool IsAnyTypeGameObject() const { return IsGameObject() || IsTransport() || IsMOTransport(); }
    bool IsGroup() const { return IS_GROUP_GUID(_guid); }
    bool IsGuild() const { return IS_GUILD_GUID(_guid); }

    TypeID GetTypeId() const
    {
        if (IsPlayer())
            return TypeID::TYPEID_PLAYER;
        if (IsAnyTypeCreature())
            return TypeID::TYPEID_UNIT;
        if (IsAnyTypeGameObject())
            return TypeID::TYPEID_GAMEOBJECT;
        if (IsItem())
            return TypeID::TYPEID_ITEM;
        if (IsDynamicObject())
            return TypeID::TYPEID_DYNAMICOBJECT;
        if (IsCorpse())
            return TypeID::TYPEID_CORPSE;
        if (IsAreaTrigger())
            return TypeID::TYPEID_AREATRIGGER;
        return TypeID::TYPEID_OBJECT;
    }

    char const* GetTypeName() const { return IsEmpty() ? "None" : GetLogNameForGuid(_guid); }

    std::string ToString() const
    {
        return std::string(GetTypeName()) + " Entry: " + std::to_string(GetEntry()) + " Low: " + std::to_string(GetCounter());
    }

private:
    uint64 _guid;
};

inline BotGuid const BotGuid::Empty = BotGuid();

inline ByteBuffer& operator<<(ByteBuffer& buf, BotGuid const& guid)
{
    buf << uint64(guid.GetRawValue());
    return buf;
}

inline ByteBuffer& operator>>(ByteBuffer& buf, BotGuid& guid)
{
    uint64 value;
    buf >> value;
    guid.Set(value);
    return buf;
}

namespace std
{
    template<>
    struct hash<BotGuid>
    {
        size_t operator()(BotGuid const& key) const { return std::hash<uint64>()(key.GetRawValue()); }
    };
}

// The container names the module uses. GuidSet repeats SkyFire's own typedef (Vehicle.h, Battlefield.h) word for
// word, which C++ allows, so both can be included together.
typedef std::set<uint64> GuidSet;
typedef std::list<BotGuid> GuidList;
typedef std::deque<BotGuid> GuidDeque;
typedef std::vector<BotGuid> GuidVector;
typedef std::unordered_set<BotGuid> GuidUnorderedSet;

#endif
