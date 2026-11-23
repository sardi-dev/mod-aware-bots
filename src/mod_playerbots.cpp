/*
** Made by Traesh https://github.com/Traesh
** AzerothCore 2019 http://www.azerothcore.org/
** Conan513 https://github.com/conan513
** Made into a module by Micrah https://github.com/milestorme/
*/

#include "ArenaLadder.h"
#include "ArenaNpcSpawn.h"
#include "BotAccounts.h"
#include "BotAuction.h"
#include "BotLfm.h"
#include "DungeonChat.h"
#include "BgCalloutReader.h"
#include "AiChat.h"
#include "GatherNodeMgr.h"
#include "Chat.h"
#include "ChannelMgr.h"
#include "Config.h"
#include "cs_playerbots.h"
#include "Group.h"
#include "LikingStore.h"
#include "Log.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSocket.h"

#include "Playerbots.h"
#include "PlayerbotAIConfig.h"
#include "NewRpgDuelSpot.h"  // #201 (#614): sDuelSpotMgr for the duel reset
#include "RandomPlayerbotMgr.h"
#include "RandomItemManager.h"
#include "PlayerbotTextMgr.h"
#include "PriceTable.h"
#include "PlayerbotWorldThreadProcessor.h"
#include "TravelNode.h"

#include "Authentication/AuthCrypt.h"
#include "CharacterHandler.h"

#include <filesystem>
#include <vector>
#include <future>
#include <unordered_set>
#include <random>
#include <thread>

#ifndef _PLAYERBOT_CONFIG
# define _PLAYERBOT_CONFIG  "playerbots.conf"
#endif

namespace QuestHubSearch { void OnGameEvent(); }

// BT-154: the ladder NPC script (Arena/ArenaNpc.cpp).
void AddSC_npc_bot_arena_ladder();
// BT-202: records when a bot last took damage (strategy/generic/BotDamageWatch.cpp).
void AddSC_bot_damage_watch();

// #72: PlayerbotAI.cpp - the world-diff table the smart activity scale reads (AC core sWorldUpdateTime; BT-370 mean).
void PlayerbotRecordWorldUpdateDiff(uint32 diff);

// #148: AC Bot/RandomBotLevelMgr.cpp ported into RandomPlayerbotMgr.cpp (file-local class, these are its hooks).
void RandomBotLevelMgrUpdate(uint32 diff);
void RandomBotLevelMgrOnLevelChanged(Player* player, uint8 oldLevel);
void RandomBotLevelMgrReloadConfig();

class mod_playerbots : public PlayerScript
{
public:
    mod_playerbots() : PlayerScript("mod_playerbots") {}

    void OnLogin(Player* player, bool /*firstLogin*/) override
    {
        // Announce Module
        ChatHandler(player->GetSession()).SendSysMessage("This server is running the |cff4CFF00mod playerbots |rmodule.");
    }

};

class PlayerbotsWorldScript : public WorldScript
{
public:
    PlayerbotsWorldScript() : WorldScript("PlayerbotsWorldScript") {}

    // BT-152: invalidate the quest-giver event snapshot after the core changes an event.
    void OnGameEvent(bool /*start*/, uint16 /*eventId*/) override { QuestHubSearch::OnGameEvent(); }

