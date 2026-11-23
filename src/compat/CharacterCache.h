/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * TrinityCore keeps an in-memory character cache (sCharacterCache); SkyFire answers the same questions through
 * ObjectMgr, World's character name data and the characters database. This class gives the module the TrinityCore
 * interface on top of those.
 */

#ifndef MOD_AWARE_BOTS_COMPAT_CHARACTERCACHE_H
#define MOD_AWARE_BOTS_COMPAT_CHARACTERCACHE_H

#include "BotGuid.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "World.h"

#include <string>

class CharacterCache
{
public:
    static CharacterCache* instance()
    {
        static CharacterCache cache;
        return &cache;
    }

    void AddCharacterCacheEntry(BotGuid const& guid, uint32 /*accountId*/, std::string const& name, uint8 gender, uint8 race, uint8 playerClass, uint8 level)
    {
        sWorld->AddCharacterNameData(guid.GetCounter(), name, gender, race, playerClass, level, realmID);
    }

    BotGuid GetCharacterGuidByName(std::string const& name) const { return BotGuid(sObjectMgr->GetPlayerGUIDByName(name)); }
    bool GetCharacterNameByGuid(BotGuid guid, std::string& name) const { return sObjectMgr->GetPlayerNameByGUID(guid, name); }
    uint32 GetCharacterAccountIdByGuid(BotGuid guid) const { return sObjectMgr->GetPlayerAccountIdByGUID(guid); }

    uint32 GetCharacterAccountIdByName(std::string const& name) const
    {
        uint64 guid = sObjectMgr->GetPlayerGUIDByName(name);
        return guid ? sObjectMgr->GetPlayerAccountIdByGUID(guid) : 0;
    }

    uint8 GetCharacterLevelByGuid(BotGuid guid) const
    {
        CharacterNameData const* data = sWorld->GetCharacterNameData(guid.GetCounter());
        return data ? data->m_level : 0;
    }

    BotGuid::LowType GetCharacterGuildIdByGuid(BotGuid guid) const { return Player::GetGuildIdFromDB(guid); }
};

#define sCharacterCache CharacterCache::instance()

#endif
