/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * Bot accounts (BotAccounts.h). The table is small and read on every bot login, so it is cached in memory and
 * written through to the playerbots database.
 */

#include "BotAccounts.h"

#include "AccountMgr.h"
#include "Chat.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerbotAIConfig.h"
#include "PlayerbotMgr.h"
#include "Playerbots.h"
#include "PlayerbotsDatabase.h"
#include "WorldSession.h"

#include <mutex>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace
{
    std::mutex sCacheLock;
    std::unordered_map<uint32, uint32> sOwnerByBotAccount;  // bot account -> owner account
    bool sCacheLoaded = false;

    // Callers hold sCacheLock.
    void LoadCacheLocked()
    {
        if (sCacheLoaded)
            return;

        sCacheLoaded = true;
        PlayerbotsPreparedStatement* stmt = PlayerbotsDatabase.GetPreparedStatement(PLAYERBOTS_SEL_BOT_ACCOUNTS);
        if (PreparedQueryResult result = PlayerbotsDatabase.Query(stmt))
        {
            do
            {
                Field* fields = result->Fetch();
                sOwnerByBotAccount[fields[0].GetUInt32()] = fields[1].GetUInt32();
            } while (result->NextRow());
        }
    }

    void Store(uint32 botAccountId, uint32 ownerAccountId)
    {
        PlayerbotsPreparedStatement* stmt = PlayerbotsDatabase.GetPreparedStatement(PLAYERBOTS_REP_BOT_ACCOUNT);
        stmt->setUInt32(0, botAccountId);
        stmt->setUInt32(1, ownerAccountId);
        PlayerbotsDatabase.DirectExecute(stmt);

        std::lock_guard<std::mutex> guard(sCacheLock);
        LoadCacheLocked();
        sOwnerByBotAccount[botAccountId] = ownerAccountId;
    }

    void Remove(uint32 botAccountId)
    {
        PlayerbotsPreparedStatement* stmt = PlayerbotsDatabase.GetPreparedStatement(PLAYERBOTS_DEL_BOT_ACCOUNT);
        stmt->setUInt32(0, botAccountId);
        PlayerbotsDatabase.DirectExecute(stmt);

        std::lock_guard<std::mutex> guard(sCacheLock);
        LoadCacheLocked();
        sOwnerByBotAccount.erase(botAccountId);
    }

    // An account id from a name the player typed, 0 when there is none.
    uint32 AccountIdByName(std::string name)
    {
        if (name.empty() || !AccountMgr::normalizeString(name))
            return 0;
        return AccountMgr::GetId(name);
    }

    std::string AccountName(uint32 accountId)
    {
        std::string name;
        if (!AccountMgr::GetName(accountId, name))
            name = "#" + std::to_string(accountId);
        return name;
    }

    void SendStatus(ChatHandler* handler, uint32 accountId)
    {
        if (uint32 owner = BotAccounts::GetOwner(accountId))
            handler->PSendSysMessage("This account is a bot account of %s. '.playerbots account botmode off' makes it "
                "a human-only account again.", AccountName(owner).c_str());
        else
            handler->SendSysMessage("This account is a human-only account.");

        std::vector<uint32> const owned = BotAccounts::OwnedBy(accountId);
        if (owned.empty())
            return;

        std::ostringstream out;
        out << "Your bot accounts:";
        for (uint32 botAccountId : owned)
            out << " " << AccountName(botAccountId);
        handler->SendSysMessage(out.str().c_str());
    }
}

uint32 BotAccounts::GetOwner(uint32 botAccountId)
{
    std::lock_guard<std::mutex> guard(sCacheLock);
    LoadCacheLocked();
    auto const itr = sOwnerByBotAccount.find(botAccountId);
    return itr != sOwnerByBotAccount.end() ? itr->second : 0;
}

bool BotAccounts::IsOwnedBy(uint32 botAccountId, uint32 ownerAccountId)
{
    return ownerAccountId && GetOwner(botAccountId) == ownerAccountId;
}

