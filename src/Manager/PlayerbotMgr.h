/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#ifndef _PLAYERBOT_PLAYERBOTMGR_H
#define _PLAYERBOT_PLAYERBOTMGR_H

#include "Common.h"
#include "Player.h"
#include "PlayerbotAIBase.h"
#include "QueryHolder.h"
#include "QueryResult.h"

#include <shared_mutex>
#include <unordered_set>
#include <vector>

class ChatHandler;
class PlayerbotAI;
class PlayerbotLoginQueryHolder;
class WorldPacket;
class BotGuid;

typedef std::map<uint64, Player*> PlayerBotMap;
typedef std::map<std::string, std::set<std::string> > PlayerBotErrorMap;

class PlayerbotHolder : public PlayerbotAIBase
{
public:
    PlayerbotHolder();
    virtual ~PlayerbotHolder() {};

    void AddPlayerBot(BotGuid guid, uint32 masterAccountId);
    // Runs once SkyFire's async login (WorldSession::BeginBotCharacterLogin) has put the bot in the world.
    void HandlePlayerBotLoginCallback(WorldSession* botSession, uint32 masterAccountId, BotGuid botGuid);

    void LogoutPlayerBot(BotGuid guid);
    void DisablePlayerBot(BotGuid guid);
    Player* GetPlayerBot(BotGuid guid) const;
    Player* GetPlayerBot(BotGuid::LowType lowGuid) const;
    PlayerBotMap::const_iterator GetPlayerBotsBegin() const { return playerBots.begin(); }
    PlayerBotMap::const_iterator GetPlayerBotsEnd() const { return playerBots.end(); }

    /// Thread-safe copy of the bot list, for callers on a map-update thread.
    ///
    /// playerBots is a std::map mutated on the world/session thread as bots log in and out.
    /// Walking it directly with the Begin()/End() iterators from a map-update worker - which is
    /// what the outgoing-packet hook used to do - races with those inserts, and a tree rebalance
    /// under an active traversal can leave ++it never reaching end(): the worker then spins
    /// forever and the world thread blocks in MapUpdater::wait(), hanging the whole server.
    std::vector<Player*> GetPlayerBotsSnapshot() const;

    void UpdateAIInternal([[maybe_unused]] uint32 elapsed, [[maybe_unused]] bool minimal = false) override {};
    void UpdateSessions();
    void HandleBotPackets(WorldSession* session);
    // Polls the bot logins started by AddPlayerBot; called from UpdateSessions.
    void PollBotLogins();
    // A holder being deleted hands its logins still loading to the shared list below (BT-010).
    void AbandonPendingLogins();
    // Frees the bot session of each abandoned login once its character query is done; called every world tick.
    static void PollAbandonedLogins();

    void LogoutAllBots();
    void OnBotLogin(Player* const bot);

    std::vector<std::string> HandlePlayerbotCommand(char const* args, Player* master = nullptr);
    std::string const ProcessBotCommand(std::string const cmd, BotGuid guid, BotGuid masterguid, bool admin,
        uint32 masterAccountId, uint32 masterGuildId);
    uint32 GetAccountId(std::string const name);
    uint32 GetAccountId(BotGuid guid);
    std::string const ListBots(Player* master);
    std::string const LookupBots(Player* master);
    uint32 GetPlayerbotsCount() { return playerBots.size(); }
    uint32 GetPlayerbotsCountByClass(uint32 cls);

protected:
    virtual void OnBotLoginInternal(Player* const bot) = 0;

    PlayerBotMap playerBots;
    std::unordered_set<BotGuid> botLoading;

    // Bot sessions whose character is still loading (SkyFire's async bot login), with the master that asked.
    struct PendingBotLogin
    {
        WorldSession* session;
        uint32 masterAccountId;
    };
    std::unordered_map<BotGuid, PendingBotLogin> botPendingLogins;
    // Sessions of logins whose holder was deleted (the master logged out); only the world thread touches it.
    static std::vector<WorldSession*> abandonedLogins;

    /// Guards playerBots. Held exclusively for insert/erase, shared for the snapshot above.
    mutable std::shared_mutex playerBotsMutex;
};

class PlayerbotMgr : public PlayerbotHolder
{
public:
    PlayerbotMgr(Player* const master);
    virtual ~PlayerbotMgr();

    static bool HandlePlayerbotMgrCommand(ChatHandler* handler, char const* args);
    void HandleMasterIncomingPacket(WorldPacket const& packet);
    void HandleMasterOutgoingPacket(WorldPacket const& packet);
    void HandleCommand(uint32 type, std::string const text);
    void OnPlayerLogin(Player* player);
    void CancelLogout();

    void UpdateAIInternal(uint32 elapsed, bool minimal = false) override;
    void TellError(std::string const botName, std::string const text);

    Player* GetMaster() const { return master; };

    void SaveToDB();

protected:
    void OnBotLoginInternal(Player* const bot) override;
    void CheckTellErrors(uint32 elapsed);
    // BT-256 (ActivityWakePolicy.h): wake the sleeping bots inside the force radius around this real player.
    void WakeNearbyBots();

private:
    Player* const master;
    PlayerBotErrorMap errors;
    time_t lastErrorTell;
    uint32 lastWakeScanMs = 0;
};

class PlayerbotsMgr
{
public:
    PlayerbotsMgr() {}
    ~PlayerbotsMgr() {}

    static PlayerbotsMgr* instance()
    {
        static PlayerbotsMgr instance;
        return &instance;
    }

    void AddPlayerbotData(Player* player, bool isBotAI);
    void RemovePlayerBotData(BotGuid const& guid, bool is_AI);

    PlayerbotAI* GetPlayerbotAI(Player* player);
    PlayerbotMgr* GetPlayerbotMgr(Player* player);

private:
    std::unordered_map<BotGuid, PlayerbotAIBase*> _playerbotsAIMap;
    std::unordered_map<BotGuid, PlayerbotAIBase*> _playerbotsMgrMap;
};

#define sPlayerbotsMgr PlayerbotsMgr::instance()

#endif