    void OnConfigLoad(bool reloaded) override
    {
        if (!reloaded)
        {
            uint32 oldMSTime = getMSTime();
            // Next to the main configuration file, as LoA installs it (playerbots.conf beside worldserver.conf).
            std::filesystem::path const mainConf = std::filesystem::absolute(sConfigMgr->GetFilename());
            std::string conf_file = (mainConf.parent_path() / _PLAYERBOT_CONFIG).generic_string();

            SF_LOG_INFO("playerbots", " ");
            SF_LOG_INFO("playerbots", "Loading Playerbots Config at %s ...", conf_file.c_str());

            if (!sConfigMgr->LoadMore(conf_file.c_str()))
            {
                SF_LOG_FATAL("playerbots", ">> Load playerbots failed: cannot read %s", conf_file.c_str());
                std::this_thread::sleep_for(std::chrono::seconds(5));
                sWorld->StopNow(1);
                return;
            }

            // SkyFire's core has no playerbots database; the module opens its own (database/PlayerbotsDatabase.h)
            // before the configuration loads, which already reads from it.
            if (!OpenPlayerbotsDatabase())
            {
                sWorld->StopNow(1);
                return;
            }

            sPlayerbotAIConfig->Initialize();

            // AiPlayerbot.Enabled = 0 is the server's switch to run without bots: stop before the caches are built,
            // so a fault in them cannot keep the server from starting.
            if (!sPlayerbotAIConfig->enabled)
            {
                SF_LOG_INFO("server.loading", ">> Playerbots disabled (AiPlayerbot.Enabled = 0)");
                return;
            }

            SF_LOG_INFO("playerbots", ">> Loaded playerbots config in %u ms", GetMSTimeDiffToNow(oldMSTime));
            SF_LOG_INFO("playerbots", " ");

            sRandomPlayerbotMgr->Reserve(sPlayerbotAIConfig->maxRandomBots);
            sRandomItemMgr->Init();

            SF_LOG_INFO("playerbots", "Playerbots enabled: %s", sPlayerbotAIConfig->enabled ? "Yes" : "No");
            SF_LOG_INFO("playerbots", "Playerbots min/max to load: %u/%u", sPlayerbotAIConfig->minRandomBots, sPlayerbotAIConfig->maxRandomBots);
            SF_LOG_INFO("playerbots", "Playerbots autologin: %s", sPlayerbotAIConfig->randomBotAutologin ? "Yes" : "No");
        }
    }
    void OnStartup() override
    {
        if (!sPlayerbotAIConfig->enabled)
            return;

        // Runs after World::SetInitialWorldSettings, so DBCs, ObjectMgr and MapMgr are all up.
        //
        // sTravelMgr.Init() is the counterpart of upstream AC's TravelMgr::Init(): it builds the
        // flight-master / travel-hub caches and the taxi graph the "travel flight" RPG action
        // needs. It is DBC + spawn data only, so it costs seconds, and it always runs.
        // #322: AC PlayerbotAIConfig.cpp:789-790 loads the bot text tables (ai_playerbot_texts /
        // ai_playerbot_texts_chance) in Initialize(), before PlayerbotFactory::Init and TravelMgr::Init.
        PlayerbotTextMgr::instance().LoadBotTexts();
        PlayerbotTextMgr::instance().LoadBotTextChance();

        sTravelMgr.Init();

        // BT-154: the arena ladder creates its tables if they are missing and loads the books (the module has no SQL updater).
        ArenaLadder::OnStartup();
        // BT-154: the ladder NPC is spawned by the module at run time (creature template from data/sql/db-world, no creature rows).
        ArenaNpcSpawn::OnStartup();

        // The index of herb and ore spawns the "do gather" rpg status walks to (DB and DBC data only).
        sGatherNodeMgr.Load();

        // Design decision 0034: the auction price ranges, from the world database; does nothing while AiPlayerbot.Auction.Enable is 0.
        PriceTable::Load();
        BotAuction::OnStartup();

        // #201 (#612): DBCs are loaded now; drop duel spots on maps that do not exist (Initialize() ran before them).
        sPlayerbotAIConfig->ValidateDuelSpotMaps();

        // The walk-node graph (TravelNode) is the other half of upstream's travel layer. Upstream
        // builds it at startup but nothing consumes it at runtime any more - its graph-routing
        // block inside MovementActions::MoveTo is commented out, and only debug commands read it.
        // It is therefore off by default here too; enabling it costs a long one-time generation.
        SF_LOG_INFO("playerbots", "Travel nodes: enableTravelNodes=%d", sPlayerbotAIConfig->enableTravelNodes ? 1 : 0);
        if (sPlayerbotAIConfig->enableTravelNodes)
            sTravelNodeMap.LoadOrGenerate();
    }

    // World thread only.
    bool startupDiffSeen = false;