std::vector<uint32> BotAccounts::OwnedBy(uint32 ownerAccountId)
{
    std::vector<uint32> owned;
    std::lock_guard<std::mutex> guard(sCacheLock);
    LoadCacheLocked();
    for (auto const& [botAccountId, owner] : sOwnerByBotAccount)
        if (owner == ownerAccountId)
            owned.push_back(botAccountId);
    return owned;
}

void BotAccounts::LogoutBotsOfAccount(uint32 accountId)
{
    // Collect under the player container's lock; logging out changes that container.
    std::vector<std::pair<uint64, uint64>> bots;  // bot GUID, master GUID
    {
        SF_SHARED_GUARD lock(*HashMapHolder<Player>::GetLock());
        for (auto const& [guid, player] : ObjectAccessor::GetPlayers())
        {
            WorldSession* session = player ? player->GetSession() : nullptr;
            if (!session || !session->IsBot() || session->GetAccountId() != accountId)
                continue;

            PlayerbotAI* botAI = GET_PLAYERBOT_AI(player);
            Player* master = botAI ? botAI->GetMaster() : nullptr;
            if (master && master != player)
                bots.emplace_back(player->GetGUID(), master->GetGUID());
        }
    }

    for (auto const& [botGuid, masterGuid] : bots)
        if (Player* master = ObjectAccessor::FindPlayer(masterGuid))
            if (PlayerbotMgr* mgr = GET_PLAYERBOT_MGR(master))
                mgr->LogoutPlayerBot(BotGuid(botGuid));
}

bool BotAccounts::HandleBotModeCommand(ChatHandler* handler, std::string const& args)
{
    WorldSession* session = handler->GetSession();
    if (!session)
    {
        handler->SendSysMessage("botmode is used from the account itself, in game.");
        return true;
    }

    if (!sPlayerbotAIConfig->allowBotAccounts)
    {
        handler->SendSysMessage("Bot accounts are turned off on this server (AiPlayerbot.AllowBotAccounts).");
        return true;
    }

    uint32 const self = session->GetAccountId();
    std::istringstream in(args);
    std::string mode, name;
    in >> mode >> name;

    if (mode.empty())
    {
        SendStatus(handler, self);
        return true;
    }

    if (mode == "on")
    {
        uint32 const owner = AccountIdByName(name);
        if (!owner)
        {
            handler->SendSysMessage("Usage: .playerbots account botmode on <your main account name>");
            return true;
        }
        if (owner == self)
        {
            handler->SendSysMessage("An account cannot be its own bot account.");
            return true;
        }
        if (sPlayerbotAIConfig->IsInRandomAccountList(self) || sPlayerbotAIConfig->IsInRandomAccountList(owner))
        {
            handler->SendSysMessage("Random bot accounts cannot be bot accounts or own them.");
            return true;
        }
        if (GetOwner(owner))
        {
            handler->SendSysMessage("That account is itself a bot account; a bot account cannot own others.");
            return true;
        }
        if (!OwnedBy(self).empty())
        {
            handler->SendSysMessage("This account owns bot accounts, so it cannot become one.");
            return true;
        }

        // A change of owner: the old owner's bots of this account go offline first.
        if (uint32 previous = GetOwner(self); previous && previous != owner)
            LogoutBotsOfAccount(self);

        Store(self, owner);
        handler->PSendSysMessage("This account is now a bot account of %s: its characters can be used as that "
            "account's bots. '.playerbots account botmode off' makes it human-only again.", AccountName(owner).c_str());
        return true;
    }

    if (mode == "off")
    {
        uint32 botAccountId = self;
        if (!name.empty())
        {
            botAccountId = AccountIdByName(name);
            if (!botAccountId || !IsOwnedBy(botAccountId, self))
            {
                handler->SendSysMessage("That is not one of your bot accounts.");
                return true;
            }
        }
        else if (!GetOwner(self))
        {
            handler->SendSysMessage("This account is not a bot account. From your main account, name the bot "
                "account: .playerbots account botmode off <bot account name>");
            return true;
        }

        LogoutBotsOfAccount(botAccountId);
        Remove(botAccountId);
        handler->PSendSysMessage("%s is a human-only account again.", AccountName(botAccountId).c_str());
        return true;
    }

    handler->SendSysMessage("Usage: .playerbots account botmode [on <main account> | off [<bot account>]]");
    return true;
}