    void OnUpdate(uint32 diff) override
    {
        // AC Script/Playerbots.cpp:466-468: run the operations bot AI queued from the map-update threads
        // (world thread, after sMapMgr->Update).
        PlayerbotWorldThreadProcessor::instance().Update(diff);
        // #148: AC RandomBotLevelMgr.cpp:1126-1129 (replaces the fork-only RandomPlayerbotBracketMgr, broken since #808).
        RandomBotLevelMgrUpdate(diff);

        // LoA ran these from its core's PlayerbotScript hooks; SkyFire has none, so the world tick runs them.
        // The first diff of the world thread reaches back to before OnStartup (WorldRunnable::Run takes its start time
        // first, then runs OnStartup, whose travel caches take seconds), so it is the startup time and not a tick. Recorded,
        // it makes the smart activity scale (AC AutoScaleActivity) see an overloaded world for the first hundred ticks.
        if (startupDiffSeen)
            PlayerbotRecordWorldUpdateDiff(diff);  // AC World::Update -> sWorldUpdateTime.UpdateWithDiff(diff)
        startupDiffSeen = true;
        sRandomPlayerbotMgr->UpdateAI(diff);
        sRandomPlayerbotMgr->UpdateSessions();
        // BT-154: the arena ladder's tick. The world scripts run after the map threads have finished their update, so the
        // ladder may touch bots and battlegrounds here (Arena/ArenaLadder.h).
        ArenaLadder::OnUpdate(diff);
        ArenaNpcSpawn::OnUpdate(diff);
        // Design decision 0033: the bots' auction visits work here for the same reason (strategy/actions/BotAuction.h).
        BotAuction::OnUpdate(diff);
        PlayerbotHolder::PollAbandonedLogins();

        // LoA updated each master's bot sessions from the master's WorldSession::Update; here the world tick does it
        // for every master (bot sessions are owned by the module, not by World's session map).
        for (auto const& [accountId, session] : sWorld->GetAllSessions())
            if (Player* player = session ? session->GetPlayer() : nullptr)
                if (PlayerbotMgr* playerbotMgr = GET_PLAYERBOT_MGR(player))
                    playerbotMgr->UpdateSessions();
    }

    void OnShutdown() override
    {
        sRandomPlayerbotMgr->LogoutAllBots();
        BotAuction::LogSummary("shutdown");
        LikingStore::Shutdown();
    }
};

class PlayerbotsServerScript : public ServerScript
{
public:
    PlayerbotsServerScript() : ServerScript("PlayerbotsServerScript") {}

    // A master's own packets, so its bots can follow what the master does.
    void OnSessionPacketReceive(WorldSession* session, WorldPacket const& packet) override
    {
        // A player opening the character list of an account takes its characters back from the bots first, so a
        // character is never in the world twice (design decision 0018). The list comes well before the login,
        // so the bots' logout saves finish first.
        if (packet.GetOpcode() == CMSG_ENUM_CHARACTERS && !session->IsBot())
            BotAccounts::LogoutBotsOfAccount(session->GetAccountId());

        Player* player = session->GetPlayer();
        if (!player)
            return;

        try
        {
            if (PlayerbotMgr* playerbotMgr = GET_PLAYERBOT_MGR(player))
                playerbotMgr->HandleMasterIncomingPacket(packet);
        }
        catch (ByteBufferException const& e)
        {
            LogUnreadablePacket(player, packet, e);
        }
    }

    // Packets the server sends: a bot's AI reads its own (bots have no client), a master's manager reads the master's.
    void OnSessionPacketSend(WorldSession* session, WorldPacket const& packet) override
    {
        Player* player = session->GetPlayer();
        if (!player)
            return;

        try
        {
            if (PlayerbotAI* botAI = GET_PLAYERBOT_AI(player))
                botAI->HandleBotOutgoingPacket(packet);

            if (PlayerbotMgr* playerbotMgr = GET_PLAYERBOT_MGR(player))
                playerbotMgr->HandleMasterOutgoingPacket(packet);
        }
        catch (ByteBufferException const& e)
        {
            LogUnreadablePacket(player, packet, e);
        }
    }

private:
    // These hooks run inside the core's send and receive paths, for real players too: a packet the module reads with
    // LoA's layout must never end the server, so the bot side skips it and logs the mismatch.
    static void LogUnreadablePacket(Player* player, WorldPacket const& packet, ByteBufferException const& e)
    {
        SF_LOG_ERROR("playerbots", "Playerbots could not read packet %u for %s: %s", uint32(packet.GetOpcode()),
            player->GetName().c_str(), e.what());
    }
};

namespace
{
// #201 (#614): azerothcore/mod-duel-reset: full health and
// mana; at the end of every duel (any end type) also every spell cooldown and Forbearance (25771).
void ResetDuelant(Player* player, bool duelEnded)
{
    if (!player)
        return;
    if (duelEnded)
    {
        player->RemoveAllSpellCooldown();
        player->RemoveAurasDueToSpell(25771);
    }
    player->SetFullHealth();
    if (player->GetMaxPower(POWER_MANA) > 0)
        player->SetPower(POWER_MANA, player->GetMaxPower(POWER_MANA));
}

// #868: refresh the activity cache (true now: PlayerbotAI::AllowActive duel rule) and end any pending AI wait, so the
// bot's next update is a full tick. Called from Player::UpdateDuelFlag (map thread, the same thread that updates both
// duellists' AI through OnAfterUpdate).
void WakeDuelant(Player* player)
{
    PlayerbotAI* botAI = player ? GET_PLAYERBOT_AI(player) : nullptr;
    if (!botAI)
        return;
    botAI->AllowActivity(ALL_ACTIVITY, true);
    // a running cast keeps its own wait (WaitForSpellCast), like the #754 battleground wake in PlayerbotAI::UpdateAI
    if (!player->IsNonMeleeSpellCasted(false, false, true))
        botAI->SetNextCheckDelay(0);
}
}  // namespace

class PlayerbotsPlayerScript : public PlayerScript
{
public:
    PlayerbotsPlayerScript() : PlayerScript("PlayerbotsPlayerScript") {}

    // #148: AC RandomBotLevelMgr.cpp:1151-1156 (ResetBotLevel on level-up).
    void OnLevelChanged(Player* player, uint8 oldLevel) override
    {
        RandomBotLevelMgrOnLevelChanged(player, oldLevel);
        // The hook can run inside the bot's own action (a quest turn-in), so the rebuild waits for its next tick.
        if (PlayerbotAI* botAI = GET_PLAYERBOT_AI(player))
            botAI->MarkStrategiesStale();

        // Design decision 0031: a human's level-up is a moment a bot near may congratulate (AI chat opener).
        AiChat::OnLevelUp(player, oldLevel);
    }

    void OnLogin(Player* player, bool /*firstLogin*/) override
    {
        if (!player->GetSession()->IsBot())
        {
            sPlayerbotsMgr->AddPlayerbotData(player, false);
            sRandomPlayerbotMgr->OnPlayerLogin(player);
            AiChat::OnHumanLogin(player);
            LikingStore::OnHumanLogin(player);

            if (sPlayerbotAIConfig->enabled || sPlayerbotAIConfig->randomBotAutologin)
            {
                std::string roundedTime =
                    std::to_string(std::ceil((sPlayerbotAIConfig->maxRandomBots * 0.11 / 60) * 10) / 10.0);
                roundedTime = roundedTime.substr(0, roundedTime.find('.') + 2);

                ChatHandler(player->GetSession()).SendSysMessage(std::string("Playerbots: bot initialization at server startup takes about '" + roundedTime + "' minutes.").c_str());
            }
        }
    }

    void OnChat(Player* player, ChatMsg chatType, Language /*lang*/, std::string& msg, Player* receiver) override
    {
        uint32 const type = uint32(chatType);  // the module keeps chat types as uint32

        if (type != CHAT_MSG_WHISPER)
            return;

        // Thanks or rudeness to a random bot moves its liking score, with the AI layer on or off (returns at once when off).
        LikingStore::OnHumanLine(player, receiver, nullptr, msg);

        if (PlayerbotAI* botAI = GET_PLAYERBOT_AI(receiver))
        {
            // A player's "inv" to a bot that leads a dungeon party is a request to join it, not a command.
            if (BotLfm::OnWhisper(receiver, player, msg))
                return;

            // Small talk to a bot goes to the AI layer when its chat is on; a command never also gets a chat reply.
            if (AiChat::OnWhisper(player, receiver, msg))
                return;

            botAI->HandleCommand(type, msg, player);
        }
    }

    // Player::Say (the only caller of this plain hook with CHAT_MSG_SAY): a nearby bot may answer a human.
    void OnChat(Player* player, ChatMsg chatType, Language /*lang*/, std::string& msg) override
    {
        if (chatType == ChatMsg::CHAT_MSG_SAY)
        {
            LikingStore::OnHumanLine(player, nullptr, nullptr, msg);
            AiChat::OnSay(player, msg);
        }
    }

    void OnChat(Player* player, ChatMsg chatType, Language /*lang*/, std::string& msg, Channel* /*channel*/) override
    {
        if (uint32(chatType) == uint32(CHAT_MSG_CHANNEL))
            BotLfm::OnChannelChat(player, msg);
    }

    void OnChat(Player* player, ChatMsg chatType, Language /*lang*/, std::string& msg, Group* group) override
    {
        uint32 const type = uint32(chatType);  // the module keeps chat types as uint32

        if (!group)
            return;

        for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            Player* const member = itr->GetSource();
            if (!member)
                continue;

            if (PlayerbotAI* botAI = GET_PLAYERBOT_AI(member))
                botAI->HandleCommand(type, msg, player);
        }

        LikingStore::OnHumanLine(player, nullptr, group, msg);

        // A simple phrase in a dungeon gets its rule reply; any other human line may get one from the AI layer.
        if (!DungeonChat::OnGroupChat(group, player, type, msg))
            AiChat::OnGroupChat(group, player, type, msg);

        BgCalloutReader::OnTeamChat(group, player, type, msg);   // BT-482: a human's call-outs in a battleground
    }

    void OnChat(Player* player, ChatMsg chatType, Language /*lang*/, std::string& msg, Guild* /*guild*/) override
    {
        uint32 const type = uint32(chatType);  // the module keeps chat types as uint32

        if (type != CHAT_MSG_GUILD)
            return;

        AiChat::OnGuildChat(player, msg);

        PlayerbotMgr* playerbotMgr = GET_PLAYERBOT_MGR(player);
        if (!playerbotMgr)
            return;

        for (PlayerBotMap::const_iterator it = playerbotMgr->GetPlayerBotsBegin(); it != playerbotMgr->GetPlayerBotsEnd(); ++it)
        {
            Player* const bot = it->second;
            if (!bot || bot->GetGuildId() != player->GetGuildId())
                continue;

            if (PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot))
                botAI->HandleCommand(type, msg, player);
        }
    }

    // Design decision 0031: a dungeon or world boss killed by a group with humans and bots is a shared moment for the AI
    // layer's memory. The core calls this for a player's killing blow only (Unit::Kill), not for a pet's.
    void OnCreatureKill(Player* killer, Creature* killed) override
    {
        AiChat::OnBossKilled(killer, killed);
    }

    // #201 (#614): duel reset for EVERY duel inside a duel spot, bots and players alike (owner decision). Gated by
    // DuelSpotEnable && DuelSpotReset. Hooks: ScriptMgr.h:752-755, called from Player::UpdateDuelFlag / DuelComplete.
    void OnDuelStart(Player* player1, Player* player2) override
    {
        // #868 (BEYOND AC): wake both bot sides when the fight starts, for every duel. AllowActive now answers true in a
        // duel, but a bot that cached "inactive" just before (AllowActivity, ~5 s cache) ran a minimal tick and then
        // slept PassiveDelay (10 s): the accepting bot answered the request in such a tick and stood ~7 s into the fight.
        WakeDuelant(player1);
        WakeDuelant(player2);
        if (!sPlayerbotAIConfig->duelSpotEnable || !sPlayerbotAIConfig->duelSpotReset ||
            !sDuelSpotMgr->IsDuelInSpot(player1, player2))
            return;
        ResetDuelant(player1, false);
        ResetDuelant(player2, false);
    }

    void OnDuelEnd(Player* winner, Player* loser, DuelCompleteType type) override
    {
        // #201 (#612, wave-13 t1): the no-rematch window starts here, for every end type (also a decline), whatever
        // DuelSpotReset says. NoteDuelEnd itself checks DuelSpotEnable and IsDuelInSpot.
        sDuelSpotMgr->NoteDuelEnd(winner, loser);
        // Interrupted says nothing about who was better; the one who fled lost (Player::DuelComplete passes it as loser).
        AiChat::OnDuelEnd(winner, loser, type != DuelCompleteType::DUEL_INTERRUPTED, type == DuelCompleteType::DUEL_FLED);
        // A duel fought to its end (not an interrupted one) is a shared moment for the liking score (random bot and human only).
        if (type != DuelCompleteType::DUEL_INTERRUPTED)
            LikingStore::OnDuelEnded(winner, loser);
        // #201 (SPEC amendment A3): reset on EVERY end type (WON / FLED / INTERRUPTED / SURRENDER), owner decision.
        if (!sPlayerbotAIConfig->duelSpotEnable || !sPlayerbotAIConfig->duelSpotReset ||
            !sDuelSpotMgr->IsDuelInSpot(winner, loser))
            return;
        ResetDuelant(winner, true);
        ResetDuelant(loser, true);
    }

    // A deleted character takes its liking rows with it, as a human and as a bot (privacy, docs/social/social.md).
    void OnDelete(uint64 guid) override
    {
        LikingStore::OnPlayerDeleted(guid);
    }

    void OnLogout(Player* player) override
    {
        if (PlayerbotMgr* playerbotMgr = GET_PLAYERBOT_MGR(player))
        {
            PlayerbotAI* botAI = GET_PLAYERBOT_AI(player);
            if (!botAI || botAI->IsRealPlayer())
                playerbotMgr->LogoutAllBots();
        }

        sRandomPlayerbotMgr->OnPlayerLogout(player);
        AiChat::OnHumanLogout(player);
        LikingStore::OnHumanLogout(player);

        // #BT-009, AC Script/Playerbots.cpp:404 (OnDestructPlayer): the manager belongs to this player object, which the
        // core deletes right after this hook; SkyFire has no destruct hook, so a new one leaked on every human login
        if (PlayerbotMgr* playerbotMgr = GET_PLAYERBOT_MGR(player))
            delete playerbotMgr;
    }

    void OnUpdate(Player* player, uint32 diff) override
    {
        if (PlayerbotAI* botAI = GET_PLAYERBOT_AI(player))
        {
            botAI->UpdateAI(diff);
        }

        if (PlayerbotMgr* playerbotMgr = GET_PLAYERBOT_MGR(player))
        {
            playerbotMgr->UpdateAI(diff);
            // Design decision 0031: the human's own timers for AI chat openers and friendly whispers (returns at once when
            // off).
            AiChat::OnHumanUpdate(player, diff);
        }
    }
};
// Design decision 0031: group joins for the AI layer's chat (a human joining bots, a bot joining humans). Group::AddMember
// calls it after the member is in, for every way into a group (invites, the Dungeon Finder, raids).
class PlayerbotsGroupScript : public GroupScript
{
public:
    PlayerbotsGroupScript() : GroupScript("PlayerbotsGroupScript") {}

    void OnAddMember(Group* group, uint64 guid) override
    {
        AiChat::OnGroupMemberAdded(group, guid);
        LikingStore::OnGroupMemberAdded(group, guid);  // the join time that starts the kick grace
    }

    // Called before the member is removed (Group::RemoveMember): a human kicking a random bot lowers its liking score.
    void OnRemoveMember(Group* group, uint64 guid, RemoveMethod method, uint64 kicker, char const* /*reason*/) override
    {
        LikingStore::OnGroupMemberRemoved(group, guid, uint8(method), kicker);
    }
};

void AddSC_mod_playerbots()
{
    new mod_playerbots();

    new PlayerbotsServerScript();
    new PlayerbotsWorldScript();
    new PlayerbotsPlayerScript();
    new PlayerbotsGroupScript();

    AddSC_playerbots_commandscript();
    AddSC_npc_bot_arena_ladder();
    AddSC_bot_damage_watch();
    AddSC_bot_auction();
}
